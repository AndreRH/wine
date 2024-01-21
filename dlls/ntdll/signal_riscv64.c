/*
 * RISCV64 signal handling routines
 *
 * Copyright 2023 André Zwing
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

#ifdef __riscv64__

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/exception.h"
#include "ntdll_misc.h"
#include "wine/debug.h"
#include "ntsyscalls.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);
WINE_DECLARE_DEBUG_CHANNEL(threadname);

TEB * (* WINAPI __wine_current_teb)(void) = NULL;

typedef struct _SCOPE_TABLE
{
    ULONG Count;
    struct
    {
        ULONG BeginAddress;
        ULONG EndAddress;
        ULONG HandlerAddress;
        ULONG JumpTarget;
    } ScopeRecord[1];
} SCOPE_TABLE, *PSCOPE_TABLE;


/* layering violation: the setjmp buffer is defined in msvcrt, but used by RtlUnwindEx */
struct MSVCRT_JUMP_BUFFER
{
    unsigned __int64 Frame;
    unsigned __int64 Reserved;
    unsigned __int64 Fp;
    unsigned __int64 S1;
    unsigned __int64 S2;
    unsigned __int64 S3;
    unsigned __int64 S4;
    unsigned __int64 S5;
    unsigned __int64 S6;
    unsigned __int64 S7;
    unsigned __int64 S8;
    unsigned __int64 S9;
    unsigned __int64 S10;
    unsigned __int64 S11;
    unsigned __int64 Ra;
    unsigned __int64 Sp;
    /*unsigned long Fpcr;
    unsigned long Fpsr;
    double D[8];*/
} _JUMP_BUFFER;

static void dump_scope_table( ULONG64 base, const SCOPE_TABLE *table )
{
    unsigned int i;

    TRACE( "scope table at %p\n", table );
    for (i = 0; i < table->Count; i++)
        TRACE( "  %u: %I64x-%I64x handler %I64x target %I64x\n", i,
               base + table->ScopeRecord[i].BeginAddress,
               base + table->ScopeRecord[i].EndAddress,
               base + table->ScopeRecord[i].HandlerAddress,
               base + table->ScopeRecord[i].JumpTarget );
}

/*******************************************************************
 *         is_valid_frame
 */
static inline BOOL is_valid_frame( ULONG_PTR frame )
{
    if (frame & 7) return FALSE;
    return ((void *)frame >= NtCurrentTeb()->Tib.StackLimit &&
            (void *)frame <= NtCurrentTeb()->Tib.StackBase);
}

/*******************************************************************
 *         syscalls
 */
#define SYSCALL_ENTRY(id,name,args) __ASM_SYSCALL_FUNC( id, name )
ALL_SYSCALLS64
#undef SYSCALL_ENTRY


/**************************************************************************
 *		__chkstk (NTDLL.@)
 *
 * Supposed to touch all the stack pages, but we shouldn't need that.
 */
__ASM_GLOBAL_FUNC( __chkstk, "ret")


/***********************************************************************
 *		RtlCaptureContext (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( RtlCaptureContext,
                    "sd ra, 0x8(a0)\n\t"         /* context->PC */
                    "sd zero, 0x10(a0)\n\t"         /* context->X1 */
                    "sd sp, 0x18(a0)\n\t"         /* context->X2 */
                    "sd gp, 0x20(a0)\n\t"         /* context->X3 */
                    "sd tp, 0x28(a0)\n\t"         /* context->X4 */
                    "sd t0, 0x30(a0)\n\t"         /* context->X5 */
                    "sd t1, 0x38(a0)\n\t"         /* context->X6 */
                    "sd t2, 0x40(a0)\n\t"         /* context->X7 */
                    "sd s0, 0x48(a0)\n\t"         /* context->X8 */
                    "sd s1, 0x50(a0)\n\t"         /* context->X9 */
                    "sd zero, 0x58(a0)\n\t"         /* context->X10 */
                    "sd a1, 0x60(a0)\n\t"         /* context->X11 */
                    "sd a2, 0x68(a0)\n\t"         /* context->X12 */
                    "sd a3, 0x70(a0)\n\t"         /* context->X13 */
                    "sd a4, 0x78(a0)\n\t"         /* context->X14 */
                    "sd a5, 0x80(a0)\n\t"         /* context->X15 */
                    "sd a6, 0x88(a0)\n\t"         /* context->X16 */
                    "sd a7, 0x90(a0)\n\t"         /* context->X17 */
                    "sd s2, 0x98(a0)\n\t"         /* context->X18 */
                    "sd s3, 0xa0(a0)\n\t"         /* context->X19 */
                    "sd s4, 0xa8(a0)\n\t"         /* context->X20 */
                    "sd s5, 0xb0(a0)\n\t"         /* context->X21 */
                    "sd s6, 0xb8(a0)\n\t"         /* context->X22 */
                    "sd s7, 0xc0(a0)\n\t"         /* context->X23 */
                    "sd s8, 0xc8(a0)\n\t"         /* context->X24 */
                    "sd s9, 0xd0(a0)\n\t"         /* context->X25 */
                    "sd s10, 0xd8(a0)\n\t"         /* context->X26 */
                    "sd s11, 0xe0(a0)\n\t"         /* context->X27 */
                    "sd t3, 0xe8(a0)\n\t"         /* context->X28 */
                    "sd t4, 0xf0(a0)\n\t"         /* context->X29 */
                    "sd t5, 0xf8(a0)\n\t"         /* context->X30 */
                    "sd t6, 0x100(a0)\n\t"         /* context->X31 */
                    "fsd f0, 0x104(a0)\n\t"         /* context->F0 */
                    "fsd f1, 0x10c(a0)\n\t"         /* context->F1 */
                    "fsd f2, 0x114(a0)\n\t"         /* context->F2 */
                    "fsd f3, 0x11c(a0)\n\t"         /* context->F3 */
                    "fsd f4, 0x124(a0)\n\t"         /* context->F4 */
                    "fsd f5, 0x12c(a0)\n\t"         /* context->F5 */
                    "fsd f6, 0x134(a0)\n\t"         /* context->F6 */
                    "fsd f7, 0x13c(a0)\n\t"         /* context->F7 */
                    "fsd f8, 0x144(a0)\n\t"         /* context->F8 */
                    "fsd f9, 0x14c(a0)\n\t"         /* context->F9 */
                    "fsd f10, 0x154(a0)\n\t"         /* context->F10 */
                    "fsd f11, 0x15c(a0)\n\t"         /* context->F11 */
                    "fsd f12, 0x164(a0)\n\t"         /* context->F12 */
                    "fsd f13, 0x16c(a0)\n\t"         /* context->F13 */
                    "fsd f14, 0x174(a0)\n\t"         /* context->F14 */
                    "fsd f15, 0x17c(a0)\n\t"         /* context->F15 */
                    "fsd f16, 0x184(a0)\n\t"         /* context->F16 */
                    "fsd f17, 0x18c(a0)\n\t"         /* context->F17 */
                    "fsd f18, 0x194(a0)\n\t"         /* context->F18 */
                    "fsd f19, 0x19c(a0)\n\t"         /* context->F19 */
                    "fsd f20, 0x1a4(a0)\n\t"         /* context->F20 */
                    "fsd f21, 0x1ac(a0)\n\t"         /* context->F21 */
                    "fsd f22, 0x1b4(a0)\n\t"         /* context->F22 */
                    "fsd f23, 0x1bc(a0)\n\t"         /* context->F23 */
                    "fsd f24, 0x1c4(a0)\n\t"         /* context->F24 */
                    "fsd f25, 0x1cc(a0)\n\t"         /* context->F25 */
                    "fsd f26, 0x1d4(a0)\n\t"         /* context->F26 */
                    "fsd f27, 0x1dc(a0)\n\t"         /* context->F27 */
                    "fsd f28, 0x1e4(a0)\n\t"         /* context->F28 */
                    "fsd f29, 0x1ec(a0)\n\t"         /* context->F29 */
                    "fsd f30, 0x1f4(a0)\n\t"         /* context->F30 */
                    "fsd f31, 0x1fc(a0)\n\t"         /* context->F31 */
                    "li t0, 0x800000\n\t"         /* CONTEXT_RISC64 */
                    "addi t0, t0, 0x7\n\t"         /* CONTEXT_FULL */
                    "sw t0, 0(a0)\n\t"         /* context->ContextFlags */
                    "ret" ) /* FIXME: FPCR FPSR CPSR? */


/**********************************************************************
 *           virtual_unwind
 */
static NTSTATUS virtual_unwind( ULONG type, DISPATCHER_CONTEXT *dispatch, CONTEXT *context )
{
    LDR_DATA_TABLE_ENTRY *module;
    NTSTATUS status;
    DWORD64 pc;

    dispatch->ImageBase        = 0;
    dispatch->ScopeIndex       = 0;
    dispatch->EstablisherFrame = 0;
    dispatch->ControlPc        = context->Pc;
    /*
     * TODO: CONTEXT_UNWOUND_TO_CALL should be cleared if unwound past a
     * signal frame.
     */
    dispatch->ControlPcIsUnwound = (context->ContextFlags & CONTEXT_UNWOUND_TO_CALL) != 0;
    pc = context->Pc - (dispatch->ControlPcIsUnwound ? 4 : 0);

    /* first look for PE exception information */

    if ((dispatch->FunctionEntry = lookup_function_info(pc,
             &dispatch->ImageBase, &module )))
    {
        dispatch->LanguageHandler = RtlVirtualUnwind( type, dispatch->ImageBase, pc,
                                                      dispatch->FunctionEntry, context,
                                                      &dispatch->HandlerData, &dispatch->EstablisherFrame,
                                                      NULL );
        return STATUS_SUCCESS;
    }

    /* then look for host system exception information */

    if (!module || (module->Flags & LDR_WINE_INTERNAL))
    {
        struct unwind_builtin_dll_params params = { type, dispatch, context };

        status = WINE_UNIX_CALL( unix_unwind_builtin_dll, &params );
        if (status != STATUS_SUCCESS) return status;

        if (dispatch->EstablisherFrame)
        {
            dispatch->FunctionEntry = NULL;
            if (dispatch->LanguageHandler && !module)
            {
                FIXME( "calling personality routine in system library not supported yet\n" );
                dispatch->LanguageHandler = NULL;
            }
            return STATUS_SUCCESS;
        }
    }
    else
    {
        status = context->Pc != context->Ra ?
                 STATUS_SUCCESS : STATUS_INVALID_DISPOSITION;
        WARN( "exception data not found in %s for %p, LR %p, status %lx\n",
               debugstr_w(module->BaseDllName.Buffer), (void*) context->Pc,
               (void*) context->Ra, status );
        dispatch->EstablisherFrame = context->Sp;
        dispatch->LanguageHandler = NULL;
        context->Pc = context->Ra;
        context->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
        return status;
    }

    dispatch->EstablisherFrame = context->Fp;
    dispatch->LanguageHandler = NULL;
    context->Pc = context->Ra;
    context->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
    return STATUS_SUCCESS;
}


struct unwind_exception_frame
{
    EXCEPTION_REGISTRATION_RECORD frame;
    DISPATCHER_CONTEXT *dispatch;
};

/**********************************************************************
 *           unwind_exception_handler
 *
 * Handler for exceptions happening while calling an unwind handler.
 */
static DWORD __cdecl unwind_exception_handler( EXCEPTION_RECORD *rec, EXCEPTION_REGISTRATION_RECORD *frame,
                                               CONTEXT *context, EXCEPTION_REGISTRATION_RECORD **dispatcher )
{
    struct unwind_exception_frame *unwind_frame = (struct unwind_exception_frame *)frame;
    DISPATCHER_CONTEXT *dispatch = (DISPATCHER_CONTEXT *)dispatcher;

    /* copy the original dispatcher into the current one, except for the TargetIp */
    dispatch->ControlPc        = unwind_frame->dispatch->ControlPc;
    dispatch->ImageBase        = unwind_frame->dispatch->ImageBase;
    dispatch->FunctionEntry    = unwind_frame->dispatch->FunctionEntry;
    dispatch->EstablisherFrame = unwind_frame->dispatch->EstablisherFrame;
    dispatch->ContextRecord    = unwind_frame->dispatch->ContextRecord;
    dispatch->LanguageHandler  = unwind_frame->dispatch->LanguageHandler;
    dispatch->HandlerData      = unwind_frame->dispatch->HandlerData;
    dispatch->HistoryTable     = unwind_frame->dispatch->HistoryTable;
    dispatch->ScopeIndex       = unwind_frame->dispatch->ScopeIndex;
    TRACE( "detected collided unwind\n" );
    return ExceptionCollidedUnwind;
}

/**********************************************************************
 *           call_unwind_handler
 *
 * Call a single unwind handler.
 */
static DWORD call_unwind_handler( EXCEPTION_RECORD *rec, DISPATCHER_CONTEXT *dispatch )
{
    struct unwind_exception_frame frame;
    DWORD res;

    frame.frame.Handler = unwind_exception_handler;
    frame.dispatch = dispatch;
    __wine_push_frame( &frame.frame );

    TRACE( "calling handler %p (rec=%p, frame=%I64x context=%p, dispatch=%p)\n",
         dispatch->LanguageHandler, rec, dispatch->EstablisherFrame, dispatch->ContextRecord, dispatch );
    res = dispatch->LanguageHandler( rec, (void *)dispatch->EstablisherFrame, dispatch->ContextRecord, dispatch );
    TRACE( "handler %p returned %lx\n", dispatch->LanguageHandler, res );

    __wine_pop_frame( &frame.frame );

    switch (res)
    {
    case ExceptionContinueSearch:
    case ExceptionCollidedUnwind:
        break;
    default:
        raise_status( STATUS_INVALID_DISPOSITION, rec );
        break;
    }

    return res;
}


/**********************************************************************
 *           call_teb_unwind_handler
 *
 * Call a single unwind handler from the TEB chain.
 */
static DWORD call_teb_unwind_handler( EXCEPTION_RECORD *rec, DISPATCHER_CONTEXT *dispatch,
                                     EXCEPTION_REGISTRATION_RECORD *teb_frame )
{
    DWORD res;

    TRACE( "calling TEB handler %p (rec=%p, frame=%p context=%p, dispatch=%p)\n",
           teb_frame->Handler, rec, teb_frame, dispatch->ContextRecord, dispatch );
    res = teb_frame->Handler( rec, teb_frame, dispatch->ContextRecord, (EXCEPTION_REGISTRATION_RECORD**)dispatch );
    TRACE( "handler at %p returned %lu\n", teb_frame->Handler, res );

    switch (res)
    {
    case ExceptionContinueSearch:
    case ExceptionCollidedUnwind:
        break;
    default:
        raise_status( STATUS_INVALID_DISPOSITION, rec );
        break;
    }

    return res;
}


static DWORD __cdecl nested_exception_handler( EXCEPTION_RECORD *rec, EXCEPTION_REGISTRATION_RECORD *frame,
                                               CONTEXT *context, EXCEPTION_REGISTRATION_RECORD **dispatcher )
{
	TRACE("bam\n");
    if (!(rec->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND)))
        rec->ExceptionFlags |= EH_NESTED_CALL;

    return ExceptionContinueSearch;
}


/**********************************************************************
 *           call_handler
 *
 * Call a single exception handler.
 * FIXME: Handle nested exceptions.
 */
static DWORD call_handler( EXCEPTION_RECORD *rec, CONTEXT *context, DISPATCHER_CONTEXT *dispatch )
{
    EXCEPTION_REGISTRATION_RECORD frame;
    DWORD res;

    frame.Handler = nested_exception_handler;
    __wine_push_frame( &frame );

    TRACE( "calling handler %p (rec=%p, frame=%I64x context=%p, dispatch=%p)\n",
           dispatch->LanguageHandler, rec, dispatch->EstablisherFrame, dispatch->ContextRecord, dispatch );
    res = dispatch->LanguageHandler( rec, (void *)dispatch->EstablisherFrame, context, dispatch );
    TRACE( "handler at %p returned %lu\n", dispatch->LanguageHandler, res );

    rec->ExceptionFlags &= EH_NONCONTINUABLE;
    __wine_pop_frame( &frame );
    return res;
}


/**********************************************************************
 *           call_teb_handler
 *
 * Call a single exception handler from the TEB chain.
 * FIXME: Handle nested exceptions.
 */
static DWORD call_teb_handler( EXCEPTION_RECORD *rec, CONTEXT *context, DISPATCHER_CONTEXT *dispatch,
                                  EXCEPTION_REGISTRATION_RECORD *teb_frame )
{
    DWORD res;

    TRACE( "calling TEB handler %p (rec=%p, frame=%p context=%p, dispatch=%p)\n",
           teb_frame->Handler, rec, teb_frame, dispatch->ContextRecord, dispatch );
    res = teb_frame->Handler( rec, teb_frame, context, (EXCEPTION_REGISTRATION_RECORD**)dispatch );
    TRACE( "handler at %p returned %lu\n", teb_frame->Handler, res );
    return res;
}


/**********************************************************************
 *           call_function_handlers
 *
 * Call the per-function handlers.
 */
static NTSTATUS call_function_handlers( EXCEPTION_RECORD *rec, CONTEXT *orig_context )
{
    EXCEPTION_REGISTRATION_RECORD *teb_frame = NtCurrentTeb()->Tib.ExceptionList;
    UNWIND_HISTORY_TABLE table;
    DISPATCHER_CONTEXT dispatch;
    CONTEXT context, prev_context;
    NTSTATUS status;

    context = *orig_context;
    dispatch.TargetPc      = 0;
    dispatch.ContextRecord = &context;
    dispatch.HistoryTable  = &table;
    prev_context = context;
    dispatch.NonVolatileRegisters = (BYTE *)&prev_context.S1; // FIXME: so wrong!

    for (;;)
    {
        status = virtual_unwind( UNW_FLAG_EHANDLER, &dispatch, &context );
        if (status != STATUS_SUCCESS) return status;

    unwind_done:
        if (!dispatch.EstablisherFrame) break;

        if (!is_valid_frame( dispatch.EstablisherFrame ))
        {
            ERR( "invalid frame %I64x (%p-%p)\n", dispatch.EstablisherFrame,
                 NtCurrentTeb()->Tib.StackLimit, NtCurrentTeb()->Tib.StackBase );
            rec->ExceptionFlags |= EH_STACK_INVALID;
            break;
        }

        if (dispatch.LanguageHandler)
        {
            switch (call_handler( rec, orig_context, &dispatch ))
            {
            case ExceptionContinueExecution:
                if (rec->ExceptionFlags & EH_NONCONTINUABLE) return STATUS_NONCONTINUABLE_EXCEPTION;
                return STATUS_SUCCESS;
            case ExceptionContinueSearch:
                break;
            case ExceptionNestedException:
                FIXME( "nested exception\n" );
                break;
            case ExceptionCollidedUnwind: {
                ULONG64 frame;

                context = *dispatch.ContextRecord;
                dispatch.ContextRecord = &context;
                RtlVirtualUnwind( UNW_FLAG_NHANDLER, dispatch.ImageBase,
                                  dispatch.ControlPc, dispatch.FunctionEntry,
                                  &context, &dispatch.HandlerData, &frame, NULL );
                goto unwind_done;
            }
            default:
                return STATUS_INVALID_DISPOSITION;
            }
        }
        /* hack: call wine handlers registered in the tib list */
        else while ((ULONG64)teb_frame < context.Sp)
        {
            TRACE( "found wine frame %p rsp %I64x handler %p\n",
                    teb_frame, context.Sp, teb_frame->Handler );
            dispatch.EstablisherFrame = (ULONG64)teb_frame;
            switch (call_teb_handler( rec, orig_context, &dispatch, teb_frame ))
            {
            case ExceptionContinueExecution:
                if (rec->ExceptionFlags & EH_NONCONTINUABLE) return STATUS_NONCONTINUABLE_EXCEPTION;
                return STATUS_SUCCESS;
            case ExceptionContinueSearch:
                break;
            case ExceptionNestedException:
                FIXME( "nested exception\n" );
                break;
            case ExceptionCollidedUnwind: {
                ULONG64 frame;

                context = *dispatch.ContextRecord;
                dispatch.ContextRecord = &context;
                RtlVirtualUnwind( UNW_FLAG_NHANDLER, dispatch.ImageBase,
                                  dispatch.ControlPc, dispatch.FunctionEntry,
                                  &context, &dispatch.HandlerData, &frame, NULL );
                teb_frame = teb_frame->Prev;
                goto unwind_done;
            }
            default:
                return STATUS_INVALID_DISPOSITION;
            }
            teb_frame = teb_frame->Prev;
        }

        if (context.Sp == (ULONG64)NtCurrentTeb()->Tib.StackBase) break;
        prev_context = context;
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

    if (pWow64PrepareForException) pWow64PrepareForException( rec, context );

    TRACE( "code=%lx flags=%lx addr=%p pc=%016I64x\n",
           rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress, context->Pc );
    for (c = 0; c < rec->NumberParameters; c++)
        TRACE( " info[%ld]=%016I64x\n", c, rec->ExceptionInformation[c] );

    if (rec->ExceptionCode == EXCEPTION_WINE_STUB)
    {
        if (rec->ExceptionInformation[1] >> 16)
            MESSAGE( "wine: Call from %p to unimplemented function %s.%s, aborting\n",
                     rec->ExceptionAddress,
                     (char*)rec->ExceptionInformation[0], (char*)rec->ExceptionInformation[1] );
        else
            MESSAGE( "wine: Call from %p to unimplemented function %s.%Id, aborting\n",
                     rec->ExceptionAddress,
                     (char*)rec->ExceptionInformation[0], rec->ExceptionInformation[1] );
    }
    else if (rec->ExceptionCode == EXCEPTION_WINE_NAME_THREAD && rec->ExceptionInformation[0] == 0x1000)
    {
        if ((DWORD)rec->ExceptionInformation[2] == -1 || (DWORD)rec->ExceptionInformation[2] == GetCurrentThreadId())
            WARN_(threadname)( "Thread renamed to %s\n", debugstr_a((char *)rec->ExceptionInformation[1]) );
        else
            WARN_(threadname)( "Thread ID %04lx renamed to %s\n", (DWORD)rec->ExceptionInformation[2],
                               debugstr_a((char *)rec->ExceptionInformation[1]) );

        set_native_thread_name((DWORD)rec->ExceptionInformation[2], (char *)rec->ExceptionInformation[1]);
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
        if (rec->ExceptionCode == STATUS_ASSERTION_FAILURE)
            ERR( "%s exception (code=%lx) raised\n", debugstr_exception_code(rec->ExceptionCode), rec->ExceptionCode );
        else
            FIXME( "%s exception (code=%lx) raised\n", debugstr_exception_code(rec->ExceptionCode), rec->ExceptionCode );

        {
			int i;
			for (i=0; i < 32; i++)
				ERR("X%02d = %016I64x\n", i, context->X[i]);
		}
        /*
        TRACE("  x0=%016I64x  x1=%016I64x  x2=%016I64x  x3=%016I64x\n",
              context->X0, context->X1, context->X2, context->X3 );
        TRACE("  x4=%016I64x  x5=%016I64x  x6=%016I64x  x7=%016I64x\n",
              context->X4, context->X5, context->X6, context->X7 );
        TRACE("  x8=%016I64x  x9=%016I64x x10=%016I64x x11=%016I64x\n",
              context->X8, context->X9, context->X10, context->X11 );
        TRACE(" x12=%016I64x x13=%016I64x x14=%016I64x x15=%016I64x\n",
              context->X12, context->X13, context->X14, context->X15 );
        TRACE(" x16=%016I64x x17=%016I64x x18=%016I64x x19=%016I64x\n",
              context->X16, context->X17, context->X18, context->X19 );
        TRACE(" x20=%016I64x x21=%016I64x x22=%016I64x x23=%016I64x\n",
              context->X20, context->X21, context->X22, context->X23 );
        TRACE(" x24=%016I64x x25=%016I64x x26=%016I64x x27=%016I64x\n",
              context->X24, context->X25, context->X26, context->X27 );
        TRACE(" x28=%016I64x  fp=%016I64x  lr=%016I64x  sp=%016I64x\n",
              context->X28, context->Fp, context->ra, context->Sp );*/
    }

    if (call_vectored_handlers( rec, context ) == EXCEPTION_CONTINUE_EXECUTION)
        NtContinue( context, FALSE );

    if ((status = call_function_handlers( rec, context )) == STATUS_SUCCESS)
        NtContinue( context, FALSE );

    if (status != STATUS_UNHANDLED_EXCEPTION) RtlRaiseStatus( status );
    return NtRaiseException( rec, context, FALSE );
}


/*******************************************************************
 *		KiUserApcDispatcher (NTDLL.@)
 */
void WINAPI KiUserApcDispatcher( CONTEXT *context, ULONG_PTR arg1, ULONG_PTR arg2, ULONG_PTR arg3,
                                 PNTAPCFUNC apc )
{
    void (CALLBACK *func)(ULONG_PTR,ULONG_PTR,ULONG_PTR,CONTEXT*) = (void *)apc;
    ERR("aqui\n");
    func( arg1, arg2, arg3, context );
    NtContinue( context, TRUE );
}


/*******************************************************************
 *		KiUserCallbackDispatcher (NTDLL.@)
 */
void WINAPI KiUserCallbackDispatcher( ULONG id, void *args, ULONG len )
{
    NTSTATUS status;

    __TRY
    {
        NTSTATUS (WINAPI *func)(void *, ULONG) = ((void **)NtCurrentTeb()->Peb->KernelCallbackTable)[id];
        status = NtCallbackReturn( NULL, 0, func( args, len ));
    }
    __EXCEPT_ALL
    {
        ERR_(seh)( "ignoring exception\n" );
        status = NtCallbackReturn( 0, 0, 0 );
    }
    __ENDTRY

    RtlRaiseStatus( status );
}


/***********************************************************************
 * Definitions for Win32 unwind tables
 */

struct unwind_info
{
    DWORD function_length : 18;
    DWORD version : 2;
    DWORD x : 1;
    DWORD e : 1;
    DWORD epilog : 5;
    DWORD codes : 5;
};

struct unwind_info_ext
{
    WORD epilog;
    BYTE codes;
    BYTE reserved;
};

struct unwind_info_epilog
{
    DWORD offset : 18;
    DWORD res : 4;
    DWORD index : 10;
};

static const BYTE unwind_code_len[256] =
{
/* 00 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* 20 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* 40 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* 60 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* 80 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* a0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/* c0 */ 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
/* e0 */ 4,1,2,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

/***********************************************************************
 *           get_sequence_len
 */
static unsigned int get_sequence_len( BYTE *ptr, BYTE *end )
{
    unsigned int ret = 0;

    while (ptr < end)
    {
        if (*ptr == 0xe4 || *ptr == 0xe5) break;
        ptr += unwind_code_len[*ptr];
        ret++;
    }
    return ret;
}


/***********************************************************************
 *           restore_regs
 */
static void restore_regs( int reg, int count, int pos, CONTEXT *context,
                          KNONVOLATILE_CONTEXT_POINTERS *ptrs )
{
    int i, offset = max( 0, pos );
    for (i = 0; i < count; i++)
    {
        if (ptrs && reg + i >= 19) (&ptrs->S1)[reg + i - 19] = (DWORD64 *)context->Sp + i + offset; // FIXME: so wrong!!!
        context->X[reg + i] = ((DWORD64 *)context->Sp)[i + offset];
    }
    if (pos < 0) context->Sp += -8 * pos;
}


/***********************************************************************
 *           restore_fpregs
 */
static void restore_fpregs( int reg, int count, int pos, CONTEXT *context,
                            KNONVOLATILE_CONTEXT_POINTERS *ptrs )
{
    int i, offset = max( 0, pos );
    for (i = 0; i < count; i++)
    {
        if (ptrs && reg + i >= 8) (&ptrs->F8)[reg + i - 8] = (DWORD64 *)context->Sp + i + offset;
        context->F[reg + i] = ((double *)context->Sp)[i + offset];
    }
    if (pos < 0) context->Sp += -8 * pos;
}

/***********************************************************************
 *           process_unwind_codes
 */
static void process_unwind_codes( BYTE *ptr, BYTE *end, CONTEXT *context,
                                  KNONVOLATILE_CONTEXT_POINTERS *ptrs, int skip )
{
    unsigned int val, len, save_next = 2;

    /* skip codes */
    while (ptr < end && skip)
    {
        if (*ptr == 0xe4 || *ptr == 0xe5) break;
        ptr += unwind_code_len[*ptr];
        skip--;
    }

    while (ptr < end)
    {
        if ((len = unwind_code_len[*ptr]) > 1)
        {
            if (ptr + len > end) break;
            val = ptr[0] * 0x100 + ptr[1];
        }
        else val = *ptr;

        if (*ptr < 0x20)  /* alloc_s */
            context->Sp += 16 * (val & 0x1f);
        else if (*ptr < 0x40)  /* save_r19r20_x */
            restore_regs( 19, save_next, -(val & 0x1f), context, ptrs );
        else if (*ptr < 0x80) /* save_fplr */
            restore_regs( 29, 2, val & 0x3f, context, ptrs );
        else if (*ptr < 0xc0)  /* save_fplr_x */
            restore_regs( 29, 2, -(val & 0x3f) - 1, context, ptrs );
        else if (*ptr < 0xc8)  /* alloc_m */
            context->Sp += 16 * (val & 0x7ff);
        else if (*ptr < 0xcc)  /* save_regp */
            restore_regs( 19 + ((val >> 6) & 0xf), save_next, val & 0x3f, context, ptrs );
        else if (*ptr < 0xd0)  /* save_regp_x */
            restore_regs( 19 + ((val >> 6) & 0xf), save_next, -(val & 0x3f) - 1, context, ptrs );
        else if (*ptr < 0xd4)  /* save_reg */
            restore_regs( 19 + ((val >> 6) & 0xf), 1, val & 0x3f, context, ptrs );
        else if (*ptr < 0xd6)  /* save_reg_x */
            restore_regs( 19 + ((val >> 5) & 0xf), 1, -(val & 0x1f) - 1, context, ptrs );
        else if (*ptr < 0xd8)  /* save_lrpair */
        {
            restore_regs( 19 + 2 * ((val >> 6) & 0x7), 1, val & 0x3f, context, ptrs );
            restore_regs( 30, 1, (val & 0x3f) + 1, context, ptrs );
        }
        else if (*ptr < 0xda)  /* save_fregp */
            restore_fpregs( 8 + ((val >> 6) & 0x7), save_next, val & 0x3f, context, ptrs );
        else if (*ptr < 0xdc)  /* save_fregp_x */
            restore_fpregs( 8 + ((val >> 6) & 0x7), save_next, -(val & 0x3f) - 1, context, ptrs );
        else if (*ptr < 0xde)  /* save_freg */
            restore_fpregs( 8 + ((val >> 6) & 0x7), 1, val & 0x3f, context, ptrs );
        else if (*ptr == 0xde)  /* save_freg_x */
            restore_fpregs( 8 + ((val >> 5) & 0x7), 1, -(val & 0x3f) - 1, context, ptrs );
        else if (*ptr == 0xe0)  /* alloc_l */
            context->Sp += 16 * ((ptr[1] << 16) + (ptr[2] << 8) + ptr[3]);
        else if (*ptr == 0xe1)  /* set_fp */
            context->Sp = context->Fp;
        else if (*ptr == 0xe2)  /* add_fp */
            context->Sp = context->Fp - 8 * (val & 0xff);
        else if (*ptr == 0xe3)  /* nop */
            /* nop */ ;
        else if (*ptr == 0xe4)  /* end */
            break;
        else if (*ptr == 0xe5)  /* end_c */
            break;
        else if (*ptr == 0xe6)  /* save_next */
        {
            save_next += 2;
            ptr += len;
            continue;
        }
        else if (*ptr == 0xe9)  /* MSFT_OP_MACHINE_FRAME */
        {
            context->Pc = ((DWORD64 *)context->Sp)[1];
            context->Sp = ((DWORD64 *)context->Sp)[0];
        }
        else if (*ptr == 0xea)  /* MSFT_OP_CONTEXT */
        {
            memcpy( context, (DWORD64 *)context->Sp, sizeof(CONTEXT) );
        }
        else
        {
            WARN( "unsupported code %02x\n", *ptr );
            return;
        }
        save_next = 2;
        ptr += len;
    }
}

/***********************************************************************
 *           unwind_full_data
 */
static void *unwind_full_data( ULONG_PTR base, ULONG_PTR pc, RUNTIME_FUNCTION *func,
                               CONTEXT *context, PVOID *handler_data, KNONVOLATILE_CONTEXT_POINTERS *ptrs )
{
    struct unwind_info *info;
    struct unwind_info_epilog *info_epilog;
    unsigned int i, codes, epilogs, len, offset;
    void *data;
    BYTE *end;

    info = (struct unwind_info *)((char *)base + func->UnwindData);
    data = info + 1;
    epilogs = info->epilog;
    codes = info->codes;
    if (!codes && !epilogs)
    {
        struct unwind_info_ext *infoex = data;
        codes = infoex->codes;
        epilogs = infoex->epilog;
        data = infoex + 1;
    }
    info_epilog = data;
    if (!info->e) data = info_epilog + epilogs;

    offset = ((pc - base) - func->BeginAddress) / 4;
    end = (BYTE *)data + codes * 4;

    TRACE( "function %I64x-%I64x: len=%#x ver=%u X=%u E=%u epilogs=%u codes=%u\n",
           base + func->BeginAddress, base + func->BeginAddress + info->function_length * 4,
           info->function_length, info->version, info->x, info->e, epilogs, codes * 4 );

    /* check for prolog */
    if (offset < codes * 4)
    {
        len = get_sequence_len( data, end );
        if (offset < len)
        {
            process_unwind_codes( data, end, context, ptrs, len - offset );
            return NULL;
        }
    }

    /* check for epilog */
    if (!info->e)
    {
        for (i = 0; i < epilogs; i++)
        {
            if (offset < info_epilog[i].offset) break;
            if (offset - info_epilog[i].offset < codes * 4 - info_epilog[i].index)
            {
                BYTE *ptr = (BYTE *)data + info_epilog[i].index;
                len = get_sequence_len( ptr, end );
                if (offset <= info_epilog[i].offset + len)
                {
                    process_unwind_codes( ptr, end, context, ptrs, offset - info_epilog[i].offset );
                    return NULL;
                }
            }
        }
    }
    else if (info->function_length - offset <= codes * 4 - epilogs)
    {
        BYTE *ptr = (BYTE *)data + epilogs;
        len = get_sequence_len( ptr, end ) + 1;
        if (offset >= info->function_length - len)
        {
            process_unwind_codes( ptr, end, context, ptrs, offset - (info->function_length - len) );
            return NULL;
        }
    }

    process_unwind_codes( data, end, context, ptrs, 0 );

    /* get handler since we are inside the main code */
    if (info->x)
    {
        DWORD *handler_rva = (DWORD *)data + codes;
        *handler_data = handler_rva + 1;
        return (char *)base + *handler_rva;
    }
    return NULL;
}


/**********************************************************************
 *              RtlVirtualUnwind   (NTDLL.@)
 */
PVOID WINAPI RtlVirtualUnwind( ULONG type, ULONG_PTR base, ULONG_PTR pc,
                               RUNTIME_FUNCTION *func, CONTEXT *context,
                               PVOID *handler_data, ULONG_PTR *frame_ret,
                               KNONVOLATILE_CONTEXT_POINTERS *ctx_ptr )
{
    void *handler;

    TRACE( "type %lx pc %I64x sp %I64x func %I64x\n", type, pc, context->Sp, base + func->BeginAddress );

    *handler_data = NULL;

    context->Pc = 0;
    handler = unwind_full_data( base, pc, func, context, handler_data, ctx_ptr );

    TRACE( "ret: lr=%I64x sp=%I64x handler=%p\n", context->Ra, context->Sp, handler );
    if (!context->Pc)
        context->Pc = context->Ra;
    context->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
    *frame_ret = context->Sp;
    return handler;
}

/*******************************************************************
 *              RtlRestoreContext (NTDLL.@)
 */
void CDECL RtlRestoreContext( CONTEXT *context, EXCEPTION_RECORD *rec )
{
    EXCEPTION_REGISTRATION_RECORD *teb_frame = NtCurrentTeb()->Tib.ExceptionList;

    if (rec && rec->ExceptionCode == STATUS_LONGJUMP && rec->NumberParameters >= 1)
    {
        struct MSVCRT_JUMP_BUFFER *jmp = (struct MSVCRT_JUMP_BUFFER *)rec->ExceptionInformation[0];
        int i;

        context->Fp   = jmp->Fp;
        context->S1   = jmp->S1;
        context->S2   = jmp->S2;
        context->S3   = jmp->S3;
        context->S4   = jmp->S4;
        context->S5   = jmp->S5;
        context->S6   = jmp->S6;
        context->S7   = jmp->S7;
        context->S8   = jmp->S8;
        context->S9   = jmp->S9;
        context->S10  = jmp->S10;
        context->S11  = jmp->S11;
        context->Ra   = jmp->Ra;
        context->Sp   = jmp->Sp;

        // FIXME
        /*for (i = 0; i < 8; i++)
            context->F[8+i] = jmp->D[i];*/
    }
    else if (rec && rec->ExceptionCode == STATUS_UNWIND_CONSOLIDATE && rec->NumberParameters >= 1)
    {
        PVOID (CALLBACK *consolidate)(EXCEPTION_RECORD *) = (void *)rec->ExceptionInformation[0];
        FIXME("calling consolidate callback NYI!\n");
        TRACE( "calling consolidate callback %p (rec=%p)\n", consolidate, rec );
        rec->ExceptionInformation[10] = (ULONG_PTR)&context->S1; /* FIXME: Which register???????? */

        //context->Pc = (ULONG64)call_consolidate_callback( context, consolidate, rec, NtCurrentTeb() );
        NtContinue( context, FALSE );
    }

    /* hack: remove no longer accessible TEB frames */
    while ((ULONG64)teb_frame < context->Sp)
    {
        TRACE( "removing TEB frame: %p\n", teb_frame );
        teb_frame = __wine_pop_frame( teb_frame );
    }

    TRACE( "returning to %I64x stack %I64x (rec %p)\n", context->Pc, context->Sp, rec );
    NtContinue( context, FALSE );
}

/*******************************************************************
 *		RtlUnwindEx (NTDLL.@)
 */
void WINAPI RtlUnwindEx( PVOID end_frame, PVOID target_ip, EXCEPTION_RECORD *rec,
                         PVOID retval, CONTEXT *context, UNWIND_HISTORY_TABLE *table )
{
    EXCEPTION_REGISTRATION_RECORD *teb_frame = NtCurrentTeb()->Tib.ExceptionList;
    EXCEPTION_RECORD record;
    DISPATCHER_CONTEXT dispatch;
    CONTEXT new_context;
    NTSTATUS status;
    DWORD i;

    ERR("end_frame = %p\n", end_frame);
    ERR("%p RtlUnwindEx\n", RtlUnwindEx);
    ERR("%p virtual_unwind\n", virtual_unwind);
    ERR("%p RtlVirtualUnwind\n", RtlVirtualUnwind);
    ERR("%p RtlRaiseStatus\n", RtlRaiseStatus);
    ERR("%p RtlRaiseException\n", RtlRaiseException);

    RtlCaptureContext( context );
    new_context = *context;

    /* build an exception record, if we do not have one */
    if (!rec)
    {
        record.ExceptionCode    = STATUS_UNWIND;
        record.ExceptionFlags   = 0;
        record.ExceptionRecord  = NULL;
        record.ExceptionAddress = (void *)context->Pc;
        record.NumberParameters = 0;
        rec = &record;
    }

    rec->ExceptionFlags |= EH_UNWINDING | (end_frame ? 0 : EH_EXIT_UNWIND);

    TRACE( "code=%lx flags=%lx end_frame=%p target_ip=%p pc=%016I64x\n",
           rec->ExceptionCode, rec->ExceptionFlags, end_frame, target_ip, context->Pc );
    for (i = 0; i < min( EXCEPTION_MAXIMUM_PARAMETERS, rec->NumberParameters ); i++)
        TRACE( " info[%ld]=%016I64x\n", i, rec->ExceptionInformation[i] );
    FIXME("ADD REGDUMP\n");
    /*
    TRACE("  x0=%016I64x  x1=%016I64x  x2=%016I64x  x3=%016I64x\n",
          context->X0, context->X1, context->X2, context->X3 );
    TRACE("  x4=%016I64x  x5=%016I64x  x6=%016I64x  x7=%016I64x\n",
          context->X4, context->X5, context->X6, context->X7 );
    TRACE("  x8=%016I64x  x9=%016I64x x10=%016I64x x11=%016I64x\n",
          context->X8, context->X9, context->X10, context->X11 );
    TRACE(" x12=%016I64x x13=%016I64x x14=%016I64x x15=%016I64x\n",
          context->X12, context->X13, context->X14, context->X15 );
    TRACE(" x16=%016I64x x17=%016I64x x18=%016I64x x19=%016I64x\n",
          context->X16, context->X17, context->X18, context->X19 );
    TRACE(" x20=%016I64x x21=%016I64x x22=%016I64x x23=%016I64x\n",
          context->X20, context->X21, context->X22, context->X23 );
    TRACE(" x24=%016I64x x25=%016I64x x26=%016I64x x27=%016I64x\n",
          context->X24, context->X25, context->X26, context->X27 );
    TRACE(" x28=%016I64x  fp=%016I64x  lr=%016I64x  sp=%016I64x\n",
          context->X28, context->Fp, context->ra, context->Sp );*/

    dispatch.TargetPc         = (ULONG64)target_ip;
    dispatch.ContextRecord    = context;
    dispatch.HistoryTable     = table;
    dispatch.NonVolatileRegisters = (BYTE *)&context->S1; // FIXME: So wrong!!!!!!!!!!!!

    for (;;)
    {
        status = virtual_unwind( UNW_FLAG_UHANDLER, &dispatch, &new_context );
        if (status != STATUS_SUCCESS) raise_status( status, rec );

    unwind_done:
        if (!dispatch.EstablisherFrame) break;

        if (!is_valid_frame( dispatch.EstablisherFrame ))
        {
            ERR( "invalid frame %I64x (%p-%p)\n", dispatch.EstablisherFrame,
                 NtCurrentTeb()->Tib.StackLimit, NtCurrentTeb()->Tib.StackBase );
            rec->ExceptionFlags |= EH_STACK_INVALID;
            break;
        }

        if (dispatch.LanguageHandler)
        {
			ERR("LanguageHandler?\n");
            if (end_frame && (dispatch.EstablisherFrame > (ULONG64)end_frame))
            {
                ERR( "invalid end frame %I64x/%p\n", dispatch.EstablisherFrame, end_frame );
                raise_status( STATUS_INVALID_UNWIND_TARGET, rec );
            }
            if (dispatch.EstablisherFrame == (ULONG64)end_frame) rec->ExceptionFlags |= EH_TARGET_UNWIND;
            if (call_unwind_handler( rec, &dispatch ) == ExceptionCollidedUnwind)
            {
                ULONG64 frame;

                *context = new_context = *dispatch.ContextRecord;
                dispatch.ContextRecord = context;
                RtlVirtualUnwind( UNW_FLAG_NHANDLER, dispatch.ImageBase,
                                  dispatch.ControlPc, dispatch.FunctionEntry,
                                  &new_context, &dispatch.HandlerData, &frame,
                                  NULL );
                rec->ExceptionFlags |= EH_COLLIDED_UNWIND;
                goto unwind_done;
            }
            rec->ExceptionFlags &= ~EH_COLLIDED_UNWIND;
        }
        else  /* hack: call builtin handlers registered in the tib list */
        {
            DWORD64 backup_frame = dispatch.EstablisherFrame;
            while ((ULONG64)teb_frame < new_context.Sp && (ULONG64)teb_frame < (ULONG64)end_frame)
            {
                TRACE( "found builtin frame %p handler %p\n", teb_frame, teb_frame->Handler );
                dispatch.EstablisherFrame = (ULONG64)teb_frame;
                if (call_teb_unwind_handler( rec, &dispatch, teb_frame ) == ExceptionCollidedUnwind)
                {
                    ULONG64 frame;

                    teb_frame = __wine_pop_frame( teb_frame );

                    *context = new_context = *dispatch.ContextRecord;
                    dispatch.ContextRecord = context;
                    RtlVirtualUnwind( UNW_FLAG_NHANDLER, dispatch.ImageBase,
                                      dispatch.ControlPc, dispatch.FunctionEntry,
                                      &new_context, &dispatch.HandlerData,
                                      &frame, NULL );
                    rec->ExceptionFlags |= EH_COLLIDED_UNWIND;
                    goto unwind_done;
                }
                teb_frame = __wine_pop_frame( teb_frame );
            }
            if ((ULONG64)teb_frame == (ULONG64)end_frame && (ULONG64)end_frame < new_context.Sp) break;
            dispatch.EstablisherFrame = backup_frame;
        }

        if (dispatch.EstablisherFrame == (ULONG64)end_frame) break;
        *context = new_context;
    }

    context->A0 = (ULONG64)retval;
    context->Pc = (ULONG64)target_ip;
    RtlRestoreContext(context, rec);
}


/***********************************************************************
 *            RtlUnwind  (NTDLL.@)
 */
void WINAPI RtlUnwind( void *frame, void *target_ip, EXCEPTION_RECORD *rec, void *retval )
{
    CONTEXT context;
    ERR("FRAME = %p\n", frame);
    RtlUnwindEx( frame, target_ip, rec, retval, &context, NULL );
}

/*******************************************************************
 *		_local_unwind (NTDLL.@)
 */
void WINAPI _local_unwind( void *frame, void *target_ip )
{
    CONTEXT context;
    RtlUnwindEx( frame, target_ip, NULL, NULL, &context, NULL );
}

#if 0
extern LONG __C_ExecuteExceptionFilter(PEXCEPTION_POINTERS ptrs, PVOID frame,
                                       PEXCEPTION_FILTER filter,
                                       PUCHAR nonvolatile);
__ASM_GLOBAL_FUNC( __C_ExecuteExceptionFilter,
                   "stp x29, x30, [sp, #-96]!\n\t"
                   "stp x19, x20, [sp, #16]\n\t"
                   "stp x21, x22, [sp, #32]\n\t"
                   "stp x23, x24, [sp, #48]\n\t"
                   "stp x25, x26, [sp, #64]\n\t"
                   "stp x27, x28, [sp, #80]\n\t"
                   "mov x29, sp\n\t"

                   "ldp x19, x20, [x3, #0]\n\t"
                   "ldp x21, x22, [x3, #16]\n\t"
                   "ldp x23, x24, [x3, #32]\n\t"
                   "ldp x25, x26, [x3, #48]\n\t"
                   "ldp x27, x28, [x3, #64]\n\t"
                   /* Overwrite the frame parameter with Fp from the
                    * nonvolatile regs */
                   "ldr x1, [x3, #80]\n\t"
                   "blr x2\n\t"
                   "ldp x19, x20, [sp, #16]\n\t"
                   "ldp x21, x22, [sp, #32]\n\t"
                   "ldp x23, x24, [sp, #48]\n\t"
                   "ldp x25, x26, [sp, #64]\n\t"
                   "ldp x27, x28, [sp, #80]\n\t"
                   "ldp x29, x30, [sp], #96\n\t"
                   "ret")

extern void __C_ExecuteTerminationHandler(BOOL abnormal, PVOID frame,
                                          PTERMINATION_HANDLER handler,
                                          PUCHAR nonvolatile);
/* This is, implementation wise, identical to __C_ExecuteExceptionFilter. */
__ASM_GLOBAL_FUNC( __C_ExecuteTerminationHandler,
                   "j " __ASM_NAME("__C_ExecuteExceptionFilter") "\n\t");
#endif

/*******************************************************************
 *		__C_specific_handler (NTDLL.@)
 */
EXCEPTION_DISPOSITION WINAPI __C_specific_handler( EXCEPTION_RECORD *rec,
                                                   void *frame,
                                                   CONTEXT *context,
                                                   struct _DISPATCHER_CONTEXT *dispatch )
{
    SCOPE_TABLE *table = dispatch->HandlerData;
    ULONG i;
    DWORD64 ControlPc = dispatch->ControlPc;

    TRACE( "%p %p %p %p\n", rec, frame, context, dispatch );
    if (TRACE_ON(seh)) dump_scope_table( dispatch->ImageBase, table );

    if (dispatch->ControlPcIsUnwound)
        ControlPc -= 4;

    if (rec->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND))
    {
        for (i = dispatch->ScopeIndex; i < table->Count; i++)
        {
            if (ControlPc >= dispatch->ImageBase + table->ScopeRecord[i].BeginAddress &&
                ControlPc < dispatch->ImageBase + table->ScopeRecord[i].EndAddress)
            {
                PTERMINATION_HANDLER handler;

                if (table->ScopeRecord[i].JumpTarget) continue;

                if (rec->ExceptionFlags & EH_TARGET_UNWIND &&
                    dispatch->TargetPc >= dispatch->ImageBase + table->ScopeRecord[i].BeginAddress &&
                    dispatch->TargetPc < dispatch->ImageBase + table->ScopeRecord[i].EndAddress)
                {
                    break;
                }

                handler = (PTERMINATION_HANDLER)(dispatch->ImageBase + table->ScopeRecord[i].HandlerAddress);
                dispatch->ScopeIndex = i+1;

                FIXME( "NYI calling __finally %p frame %p\n", handler, frame );
                TRACE( "calling __finally %p frame %p\n", handler, frame );
                //__C_ExecuteTerminationHandler( TRUE, frame, handler,
                //                               dispatch->NonVolatileRegisters );
                return ExceptionContinueSearch;
            }
        }
        return ExceptionContinueSearch;
    }

    for (i = dispatch->ScopeIndex; i < table->Count; i++)
    {
        if (ControlPc >= dispatch->ImageBase + table->ScopeRecord[i].BeginAddress &&
            ControlPc < dispatch->ImageBase + table->ScopeRecord[i].EndAddress)
        {
            if (!table->ScopeRecord[i].JumpTarget) continue;
            if (table->ScopeRecord[i].HandlerAddress != EXCEPTION_EXECUTE_HANDLER)
            {
                EXCEPTION_POINTERS ptrs;
                PEXCEPTION_FILTER filter;

                filter = (PEXCEPTION_FILTER)(dispatch->ImageBase + table->ScopeRecord[i].HandlerAddress);
                ptrs.ExceptionRecord = rec;
                ptrs.ContextRecord = context;
                FIXME( "NYI calling filter %p ptrs %p frame %p\n", filter, &ptrs, frame );
                TRACE( "calling filter %p ptrs %p frame %p\n", filter, &ptrs, frame );
                /*switch (__C_ExecuteExceptionFilter( &ptrs, frame, filter,
                                                    dispatch->NonVolatileRegisters ))
                {
                case EXCEPTION_EXECUTE_HANDLER:
                    break;
                case EXCEPTION_CONTINUE_SEARCH:
                    continue;
                case EXCEPTION_CONTINUE_EXECUTION:
                    return ExceptionContinueExecution;
                }*/
                return ExceptionContinueSearch;
            }
            TRACE( "unwinding to target %I64x\n", dispatch->ImageBase + table->ScopeRecord[i].JumpTarget );
            RtlUnwindEx( frame, (char *)dispatch->ImageBase + table->ScopeRecord[i].JumpTarget,
                         rec, 0, dispatch->ContextRecord, dispatch->HistoryTable );
        }
    }
    return ExceptionContinueSearch;
}


/***********************************************************************
 *		RtlRaiseException (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( RtlRaiseException,
                   "addi sp, sp, -0x220\n\t" /* 0x208 (context) + 0x18 */
                   "sd ra, 0x218(sp)\n\t"
                   "sd fp, 0x210(sp)\n\t"
                   __ASM_CFI(".cfi_def_cfa fp, 544\n\t")
                   __ASM_CFI(".cfi_offset ra, -8\n\t")
                   __ASM_CFI(".cfi_offset fp, -16\n\t")
                   "mv fp, sp\n\t"
                   "sd a0, 0x208(sp)\n\t"
                   "mv a0, sp\n\t"
                   "jal " __ASM_NAME("RtlCaptureContext") "\n\t"
                   "mv a1, sp\n\t" /* context pointer */
                   "addi a2, sp, 0x220\n\t" /* orig stack pointer */
                   "sd a2, 0x18(a1)\n\t" /* context->sp */
                   "ld a0, 0x208(sp)\n\t" /* original first parameter */
                   "sd a0, 0x58(a1)\n\t" /* context->a0 */
                   "ld a4, 0x218(sp)\n\t" /* ra */
                   "sd a4, 0x10(a1)\n\t" /* context->ra */
                   "ld a5, 0x210(sp)\n\t" /* fp */
                   "sd a5, 0x48(a1)\n\t" /* context->fp */
                   "sd a5, 0x08(a1)\n\t" /* context->pc */
                   "sd a4, 0x10(a0)\n\t" /* rec->ExceptionAddress */
                   "li a2, 0x01\n\t"
                   "jal " __ASM_NAME("NtRaiseException") "\n\t"
                   "jal " __ASM_NAME("RtlRaiseStatus") /* does not return */ );

/*************************************************************************
 *             RtlCaptureStackBackTrace (NTDLL.@)
 */
USHORT WINAPI RtlCaptureStackBackTrace( ULONG skip, ULONG count, PVOID *buffer, ULONG *hash )
{
    FIXME( "(%ld, %ld, %p, %p) stub!\n", skip, count, buffer, hash );
    return 0;
}

/***********************************************************************
 *           RtlUserThreadStart (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( RtlUserThreadStart,
                   "addi sp, sp, -16\n\t"
                   "sd ra, 8(sp)\n\t"
                   "sd fp, 0(sp)\n\t"
                   "la t0, " __ASM_NAME("pBaseThreadInitThunk") "\n\t"
                   "ld t0, 0(t0)\n\t"
                   "mv a2, a1\n\t"
                   "mv a1, a0\n\t"
                   "li a0, 0\n\t"
                   "jalr t0\n\t")

/******************************************************************
 *		LdrInitializeThunk (NTDLL.@)
 */
void WINAPI LdrInitializeThunk( CONTEXT *context, ULONG_PTR unk2, ULONG_PTR unk3, ULONG_PTR unk4 )
{
    if (!context) ERR("aqui %p\n", context);
    loader_init( context, (void **)&context->A0 );
    NtContinue( context, TRUE );
}

/**********************************************************************
 *              DbgBreakPoint   (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( DbgBreakPoint, "ebreak; ret"
                    "\n\tnop; nop; nop; nop; nop; nop; nop; nop"
                    "\n\tnop; nop; nop; nop; nop; nop" );

/**********************************************************************
 *              DbgUserBreakPoint   (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( DbgUserBreakPoint, "ebreak; ret"
                    "\n\tnop; nop; nop; nop; nop; nop; nop; nop"
                    "\n\tnop; nop; nop; nop; nop; nop" );

static void wow64_suspend_helper( void *arg )
{
    ULONG count;
    pWow64SuspendLocalThread( (HANDLE)arg, &count );
}
/***********************************************************************
 *              RtlWow64SuspendThread (NTDLL.@)
 */
NTSTATUS WINAPI RtlWow64SuspendThread( HANDLE thread, ULONG *count )
{
    THREAD_BASIC_INFORMATION tbi;
    HANDLE target_thread;
    NTSTATUS ret = NtDuplicateObject( NtCurrentProcess(), thread, NtCurrentProcess(), &target_thread,
                                      THREAD_ALL_ACCESS, 0, 0);
    if (ret) return ret;

    ret = NtQueryInformationThread( target_thread, ThreadBasicInformation, &tbi, sizeof(tbi), NULL );
    if (ret) goto end;

    if (tbi.ClientId.UniqueProcess != NtCurrentTeb()->ClientId.UniqueProcess)
    {
        HANDLE process;
        HANDLE remote_suspender_thread;
        HANDLE remote_target_thread;
        ret = NtOpenProcess( &process, PROCESS_ALL_ACCESS, NULL, &tbi.ClientId );
        if (ret) goto end;

        ret = NtDuplicateObject( NtCurrentProcess(), target_thread, process, &remote_target_thread,
                                 THREAD_ALL_ACCESS, 0, 0 );
        if (ret) goto close_process;

        ret = NtCreateThreadEx( &remote_suspender_thread, THREAD_ALL_ACCESS, NULL, process,
                                &wow64_suspend_helper, remote_target_thread, THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER,
                                0, 0, 0, NULL );
        if (ret) goto close_remote_target_thread;

        NtWaitForSingleObject( remote_suspender_thread, FALSE, NULL );
        ret = NtQueryInformationThread( target_thread, ThreadSuspendCount, count, sizeof(*count), NULL );

        NtClose( remote_suspender_thread );
close_remote_target_thread:
        NtClose( remote_target_thread );
close_process:
        NtClose( process );
    } else {
        ret = pWow64SuspendLocalThread( target_thread, count );
    }

end:
    NtClose( target_thread );
    return ret;
}

/**********************************************************************
 *           NtCurrentTeb   (NTDLL.@)
 */
TEB * WINAPI NtCurrentTeb(void)
{
    return __wine_current_teb();
}

#endif  /* __aarch64__ */
