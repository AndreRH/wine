/*
 * PowerPC64 signal handling routines
 *
 * Copyright 2019 Timothy Pearson <tpearson@raptorengineering.com>
 * Copyright 2020 André Hentschel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifdef __powerpc64__

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/exception.h"
#include "ntdll_misc.h"
#include "wine/debug.h"
#include "winnt.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);

/* layering violation: the setjmp buffer is defined in msvcrt, but used by RtlUnwindEx */
struct MSVCRT_JUMP_BUFFER
{
    unsigned __int64 Frame;
    unsigned __int64 Reserved;
    unsigned __int64 X19;
    unsigned __int64 X20;
    unsigned __int64 X21;
    unsigned __int64 X22;
    unsigned __int64 X23;
    unsigned __int64 X24;
    unsigned __int64 X25;
    unsigned __int64 X26;
    unsigned __int64 X27;
    unsigned __int64 X28;
    unsigned __int64 Fp;
    unsigned __int64 Lr;
    unsigned __int64 Sp;
    unsigned long Fpcr;
    unsigned long Fpsr;
    double D[8];
};


/*******************************************************************
 *         is_valid_frame
 */
static inline BOOL is_valid_frame( void *frame )
{
    if ((ULONG_PTR)frame & 7) return FALSE;
    return (frame >= NtCurrentTeb()->Tib.StackLimit &&
            (void **)frame < (void **)NtCurrentTeb()->Tib.StackBase - 1);
}


/***********************************************************************
 *		RtlCaptureContext (NTDLL.@)
 */
__ASM_STDCALL_FUNC( RtlCaptureContext, 8,
                    /* TODO: fp regs */
                    "std  0, 264(3)\n\t"        /* context->Gpr0 */
                    "std  1, 272(3)\n\t"        /* context->Gpr1 */
                    "std  2, 280(3)\n\t"        /* context->Gpr2 */
                    "std  3, 288(3)\n\t"        /* context->Gpr3 */
                    "std  4, 296(3)\n\t"        /* context->Gpr4 */
                    "std  5, 304(3)\n\t"        /* context->Gpr5 */
                    "std  6, 312(3)\n\t"        /* context->Gpr6 */
                    "std  7, 320(3)\n\t"        /* context->Gpr7 */
                    "std  8, 328(3)\n\t"        /* context->Gpr8 */
                    "std  9, 336(3)\n\t"        /* context->Gpr9 */
                    "std 10, 344(3)\n\t"        /* context->Gpr10 */
                    "std 11, 352(3)\n\t"        /* context->Gpr11 */
                    "std 12, 360(3)\n\t"        /* context->Gpr12 */
                    "std 13, 368(3)\n\t"        /* context->Gpr13 */
                    "std 14, 376(3)\n\t"        /* context->Gpr14 */
                    "std 15, 384(3)\n\t"        /* context->Gpr15 */
                    "std 16, 392(3)\n\t"        /* context->Gpr16 */
                    "std 17, 400(3)\n\t"        /* context->Gpr17 */
                    "std 18, 408(3)\n\t"        /* context->Gpr18 */
                    "std 19, 416(3)\n\t"        /* context->Gpr19 */
                    "std 20, 424(3)\n\t"        /* context->Gpr20 */
                    "std 21, 432(3)\n\t"        /* context->Gpr21 */
                    "std 22, 440(3)\n\t"        /* context->Gpr22 */
                    "std 23, 448(3)\n\t"        /* context->Gpr23 */
                    "std 24, 456(3)\n\t"        /* context->Gpr24 */
                    "std 25, 464(3)\n\t"        /* context->Gpr25 */
                    "std 26, 472(3)\n\t"        /* context->Gpr26 */
                    "std 27, 480(3)\n\t"        /* context->Gpr27 */
                    "std 28, 488(3)\n\t"        /* context->Gpr28 */
                    "std 29, 496(3)\n\t"        /* context->Gpr29 */
                    "std 30, 504(3)\n\t"        /* context->Gpr30 */
                    "std 31, 512(3)\n\t"        /* context->Gpr31 */
                    /* TODO: cr */
                    /* TODO: xer */
                    /* TODO:  msr (536) */
                    "mflr 0\n\t"
                    "std 0, 544(3)\n\t"         /* context->Iar */
                    "std 0, 552(3)\n\t"         /* context->Lr */
                    "mfctr 0\n\t"
                    "std 0, 560(3)\n\t"         /* context->Ctr */
                    "lis 0, 0x80\n\t"
                    "ori 0, 0, 7\n\t"
                    "std 0, 568(1)\n\t"         /* context->ContextFlags = CONTEXT_FULL */
                    "blr" )


/**********************************************************************
 *           call_stack_handlers
 *
 * Call the stack handlers chain.
 */
static NTSTATUS call_stack_handlers( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    EXCEPTION_REGISTRATION_RECORD *frame, *dispatch, *nested_frame;
    DWORD res;

    frame = NtCurrentTeb()->Tib.ExceptionList;
    nested_frame = NULL;
    while (frame != (EXCEPTION_REGISTRATION_RECORD*)~0UL)
    {
        /* Check frame address */
        if (!is_valid_frame( frame ))
        {
            rec->ExceptionFlags |= EH_STACK_INVALID;
            break;
        }

        /* Call handler */
        TRACE( "calling handler at %p code=%x flags=%x\n",
               frame->Handler, rec->ExceptionCode, rec->ExceptionFlags );
        res = frame->Handler( rec, frame, context, &dispatch );
        TRACE( "handler at %p returned %x\n", frame->Handler, res );

        if (frame == nested_frame)
        {
            /* no longer nested */
            nested_frame = NULL;
            rec->ExceptionFlags &= ~EH_NESTED_CALL;
        }

        switch(res)
        {
        case ExceptionContinueExecution:
            if (!(rec->ExceptionFlags & EH_NONCONTINUABLE)) return STATUS_SUCCESS;
            return STATUS_NONCONTINUABLE_EXCEPTION;
        case ExceptionContinueSearch:
            break;
        case ExceptionNestedException:
            if (nested_frame < dispatch) nested_frame = dispatch;
            rec->ExceptionFlags |= EH_NESTED_CALL;
            break;
        default:
            return STATUS_INVALID_DISPOSITION;
        }
        frame = frame->Prev;
    }
    return STATUS_UNHANDLED_EXCEPTION;
}


/*******************************************************************
 *		KiUserExceptionDispatcher (NTDLL.@)
 */
NTSTATUS WINAPI KiUserExceptionDispatcher( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    NTSTATUS status;
    DWORD c;

    TRACE( "code=%x flags=%x addr=%p pc=%lx tid=%04x\n",
           rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress,
           context->Iar, GetCurrentThreadId() );
    for (c = 0; c < rec->NumberParameters; c++)
        TRACE( " info[%d]=%016lx\n", c, rec->ExceptionInformation[c] );

    if (rec->ExceptionCode == EXCEPTION_WINE_STUB)
    {
        if (rec->ExceptionInformation[1] >> 16)
            MESSAGE( "wine: Call from %p to unimplemented function %s.%s, aborting\n",
                     rec->ExceptionAddress,
                     (char*)rec->ExceptionInformation[0], (char*)rec->ExceptionInformation[1] );
        else
            MESSAGE( "wine: Call from %p to unimplemented function %s.%ld, aborting\n",
                     rec->ExceptionAddress,
                     (char*)rec->ExceptionInformation[0], rec->ExceptionInformation[1] );
    }
    else if (rec->ExceptionCode == EXCEPTION_WINE_NAME_THREAD && rec->ExceptionInformation[0] == 0x1000)
    {
        WARN( "Thread %04x renamed to %s\n", (DWORD)rec->ExceptionInformation[2], debugstr_a((char *)rec->ExceptionInformation[1]) );
    }
    else if (rec->ExceptionCode == DBG_PRINTEXCEPTION_C)
    {
        WARN( "%s\n", debugstr_an((char *)rec->ExceptionInformation[1], rec->ExceptionInformation[0] - 1) );
    }
    else if (rec->ExceptionCode == DBG_PRINTEXCEPTION_WIDE_C)
    {
        WARN( "%s\n", debugstr_wn((WCHAR *)rec->ExceptionInformation[1], rec->ExceptionInformation[0] - 1) );
    }
    else
    {
        if (rec->ExceptionFlags & EH_NONCONTINUABLE)
            ERR( "Fatal %s exception (code=%x) raised\n", debugstr_exception_code(rec->ExceptionCode), rec->ExceptionCode );
        else
            WARN( "%s exception (code=%x) raised\n", debugstr_exception_code(rec->ExceptionCode), rec->ExceptionCode );

        TRACE("  r0=%016lx  r1=%016lx  r2=%016lx  r3=%016lx\n",
              context->Gpr0, context->Gpr1, context->Gpr2, context->Gpr3 );
        TRACE("  r4=%016lx  r5=%016lx  r6=%016lx  r7=%016lx\n",
              context->Gpr4, context->Gpr5, context->Gpr6, context->Gpr7 );
        TRACE("  r8=%016lx  r9=%016lx r10=%016lx r11=%016lx\n",
              context->Gpr8, context->Gpr9, context->Gpr10, context->Gpr11 );
        TRACE(" r12=%016lx r13=%016lx r14=%016lx r15=%016lx\n",
              context->Gpr12, context->Gpr13, context->Gpr14, context->Gpr15 );
        TRACE(" r16=%016lx r17=%016lx r18=%016lx r19=%016lx\n",
              context->Gpr16, context->Gpr17, context->Gpr18, context->Gpr19 );
        TRACE(" r20=%016lx r21=%016lx r22=%016lx r23=%016lx\n",
              context->Gpr20, context->Gpr21, context->Gpr22, context->Gpr23 );
        TRACE(" r24=%016lx r25=%016lx r26=%016lx r27=%016lx\n",
              context->Gpr24, context->Gpr25, context->Gpr26, context->Gpr27 );
        TRACE(" r28=%016lx r29=%016lx r30=%016lx r31=%016lx\n",
              context->Gpr28, context->Gpr29, context->Gpr30, context->Gpr31 );
        TRACE(" ctr=%016lx  fpscr=%016lx  lr=%016lx\n",
              context->Ctr, context->Fpscr, context->Lr );
    }

    if (call_vectored_handlers( rec, context ) == EXCEPTION_CONTINUE_EXECUTION)
        NtContinue( context, FALSE );

    if ((status = call_stack_handlers( rec, context )) == STATUS_SUCCESS)
        NtContinue( context, FALSE );

    if (status != STATUS_UNHANDLED_EXCEPTION) RtlRaiseStatus( status );
    return NtRaiseException( rec, context, FALSE );
}

/*******************************************************************
 *             KiUserApcDispatcher (NTDLL.@)
 */
void WINAPI KiUserApcDispatcher( CONTEXT *context, ULONG_PTR ctx, ULONG_PTR arg1, ULONG_PTR arg2,
                                 PNTAPCFUNC func )
{
    func( ctx, arg1, arg2 );
    NtContinue( context, TRUE );
}

/**********************************************************************
 *              RtlVirtualUnwind   (NTDLL.@)
 */
PVOID WINAPI RtlVirtualUnwind( ULONG type, ULONG_PTR base, ULONG_PTR pc,
                               void *func, CONTEXT *context,
                               PVOID *handler_data, ULONG_PTR *frame_ret,
                               void *ctx_ptr )
{
    FIXME( "stub!\n" );
    return NULL;
}

/*******************************************************************
 *              RtlRestoreContext (NTDLL.@)
 */
void CDECL RtlRestoreContext( CONTEXT *context, EXCEPTION_RECORD *rec )
{
    FIXME( "stub!\n" );
}

/***********************************************************************
 *            RtlUnwind  (NTDLL.@)
 */
void WINAPI RtlUnwind( void *end_frame, void *target_ip, EXCEPTION_RECORD *rec, void *retval )
{
    CONTEXT context;
    EXCEPTION_RECORD record;
    EXCEPTION_REGISTRATION_RECORD *frame, *dispatch;
    DWORD res;

    RtlCaptureContext( &context );
    context.Gpr3 = (DWORD64)retval;

    /* build an exception record, if we do not have one */
    if (!rec)
    {
        record.ExceptionCode    = STATUS_UNWIND;
        record.ExceptionFlags   = 0;
        record.ExceptionRecord  = NULL;
        record.ExceptionAddress = (void *)context.Iar;
        record.NumberParameters = 0;
        rec = &record;
    }

    rec->ExceptionFlags |= EH_UNWINDING | (end_frame ? 0 : EH_EXIT_UNWIND);

    TRACE( "code=%x flags=%x\n", rec->ExceptionCode, rec->ExceptionFlags );

    /* get chain of exception frames */
    frame = NtCurrentTeb()->Tib.ExceptionList;
    while ((frame != (EXCEPTION_REGISTRATION_RECORD*)~0UL) && (frame != end_frame))
    {
        /* Check frame address */
        if (end_frame && ((void*)frame > end_frame))
            raise_status( STATUS_INVALID_UNWIND_TARGET, rec );

        if (!is_valid_frame( frame )) raise_status( STATUS_BAD_STACK, rec );

        /* Call handler */
        TRACE( "calling handler at %p code=%x flags=%x\n",
               frame->Handler, rec->ExceptionCode, rec->ExceptionFlags );
        res = frame->Handler(rec, frame, &context, &dispatch);
        TRACE( "handler at %p returned %x\n", frame->Handler, res );

        switch(res)
        {
        case ExceptionContinueSearch:
            break;
        case ExceptionCollidedUnwind:
            frame = dispatch;
            break;
        default:
            raise_status( STATUS_INVALID_DISPOSITION, rec );
            break;
        }
        frame = __wine_pop_frame( frame );
    }
}


/***********************************************************************
 *		RtlRaiseException (NTDLL.@)
 */
__ASM_STDCALL_FUNC( RtlRaiseException, 4,
                   "mflr 0\n\t"
                   "std 0, 16(1)\n\t"
                   "stdu 1, -0x2d8(1)\n\t"      /* 0x298 (context) + 0x40 */
                   "std 3, 32(1)\n\t"
                   "addi 3, 1, 0x40\n\t"
                   "lis 12, " __ASM_NAME("RtlCaptureContext") "@highest\n\t"
                   "ori 12, 12, " __ASM_NAME("RtlCaptureContext") "@higher\n\t"
                   "rldicr 12, 12, 32, 31\n\t"
                   "oris 12, 12, " __ASM_NAME("RtlCaptureContext") "@high\n\t"
                   "ori 12, 12, " __ASM_NAME("RtlCaptureContext") "@l\n\t"
                   "mtctr 12\n\t"
                   "bctrl\n\t"
                   "ori 0, 0, 0\n\t"
                   "addi 4, 1, 0x40\n\t"        /* context pointer */
                   "addi 5, 1, 0x2d8\n\t"       /* orig stack pointer */
                   "std 5, 272(4)\n\t"          /* context->Gpr1 aka SP */
                   "ld 3, 32(1)\n\t"            /* original first parameter */
                   "std 3, 288(4)\n\t"          /* context->Gpr3 */
                   "ld 6, 0x2e8(1)\n\t"
                   "std 6, 544(4)\n\t"          /* context->Iar */
                   "std 6, 552(4)\n\t"          /* context->Lr */
                   "std 6, 16(3)\n\t"           /* rec->ExceptionAddress */
                   "li 5, 1\n\t"
                   "lis 12, " __ASM_NAME("NtRaiseException") "@highest\n\t"
                   "ori 12, 12, " __ASM_NAME("NtRaiseException") "@higher\n\t"
                   "rldicr 12, 12, 32, 31\n\t"
                   "oris 12, 12, " __ASM_NAME("NtRaiseException") "@high\n\t"
                   "ori 12, 12, " __ASM_NAME("NtRaiseException") "@l\n\t"
                   "mtctr 12\n\t"
                   "bctrl\n\t"
                   "ori 0, 0, 0\n\t"
                   "lis 12, " __ASM_NAME("RtlRaiseStatus") "@highest\n\t"
                   "ori 12, 12, " __ASM_NAME("RtlRaiseStatus") "@higher\n\t"
                   "rldicr 12, 12, 32, 31\n\t"
                   "oris 12, 12, " __ASM_NAME("RtlRaiseStatus") "@high\n\t"
                   "ori 12, 12, " __ASM_NAME("RtlRaiseStatus") "@l\n\t"
                   "mtctr 12\n\t"
                   "bctrl\n\t"                  /* does not return */
                   "ori 0, 0, 0" );

/*************************************************************************
 *             RtlCaptureStackBackTrace (NTDLL.@)
 */
USHORT WINAPI RtlCaptureStackBackTrace( ULONG skip, ULONG count, PVOID *buffer, ULONG *hash )
{
    FIXME( "(%d, %d, %p, %p) stub!\n", skip, count, buffer, hash );
    return 0;
}

/***********************************************************************
 *           signal_start_thread
 */
__ASM_GLOBAL_FUNC( signal_start_thread,
                   "mr 1, 3\n\t"  /* context */
                   "li 4, 1\n\t"
                   "lis 12, " __ASM_NAME("NtContinue") "@highest\n\t"
                   "ori 12, 12, " __ASM_NAME("NtContinue") "@higher\n\t"
                   "rldicr 12, 12, 32, 31\n\t"
                   "oris 12, 12, " __ASM_NAME("NtContinue") "@high\n\t"
                   "ori 12, 12, " __ASM_NAME("NtContinue") "@l\n\t"
                   "mtctr 12\n\t"
                   "bctr" )

/**********************************************************************
 *              DbgBreakPoint   (NTDLL.@)
 */
void WINAPI DbgBreakPoint(void)
{
    FIXME( "stub!\n" );
}

/**********************************************************************
 *              DbgUserBreakPoint   (NTDLL.@)
 */
void WINAPI DbgUserBreakPoint(void)
{
    FIXME( "stub!\n" );
}
/**********************************************************************
 *           NtCurrentTeb   (NTDLL.@)
 */
TEB * WINAPI NtCurrentTeb(void)
{
    return unix_funcs->NtCurrentTeb();
}

#endif  /* __aarch64__ */
