/*
 * x86-64 signal handling routines
 *
 * Copyright 1999, 2005 Alexandre Julliard
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

#if defined(__x86_64__) && !defined(__arm64ec__)

#include <stdlib.h>
#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/exception.h"
#include "wine/list.h"
#include "ntdll_misc.h"
#include "wine/debug.h"
#include "ntsyscalls.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);
WINE_DECLARE_DEBUG_CHANNEL(relay);


/* layering violation: the setjmp buffer is defined in msvcrt, but used by RtlUnwindEx */
struct MSVCRT_JUMP_BUFFER
{
    ULONG64 Frame;
    ULONG64 Rbx;
    ULONG64 Rsp;
    ULONG64 Rbp;
    ULONG64 Rsi;
    ULONG64 Rdi;
    ULONG64 R12;
    ULONG64 R13;
    ULONG64 R14;
    ULONG64 R15;
    ULONG64 Rip;
    ULONG  MxCsr;
    USHORT FpCsr;
    USHORT Spare;
    M128A   Xmm6;
    M128A   Xmm7;
    M128A   Xmm8;
    M128A   Xmm9;
    M128A   Xmm10;
    M128A   Xmm11;
    M128A   Xmm12;
    M128A   Xmm13;
    M128A   Xmm14;
    M128A   Xmm15;
};


/*******************************************************************
 *         syscalls
 */
#define SYSCALL_ENTRY(id,name,args) __ASM_SYSCALL_FUNC( id, name )
ALL_SYSCALLS64
#undef SYSCALL_ENTRY


static void dump_scope_table( ULONG64 base, const SCOPE_TABLE *table )
{
    unsigned int i;

    TRACE( "scope table at %p\n", table );
    for (i = 0; i < table->Count; i++)
        TRACE( "  %u: %p-%p handler %p target %p\n", i,
               (char *)base + table->ScopeRecord[i].BeginAddress,
               (char *)base + table->ScopeRecord[i].EndAddress,
               (char *)base + table->ScopeRecord[i].HandlerAddress,
               (char *)base + table->ScopeRecord[i].JumpTarget );
}


/***********************************************************************
 *           virtual_unwind
 */
static NTSTATUS virtual_unwind( ULONG type, DISPATCHER_CONTEXT *dispatch, CONTEXT *context )
{
    LDR_DATA_TABLE_ENTRY *module;
    NTSTATUS status;

    dispatch->ImageBase = 0;
    dispatch->ScopeIndex = 0;
    dispatch->ControlPc = context->Rip;
    dispatch->FunctionEntry = RtlLookupFunctionEntry( context->Rip, &dispatch->ImageBase,
                                                      dispatch->HistoryTable );

    /* look for host system exception information */
    if (!dispatch->FunctionEntry &&
        (LdrFindEntryForAddress( (void *)context->Rip, &module ) || (module->Flags & LDR_WINE_INTERNAL)))
    {
        struct unwind_builtin_dll_params params = { type, dispatch, context };

        status = WINE_UNIX_CALL( unix_unwind_builtin_dll, &params );
        if (!status && dispatch->LanguageHandler && !module)
        {
            FIXME( "calling personality routine in system library not supported yet\n" );
            dispatch->LanguageHandler = NULL;
        }
        if (status != STATUS_UNSUCCESSFUL) return status;
    }
    else WARN( "exception data not found for pc %p\n", (void *)context->Rip );

    dispatch->LanguageHandler = RtlVirtualUnwind( type, dispatch->ImageBase, context->Rip,
                                                  dispatch->FunctionEntry, context,
                                                  &dispatch->HandlerData, &dispatch->EstablisherFrame,
                                                  NULL );
    return STATUS_SUCCESS;
}


/**************************************************************************
 *		__chkstk (NTDLL.@)
 *
 * Supposed to touch all the stack pages, but we shouldn't need that.
 */
__ASM_GLOBAL_FUNC( __chkstk, "ret" );


/***********************************************************************
 *		RtlCaptureContext (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( RtlCaptureContext,
                   "pushfq\n\t"
                   __ASM_CFI(".cfi_adjust_cfa_offset 8\n\t")
                   "movl $0x10000f,0x30(%rcx)\n\t"  /* context->ContextFlags */
                   "stmxcsr 0x34(%rcx)\n\t"         /* context->MxCsr */
                   "movw %cs,0x38(%rcx)\n\t"        /* context->SegCs */
                   "movw %ds,0x3a(%rcx)\n\t"        /* context->SegDs */
                   "movw %es,0x3c(%rcx)\n\t"        /* context->SegEs */
                   "movw %fs,0x3e(%rcx)\n\t"        /* context->SegFs */
                   "movw %gs,0x40(%rcx)\n\t"        /* context->SegGs */
                   "movw %ss,0x42(%rcx)\n\t"        /* context->SegSs */
                   "popq 0x44(%rcx)\n\t"            /* context->Eflags */
                   __ASM_CFI(".cfi_adjust_cfa_offset -8\n\t")
                   "movq %rax,0x78(%rcx)\n\t"       /* context->Rax */
                   "movq %rcx,0x80(%rcx)\n\t"       /* context->Rcx */
                   "movq %rdx,0x88(%rcx)\n\t"       /* context->Rdx */
                   "movq %rbx,0x90(%rcx)\n\t"       /* context->Rbx */
                   "leaq 8(%rsp),%rax\n\t"
                   "movq %rax,0x98(%rcx)\n\t"       /* context->Rsp */
                   "movq %rbp,0xa0(%rcx)\n\t"       /* context->Rbp */
                   "movq %rsi,0xa8(%rcx)\n\t"       /* context->Rsi */
                   "movq %rdi,0xb0(%rcx)\n\t"       /* context->Rdi */
                   "movq %r8,0xb8(%rcx)\n\t"        /* context->R8 */
                   "movq %r9,0xc0(%rcx)\n\t"        /* context->R9 */
                   "movq %r10,0xc8(%rcx)\n\t"       /* context->R10 */
                   "movq %r11,0xd0(%rcx)\n\t"       /* context->R11 */
                   "movq %r12,0xd8(%rcx)\n\t"       /* context->R12 */
                   "movq %r13,0xe0(%rcx)\n\t"       /* context->R13 */
                   "movq %r14,0xe8(%rcx)\n\t"       /* context->R14 */
                   "movq %r15,0xf0(%rcx)\n\t"       /* context->R15 */
                   "movq (%rsp),%rax\n\t"
                   "movq %rax,0xf8(%rcx)\n\t"       /* context->Rip */
                   "fxsave 0x100(%rcx)\n\t"         /* context->FltSave */
                   "ret" );

/***********************************************************************
 *		exception_handler_call_wrapper
 */
#ifdef __WINE_PE_BUILD
DWORD WINAPI exception_handler_call_wrapper( EXCEPTION_RECORD *rec, void *frame,
                                      CONTEXT *context, DISPATCHER_CONTEXT *dispatch );

C_ASSERT( offsetof(DISPATCHER_CONTEXT, LanguageHandler) == 0x30 );

__ASM_GLOBAL_FUNC( exception_handler_call_wrapper,
                   "subq $0x28, %rsp\n\t"
                   ".seh_stackalloc 0x28\n\t"
                   ".seh_endprologue\n\t"
                   "callq *0x30(%r9)\n\t"       /* dispatch->LanguageHandler */
                   "nop\n\t"                    /* avoid epilogue so handler is called */
                   "addq $0x28, %rsp\n\t"
                   "ret\n\t"
                   ".seh_handler " __ASM_NAME("nested_exception_handler") ", @except\n\t" )
#else
static DWORD exception_handler_call_wrapper( EXCEPTION_RECORD *rec, void *frame,
                                             CONTEXT *context, DISPATCHER_CONTEXT *dispatch )
{
    EXCEPTION_REGISTRATION_RECORD wrapper_frame;
    DWORD res;

    wrapper_frame.Handler = (PEXCEPTION_HANDLER)nested_exception_handler;
    __wine_push_frame( &wrapper_frame );
    res = dispatch->LanguageHandler( rec, (void *)dispatch->EstablisherFrame, context, dispatch );
    __wine_pop_frame( &wrapper_frame );
    return res;
}
#endif

/**********************************************************************
 *           call_handler
 *
 * Call a single exception handler.
 */
static DWORD call_handler( EXCEPTION_RECORD *rec, CONTEXT *context, DISPATCHER_CONTEXT *dispatch )
{
    DWORD res;

    TRACE( "calling handler %p (rec=%p, frame=%p context=%p, dispatch=%p)\n",
           dispatch->LanguageHandler, rec, (void *)dispatch->EstablisherFrame, dispatch->ContextRecord, dispatch );
    res = exception_handler_call_wrapper( rec, (void *)dispatch->EstablisherFrame, context, dispatch );
    TRACE( "handler at %p returned %lu\n", dispatch->LanguageHandler, res );

    rec->ExceptionFlags &= EH_NONCONTINUABLE;
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
 *           call_seh_handlers
 *
 * Call the SEH handlers chain.
 */
NTSTATUS call_seh_handlers( EXCEPTION_RECORD *rec, CONTEXT *orig_context )
{
    EXCEPTION_REGISTRATION_RECORD *teb_frame = NtCurrentTeb()->Tib.ExceptionList;
    UNWIND_HISTORY_TABLE table;
    DISPATCHER_CONTEXT dispatch;
    CONTEXT context;
    NTSTATUS status;

    context = *orig_context;
    context.ContextFlags &= ~0x40; /* Clear xstate flag. */

    dispatch.TargetIp      = 0;
    dispatch.ContextRecord = &context;
    dispatch.HistoryTable  = &table;
    for (;;)
    {
        status = virtual_unwind( UNW_FLAG_EHANDLER, &dispatch, &context );
        if (status != STATUS_SUCCESS) return status;

    unwind_done:
        if (!dispatch.EstablisherFrame) break;

        if (!is_valid_frame( dispatch.EstablisherFrame ))
        {
            ERR( "invalid frame %p (%p-%p)\n", (void *)dispatch.EstablisherFrame,
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
                rec->ExceptionFlags |= EH_NESTED_CALL;
                TRACE( "nested exception\n" );
                break;
            case ExceptionCollidedUnwind: {
                ULONG64 frame;

                context = *dispatch.ContextRecord;
                dispatch.ContextRecord = &context;
                RtlVirtualUnwind( UNW_FLAG_NHANDLER, dispatch.ImageBase,
                        dispatch.ControlPc, dispatch.FunctionEntry,
                        &context, NULL, &frame, NULL );
                goto unwind_done;
            }
            default:
                return STATUS_INVALID_DISPOSITION;
            }
        }
        /* hack: call wine handlers registered in the tib list */
        else while (is_valid_frame( (ULONG_PTR)teb_frame ) && (ULONG64)teb_frame < context.Rsp)
        {
            TRACE( "found wine frame %p rsp %p handler %p\n",
                   teb_frame, (void *)context.Rsp, teb_frame->Handler );
            dispatch.EstablisherFrame = (ULONG64)teb_frame;
            switch (call_teb_handler( rec, orig_context, &dispatch, teb_frame ))
            {
            case ExceptionContinueExecution:
                if (rec->ExceptionFlags & EH_NONCONTINUABLE) return STATUS_NONCONTINUABLE_EXCEPTION;
                return STATUS_SUCCESS;
            case ExceptionContinueSearch:
                break;
            case ExceptionNestedException:
                rec->ExceptionFlags |= EH_NESTED_CALL;
                TRACE( "nested exception\n" );
                break;
            case ExceptionCollidedUnwind: {
                ULONG64 frame;

                context = *dispatch.ContextRecord;
                dispatch.ContextRecord = &context;
                RtlVirtualUnwind( UNW_FLAG_NHANDLER, dispatch.ImageBase,
                        dispatch.ControlPc, dispatch.FunctionEntry,
                        &context, NULL, &frame, NULL );
                teb_frame = teb_frame->Prev;
                goto unwind_done;
            }
            default:
                return STATUS_INVALID_DISPOSITION;
            }
            teb_frame = teb_frame->Prev;
        }

        if (context.Rsp == (ULONG64)NtCurrentTeb()->Tib.StackBase) break;
    }
    return STATUS_UNHANDLED_EXCEPTION;
}


/*******************************************************************
 *		KiUserExceptionDispatcher (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( KiUserExceptionDispatcher,
                   __ASM_SEH(".seh_pushframe\n\t")
                   __ASM_SEH(".seh_stackalloc 0x590\n\t")
                   __ASM_SEH(".seh_savereg %rbx,0x90\n\t")
                   __ASM_SEH(".seh_savereg %rbp,0xa0\n\t")
                   __ASM_SEH(".seh_savereg %rsi,0xa8\n\t")
                   __ASM_SEH(".seh_savereg %rdi,0xb0\n\t")
                   __ASM_SEH(".seh_savereg %r12,0xd8\n\t")
                   __ASM_SEH(".seh_savereg %r13,0xe0\n\t")
                   __ASM_SEH(".seh_savereg %r14,0xe8\n\t")
                   __ASM_SEH(".seh_savereg %r15,0xf0\n\t")
                   __ASM_SEH(".seh_endprologue\n\t")
                   __ASM_CFI(".cfi_signal_frame\n\t")
                   __ASM_CFI(".cfi_def_cfa_offset 0\n\t")
                   __ASM_CFI(".cfi_offset %rbx,0x90\n\t")
                   __ASM_CFI(".cfi_offset %rbp,0xa0\n\t")
                   __ASM_CFI(".cfi_offset %rsi,0xa8\n\t")
                   __ASM_CFI(".cfi_offset %rdi,0xb0\n\t")
                   __ASM_CFI(".cfi_offset %r12,0xd8\n\t")
                   __ASM_CFI(".cfi_offset %r13,0xe0\n\t")
                   __ASM_CFI(".cfi_offset %r14,0xe8\n\t")
                   __ASM_CFI(".cfi_offset %r15,0xf0\n\t")
                   __ASM_CFI(".cfi_offset %rip,0x590\n\t")
                   __ASM_CFI(".cfi_offset %rsp,0x5a8\n\t")
                   "cld\n\t"
                   /* some anticheats need this exact instruction here */
                   "mov " __ASM_NAME("pWow64PrepareForException") "(%rip),%rax\n\t"
                   "test %rax,%rax\n\t"
                   "jz 1f\n\t"
                   "mov %rsp,%rdx\n\t"           /* context */
                   "lea 0x4f0(%rsp),%rcx\n\t"    /* rec */
                   "call *%rax\n"
                   "1:\tmov %rsp,%rdx\n\t" /* context */
                   "lea 0x4f0(%rsp),%rcx\n\t" /* rec */
                   "call " __ASM_NAME("dispatch_exception") "\n\t"
                   "int3" )


/*******************************************************************
 *		KiUserApcDispatcher (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( KiUserApcDispatcher,
                   __ASM_SEH(".seh_pushframe\n\t")
                   __ASM_SEH(".seh_stackalloc 0x4d0\n\t")  /* sizeof(CONTEXT) */
                   __ASM_SEH(".seh_savereg %rbx,0x90\n\t")
                   __ASM_SEH(".seh_savereg %rbp,0xa0\n\t")
                   __ASM_SEH(".seh_savereg %rsi,0xa8\n\t")
                   __ASM_SEH(".seh_savereg %rdi,0xb0\n\t")
                   __ASM_SEH(".seh_savereg %r12,0xd8\n\t")
                   __ASM_SEH(".seh_savereg %r13,0xe0\n\t")
                   __ASM_SEH(".seh_savereg %r14,0xe8\n\t")
                   __ASM_SEH(".seh_savereg %r15,0xf0\n\t")
                   __ASM_SEH(".seh_endprologue\n\t")
                   __ASM_CFI(".cfi_signal_frame\n\t")
                   __ASM_CFI(".cfi_def_cfa_offset 0\n\t")
                   __ASM_CFI(".cfi_offset %rbx,0x90\n\t")
                   __ASM_CFI(".cfi_offset %rbp,0xa0\n\t")
                   __ASM_CFI(".cfi_offset %rsi,0xa8\n\t")
                   __ASM_CFI(".cfi_offset %rdi,0xb0\n\t")
                   __ASM_CFI(".cfi_offset %r12,0xd8\n\t")
                   __ASM_CFI(".cfi_offset %r13,0xe0\n\t")
                   __ASM_CFI(".cfi_offset %r14,0xe8\n\t")
                   __ASM_CFI(".cfi_offset %r15,0xf0\n\t")
                   __ASM_CFI(".cfi_offset %rip,0x4d0\n\t")
                   __ASM_CFI(".cfi_offset %rsp,0x4e8\n\t")
                   "movq 0x00(%rsp),%rcx\n\t"  /* context->P1Home = arg1 */
                   "movq 0x08(%rsp),%rdx\n\t"  /* context->P2Home = arg2 */
                   "movq 0x10(%rsp),%r8\n\t"   /* context->P3Home = arg3 */
                   "movq 0x18(%rsp),%rax\n\t"  /* context->P4Home = func */
                   "movq %rsp,%r9\n\t"         /* context */
                   "callq *%rax\n\t"
                   "movq %rsp,%rcx\n\t"        /* context */
                   "movl $1,%edx\n\t"          /* alertable */
                   "call " __ASM_NAME("NtContinue") "\n\t"
                   "int3" )


/*******************************************************************
 *		KiUserCallbackDispatcher (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( KiUserCallbackDispatcher,
                   __ASM_SEH(".seh_pushframe\n\t")
                   __ASM_SEH(".seh_stackalloc 0x30\n\t")
                   __ASM_SEH(".seh_endprologue\n\t")
                   __ASM_CFI(".cfi_signal_frame\n\t")
                   __ASM_CFI(".cfi_def_cfa_offset 0\n\t")
                   __ASM_CFI(".cfi_offset %rip,0x30\n\t")
                   __ASM_CFI(".cfi_offset %rsp,0x48\n\t")
                   "movq 0x20(%rsp),%rcx\n\t"  /* args */
                   "movl 0x28(%rsp),%edx\n\t"  /* len */
                   "movl 0x2c(%rsp),%r8d\n\t"  /* id */
#ifdef __WINE_PE_BUILD
                   "movq %gs:0x30,%rax\n\t"     /* NtCurrentTeb() */
                   "movq 0x60(%rax),%rax\n\t"   /* peb */
                   "movq 0x58(%rax),%rax\n\t"   /* peb->KernelCallbackTable */
                   "call *(%rax,%r8,8)\n\t"     /* KernelCallbackTable[id] */
                   ".seh_handler " __ASM_NAME("user_callback_handler") ", @except\n\t"
                   ".globl " __ASM_NAME("KiUserCallbackDispatcherReturn") "\n"
                   __ASM_NAME("KiUserCallbackDispatcherReturn") ":\n\t"
#else
                   "call " __ASM_NAME("dispatch_user_callback") "\n\t"
#endif
                   "xorq %rcx,%rcx\n\t"         /* ret_ptr */
                   "xorl %edx,%edx\n\t"         /* ret_len */
                   "movl %eax,%r8d\n\t"         /* status */
                   "call " __ASM_NAME("NtCallbackReturn") "\n\t"
                   "movl %eax,%ecx\n\t"         /* status */
                   "call " __ASM_NAME("RtlRaiseStatus") "\n\t"
                   "int3" )


/**************************************************************************
 *              RtlIsEcCode (NTDLL.@)
 */
BOOLEAN WINAPI RtlIsEcCode( const void *ptr )
{
    return FALSE;
}


struct unwind_exception_frame
{
    EXCEPTION_REGISTRATION_RECORD frame;
    char dummy[0x10]; /* Layout 'dispatch' accessed from unwind_exception_handler() so it is above register
                       * save space when .seh handler is used. */
    DISPATCHER_CONTEXT *dispatch;
};

/**********************************************************************
 *           unwind_exception_handler
 *
 * Handler for exceptions happening while calling an unwind handler.
 */
DWORD __cdecl unwind_exception_handler( EXCEPTION_RECORD *rec, EXCEPTION_REGISTRATION_RECORD *frame,
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

/***********************************************************************
 *		unwind_handler_call_wrapper
 */
#ifdef __WINE_PE_BUILD
DWORD WINAPI unwind_handler_call_wrapper( EXCEPTION_RECORD *rec, void *frame,
                                   CONTEXT *context, DISPATCHER_CONTEXT *dispatch );

C_ASSERT( sizeof(struct unwind_exception_frame) == 0x28 );
C_ASSERT( offsetof(struct unwind_exception_frame, dispatch) == 0x20 );
C_ASSERT( offsetof(DISPATCHER_CONTEXT, LanguageHandler) == 0x30 );

__ASM_GLOBAL_FUNC( unwind_handler_call_wrapper,
                   "subq $0x28,%rsp\n\t"
                   ".seh_stackalloc 0x28\n\t"
                   ".seh_endprologue\n\t"
                   "movq %r9,0x20(%rsp)\n\t"   /* unwind_exception_frame->dispatch */
                   "callq *0x30(%r9)\n\t"      /* dispatch->LanguageHandler */
                   "nop\n\t"                   /* avoid epilogue so handler is called */
                   "addq $0x28, %rsp\n\t"
                   "ret\n\t"
                   ".seh_handler " __ASM_NAME("unwind_exception_handler") ", @except, @unwind\n\t" )
#else
static DWORD unwind_handler_call_wrapper( EXCEPTION_RECORD *rec, void *frame,
                                          CONTEXT *context, DISPATCHER_CONTEXT *dispatch )
{
    struct unwind_exception_frame wrapper_frame;
    DWORD res;

    wrapper_frame.frame.Handler = unwind_exception_handler;
    wrapper_frame.dispatch = dispatch;
    __wine_push_frame( &wrapper_frame.frame );
    res = dispatch->LanguageHandler( rec, (void *)dispatch->EstablisherFrame, dispatch->ContextRecord, dispatch );
    __wine_pop_frame( &wrapper_frame.frame );
    return res;
}
#endif

/**********************************************************************
 *           call_unwind_handler
 *
 * Call a single unwind handler.
 */
DWORD call_unwind_handler( EXCEPTION_RECORD *rec, DISPATCHER_CONTEXT *dispatch )
{
    DWORD res;

    TRACE( "calling handler %p (rec=%p, frame=%p context=%p, dispatch=%p)\n",
           dispatch->LanguageHandler, rec, (void *)dispatch->EstablisherFrame, dispatch->ContextRecord, dispatch );
    res = unwind_handler_call_wrapper( rec, (void *)dispatch->EstablisherFrame, dispatch->ContextRecord, dispatch );
    TRACE( "handler %p returned %lx\n", dispatch->LanguageHandler, res );

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


/**********************************************************************
 *           call_consolidate_callback
 *
 * Wrapper function to call a consolidate callback from a fake frame.
 * If the callback executes RtlUnwindEx (like for example done in C++ handlers),
 * we have to skip all frames which were already processed. To do that we
 * trick the unwinding functions into thinking the call came from the specified
 * context. All CFI instructions are either DW_CFA_def_cfa_expression or
 * DW_CFA_expression, and the expressions have the following format:
 *
 * DW_OP_breg6; sleb128 0x10            | Load %rbp + 0x10
 * DW_OP_deref                          | Get *(%rbp + 0x10) == context
 * DW_OP_plus_uconst; uleb128 <OFFSET>  | Add offset to get struct member
 * [DW_OP_deref]                        | Dereference, only for CFA
 */
extern void * WINAPI call_consolidate_callback( CONTEXT *context,
                                                void *(CALLBACK *callback)(EXCEPTION_RECORD *),
                                                EXCEPTION_RECORD *rec );
__ASM_GLOBAL_FUNC( call_consolidate_callback,
                   "pushq %rbp\n\t"
                   __ASM_CFI(".cfi_adjust_cfa_offset 8\n\t")
                   __ASM_CFI(".cfi_rel_offset %rbp,0\n\t")
                   "movq %rsp,%rbp\n\t"
                   __ASM_CFI(".cfi_def_cfa_register %rbp\n\t")

                   /* Setup SEH machine frame. */
                   "subq $0x28,%rsp\n\t"
                   __ASM_CFI(".cfi_adjust_cfa_offset 0x28\n\t")
                   "movq 0xf8(%rcx),%rax\n\t" /* Context->Rip */
                   "movq %rax,(%rsp)\n\t"
                   "movq 0x98(%rcx),%rax\n\t" /* context->Rsp */
                   "movq %rax,0x18(%rsp)\n\t"
                   __ASM_SEH(".seh_pushframe\n\t")
                   __ASM_SEH(".seh_endprologue\n\t")

                   "subq $0x108,%rsp\n\t" /* 10*16 (float regs) + 8*8 (int regs) + 32 (shadow store) + 8 (align). */
                   __ASM_SEH(".seh_stackalloc 0x108\n\t")
                   __ASM_CFI(".cfi_adjust_cfa_offset 0x108\n\t")

                   /* Setup CFI unwind to context. */
                   "movq %rcx,0x10(%rbp)\n\t"
                   __ASM_CFI(".cfi_remember_state\n\t")
                   __ASM_CFI(".cfi_escape 0x0f,0x07,0x76,0x10,0x06,0x23,0x98,0x01,0x06\n\t") /* CFA    */
                   __ASM_CFI(".cfi_escape 0x10,0x03,0x06,0x76,0x10,0x06,0x23,0x90,0x01\n\t") /* %rbx   */
                   __ASM_CFI(".cfi_escape 0x10,0x04,0x06,0x76,0x10,0x06,0x23,0xa8,0x01\n\t") /* %rsi   */
                   __ASM_CFI(".cfi_escape 0x10,0x05,0x06,0x76,0x10,0x06,0x23,0xb0,0x01\n\t") /* %rdi   */
                   __ASM_CFI(".cfi_escape 0x10,0x06,0x06,0x76,0x10,0x06,0x23,0xa0,0x01\n\t") /* %rbp   */
                   __ASM_CFI(".cfi_escape 0x10,0x0c,0x06,0x76,0x10,0x06,0x23,0xd8,0x01\n\t") /* %r12   */
                   __ASM_CFI(".cfi_escape 0x10,0x0d,0x06,0x76,0x10,0x06,0x23,0xe0,0x01\n\t") /* %r13   */
                   __ASM_CFI(".cfi_escape 0x10,0x0e,0x06,0x76,0x10,0x06,0x23,0xe8,0x01\n\t") /* %r14   */
                   __ASM_CFI(".cfi_escape 0x10,0x0f,0x06,0x76,0x10,0x06,0x23,0xf0,0x01\n\t") /* %r15   */
                   __ASM_CFI(".cfi_escape 0x10,0x10,0x06,0x76,0x10,0x06,0x23,0xf8,0x01\n\t") /* %rip   */
                   __ASM_CFI(".cfi_escape 0x10,0x17,0x06,0x76,0x10,0x06,0x23,0x80,0x04\n\t") /* %xmm6  */
                   __ASM_CFI(".cfi_escape 0x10,0x18,0x06,0x76,0x10,0x06,0x23,0x90,0x04\n\t") /* %xmm7  */
                   __ASM_CFI(".cfi_escape 0x10,0x19,0x06,0x76,0x10,0x06,0x23,0xa0,0x04\n\t") /* %xmm8  */
                   __ASM_CFI(".cfi_escape 0x10,0x1a,0x06,0x76,0x10,0x06,0x23,0xb0,0x04\n\t") /* %xmm9  */
                   __ASM_CFI(".cfi_escape 0x10,0x1b,0x06,0x76,0x10,0x06,0x23,0xc0,0x04\n\t") /* %xmm10 */
                   __ASM_CFI(".cfi_escape 0x10,0x1c,0x06,0x76,0x10,0x06,0x23,0xd0,0x04\n\t") /* %xmm11 */
                   __ASM_CFI(".cfi_escape 0x10,0x1d,0x06,0x76,0x10,0x06,0x23,0xe0,0x04\n\t") /* %xmm12 */
                   __ASM_CFI(".cfi_escape 0x10,0x1e,0x06,0x76,0x10,0x06,0x23,0xf0,0x04\n\t") /* %xmm13 */
                   __ASM_CFI(".cfi_escape 0x10,0x1f,0x06,0x76,0x10,0x06,0x23,0x80,0x05\n\t") /* %xmm14 */
                   __ASM_CFI(".cfi_escape 0x10,0x20,0x06,0x76,0x10,0x06,0x23,0x90,0x05\n\t") /* %xmm15 */

                   /* Setup SEH unwind registers restore. */
                   "movq 0xa0(%rcx),%rax\n\t" /* context->Rbp */
                   "movq %rax,0x100(%rsp)\n\t"
                   __ASM_SEH(".seh_savereg %rbp, 0x100\n\t")
                   "movq 0x90(%rcx),%rax\n\t" /* context->Rbx */
                   "movq %rax,0x20(%rsp)\n\t"
                   __ASM_SEH(".seh_savereg %rbx, 0x20\n\t")
                   "movq 0xa8(%rcx),%rax\n\t" /* context->Rsi */
                   "movq %rax,0x28(%rsp)\n\t"
                   __ASM_SEH(".seh_savereg %rsi, 0x28\n\t")
                   "movq 0xb0(%rcx),%rax\n\t" /* context->Rdi */
                   "movq %rax,0x30(%rsp)\n\t"
                   __ASM_SEH(".seh_savereg %rdi, 0x30\n\t")

                   "movq 0xd8(%rcx),%rax\n\t" /* context->R12 */
                   "movq %rax,0x38(%rsp)\n\t"
                   __ASM_SEH(".seh_savereg %r12, 0x38\n\t")
                   "movq 0xe0(%rcx),%rax\n\t" /* context->R13 */
                   "movq %rax,0x40(%rsp)\n\t"
                   __ASM_SEH(".seh_savereg %r13, 0x40\n\t")
                   "movq 0xe8(%rcx),%rax\n\t" /* context->R14 */
                   "movq %rax,0x48(%rsp)\n\t"
                   __ASM_SEH(".seh_savereg %r14, 0x48\n\t")
                   "movq 0xf0(%rcx),%rax\n\t" /* context->R15 */
                   "movq %rax,0x50(%rsp)\n\t"
                   __ASM_SEH(".seh_savereg %r15, 0x50\n\t")
                   "pushq %rsi\n\t"
                   "pushq %rdi\n\t"
                   "leaq 0x200(%rcx),%rsi\n\t"
                   "leaq 0x70(%rsp),%rdi\n\t"
                   "movq $0x14,%rcx\n\t"
                   "cld\n\t"
                   "rep; movsq\n\t"
                   "popq %rdi\n\t"
                   "popq %rsi\n\t"
                   __ASM_SEH(".seh_savexmm %xmm6, 0x60\n\t")
                   __ASM_SEH(".seh_savexmm %xmm7, 0x70\n\t")
                   __ASM_SEH(".seh_savexmm %xmm8, 0x80\n\t")
                   __ASM_SEH(".seh_savexmm %xmm9, 0x90\n\t")
                   __ASM_SEH(".seh_savexmm %xmm10, 0xa0\n\t")
                   __ASM_SEH(".seh_savexmm %xmm11, 0xb0\n\t")
                   __ASM_SEH(".seh_savexmm %xmm12, 0xc0\n\t")
                   __ASM_SEH(".seh_savexmm %xmm13, 0xd0\n\t")
                   __ASM_SEH(".seh_savexmm %xmm14, 0xe0\n\t")
                   __ASM_SEH(".seh_savexmm %xmm15, 0xf0\n\t")

                   /* call the callback. */
                   "movq %r8,%rcx\n\t"
                   "callq *%rdx\n\t"
                   __ASM_CFI(".cfi_restore_state\n\t")
                   "nop\n\t" /* Otherwise RtlVirtualUnwind() will think we are inside epilogue and
                              * interpret / execute the rest of opcodes here instead of unwind through
                              * machine frame. */
                   "leaq 0(%rbp),%rsp\n\t"
                   __ASM_CFI(".cfi_def_cfa_register %rsp\n\t")
                   "popq %rbp\n\t"
                   __ASM_CFI(".cfi_adjust_cfa_offset -8\n\t")
                   __ASM_CFI(".cfi_same_value %rbp\n\t")
                   "ret")

/*******************************************************************
 *              RtlRestoreContext (NTDLL.@)
 */
void CDECL RtlRestoreContext( CONTEXT *context, EXCEPTION_RECORD *rec )
{
    EXCEPTION_REGISTRATION_RECORD *teb_frame = NtCurrentTeb()->Tib.ExceptionList;

    if (rec && rec->ExceptionCode == STATUS_LONGJUMP && rec->NumberParameters >= 1)
    {
        struct MSVCRT_JUMP_BUFFER *jmp = (struct MSVCRT_JUMP_BUFFER *)rec->ExceptionInformation[0];
        context->Rbx   = jmp->Rbx;
        context->Rsp   = jmp->Rsp;
        context->Rbp   = jmp->Rbp;
        context->Rsi   = jmp->Rsi;
        context->Rdi   = jmp->Rdi;
        context->R12   = jmp->R12;
        context->R13   = jmp->R13;
        context->R14   = jmp->R14;
        context->R15   = jmp->R15;
        context->Rip   = jmp->Rip;
        context->Xmm6  = jmp->Xmm6;
        context->Xmm7  = jmp->Xmm7;
        context->Xmm8  = jmp->Xmm8;
        context->Xmm9  = jmp->Xmm9;
        context->Xmm10 = jmp->Xmm10;
        context->Xmm11 = jmp->Xmm11;
        context->Xmm12 = jmp->Xmm12;
        context->Xmm13 = jmp->Xmm13;
        context->Xmm14 = jmp->Xmm14;
        context->Xmm15 = jmp->Xmm15;
        context->MxCsr = jmp->MxCsr;
        context->FltSave.MxCsr = jmp->MxCsr;
        context->FltSave.ControlWord = jmp->FpCsr;
    }
    else if (rec && rec->ExceptionCode == STATUS_UNWIND_CONSOLIDATE && rec->NumberParameters >= 1)
    {
        PVOID (CALLBACK *consolidate)(EXCEPTION_RECORD *) = (void *)rec->ExceptionInformation[0];
        TRACE( "calling consolidate callback %p (rec=%p)\n", consolidate, rec );
        context->Rip = (ULONG64)call_consolidate_callback( context, consolidate, rec );
    }

    /* hack: remove no longer accessible TEB frames */
    while (is_valid_frame( (ULONG_PTR)teb_frame ) && (ULONG64)teb_frame < context->Rsp)
    {
        TRACE( "removing TEB frame: %p\n", teb_frame );
        teb_frame = __wine_pop_frame( teb_frame );
    }

    TRACE( "returning to %p stack %p\n", (void *)context->Rip, (void *)context->Rsp );
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

    RtlCaptureContext( context );
    new_context = *context;

    /* build an exception record, if we do not have one */
    if (!rec)
    {
        record.ExceptionCode    = STATUS_UNWIND;
        record.ExceptionFlags   = 0;
        record.ExceptionRecord  = NULL;
        record.ExceptionAddress = (void *)context->Rip;
        record.NumberParameters = 0;
        rec = &record;
    }

    rec->ExceptionFlags |= EH_UNWINDING | (end_frame ? 0 : EH_EXIT_UNWIND);

    TRACE( "code=%lx flags=%lx end_frame=%p target_ip=%p\n",
           rec->ExceptionCode, rec->ExceptionFlags, end_frame, target_ip );
    for (i = 0; i < min( EXCEPTION_MAXIMUM_PARAMETERS, rec->NumberParameters ); i++)
        TRACE( " info[%ld]=%016I64x\n", i, rec->ExceptionInformation[i] );
    TRACE_CONTEXT( context );

    dispatch.EstablisherFrame = context->Rsp;
    dispatch.TargetIp         = (ULONG64)target_ip;
    dispatch.ContextRecord    = context;
    dispatch.HistoryTable     = table;

    for (;;)
    {
        status = virtual_unwind( UNW_FLAG_UHANDLER, &dispatch, &new_context );
        if (status != STATUS_SUCCESS) raise_status( status, rec );

    unwind_done:
        if (!dispatch.EstablisherFrame) break;

        if (!is_valid_frame( dispatch.EstablisherFrame ))
        {
            ERR( "invalid frame %p (%p-%p)\n", (void *)dispatch.EstablisherFrame,
                 NtCurrentTeb()->Tib.StackLimit, NtCurrentTeb()->Tib.StackBase );
            rec->ExceptionFlags |= EH_STACK_INVALID;
            break;
        }

        if (dispatch.LanguageHandler)
        {
            if (end_frame && (dispatch.EstablisherFrame > (ULONG64)end_frame))
            {
                ERR( "invalid end frame %p/%p\n", (void *)dispatch.EstablisherFrame, end_frame );
                raise_status( STATUS_INVALID_UNWIND_TARGET, rec );
            }
            if (dispatch.EstablisherFrame == (ULONG64)end_frame) rec->ExceptionFlags |= EH_TARGET_UNWIND;
            if (call_unwind_handler( rec, &dispatch ) == ExceptionCollidedUnwind)
            {
                ULONG64 frame;

                new_context = *dispatch.ContextRecord;
                new_context.ContextFlags &= ~0x40;
                *context = new_context;
                dispatch.ContextRecord = context;
                RtlVirtualUnwind( UNW_FLAG_NHANDLER, dispatch.ImageBase,
                        dispatch.ControlPc, dispatch.FunctionEntry,
                        &new_context, NULL, &frame, NULL );
                rec->ExceptionFlags |= EH_COLLIDED_UNWIND;
                goto unwind_done;
            }
            rec->ExceptionFlags &= ~EH_COLLIDED_UNWIND;
        }
        else  /* hack: call builtin handlers registered in the tib list */
        {
            DWORD64 backup_frame = dispatch.EstablisherFrame;
            while (is_valid_frame( (ULONG_PTR)teb_frame ) &&
                   (ULONG64)teb_frame < new_context.Rsp &&
                   (ULONG64)teb_frame < (ULONG64)end_frame)
            {
                TRACE( "found builtin frame %p handler %p\n", teb_frame, teb_frame->Handler );
                dispatch.EstablisherFrame = (ULONG64)teb_frame;
                if (call_teb_unwind_handler( rec, &dispatch, teb_frame ) == ExceptionCollidedUnwind)
                {
                    ULONG64 frame;

                    teb_frame = __wine_pop_frame( teb_frame );

                    new_context = *dispatch.ContextRecord;
                    new_context.ContextFlags &= ~0x40;
                    *context = new_context;
                    dispatch.ContextRecord = context;
                    RtlVirtualUnwind( UNW_FLAG_NHANDLER, dispatch.ImageBase,
                            dispatch.ControlPc, dispatch.FunctionEntry,
                            &new_context, NULL, &frame, NULL );
                    rec->ExceptionFlags |= EH_COLLIDED_UNWIND;
                    goto unwind_done;
                }
                teb_frame = __wine_pop_frame( teb_frame );
            }
            if ((ULONG64)teb_frame == (ULONG64)end_frame && (ULONG64)end_frame < new_context.Rsp) break;
            dispatch.EstablisherFrame = backup_frame;
        }

        if (dispatch.EstablisherFrame == (ULONG64)end_frame) break;
        *context = new_context;
    }

    context->Rax = (ULONG64)retval;
    context->Rip = (ULONG64)target_ip;
    RtlRestoreContext(context, rec);
}


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

    TRACE( "%p %p %p %p\n", rec, frame, context, dispatch );
    if (TRACE_ON(seh)) dump_scope_table( dispatch->ImageBase, table );

    if (rec->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND))
    {
        for (i = dispatch->ScopeIndex; i < table->Count; i++)
        {
            if (dispatch->ControlPc >= dispatch->ImageBase + table->ScopeRecord[i].BeginAddress &&
                dispatch->ControlPc < dispatch->ImageBase + table->ScopeRecord[i].EndAddress)
            {
                PTERMINATION_HANDLER handler;

                if (table->ScopeRecord[i].JumpTarget) continue;

                if (rec->ExceptionFlags & EH_TARGET_UNWIND &&
                    dispatch->TargetIp >= dispatch->ImageBase + table->ScopeRecord[i].BeginAddress &&
                    dispatch->TargetIp < dispatch->ImageBase + table->ScopeRecord[i].EndAddress)
                {
                    break;
                }

                handler = (PTERMINATION_HANDLER)(dispatch->ImageBase + table->ScopeRecord[i].HandlerAddress);
                dispatch->ScopeIndex = i+1;

                TRACE( "calling __finally %p frame %p\n", handler, frame );
                handler( TRUE, frame );
            }
        }
        return ExceptionContinueSearch;
    }

    for (i = dispatch->ScopeIndex; i < table->Count; i++)
    {
        if (dispatch->ControlPc >= dispatch->ImageBase + table->ScopeRecord[i].BeginAddress &&
            dispatch->ControlPc < dispatch->ImageBase + table->ScopeRecord[i].EndAddress)
        {
            if (!table->ScopeRecord[i].JumpTarget) continue;
            if (table->ScopeRecord[i].HandlerAddress != EXCEPTION_EXECUTE_HANDLER)
            {
                EXCEPTION_POINTERS ptrs;
                PEXCEPTION_FILTER filter;

                filter = (PEXCEPTION_FILTER)(dispatch->ImageBase + table->ScopeRecord[i].HandlerAddress);
                ptrs.ExceptionRecord = rec;
                ptrs.ContextRecord = context;
                TRACE( "calling filter %p ptrs %p frame %p\n", filter, &ptrs, frame );
                switch (filter( &ptrs, frame ))
                {
                case EXCEPTION_EXECUTE_HANDLER:
                    break;
                case EXCEPTION_CONTINUE_SEARCH:
                    continue;
                case EXCEPTION_CONTINUE_EXECUTION:
                    return ExceptionContinueExecution;
                }
            }
            TRACE( "unwinding to target %p\n", (char *)dispatch->ImageBase + table->ScopeRecord[i].JumpTarget );
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
                   "sub $0x4f8,%rsp\n\t"
                   __ASM_SEH(".seh_stackalloc 0x4f8\n\t")
                   __ASM_SEH(".seh_endprologue\n\t")
                   __ASM_CFI(".cfi_adjust_cfa_offset 0x4f8\n\t")
                   "movq %rcx,0x500(%rsp)\n\t"
                   "leaq 0x20(%rsp),%rcx\n\t"
                   "call " __ASM_NAME("RtlCaptureContext") "\n\t"
                   "leaq 0x20(%rsp),%rdx\n\t"   /* context pointer */
                   "leaq 0x500(%rsp),%rax\n\t"  /* orig stack pointer */
                   "movq %rax,0x98(%rdx)\n\t"   /* context->Rsp */
                   "movq (%rax),%rcx\n\t"       /* original first parameter */
                   "movq %rcx,0x80(%rdx)\n\t"   /* context->Rcx */
                   "movq 0x4f8(%rsp),%rax\n\t"  /* return address */
                   "movq %rax,0xf8(%rdx)\n\t"   /* context->Rip */
                   "movq %rax,0x10(%rcx)\n\t"   /* rec->ExceptionAddress */
                   "movl $1,%r8d\n\t"
                   "movq %gs:(0x30),%rax\n\t"   /* Teb */
                   "movq 0x60(%rax),%rax\n\t"   /* Peb */
                   "cmpb $0,0x02(%rax)\n\t"     /* BeingDebugged */
                   "jne 1f\n\t"
                   "call " __ASM_NAME("dispatch_exception") "\n"
                   "1:\tcall " __ASM_NAME("NtRaiseException") "\n\t"
                   "movq %rax,%rcx\n\t"
                   "call " __ASM_NAME("RtlRaiseStatus") /* does not return */ );


static inline ULONG hash_pointers( void **ptrs, ULONG count )
{
    /* Based on MurmurHash2, which is in the public domain */
    static const ULONG m = 0x5bd1e995;
    static const ULONG r = 24;
    ULONG hash = count * sizeof(void*);
    for (; count > 0; ptrs++, count--)
    {
        ULONG_PTR data = (ULONG_PTR)*ptrs;
        ULONG k1 = (ULONG)(data & 0xffffffff), k2 = (ULONG)(data >> 32);
        k1 *= m;
        k1 = (k1 ^ (k1 >> r)) * m;
        k2 *= m;
        k2 = (k2 ^ (k2 >> r)) * m;
        hash = (((hash * m) ^ k1) * m) ^ k2;
    }
    hash = (hash ^ (hash >> 13)) * m;
    return hash ^ (hash >> 15);
}


/*************************************************************************
 *		RtlCaptureStackBackTrace (NTDLL.@)
 */
USHORT WINAPI RtlCaptureStackBackTrace( ULONG skip, ULONG count, PVOID *buffer, ULONG *hash )
{
    UNWIND_HISTORY_TABLE table;
    DISPATCHER_CONTEXT dispatch;
    CONTEXT context;
    NTSTATUS status;
    ULONG i;
    USHORT num_entries = 0;

    TRACE( "(%lu, %lu, %p, %p)\n", skip, count, buffer, hash );

    RtlCaptureContext( &context );
    dispatch.TargetIp      = 0;
    dispatch.ContextRecord = &context;
    dispatch.HistoryTable  = &table;
    if (hash) *hash = 0;
    for (i = 0; i < skip + count; i++)
    {
        status = virtual_unwind( UNW_FLAG_NHANDLER, &dispatch, &context );
        if (status != STATUS_SUCCESS) return i;

        if (!dispatch.EstablisherFrame) break;

        if (!is_valid_frame( dispatch.EstablisherFrame ))
        {
            ERR( "invalid frame %p (%p-%p)\n", (void *)dispatch.EstablisherFrame,
                 NtCurrentTeb()->Tib.StackLimit, NtCurrentTeb()->Tib.StackBase );
            break;
        }

        if (context.Rsp == (ULONG64)NtCurrentTeb()->Tib.StackBase) break;

        if (i >= skip) buffer[num_entries++] = (void *)context.Rip;
    }
    if (hash && num_entries > 0) *hash = hash_pointers( buffer, num_entries );
    TRACE( "captured %hu frames\n", num_entries );
    return num_entries;
}


/***********************************************************************
 *           RtlUserThreadStart (NTDLL.@)
 */
#ifdef __WINE_PE_BUILD
__ASM_GLOBAL_FUNC( RtlUserThreadStart,
                   "subq $0x28,%rsp\n\t"
                   ".seh_stackalloc 0x28\n\t"
                   ".seh_endprologue\n\t"
                   "movq %rdx,%r8\n\t"
                   "movq %rcx,%rdx\n\t"
                   "xorq %rcx,%rcx\n\t"
                   "movq " __ASM_NAME( "pBaseThreadInitThunk" ) "(%rip),%r9\n\t"
                   "call *%r9\n\t"
                   "int3\n\t"
                   ".seh_handler " __ASM_NAME("call_unhandled_exception_handler") ", @except" )
#else
void WINAPI RtlUserThreadStart( PRTL_THREAD_START_ROUTINE entry, void *arg )
{
    __TRY
    {
        pBaseThreadInitThunk( 0, (LPTHREAD_START_ROUTINE)entry, arg );
    }
    __EXCEPT(call_unhandled_exception_filter)
    {
        NtTerminateProcess( GetCurrentProcess(), GetExceptionCode() );
    }
    __ENDTRY
}
#endif


/***********************************************************************
 *           signal_start_thread
 */
extern void CDECL DECLSPEC_NORETURN signal_start_thread( CONTEXT *ctx );
__ASM_GLOBAL_FUNC( signal_start_thread,
                   "movq %rcx,%rbx\n\t"        /* context */
                   /* clear the thread stack */
                   "andq $~0xfff,%rcx\n\t"     /* round down to page size */
                   "leaq -0xf0000(%rcx),%rdi\n\t"
                   "movq %rdi,%rsp\n\t"
                   "subq %rdi,%rcx\n\t"
                   "xorl %eax,%eax\n\t"
                   "shrq $3,%rcx\n\t"
                   "rep; stosq\n\t"
                   /* switch to the initial context */
                   "leaq -32(%rbx),%rsp\n\t"
                   "movq %rbx,%rcx\n\t"
                   "movl $1,%edx\n\t"
                   "call " __ASM_NAME("NtContinue") )


/******************************************************************
 *		LdrInitializeThunk (NTDLL.@)
 */
void WINAPI LdrInitializeThunk( CONTEXT *context, ULONG_PTR unk2, ULONG_PTR unk3, ULONG_PTR unk4 )
{
    loader_init( context, (void **)&context->Rcx );
    TRACE_(relay)( "\1Starting thread proc %p (arg=%p)\n", (void *)context->Rcx, (void *)context->Rdx );
    signal_start_thread( context );
}


/***********************************************************************
 *           process_breakpoint
 */
#ifdef __WINE_PE_BUILD
__ASM_GLOBAL_FUNC( process_breakpoint,
                   ".seh_endprologue\n\t"
                   ".seh_handler process_breakpoint_handler, @except\n\t"
                   "int $3\n\t"
                   "ret\n"
                   "process_breakpoint_handler:\n\t"
                   "incq 0xf8(%r8)\n\t"  /* context->Rip */
                   "xorl %eax,%eax\n\t"  /* ExceptionContinueExecution */
                   "ret" )
#else
void WINAPI process_breakpoint(void)
{
    __TRY
    {
        __asm__ volatile("int $3");
    }
    __EXCEPT_ALL
    {
        /* do nothing */
    }
    __ENDTRY
}
#endif


/***********************************************************************
 *		DbgUiRemoteBreakin   (NTDLL.@)
 */
#ifdef __WINE_PE_BUILD
__ASM_GLOBAL_FUNC( DbgUiRemoteBreakin,
                   "subq $0x28,%rsp\n\t"
                   ".seh_stackalloc 0x28\n\t"
                   ".seh_endprologue\n\t"
                   ".seh_handler DbgUiRemoteBreakin_handler, @except\n\t"
                   "mov %gs:0x30,%rax\n\t"
                   "mov 0x60(%rax),%rax\n\t"
                   "cmpb $0,2(%rax)\n\t"
                   "je 1f\n\t"
                   "call " __ASM_NAME("DbgBreakPoint") "\n"
                   "1:\txorl %ecx,%ecx\n\t"
                   "call " __ASM_NAME("RtlExitUserThread") "\n"
                   "DbgUiRemoteBreakin_handler:\n\t"
                   "movq %rdx,%rsp\n\t"  /* frame */
                   "jmp 1b" )
#else
void WINAPI DbgUiRemoteBreakin( void *arg )
{
    if (NtCurrentTeb()->Peb->BeingDebugged)
    {
        __TRY
        {
            DbgBreakPoint();
        }
        __EXCEPT_ALL
        {
            /* do nothing */
        }
        __ENDTRY
    }
    RtlExitUserThread( STATUS_SUCCESS );
}
#endif

/**********************************************************************
 *		DbgBreakPoint   (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( DbgBreakPoint, "int $3; ret"
                    "\n\tnop; nop; nop; nop; nop; nop; nop; nop"
                    "\n\tnop; nop; nop; nop; nop; nop" );

/**********************************************************************
 *		DbgUserBreakPoint   (NTDLL.@)
 */
__ASM_GLOBAL_FUNC( DbgUserBreakPoint, "int $3; ret"
                    "\n\tnop; nop; nop; nop; nop; nop; nop; nop"
                    "\n\tnop; nop; nop; nop; nop; nop" );

/***********************************************************************
 *              RtlWow64SuspendThread (NTDLL.@)
 */
NTSTATUS WINAPI RtlWow64SuspendThread( HANDLE thread, ULONG *count )
{
    return NtSuspendThread( thread, count );
}

#endif  /* __x86_64__ */
