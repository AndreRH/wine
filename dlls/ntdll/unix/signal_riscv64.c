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

#if 0
#pragma makedep unix
#endif

#ifdef __riscv64__

#include "config.h"

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYSCALL_H
# include <syscall.h>
#else
# ifdef HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
# endif
#endif
#ifdef HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif
#ifdef HAVE_SYS_UCONTEXT_H
# include <sys/ucontext.h>
#endif
#ifdef HAVE_LIBUNWIND
# define UNW_LOCAL_ONLY
# include <libunwind.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "wine/asm.h"
#include "unix_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);

#include "dwarf.h"

/***********************************************************************
 * signal context platform-specific definitions
 */
#ifdef linux

/* All Registers access - only for local access */
# define REGn_sig(reg_num, context) ((context)->uc_mcontext.__gregs[reg_num])

/* Special Registers access  */
# define SP_sig(context)            REGn_sig(2, context)    /* Stack pointer */
# define PC_sig(context)            REGn_sig(0, context)    /* Program counter */
# define FP_sig(context)            REGn_sig(8, context)    /* Frame pointer */
# define RA_sig(context)            REGn_sig(1, context)    /* Return Address */

#endif /* linux */

struct syscall_frame
{
    ULONG64               x[32];          /* 000 */ // 0==pc
    ULONG                 restore_flags;  /* 100 */
    ULONG                 align1;         /* 104 */
    struct syscall_frame *prev_frame;     /* 108 */
    SYSTEM_SERVICE_TABLE *syscall_table;  /* 110 */
    //NEON128               v[32];          /* 130 */
};

// TODO: C_ASSERT( sizeof( struct syscall_frame ) == 0x330 );

struct riscv64_thread_data
{
    void                 *exit_frame;    /* 02f0 exit frame pointer */
    struct syscall_frame *syscall_frame; /* 02f8 frame pointer on syscall entry */
};

C_ASSERT( sizeof(struct riscv64_thread_data) <= sizeof(((struct ntdll_thread_data *)0)->cpu_data) );
C_ASSERT( offsetof( TEB, GdiTebBatch ) + offsetof( struct riscv64_thread_data, exit_frame ) == 0x2f0 );
C_ASSERT( offsetof( TEB, GdiTebBatch ) + offsetof( struct riscv64_thread_data, syscall_frame ) == 0x2f8 );

static inline struct riscv64_thread_data *riscv64_thread_data(void)
{
    return (struct riscv64_thread_data *)ntdll_get_thread_data()->cpu_data;
}

static BOOL is_inside_syscall( ucontext_t *sigcontext )
{
    return ((char *)SP_sig(sigcontext) >= (char *)ntdll_get_thread_data()->kernel_stack &&
            (char *)SP_sig(sigcontext) <= (char *)riscv64_thread_data()->syscall_frame);
}

extern void raise_func_trampoline( EXCEPTION_RECORD *rec, CONTEXT *context, void *dispatcher );

/***********************************************************************
 *           dwarf_virtual_unwind
 *
 * Equivalent of RtlVirtualUnwind for builtin modules.
 */
static NTSTATUS dwarf_virtual_unwind( ULONG64 ip, ULONG64 *frame, CONTEXT *context,
                                      const struct dwarf_fde *fde, const struct dwarf_eh_bases *bases,
                                      PEXCEPTION_ROUTINE *handler, void **handler_data )
{
    const struct dwarf_cie *cie;
    const unsigned char *ptr, *augmentation, *end;
    ULONG_PTR len, code_end;
    struct frame_info info;
    struct frame_state state_stack[MAX_SAVED_STATES];
    int aug_z_format = 0;
    unsigned char lsda_encoding = DW_EH_PE_omit;
    DWORD64 prev_s11 = context->S11;

    ERR("%p dwarf_virtual_unwind\n", dwarf_virtual_unwind);
    ERR("%p unwind_builtin_dll\n", unwind_builtin_dll);
    ERR("%p __wine_syscall_dispatcher\n", __wine_syscall_dispatcher);
    ERR("%p __wine_syscall_dispatcher_return\n", __wine_syscall_dispatcher_return);
    ERR("%p __wine_unix_call_dispatcher\n", __wine_unix_call_dispatcher);
    ERR("%p raise_func_trampoline\n", raise_func_trampoline);

    memset( &info, 0, sizeof(info) );
    info.state_stack = state_stack;
    info.ip = (ULONG_PTR)bases->func;
    *handler = NULL;

    cie = (const struct dwarf_cie *)((const char *)&fde->cie_offset - fde->cie_offset);

    /* parse the CIE first */

    if (cie->version != 1 && cie->version != 3)
    {
        FIXME( "unknown CIE version %u at %p\n", cie->version, cie );
        return STATUS_INVALID_DISPOSITION;
    }
    ptr = cie->augmentation + strlen((const char *)cie->augmentation) + 1;

    info.code_align = dwarf_get_uleb128( &ptr );
    info.data_align = dwarf_get_sleb128( &ptr );
    if (cie->version == 1)
        info.retaddr_reg = *ptr++;
    else
        info.retaddr_reg = dwarf_get_uleb128( &ptr );
    info.state.cfa_rule = RULE_CFA_OFFSET;

    TRACE( "function %lx base %p cie %p len %x id %x version %x aug '%s' code_align %lu data_align %ld retaddr %s\n",
           ip, bases->func, cie, cie->length, cie->id, cie->version, cie->augmentation,
           info.code_align, info.data_align, dwarf_reg_names[info.retaddr_reg] );

    end = NULL;
    for (augmentation = cie->augmentation; *augmentation; augmentation++)
    {
        switch (*augmentation)
        {
        case 'z':
            len = dwarf_get_uleb128( &ptr );
            end = ptr + len;
            aug_z_format = 1;
            continue;
        case 'L':
            lsda_encoding = *ptr++;
            continue;
        case 'P':
        {
            unsigned char encoding = *ptr++;
            *handler = (void *)dwarf_get_ptr( &ptr, encoding, bases );
            continue;
        }
        case 'R':
            info.fde_encoding = *ptr++;
            continue;
        case 'S':
            info.signal_frame = 1;
            continue;
        }
        FIXME( "unknown augmentation '%c'\n", *augmentation );
        if (!end) return STATUS_INVALID_DISPOSITION;  /* cannot continue */
        break;
    }
    if (end) ptr = end;

    end = (const unsigned char *)(&cie->length + 1) + cie->length;
    execute_cfa_instructions( ptr, end, ip, &info, bases );

    ptr = (const unsigned char *)(fde + 1);
    info.ip = dwarf_get_ptr( &ptr, info.fde_encoding, bases );  /* fde code start */
    code_end = info.ip + dwarf_get_ptr( &ptr, info.fde_encoding & 0x0f, bases );  /* fde code length */

    if (aug_z_format)  /* get length of augmentation data */
    {
        len = dwarf_get_uleb128( &ptr );
        end = ptr + len;
    }
    else end = NULL;

    *handler_data = (void *)dwarf_get_ptr( &ptr, lsda_encoding, bases );
    if (end) ptr = end;

    end = (const unsigned char *)(&fde->length + 1) + fde->length;
    TRACE( "fde %p len %x personality %p lsda %p code %lx-%lx\n",
           fde, fde->length, *handler, *handler_data, info.ip, code_end );
    execute_cfa_instructions( ptr, end, ip, &info, bases );
    *frame = context->Sp;
    apply_frame_state( context, &info.state, bases );
    context->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
    /* Set Pc based on Ra; libunwind also does this as part of unw_step. */
    context->Pc = context->Ra;

    if (bases->func == (void *)raise_func_trampoline) {
        /* raise_func_trampoline has a pointer to a full CONTEXT stored in x28;
         * restore the original Lr value from there. The function we unwind to
         * might be a leaf function that hasn't backed up its own original Lr
         * value on the stack.
         * We could also just restore the full context here without doing
         * unw_step at all. */
        const CONTEXT *next_ctx = (const CONTEXT *)prev_s11;
        ERR( "Ra from S11: %lx (Before %lx)\n", next_ctx->Ra, context->Ra );
        context->Ra = next_ctx->Ra;
    }

	if (ip == context->Pc)
	{
		ERR("Loop detected %p\n", (void*)ip);
		sleep(5);
	}

    TRACE( "next function pc=%016lx\n", context->Pc );
        {
			int i;
			for (i=0; i < 32; i++)
				TRACE("X%02d = %lx\n", i, context->X[i]);
		}
/*
    TRACE("  x0=%016lx  x1=%016lx  x2=%016lx  x3=%016lx\n",
          context->X0, context->X1, context->X2, context->X3 );
    TRACE("  x4=%016lx  x5=%016lx  x6=%016lx  x7=%016lx\n",
          context->X4, context->X5, context->X6, context->X7 );
    TRACE("  x8=%016lx  x9=%016lx x10=%016lx x11=%016lx\n",
          context->X8, context->X9, context->X10, context->X11 );
    TRACE(" x12=%016lx x13=%016lx x14=%016lx x15=%016lx\n",
          context->X12, context->X13, context->X14, context->X15 );
    TRACE(" x16=%016lx x17=%016lx x18=%016lx x19=%016lx\n",
          context->X16, context->X17, context->X18, context->X19 );
    TRACE(" x20=%016lx x21=%016lx x22=%016lx x23=%016lx\n",
          context->X20, context->X21, context->X22, context->X23 );
    TRACE(" x24=%016lx x25=%016lx x26=%016lx x27=%016lx\n",
          context->X24, context->X25, context->X26, context->X27 );
    TRACE(" x28=%016lx  fp=%016lx  lr=%016lx  sp=%016lx\n",
          context->X28, context->Fp, context->Lr, context->Sp );
*/
    return STATUS_SUCCESS;
}

#ifdef HAVE_LIBUNWIND
static NTSTATUS libunwind_virtual_unwind( ULONG_PTR ip, ULONG_PTR *frame, CONTEXT *context,
                                          PEXCEPTION_ROUTINE *handler, void **handler_data )
{
    unw_context_t unw_context;
    unw_cursor_t cursor;
    unw_proc_info_t info;
    int rc;
    DWORD64 prev_s11 = context->S11;

    memcpy( unw_context.uc_mcontext.__gregs, context->X, sizeof(context->X) );

    rc = unw_init_local( &cursor, &unw_context );

    if (rc != UNW_ESUCCESS)
    {
        WARN( "setup failed: %d\n", rc );
        return STATUS_INVALID_DISPOSITION;
    }
    rc = unw_get_proc_info( &cursor, &info );
    if (UNW_ENOINFO < 0) rc = -rc;  /* LLVM libunwind has negative error codes */
    if (rc != UNW_ESUCCESS && rc != -UNW_ENOINFO)
    {
        WARN( "failed to get info: %d\n", rc );
        return STATUS_INVALID_DISPOSITION;
    }
    if (rc == -UNW_ENOINFO || ip < info.start_ip || ip > info.end_ip)
    {
        TRACE( "no info found for %lx ip %lx-%lx, assuming leaf function\n",
               ip, info.start_ip, info.end_ip );
        *handler = NULL;
        *frame = context->Sp;
        context->Pc = context->Ra;
        context->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
        return STATUS_SUCCESS;
    }

    TRACE( "ip %#lx function %#lx-%#lx personality %#lx lsda %#lx fde %#lx\n",
           ip, (unsigned long)info.start_ip, (unsigned long)info.end_ip, (unsigned long)info.handler,
           (unsigned long)info.lsda, (unsigned long)info.unwind_info );

    rc = unw_step( &cursor );
    if (rc < 0)
    {
        WARN( "failed to unwind: %d %d\n", rc, UNW_ENOINFO );
        return STATUS_INVALID_DISPOSITION;
    }

    *handler      = (void *)info.handler;
    *handler_data = (void *)info.lsda;
    *frame        = context->Sp;

    unw_get_reg( &cursor, UNW_RISCV_PC,  (unw_word_t *)&context->X[0] );
    unw_get_reg( &cursor, UNW_RISCV_X1,  (unw_word_t *)&context->X[1] );
    unw_get_reg( &cursor, UNW_RISCV_X2,  (unw_word_t *)&context->X[2] );
    unw_get_reg( &cursor, UNW_RISCV_X3,  (unw_word_t *)&context->X[3] );
    unw_get_reg( &cursor, UNW_RISCV_X4,  (unw_word_t *)&context->X[4] );
    unw_get_reg( &cursor, UNW_RISCV_X5,  (unw_word_t *)&context->X[5] );
    unw_get_reg( &cursor, UNW_RISCV_X6,  (unw_word_t *)&context->X[6] );
    unw_get_reg( &cursor, UNW_RISCV_X7,  (unw_word_t *)&context->X[7] );
    unw_get_reg( &cursor, UNW_RISCV_X8,  (unw_word_t *)&context->X[8] );
    unw_get_reg( &cursor, UNW_RISCV_X9,  (unw_word_t *)&context->X[9] );
    unw_get_reg( &cursor, UNW_RISCV_X10, (unw_word_t *)&context->X[10] );
    unw_get_reg( &cursor, UNW_RISCV_X11, (unw_word_t *)&context->X[11] );
    unw_get_reg( &cursor, UNW_RISCV_X12, (unw_word_t *)&context->X[12] );
    unw_get_reg( &cursor, UNW_RISCV_X13, (unw_word_t *)&context->X[13] );
    unw_get_reg( &cursor, UNW_RISCV_X14, (unw_word_t *)&context->X[14] );
    unw_get_reg( &cursor, UNW_RISCV_X15, (unw_word_t *)&context->X[15] );
    unw_get_reg( &cursor, UNW_RISCV_X16, (unw_word_t *)&context->X[16] );
    unw_get_reg( &cursor, UNW_RISCV_X17, (unw_word_t *)&context->X[17] );
    unw_get_reg( &cursor, UNW_RISCV_X18, (unw_word_t *)&context->X[18] );
    unw_get_reg( &cursor, UNW_RISCV_X19, (unw_word_t *)&context->X[19] );
    unw_get_reg( &cursor, UNW_RISCV_X20, (unw_word_t *)&context->X[20] );
    unw_get_reg( &cursor, UNW_RISCV_X21, (unw_word_t *)&context->X[21] );
    unw_get_reg( &cursor, UNW_RISCV_X22, (unw_word_t *)&context->X[22] );
    unw_get_reg( &cursor, UNW_RISCV_X23, (unw_word_t *)&context->X[23] );
    unw_get_reg( &cursor, UNW_RISCV_X24, (unw_word_t *)&context->X[24] );
    unw_get_reg( &cursor, UNW_RISCV_X25, (unw_word_t *)&context->X[25] );
    unw_get_reg( &cursor, UNW_RISCV_X26, (unw_word_t *)&context->X[26] );
    unw_get_reg( &cursor, UNW_RISCV_X27, (unw_word_t *)&context->X[27] );
    unw_get_reg( &cursor, UNW_RISCV_X28, (unw_word_t *)&context->X[28] );
    unw_get_reg( &cursor, UNW_RISCV_X29, (unw_word_t *)&context->X[29] );
    unw_get_reg( &cursor, UNW_RISCV_X30, (unw_word_t *)&context->X[30] );
    unw_get_reg( &cursor, UNW_RISCV_X31, (unw_word_t *)&context->X[31] );



    //unw_get_reg( &cursor, UNW_REG_IP,      (unw_word_t *)&context->Pc );
    context->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;

    if (info.start_ip == (unw_word_t)raise_func_trampoline) {
        /* raise_func_trampoline has a pointer to a full CONTEXT stored in s11;
         * restore the original Ra value from there. The function we unwind to
         * might be a leaf function that hasn't backed up its own original Ra
         * value on the stack.
         * We could also just restore the full context here without doing
         * unw_step at all. */
        const CONTEXT *next_ctx = (const CONTEXT *)prev_s11;
        ERR( "Ra from S11: %lx (Before %lx)\n", next_ctx->Ra, context->Ra );
        context->Ra = next_ctx->Ra;
    }
/*
    if (context->X[0] == context->X[1])
    {
        ERR( "failed to unwind: PC==RA==%lx\n", context->X[0] );
        ERR( "raise_func_trampoline == %p\n", raise_func_trampoline);
        ERR( "__wine_syscall_dispatcher == %p\n", __wine_syscall_dispatcher);
        ERR( "__wine_unix_call_dispatcher == %p\n", __wine_unix_call_dispatcher);
        return libunwind_virtual_unwind(context->Pc, frame, context, handler, handler_data);
        return STATUS_INVALID_DISPOSITION;
    }
*/
    TRACE( "next function pc=%016lx%s\n", context->Pc, rc ? "" : " (last frame)" );

        {
			int i;
			for (i=0; i < 32; i++)
				TRACE("X%02d = %lx\n", i, context->X[i]);
		}
    /*TRACE("  x0=%016lx  x1=%016lx  x2=%016lx  x3=%016lx\n",
          context->X0, context->X1, context->X2, context->X3 );
    TRACE("  x4=%016lx  x5=%016lx  x6=%016lx  x7=%016lx\n",
          context->X4, context->X5, context->X6, context->X7 );
    TRACE("  x8=%016lx  x9=%016lx x10=%016lx x11=%016lx\n",
          context->X8, context->X9, context->X10, context->X11 );
    TRACE(" x12=%016lx x13=%016lx x14=%016lx x15=%016lx\n",
          context->X12, context->X13, context->X14, context->X15 );
    TRACE(" x16=%016lx x17=%016lx x18=%016lx x19=%016lx\n",
          context->X16, context->X17, context->X18, context->X19 );
    TRACE(" x20=%016lx x21=%016lx x22=%016lx x23=%016lx\n",
          context->X20, context->X21, context->X22, context->X23 );
    TRACE(" x24=%016lx x25=%016lx x26=%016lx x27=%016lx\n",
          context->X24, context->X25, context->X26, context->X27 );
    TRACE(" x28=%016lx  fp=%016lx  lr=%016lx  sp=%016lx\n",
          context->X28, context->Fp, context->Lr, context->Sp );*/
    return STATUS_SUCCESS;
}
#endif

/***********************************************************************
 *           unwind_builtin_dll
 *
 * Equivalent of RtlVirtualUnwind for builtin modules.
 */
NTSTATUS unwind_builtin_dll( void *args )
{
    struct unwind_builtin_dll_params *params = args;
    DISPATCHER_CONTEXT *dispatch = params->dispatch;
    CONTEXT *context = params->context;
#if 1
    struct dwarf_eh_bases bases;
    const struct dwarf_fde *fde = _Unwind_Find_FDE( (void *)(context->Pc - 1), &bases );

    if (fde)
        return dwarf_virtual_unwind( context->Pc, &dispatch->EstablisherFrame, context, fde,
                                     &bases, &dispatch->LanguageHandler, &dispatch->HandlerData );
#endif
#ifdef HAVE_LIBUNWIND
    return libunwind_virtual_unwind( context->Pc, &dispatch->EstablisherFrame, context,
                                     &dispatch->LanguageHandler, &dispatch->HandlerData );
#else
    ERR("libunwind not available, unable to unwind\n");
    return STATUS_INVALID_DISPOSITION;
#endif
}


/***********************************************************************
 *           syscall_frame_fixup_for_fastpath
 *
 * Fixes up the given syscall frame such that the syscall dispatcher
 * can return via the fast path if CONTEXT_INTEGER is set in
 * restore_flags.
 *
 * Clobbers the frame's X16 and X17 register values.
 */
static void syscall_frame_fixup_for_fastpath( struct syscall_frame *frame )
{
    /*
    frame->x[16] = frame->pc;
    frame->x[17] = frame->sp;
    */
    frame->x[30] = frame->x[0];
    frame->x[31] = frame->x[2];
}

/***********************************************************************
 *           save_fpu
 *
 * Set the FPU context from a sigcontext.
 */
static void save_fpu( CONTEXT *context, const ucontext_t *sigcontext )
{
    FIXME("NYI\n");
}


/***********************************************************************
 *           restore_fpu
 *
 * Restore the FPU context to a sigcontext.
 */
static void restore_fpu( const CONTEXT *context, ucontext_t *sigcontext )
{
    FIXME("NYI\n");
}


/***********************************************************************
 *           save_context
 *
 * Set the register values from a sigcontext.
 */
static void save_context( CONTEXT *context, const ucontext_t *sigcontext )
{
    DWORD i;

    context->ContextFlags = CONTEXT_FULL;
    for (i = 0; i < 32; i++) context->X[i] = REGn_sig( i, sigcontext );
    save_fpu( context, sigcontext );
}


/***********************************************************************
 *           restore_context
 *
 * Build a sigcontext from the register values.
 */
static void restore_context( const CONTEXT *context, ucontext_t *sigcontext )
{
    DWORD i;

    for (i = 0; i < 32; i++) REGn_sig( i, sigcontext ) = context->X[i];
    restore_fpu( context, sigcontext );
}


/***********************************************************************
 *           signal_set_full_context
 */
NTSTATUS signal_set_full_context( CONTEXT *context )
{
    NTSTATUS status = NtSetContextThread( GetCurrentThread(), context );

    if (!status && (context->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER)
        riscv64_thread_data()->syscall_frame->restore_flags |= CONTEXT_INTEGER;
    return status;
}


/***********************************************************************
 *              get_native_context
 */
void *get_native_context( CONTEXT *context )
{
    return context;
}


/***********************************************************************
 *              get_wow_context
 */
void *get_wow_context( CONTEXT *context )
{
    return get_cpu_area( main_image_info.Machine );
}


/***********************************************************************
 *              NtSetContextThread  (NTDLL.@)
 *              ZwSetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetContextThread( HANDLE handle, const CONTEXT *context )
{
    struct syscall_frame *frame = riscv64_thread_data()->syscall_frame;
    NTSTATUS ret = STATUS_SUCCESS;
    BOOL self = (handle == GetCurrentThread());
    DWORD flags = context->ContextFlags & ~CONTEXT_RISCV64;

    if (self && (flags & CONTEXT_DEBUG_REGISTERS)) self = FALSE;

    if (!self)
    {
ERR("not self\n");
        ret = set_thread_context( handle, context, &self, IMAGE_FILE_MACHINE_RISCV64 );
        if (ret || !self) return ret;
    }

    if (flags & CONTEXT_INTEGER)
    {
        frame->x[3]  = context->X[3];
        frame->x[4]  = context->X[4];
        frame->x[5]  = context->X[5];
        frame->x[6]  = context->X[6];
        frame->x[7]  = context->X[7];
        frame->x[9]  = context->X[9];
        frame->x[10]  = context->X[10];
        frame->x[11]  = context->X[11];
        frame->x[12]  = context->X[12];
        frame->x[13]  = context->X[13];
        frame->x[14]  = context->X[14];
        frame->x[15]  = context->X[15];
        frame->x[16]  = context->X[16];
        frame->x[17]  = context->X[17];
        frame->x[18]  = context->X[18];
        frame->x[19]  = context->X[19];
        frame->x[20]  = context->X[20];
        frame->x[21]  = context->X[21];
        frame->x[22]  = context->X[22];
        frame->x[23]  = context->X[23];
        frame->x[24]  = context->X[24];
        frame->x[25]  = context->X[25];
        frame->x[26]  = context->X[26];
        frame->x[27]  = context->X[27];
        frame->x[28]  = context->X[28];
        frame->x[29]  = context->X[29];
        frame->x[30]  = context->X[30];
        frame->x[31]  = context->X[31];
        // FIXME: Skip TEB (X4?)
    }
    if (flags & CONTEXT_CONTROL)
    {
        frame->x[8]  = context->Fp;
        frame->x[1]  = context->Ra;
        frame->x[2]  = context->Sp;
        frame->x[0]  = context->Pc;
        if (0)
        {
			int i;
			for (i=0; i < 32; i++)
				ERR("X%02d = %16lx\n", i, frame->x[i]);
		}
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        // FIXME: frame->fpcr = context->Fpcr;
        // FIXME: frame->fpsr = context->Fpsr;
        //memcpy( frame->f, context->F, sizeof(frame->f) );
    }
    /* FIXME: Make the same for X4?
    if (flags & CONTEXT_ARM64_X18)
    {
        frame->x[18] = context->X[18];
    }
    */
    if (flags & CONTEXT_DEBUG_REGISTERS) FIXME( "debug registers not supported\n" );
    frame->restore_flags |= flags & ~CONTEXT_INTEGER;
    return STATUS_SUCCESS;
}


/***********************************************************************
 *              NtGetContextThread  (NTDLL.@)
 *              ZwGetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtGetContextThread( HANDLE handle, CONTEXT *context )
{
    struct syscall_frame *frame = riscv64_thread_data()->syscall_frame;
    DWORD needed_flags = context->ContextFlags & ~CONTEXT_RISCV64;
    BOOL self = (handle == GetCurrentThread());

    if (!self)
    {
        NTSTATUS ret = get_thread_context( handle, context, &self, IMAGE_FILE_MACHINE_RISCV64 );
        if (ret || !self) return ret;
    }

    if (needed_flags & CONTEXT_INTEGER)
    {
        context->X[3] = frame->x[3];
        context->X[4] = frame->x[4];
        context->X[5] = frame->x[5];
        context->X[6] = frame->x[6];
        context->X[7] = frame->x[7];
        context->X[9] = frame->x[9];
        context->X[10] = frame->x[10];
        context->X[11] = frame->x[11];
        context->X[12] = frame->x[12];
        context->X[13] = frame->x[13];
        context->X[14] = frame->x[14];
        context->X[15] = frame->x[15];
        context->X[16] = frame->x[16];
        context->X[17] = frame->x[17];
        context->X[18] = frame->x[18];
        context->X[19] = frame->x[19];
        context->X[20] = frame->x[20];
        context->X[21] = frame->x[21];
        context->X[22] = frame->x[22];
        context->X[23] = frame->x[23];
        context->X[24] = frame->x[24];
        context->X[25] = frame->x[25];
        context->X[26] = frame->x[26];
        context->X[27] = frame->x[27];
        context->X[28] = frame->x[28];
        context->X[29] = frame->x[29];
        context->X[30] = frame->x[30];
        context->X[31] = frame->x[31];
        context->ContextFlags |= CONTEXT_INTEGER;
    }
    if (needed_flags & CONTEXT_CONTROL)
    {
        context->Fp   = frame->x[8];
        context->Ra   = frame->x[1];
        context->Sp   = frame->x[2];
        context->Pc   = frame->x[0];
        context->ContextFlags |= CONTEXT_CONTROL;
    }
    if (needed_flags & CONTEXT_FLOATING_POINT)
    {
        // FIXME: context->Fpcr = frame->fpcr;
        // FIXME: context->Fpsr = frame->fpsr;
        //memcpy( context->F, frame->f, sizeof(context->F) );
        context->ContextFlags |= CONTEXT_FLOATING_POINT;
    }
    if (needed_flags & CONTEXT_DEBUG_REGISTERS) FIXME( "debug registers not supported\n" );
    return STATUS_SUCCESS;
}


/***********************************************************************
 *              set_thread_wow64_context
 */
NTSTATUS set_thread_wow64_context( HANDLE handle, const void *ctx, ULONG size )
{
    FIXME("NYI\n");
#if 0
    BOOL self = (handle == GetCurrentThread());
    USHORT machine;
    void *frame;

    switch (size)
    {
    case sizeof(I386_CONTEXT): machine = IMAGE_FILE_MACHINE_I386; break;
    case sizeof(ARM_CONTEXT): machine = IMAGE_FILE_MACHINE_ARMNT; break;
    default: return STATUS_INFO_LENGTH_MISMATCH;
    }

    if (!self)
    {
        NTSTATUS ret = set_thread_context( handle, ctx, &self, machine );
        if (ret || !self) return ret;
    }

    if (!(frame = get_cpu_area( machine ))) return STATUS_INVALID_PARAMETER;

    switch (machine)
    {
    case IMAGE_FILE_MACHINE_I386:
    {
        I386_CONTEXT *wow_frame = frame;
        const I386_CONTEXT *context = ctx;
        DWORD flags = context->ContextFlags & ~CONTEXT_i386;

        if (flags & CONTEXT_I386_INTEGER)
        {
            wow_frame->Eax = context->Eax;
            wow_frame->Ebx = context->Ebx;
            wow_frame->Ecx = context->Ecx;
            wow_frame->Edx = context->Edx;
            wow_frame->Esi = context->Esi;
            wow_frame->Edi = context->Edi;
        }
        if (flags & CONTEXT_I386_CONTROL)
        {
            WOW64_CPURESERVED *cpu = NtCurrentTeb()->TlsSlots[WOW64_TLS_CPURESERVED];

            wow_frame->Esp    = context->Esp;
            wow_frame->Ebp    = context->Ebp;
            wow_frame->Eip    = context->Eip;
            wow_frame->EFlags = context->EFlags;
            wow_frame->SegCs  = LOWORD(context->SegCs);
            wow_frame->SegSs  = LOWORD(context->SegSs);
            cpu->Flags |= WOW64_CPURESERVED_FLAG_RESET_STATE;
        }
        if (flags & CONTEXT_I386_SEGMENTS)
        {
            wow_frame->SegDs = LOWORD(context->SegDs);
            wow_frame->SegEs = LOWORD(context->SegEs);
            wow_frame->SegFs = LOWORD(context->SegFs);
            wow_frame->SegGs = LOWORD(context->SegGs);
        }
        if (flags & CONTEXT_I386_DEBUG_REGISTERS)
        {
            wow_frame->Dr0 = context->Dr0;
            wow_frame->Dr1 = context->Dr1;
            wow_frame->Dr2 = context->Dr2;
            wow_frame->Dr3 = context->Dr3;
            wow_frame->Dr6 = context->Dr6;
            wow_frame->Dr7 = context->Dr7;
        }
        if (flags & CONTEXT_I386_EXTENDED_REGISTERS)
        {
            memcpy( &wow_frame->ExtendedRegisters, context->ExtendedRegisters, sizeof(context->ExtendedRegisters) );
        }
        if (flags & CONTEXT_I386_FLOATING_POINT)
        {
            memcpy( &wow_frame->FloatSave, &context->FloatSave, sizeof(context->FloatSave) );
        }
        /* FIXME: CONTEXT_I386_XSTATE */
        break;
    }

    case IMAGE_FILE_MACHINE_ARMNT:
    {
        ARM_CONTEXT *wow_frame = frame;
        const ARM_CONTEXT *context = ctx;
        DWORD flags = context->ContextFlags & ~CONTEXT_ARM;

        if (flags & CONTEXT_INTEGER)
        {
            wow_frame->R0  = context->R0;
            wow_frame->R1  = context->R1;
            wow_frame->R2  = context->R2;
            wow_frame->R3  = context->R3;
            wow_frame->R4  = context->R4;
            wow_frame->R5  = context->R5;
            wow_frame->R6  = context->R6;
            wow_frame->R7  = context->R7;
            wow_frame->R8  = context->R8;
            wow_frame->R9  = context->R9;
            wow_frame->R10 = context->R10;
            wow_frame->R11 = context->R11;
            wow_frame->R12 = context->R12;
        }
        if (flags & CONTEXT_CONTROL)
        {
            wow_frame->Sp = context->Sp;
            wow_frame->Lr = context->Lr;
            wow_frame->Pc = context->Pc & ~1;
            wow_frame->Cpsr = context->Cpsr;
            if (context->Cpsr & 0x20) wow_frame->Pc |= 1; /* thumb */
        }
        if (flags & CONTEXT_FLOATING_POINT)
        {
            wow_frame->Fpscr = context->Fpscr;
            memcpy( wow_frame->D, context->D, sizeof(context->D) );
        }
        break;
    }

    }
#endif
    return STATUS_SUCCESS;
}


/***********************************************************************
 *              get_thread_wow64_context
 */
NTSTATUS get_thread_wow64_context( HANDLE handle, void *ctx, ULONG size )
{
    FIXME("NYI\n");
#if 0
    BOOL self = (handle == GetCurrentThread());
    USHORT machine;
    void *frame;

    switch (size)
    {
    case sizeof(I386_CONTEXT): machine = IMAGE_FILE_MACHINE_I386; break;
    case sizeof(ARM_CONTEXT): machine = IMAGE_FILE_MACHINE_ARMNT; break;
    default: return STATUS_INFO_LENGTH_MISMATCH;
    }

    if (!self)
    {
        NTSTATUS ret = get_thread_context( handle, ctx, &self, machine );
        if (ret || !self) return ret;
    }

    if (!(frame = get_cpu_area( machine ))) return STATUS_INVALID_PARAMETER;

    switch (machine)
    {
    case IMAGE_FILE_MACHINE_I386:
    {
        I386_CONTEXT *wow_frame = frame, *context = ctx;
        DWORD needed_flags = context->ContextFlags & ~CONTEXT_i386;

        if (needed_flags & CONTEXT_I386_INTEGER)
        {
            context->Eax = wow_frame->Eax;
            context->Ebx = wow_frame->Ebx;
            context->Ecx = wow_frame->Ecx;
            context->Edx = wow_frame->Edx;
            context->Esi = wow_frame->Esi;
            context->Edi = wow_frame->Edi;
            context->ContextFlags |= CONTEXT_I386_INTEGER;
        }
        if (needed_flags & CONTEXT_I386_CONTROL)
        {
            context->Esp    = wow_frame->Esp;
            context->Ebp    = wow_frame->Ebp;
            context->Eip    = wow_frame->Eip;
            context->EFlags = wow_frame->EFlags;
            context->SegCs  = LOWORD(wow_frame->SegCs);
            context->SegSs  = LOWORD(wow_frame->SegSs);
            context->ContextFlags |= CONTEXT_I386_CONTROL;
        }
        if (needed_flags & CONTEXT_I386_SEGMENTS)
        {
            context->SegDs = LOWORD(wow_frame->SegDs);
            context->SegEs = LOWORD(wow_frame->SegEs);
            context->SegFs = LOWORD(wow_frame->SegFs);
            context->SegGs = LOWORD(wow_frame->SegGs);
            context->ContextFlags |= CONTEXT_I386_SEGMENTS;
        }
        if (needed_flags & CONTEXT_I386_EXTENDED_REGISTERS)
        {
            memcpy( context->ExtendedRegisters, &wow_frame->ExtendedRegisters, sizeof(context->ExtendedRegisters) );
            context->ContextFlags |= CONTEXT_I386_EXTENDED_REGISTERS;
        }
        if (needed_flags & CONTEXT_I386_FLOATING_POINT)
        {
            memcpy( &context->FloatSave, &wow_frame->FloatSave, sizeof(context->FloatSave) );
            context->ContextFlags |= CONTEXT_I386_FLOATING_POINT;
        }
        if (needed_flags & CONTEXT_I386_DEBUG_REGISTERS)
        {
            context->Dr0 = wow_frame->Dr0;
            context->Dr1 = wow_frame->Dr1;
            context->Dr2 = wow_frame->Dr2;
            context->Dr3 = wow_frame->Dr3;
            context->Dr6 = wow_frame->Dr6;
            context->Dr7 = wow_frame->Dr7;
        }
        /* FIXME: CONTEXT_I386_XSTATE */
        break;
    }

    case IMAGE_FILE_MACHINE_ARMNT:
    {
        ARM_CONTEXT *wow_frame = frame, *context = ctx;
        DWORD needed_flags = context->ContextFlags & ~CONTEXT_ARM;

        if (needed_flags & CONTEXT_INTEGER)
        {
            context->R0  = wow_frame->R0;
            context->R1  = wow_frame->R1;
            context->R2  = wow_frame->R2;
            context->R3  = wow_frame->R3;
            context->R4  = wow_frame->R4;
            context->R5  = wow_frame->R5;
            context->R6  = wow_frame->R6;
            context->R7  = wow_frame->R7;
            context->R8  = wow_frame->R8;
            context->R9  = wow_frame->R9;
            context->R10 = wow_frame->R10;
            context->R11 = wow_frame->R11;
            context->R12 = wow_frame->R12;
            context->ContextFlags |= CONTEXT_INTEGER;
        }
        if (needed_flags & CONTEXT_CONTROL)
        {
            context->Sp   = wow_frame->Sp;
            context->Lr   = wow_frame->Lr;
            context->Pc   = wow_frame->Pc;
            context->Cpsr = wow_frame->Cpsr;
            context->ContextFlags |= CONTEXT_CONTROL;
        }
        if (needed_flags & CONTEXT_FLOATING_POINT)
        {
            context->Fpscr = wow_frame->Fpscr;
            memcpy( context->D, wow_frame->D, sizeof(wow_frame->D) );
            context->ContextFlags |= CONTEXT_FLOATING_POINT;
        }
        break;
    }

    }
#endif
    return STATUS_SUCCESS;
}

#undef __ASM_CFI_REG_IS_AT2
#define __ASM_CFI_REG_IS_AT2(a, b, c, d)
#undef __ASM_CFI_CFA_IS_AT2
#define __ASM_CFI_CFA_IS_AT2(a, b, c)
/* Note, unwind_builtin_dll above has hardcoded assumptions on how this
 * function stores a CONTEXT pointer in s11; if modified, modify that one in
 * sync as well. */
__ASM_GLOBAL_FUNC( raise_func_trampoline,
                   "addi sp, sp, -0x20\n\t"
                   "sd ra, 0x18(sp)\n\t"
                   "sd fp, 0x10(sp)\n\t"
                   "sd s11, 0x00(sp)\n\t"
                   "mv fp, sp\n\t"
                   "mv s11, a1\n\t"
                   "jalr a2\n\t"
                   "ebreak")

/***********************************************************************
 *           setup_exception
 *
 * Modify the signal context to call the exception raise function.
 */
static void setup_exception( ucontext_t *sigcontext, EXCEPTION_RECORD *rec )
{
    struct
    {
        CONTEXT           context;
        EXCEPTION_RECORD  rec;
        void             *redzone[3];
    } *stack;

    void *stack_ptr = (void *)(SP_sig(sigcontext) & ~15);
    CONTEXT context;
    NTSTATUS status;

    rec->ExceptionAddress = (void *)PC_sig(sigcontext);
    save_context( &context, sigcontext );

    status = send_debug_event( rec, &context, TRUE );
    if (status == DBG_CONTINUE || status == DBG_EXCEPTION_HANDLED)
    {
        restore_context( &context, sigcontext );
        return;
    }

    /* fix up instruction pointer in context for EXCEPTION_BREAKPOINT */
    if (rec->ExceptionCode == EXCEPTION_BREAKPOINT) context.Pc -= 4;

    stack = virtual_setup_exception( stack_ptr, (sizeof(*stack) + 15) & ~15, rec );
    stack->rec = *rec;
    stack->context = context;

    SP_sig(sigcontext) = (ULONG_PTR)stack;
    RA_sig(sigcontext) = PC_sig(sigcontext);
    PC_sig(sigcontext) = (ULONG_PTR)raise_func_trampoline;
    REGn_sig(10, sigcontext) = (ULONG_PTR)&stack->rec;  /* first arg for KiUserExceptionDispatcher */
    REGn_sig(11, sigcontext) = (ULONG_PTR)&stack->context; /* second arg for KiUserExceptionDispatcher */
    REGn_sig(12, sigcontext) = (ULONG_PTR)pKiUserExceptionDispatcher; /* dispatcher arg for raise_func_trampoline */
    //REGn_sig(4, sigcontext)  = (ULONG_PTR)NtCurrentTeb();
}


/***********************************************************************
 *           call_user_apc_dispatcher
 */
NTSTATUS call_user_apc_dispatcher( CONTEXT *context, ULONG_PTR arg1, ULONG_PTR arg2, ULONG_PTR arg3,
                                   PNTAPCFUNC func, NTSTATUS status )
{
    struct syscall_frame *frame = riscv64_thread_data()->syscall_frame;
    ULONG64 sp = context ? context->Sp : frame->x[2];
    struct apc_stack_layout { CONTEXT context; } *stack;

    sp &= ~15;
    stack = (struct apc_stack_layout *)sp - 1;
    if (context)
    {
        memmove( &stack->context, context, sizeof(stack->context) );
        NtSetContextThread( GetCurrentThread(), &stack->context );
    }
    else
    {
        stack->context.ContextFlags = CONTEXT_FULL;
        NtGetContextThread( GetCurrentThread(), &stack->context );
        stack->context.A0 = status;
    }
    frame->x[2]  = (ULONG64)stack;
    frame->x[0]  = (ULONG64)pKiUserApcDispatcher;
    frame->x[10] = (ULONG64)&stack->context;
    frame->x[11] = arg1;
    frame->x[12] = arg2;
    frame->x[13] = arg3;
    frame->x[14] = (ULONG64)func;
    frame->restore_flags |= CONTEXT_CONTROL | CONTEXT_INTEGER;
    syscall_frame_fixup_for_fastpath( frame );
    return status;
}


/***********************************************************************
 *           call_raise_user_exception_dispatcher
 */
void call_raise_user_exception_dispatcher(void)
{
    riscv64_thread_data()->syscall_frame->x[0] = (UINT64)pKiRaiseUserExceptionDispatcher;
}


/***********************************************************************
 *           call_user_exception_dispatcher
 */
NTSTATUS call_user_exception_dispatcher( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    struct syscall_frame *frame = riscv64_thread_data()->syscall_frame;
    ULONG64 fp = frame->x[8];
    ULONG64 ra = frame->x[1];
    ULONG64 sp = frame->x[2];
    NTSTATUS status = NtSetContextThread( GetCurrentThread(), context );

    if (status) return status;
    frame->x[10] = (ULONG64)rec;
    frame->x[11] = (ULONG64)context;
    frame->x[0]  = (ULONG64)pKiUserExceptionDispatcher;
    frame->x[8]  = fp;
    frame->x[1]  = ra;
    frame->x[2]  = sp;
    frame->restore_flags |= CONTEXT_INTEGER | CONTEXT_CONTROL;
    syscall_frame_fixup_for_fastpath( frame );
    return status;
}


/***********************************************************************
 *           call_user_mode_callback
 */
extern NTSTATUS call_user_mode_callback( ULONG id, void *args, ULONG len, void **ret_ptr,
                                         ULONG *ret_len, void *func, TEB *teb );
__ASM_GLOBAL_FUNC( call_user_mode_callback,
                   "addi sp, sp, -0xc0\n\t"
                   "sd fp, 0xb0(sp)\n\t"
                   "sd ra, 0xb8(sp)\n\t"
                   "mv fp, sp\n\t"
                   "sd s0, 0x00(fp)\n\t"
                   "sd s1, 0x08(fp)\n\t"
                   "sd s2, 0x10(fp)\n\t"
                   "sd s3, 0x18(fp)\n\t"
                   "sd s4, 0x20(fp)\n\t"
                   "sd s5, 0x28(fp)\n\t"
                   "sd s6, 0x30(fp)\n\t"
                   "sd s7, 0x38(fp)\n\t"
                   "sd s8, 0x40(fp)\n\t"
                   "sd s9, 0x48(fp)\n\t"
                   "sd s10, 0x50(fp)\n\t"
                   "sd s11, 0x58(fp)\n\t"
                   "sd a3, 0x90(fp)\n\t" /* ret_ptr */
                   "sd a4, 0x98(fp)\n\t" /* ret_len */
                   //"mv tp, a6\n\t"              /* teb */
                   "ld a4, 0(a6)\n\t"            /* teb->Tib.ExceptionList */
                   "sd a4, 0xa8(fp)\n\t"

                   "ld a7, 0x2f8(a6)\n\t"    /* riscv64_thread_data()->syscall_frame */
                   "addi a3, sp, -0x330\n\t"       /* sizeof(struct syscall_frame) */   // TODO: 330
                   "sd a3, 0x2f8(a6)\n\t"    /* riscv64_thread_data()->syscall_frame */
                   "ld t0, 0x110(a7)\n\t"     /* prev_frame->syscall_table */
                   "mv sp, a1\n\t"               /* stack */
                   "sd a7, 0x108(a3)\n\t" /* frame->prev_frame */
                   "sd t0, 0x110(a3)\n\t" /* frame->syscall_table */
                   "jr a5" )

/***********************************************************************
 *           user_mode_callback_return
 */
extern void DECLSPEC_NORETURN user_mode_callback_return( void *ret_ptr, ULONG ret_len,
                                                         NTSTATUS status, TEB *teb );
__ASM_GLOBAL_FUNC( user_mode_callback_return,
                   "ld a4, 0x2f8(a3)\n\t"     /* riscv64_thread_data()->syscall_frame */
                   "ld a5, 0x108(a4)\n\t"     /* prev_frame */
                   "sd a5, 0x2f8(a3)\n\t"     /* riscv64_thread_data()->syscall_frame */
                   "addi fp, a4, 0x330\n\t"      /* sizeof(struct syscall_frame) */ // TODO: 330
                   "ld a6, 0xb8(fp)\n\t"
                   "sd a6, 0(a3)\n\t"     /* teb->Tib.ExceptionList */
                   "ld s0, 0x00(fp)\n\t"  // FIXME s0==fp
                   "ld s1, 0x08(fp)\n\t"
                   "ld s2, 0x10(fp)\n\t"
                   "ld s3, 0x18(fp)\n\t"
                   "ld s4, 0x20(fp)\n\t"
                   "ld s5, 0x28(fp)\n\t"
                   "ld s6, 0x30(fp)\n\t"
                   "ld s7, 0x38(fp)\n\t"
                   "ld s8, 0x40(fp)\n\t"
                   "ld s9, 0x48(fp)\n\t"
                   "ld s10, 0x50(fp)\n\t"
                   "ld s11, 0x58(fp)\n\t"
                   "ld a5, 0x90(fp)\n\t" /* ret_ptr */
                   "ld a6, 0x98(fp)\n\t" /* ret_len */
                   "sd a0, 0(a5)\n\t"    /* ret_ptr */
                   "sw a1, 0(a6)\n\t"    /* ret_len */
                   "mv a0, a2\n\t"       /* status */
                   "mv sp, fp\n\t"
                   "ld ra, 0xb8(sp)\n\t"
                   "ld fp, 0xb0(sp)\n\t"
                   "addi sp, sp, 0xc0\n\t"
                   "ret" )


/***********************************************************************
 *           KeUserModeCallback
 */
NTSTATUS KeUserModeCallback( ULONG id, const void *args, ULONG len, void **ret_ptr, ULONG *ret_len )
{
    struct syscall_frame *frame = riscv64_thread_data()->syscall_frame;
    void *args_data = (void *)((frame->x[2] - len) & ~15);

    if ((char *)ntdll_get_thread_data()->kernel_stack + min_kernel_stack > (char *)&frame)
        return STATUS_STACK_OVERFLOW;

    memcpy( args_data, args, len );
    return call_user_mode_callback( id, args_data, len, ret_ptr, ret_len,
                                    pKiUserCallbackDispatcher, NtCurrentTeb() );
}


/***********************************************************************
 *           NtCallbackReturn  (NTDLL.@)
 */
NTSTATUS WINAPI NtCallbackReturn( void *ret_ptr, ULONG ret_len, NTSTATUS status )
{
    if (!riscv64_thread_data()->syscall_frame->prev_frame) return STATUS_NO_CALLBACK_ACTIVE;
    user_mode_callback_return( ret_ptr, ret_len, status, NtCurrentTeb() );
}


/***********************************************************************
 *           handle_syscall_fault
 *
 * Handle a page fault happening during a system call.
 */
static BOOL handle_syscall_fault( ucontext_t *context, EXCEPTION_RECORD *rec )
{
    struct syscall_frame *frame = riscv64_thread_data()->syscall_frame;
    DWORD i;

    if (!is_inside_syscall( context )) return FALSE;
    FIXME("NYI\n");
return FALSE;
}


/**********************************************************************
 *		segv_handler
 *
 * Handler for SIGSEGV.
 */
static void segv_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { 0 };
    ucontext_t *context = sigcontext;
ERR("segv...\n");
abort_thread(0);
    rec.NumberParameters = 2;
    rec.ExceptionInformation[0] = 0; // FIXME: (get_fault_esr( context ) & 0x40) != 0;
    rec.ExceptionInformation[1] = (ULONG_PTR)siginfo->si_addr;
    rec.ExceptionCode = virtual_handle_fault( siginfo->si_addr, rec.ExceptionInformation[0],
                                              (void *)SP_sig(context) );
    if (!rec.ExceptionCode) return;
    if (handle_syscall_fault( context, &rec )) return;
    setup_exception( context, &rec );
}


/**********************************************************************
 *		ill_handler
 *
 * Handler for SIGILL.
 */
static void ill_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { EXCEPTION_ILLEGAL_INSTRUCTION };
ERR("ill...\n");
    setup_exception( sigcontext, &rec );
}


/**********************************************************************
 *		bus_handler
 *
 * Handler for SIGBUS.
 */
static void bus_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { EXCEPTION_DATATYPE_MISALIGNMENT };
ERR("bus...\n");
    setup_exception( sigcontext, &rec );
}


/**********************************************************************
 *		trap_handler
 *
 * Handler for SIGTRAP.
 */
static void trap_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { 0 };
    ucontext_t *context = sigcontext;
ERR("trap...\n");
    switch (siginfo->si_code)
    {
    case TRAP_TRACE:
        rec.ExceptionCode = EXCEPTION_SINGLE_STEP;
        break;
    case TRAP_BRKPT:
    default:
        FIXME("NYI\n");
#if 0
        /* debug exceptions do not update ESR on Linux, so we fetch the instruction directly. */
        if (!(PSTATE_sig( context ) & 0x10) && /* AArch64 (not WoW) */
            !(PC_sig( context ) & 3) &&
            *(ULONG *)PC_sig( context ) == 0xd43e0060UL) /* brk #0xf003 -> __fastfail */
        {
            CONTEXT ctx;
            save_context( &ctx, sigcontext );
            rec.ExceptionCode = STATUS_STACK_BUFFER_OVERRUN;
            rec.ExceptionAddress = (void *)ctx.Pc;
            rec.ExceptionFlags = EH_NONCONTINUABLE;
            rec.NumberParameters = 1;
            rec.ExceptionInformation[0] = ctx.X[0];
            NtRaiseException( &rec, &ctx, FALSE );
            return;
        }
        PC_sig( context ) += 4;  /* skip the brk instruction */
        rec.ExceptionCode = EXCEPTION_BREAKPOINT;
        rec.NumberParameters = 1;
#endif
        break;
    }
    setup_exception( sigcontext, &rec );
}


/**********************************************************************
 *		fpe_handler
 *
 * Handler for SIGFPE.
 */
static void fpe_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { 0 };
ERR("fpe...\n");
    switch (siginfo->si_code & 0xffff )
    {
#ifdef FPE_FLTSUB
    case FPE_FLTSUB:
        rec.ExceptionCode = EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
        break;
#endif
#ifdef FPE_INTDIV
    case FPE_INTDIV:
        rec.ExceptionCode = EXCEPTION_INT_DIVIDE_BY_ZERO;
        break;
#endif
#ifdef FPE_INTOVF
    case FPE_INTOVF:
        rec.ExceptionCode = EXCEPTION_INT_OVERFLOW;
        break;
#endif
#ifdef FPE_FLTDIV
    case FPE_FLTDIV:
        rec.ExceptionCode = EXCEPTION_FLT_DIVIDE_BY_ZERO;
        break;
#endif
#ifdef FPE_FLTOVF
    case FPE_FLTOVF:
        rec.ExceptionCode = EXCEPTION_FLT_OVERFLOW;
        break;
#endif
#ifdef FPE_FLTUND
    case FPE_FLTUND:
        rec.ExceptionCode = EXCEPTION_FLT_UNDERFLOW;
        break;
#endif
#ifdef FPE_FLTRES
    case FPE_FLTRES:
        rec.ExceptionCode = EXCEPTION_FLT_INEXACT_RESULT;
        break;
#endif
#ifdef FPE_FLTINV
    case FPE_FLTINV:
#endif
    default:
        rec.ExceptionCode = EXCEPTION_FLT_INVALID_OPERATION;
        break;
    }
    setup_exception( sigcontext, &rec );
}


/**********************************************************************
 *		int_handler
 *
 * Handler for SIGINT.
 */
static void int_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    HANDLE handle;

    if (!p__wine_ctrl_routine) return;
    if (!NtCreateThreadEx( &handle, THREAD_ALL_ACCESS, NULL, NtCurrentProcess(),
                           p__wine_ctrl_routine, 0 /* CTRL_C_EVENT */, 0, 0, 0, 0, NULL ))
        NtClose( handle );
}


/**********************************************************************
 *		abrt_handler
 *
 * Handler for SIGABRT.
 */
static void abrt_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { EXCEPTION_WINE_ASSERTION, EH_NONCONTINUABLE };
ERR("abrt...\n");
    setup_exception( sigcontext, &rec );
}


/**********************************************************************
 *		quit_handler
 *
 * Handler for SIGQUIT.
 */
static void quit_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    abort_thread(0);
}


/**********************************************************************
 *		usr1_handler
 *
 * Handler for SIGUSR1, used to signal a thread that it got suspended.
 */
static void usr1_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    CONTEXT context;
ERR("here\n");
    if (is_inside_syscall( sigcontext ))
    {
        context.ContextFlags = CONTEXT_FULL;
        NtGetContextThread( GetCurrentThread(), &context );
        wait_suspend( &context );
        ERR("here A %lx\n", context.Pc);
        NtSetContextThread( GetCurrentThread(), &context );
    }
    else
    {
        save_context( &context, sigcontext );
        wait_suspend( &context );
        ERR("here B %lx\n", context.Pc);
        restore_context( &context, sigcontext );
    }
}


/**********************************************************************
 *		usr2_handler
 *
 * Handler for SIGUSR2, used to set a thread context.
 */
static void usr2_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    struct syscall_frame *frame = riscv64_thread_data()->syscall_frame;
    ucontext_t *context = sigcontext;
    //struct fpsimd_context *fp;
    DWORD i;

    if (!is_inside_syscall( sigcontext )) return;

    /*FP_sig(context)     = frame->x[8];
    RA_sig(context)     = frame->x[1];
    SP_sig(context)     = frame->x[2];
    PC_sig(context)     = frame->x[0];*/
    // FIXME: PSTATE_sig(context) = frame->cpsr;
    for (i = 0; i <= 31; i++) TRACE("Before X%u = %lx\n", i, REGn_sig( i, context ));
    REGn_sig( 0, context ) = frame->x[0];
    REGn_sig( 1, context ) = frame->x[1];
    REGn_sig( 2, context ) = frame->x[2];
    if (frame->x[3]) REGn_sig( 3, context ) = frame->x[3];
    if (frame->x[4]) REGn_sig( 4, context ) = frame->x[4];
    for (i = 5; i <= 31; i++) REGn_sig( i, context ) = frame->x[i];
    for (i = 0; i <= 31; i++) TRACE("After X%u = %lx\n",  i, REGn_sig( i, context ));

// FIXME: 
#if 0
    fp = get_fpsimd_context( sigcontext );
    if (fp)
    {
        fp->fpcr = frame->fpcr;
        fp->fpsr = frame->fpsr;
        memcpy( fp->vregs, frame->v, sizeof(fp->vregs) );
    }
#endif
}


/**********************************************************************
 *           get_thread_ldt_entry
 */
NTSTATUS get_thread_ldt_entry( HANDLE handle, void *data, ULONG len, ULONG *ret_len )
{
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *           NtSetLdtEntries   (NTDLL.@)
 *           ZwSetLdtEntries   (NTDLL.@)
 */
NTSTATUS WINAPI NtSetLdtEntries( ULONG sel1, LDT_ENTRY entry1, ULONG sel2, LDT_ENTRY entry2 )
{
    return STATUS_NOT_IMPLEMENTED;
}


/**********************************************************************
 *             signal_init_threading
 */
void signal_init_threading(void)
{
}


/**********************************************************************
 *             signal_alloc_thread
 */
NTSTATUS signal_alloc_thread( TEB *teb )
{
    return STATUS_SUCCESS;
}


/**********************************************************************
 *             signal_free_thread
 */
void signal_free_thread( TEB *teb )
{
}


/**********************************************************************
 *		signal_init_process
 */
void signal_init_process(void)
{
    struct sigaction sig_act;
    void *kernel_stack = (char *)ntdll_get_thread_data()->kernel_stack + kernel_stack_size;

    riscv64_thread_data()->syscall_frame = (struct syscall_frame *)kernel_stack - 1;

    sig_act.sa_mask = server_block_set;
    sig_act.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;

    sig_act.sa_sigaction = int_handler;
    if (sigaction( SIGINT, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = fpe_handler;
    if (sigaction( SIGFPE, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = abrt_handler;
    if (sigaction( SIGABRT, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = quit_handler;
    if (sigaction( SIGQUIT, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = usr1_handler;
    if (sigaction( SIGUSR1, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = usr2_handler;
    if (sigaction( SIGUSR2, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = trap_handler;
    if (sigaction( SIGTRAP, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = segv_handler;
    if (sigaction( SIGSEGV, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = ill_handler;
    if (sigaction( SIGILL, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = bus_handler;
    if (sigaction( SIGBUS, &sig_act, NULL ) == -1) goto error;
    return;

 error:
    perror("sigaction");
    exit(1);
}


/***********************************************************************
 *           syscall_dispatcher_return_slowpath
 */
void syscall_dispatcher_return_slowpath(void)
{
    raise( SIGUSR2 );
}

/***********************************************************************
 *           call_init_thunk
 */
void call_init_thunk( LPTHREAD_START_ROUTINE entry, void *arg, BOOL suspend, TEB *teb )
{
    struct riscv64_thread_data *thread_data = (struct riscv64_thread_data *)&teb->GdiTebBatch;
    struct syscall_frame *frame = thread_data->syscall_frame;
    CONTEXT *ctx, context = { CONTEXT_ALL };
    I386_CONTEXT *i386_context;
    ARM_CONTEXT *arm_context;

    context.A0  = (DWORD64)entry;
    context.A1  = (DWORD64)arg;
    context.Sp  = (DWORD64)teb->Tib.StackBase;
    context.Pc  = (DWORD64)pRtlUserThreadStart;
    //context.Ra  = 0x12345678;

    if ((i386_context = get_cpu_area( IMAGE_FILE_MACHINE_I386 )))
    {
        XMM_SAVE_AREA32 *fpu = (XMM_SAVE_AREA32 *)i386_context->ExtendedRegisters;
        i386_context->ContextFlags = CONTEXT_I386_ALL;
        i386_context->Eax = (ULONG_PTR)entry;
        i386_context->Ebx = (arg == peb ? (ULONG_PTR)wow_peb : (ULONG_PTR)arg);
        i386_context->Esp = get_wow_teb( teb )->Tib.StackBase - 16;
        i386_context->Eip = pLdrSystemDllInitBlock->pRtlUserThreadStart;
        i386_context->SegCs = 0x23;
        i386_context->SegDs = 0x2b;
        i386_context->SegEs = 0x2b;
        i386_context->SegFs = 0x53;
        i386_context->SegGs = 0x2b;
        i386_context->SegSs = 0x2b;
        i386_context->EFlags = 0x202;
        fpu->ControlWord = 0x27f;
        fpu->MxCsr = 0x1f80;
        fpux_to_fpu( &i386_context->FloatSave, fpu );
    }
    else if ((arm_context = get_cpu_area( IMAGE_FILE_MACHINE_ARMNT )))
    {
        arm_context->ContextFlags = CONTEXT_ARM_ALL;
        arm_context->R0 = (ULONG_PTR)entry;
        arm_context->R1 = (arg == peb ? (ULONG_PTR)wow_peb : (ULONG_PTR)arg);
        arm_context->Sp = get_wow_teb( teb )->Tib.StackBase;
        arm_context->Pc = pLdrSystemDllInitBlock->pRtlUserThreadStart;
        if (arm_context->Pc & 1) arm_context->Cpsr |= 0x20; /* thumb mode */
    }

    if (suspend) {ERR("suspend...\n");wait_suspend( &context );}

    ctx = (CONTEXT *)((ULONG_PTR)context.Sp & ~15) - 1;
    *ctx = context;
    ctx->ContextFlags = CONTEXT_FULL;
    memset( frame, 0, sizeof(*frame) );
    NtSetContextThread( GetCurrentThread(), ctx );

    frame->x[2]  = (ULONG64)ctx;
    frame->x[0]  = (ULONG64)pLdrInitializeThunk;
    frame->x[10] = (ULONG64)ctx;
    //frame->x[4]  = (ULONG64)teb;
    frame->prev_frame = NULL;
    frame->restore_flags |= CONTEXT_INTEGER;
    syscall_frame_fixup_for_fastpath( frame );
    frame->syscall_table = KeServiceDescriptorTable;

    pthread_sigmask( SIG_UNBLOCK, &server_block_set, NULL );
    __wine_syscall_dispatcher_return( frame, 0 );
}


/***********************************************************************
 *           signal_start_thread
 */
__ASM_GLOBAL_FUNC( signal_start_thread,
                   "addi sp, sp, -16\n\t"
                   "sd ra, 8(sp)\n\t"
                   "sd fp, 0(sp)\n\t"
                   /* store exit frame */
                   "mv fp, sp\n\t"
                   "sd fp, 0x2f0(a3)\n\t"  /* riscv64_thread_data()->exit_frame */
                   /* set syscall frame */
                   "ld t0, 0x2f8(a3)\n\t"   /* riscv64_thread_data()->syscall_frame */
                   "bnez t0, 1f\n\t"
                   "addi t0, sp, -0x330\n\t"     /* sizeof(struct syscall_frame) */ // TODO: 0x330 is expected sizeof(struct syscall_frame) !!!
                   "sd t0, 0x2f8(a3)\n\t"   /* riscv64_thread_data()->syscall_frame */
                   "1:\tmv sp, t0\n\t"
                   "jal " __ASM_NAME("call_init_thunk") )

/***********************************************************************
 *           signal_exit_thread
 */
__ASM_GLOBAL_FUNC( signal_exit_thread,
                   "addi sp, sp, -16\n\t"
                   "sd ra, 8(sp)\n\t"
                   "sd fp, 0(sp)\n\t"
                   "ld a3, 0x2f0(a2)\n\t"   /* riscv64_thread_data()->exit_frame */
                   "sd zero, 0x2f0(a2)\n\t"
                   "beqz a3, 1f\n\t"
                   "mv sp, a3\n\t"
                   "1:\tld ra, 8(sp)\n\t"
                   "ld fp, 0(sp)\n\t"
                   "jr a1" )


/***********************************************************************
 *           __wine_syscall_dispatcher
 */
__ASM_GLOBAL_FUNC( __wine_syscall_dispatcher,
                   "addi sp, sp, -0x60\n\t"
                   "sd ra, 0x58(sp)\n\t"
                   "sd fp, 0x50(sp)\n\t"
                   "sd a0, 0x00(sp)\n\t"
                   "sd a1, 0x08(sp)\n\t"
                   "sd a2, 0x10(sp)\n\t"
                   "sd a3, 0x18(sp)\n\t"
                   "sd a4, 0x20(sp)\n\t"
                   "sd a5, 0x28(sp)\n\t"
                   "sd a6, 0x30(sp)\n\t"
                   "sd a7, 0x38(sp)\n\t"
                   "sd t0, 0x40(sp)\n\t"
                   "sd t1, 0x48(sp)\n\t"
                   "jal " __ASM_NAME("NtCurrentTeb") "\n\t"
                   "ld t2, 0x2f8(a0)\n\t"   /* riscv64_thread_data()->syscall_frame */
                   "ld a0, 0x00(sp)\n\t"
                   "ld a1, 0x08(sp)\n\t"
                   "ld a2, 0x10(sp)\n\t"
                   "ld a3, 0x18(sp)\n\t"
                   "ld a4, 0x20(sp)\n\t"
                   "ld a5, 0x28(sp)\n\t"
                   "ld a6, 0x30(sp)\n\t"
                   "ld a7, 0x38(sp)\n\t"
                   "ld t0, 0x40(sp)\n\t"
                   "ld t1, 0x48(sp)\n\t"
                   "ld fp, 0x50(sp)\n"
                   "ld ra, 0x58(sp)\n"
                   "addi sp, sp, 0x60\n"

                   //"ld t2, 0x2f8(tp)\n\t"   /* riscv64_thread_data()->syscall_frame */
                   "sd ra, 0x00(t2)\n\t"
                   "sd t1, 0x08(t2)\n\t"
                   "sd sp, 0x10(t2)\n\t"
                   "sd gp, 0x18(t2)\n\t"
                   "sd tp, 0x20(t2)\n\t"
                   "sd t0, 0x28(t2)\n\t"
                   "sd t1, 0x30(t2)\n\t"
                   "sd t2, 0x38(t2)\n\t"
                   "sd s0, 0x40(t2)\n\t"
                   "sd s1, 0x48(t2)\n\t"
                   "sd a0, 0x50(t2)\n\t"
                   "sd a1, 0x58(t2)\n\t"
                   "sd a2, 0x60(t2)\n\t"
                   "sd a3, 0x68(t2)\n\t"
                   "sd a4, 0x70(t2)\n\t"
                   "sd a5, 0x78(t2)\n\t"
                   "sd a6, 0x80(t2)\n\t"
                   "sd a7, 0x88(t2)\n\t"
                   "sd s2, 0x90(t2)\n\t"
                   "sd s3, 0x98(t2)\n\t"
                   "sd s4, 0xa0(t2)\n\t"
                   "sd s5, 0xa8(t2)\n\t"
                   "sd s6, 0xb0(t2)\n\t"
                   "sd s7, 0xb8(t2)\n\t"
                   "sd s8, 0xc0(t2)\n\t"
                   "sd s9, 0xc8(t2)\n\t"
                   "sd s10, 0xd0(t2)\n\t"
                   "sd s11, 0xd8(t2)\n\t"
                   "sd t3, 0xe0(t2)\n\t"
                   "sd t4, 0xe8(t2)\n\t"
                   "sd t5, 0xf0(t2)\n\t"
                   "sd t6, 0xf8(t2)\n\t"
                   "sw zero, 0x100(t2)\n\t"   /* frame->restore_flags */ // fixme: not obv documented in arm64
                   "mv s2, sp\n\t"
                   "mv sp, t2\n\t"
                   "slli t3, t0, 52\n\t"
                   "srli t3, t3, 52\n\t"  /* syscall number */
                   "li t5, 0x3000\n\t"
                   "and t0, t0, t5\n\t"
                   "srli t0, t0, 12\n\t"     /* syscall table number */
                   "ld t5, 0x110(t2)\n\t"    /* frame->syscall_table */
                   "slli t0, t0, 5\n\t"
                   "add t0, t5, t0\n\t"
                   "ld t5, 0x10(t0)\n\t"     /* table->ServiceLimit */
                   "bge t3, t5, 4f\n\t"
                   //"ld tp, 0x118(t2)\n\t"
                   "mv s11, sp\n\t"
                   "ld t5, 0x18(t0)\n\t"     /* table->ArgumentTable */
                   "li t4, 0\n\t" // FIXME: might not be needed
                   "add s1, t5, t3\n\t"
                   "lbu t4, 0(s1)\n\t"
                   "addi t4, t4, -0x40\n\t"
                   "blez t4, 2f\n\t"
                   "sub sp, sp, t4\n\t"
                   "andi s1, t4, 0x08\n\t"
                   "beqz s1, 1f\n\t"
                   "addi sp, sp, -0x08\n\t"
                   "1:\taddi t4, t4, -0x08\n\t"
                   "add s1, s2, t4\n\t"
                   "ld t2, 0(s1)\n\t"
                   "add s1, sp, t4\n\t"
                   "sd t2, 0(s1)\n\t"
                   "bnez t4, 1b\n"
                   "2:\tld t5, 0(t0)\n\t"     /* table->ServiceTable */
                   "slli s1, t3, 3\n\t"
                   "add s1, t5, s1\n\t"
                   "ld t5, 0(s1)\n\t"
                   "jalr t5\n\t"
                   "mv sp, s11\n\t"
                   //"sd a0, 0x50(sp)\n\t" // fixme: not analogue to arm64, but why
                   __ASM_LOCAL_LABEL("__wine_syscall_dispatcher_return") ":\n\t"

                   "lwu t5, 0x100(sp)\n\t"   /* frame->restore_flags */
                   "andi s1, t5, 0x02\n\t" /* CONTEXT_INTEGER */
                   "beqz s1, 2f\n\t"
                   "ld t3, 0x00(sp)\n\t" /* frame->pc */
                   "ld t5, 0xf0(sp)\n\t" /* frame->t5 */
                   "sub t2, t5, t3\n\t"
                   "snez t1, t2\n\t"
                   "ld t4, 0x10(sp)\n\t" /* frame->sp */
                   "ld t6, 0xf8(sp)\n\t" /* frame->t6 */
                   "sub t2, t6, t4\n\t"
                   "add t2, t2, t1\n\t"
                   "beqz t2, 1f\n\t"
                   "jal " __ASM_NAME("syscall_dispatcher_return_slowpath") "\n"
                   "1:\tld t0, 0x28(sp)\n\t"
                   "ld t1, 0x30(sp)\n\t"
                   "ld t2, 0x38(sp)\n\t"
                   "ld a0, 0x50(sp)\n\t"
                   "ld a1, 0x58(sp)\n\t"
                   "ld a2, 0x60(sp)\n\t"
                   "ld a3, 0x68(sp)\n\t"
                   "ld a4, 0x70(sp)\n\t"
                   "ld a5, 0x78(sp)\n\t"
                   "ld a6, 0x80(sp)\n\t"
                   "ld a7, 0x88(sp)\n\t"
                   "ld t3, 0xe0(sp)\n\t"
                   "ld t4, 0xe8(sp)\n\t"
                   "ld t5, 0xf0(sp)\n\t"
                   "ld t6, 0xf8(sp)\n\t"
                   "2:\tld ra, 0x08(sp)\n\t"
                   //"ld gp, 0x18(sp)\n\t"
                   //"ld tp, 0x20(sp)\n\t"
                   "ld s0, 0x40(sp)\n\t"
                   "ld s1, 0x48(sp)\n\t"
                   "ld s2, 0x90(sp)\n\t"
                   "ld s3, 0x98(sp)\n\t"
                   "ld s4, 0xa0(sp)\n\t"
                   "ld s5, 0xa8(sp)\n\t"
                   "ld s6, 0xb0(sp)\n\t"
                   "ld s7, 0xb8(sp)\n\t"
                   "ld s8, 0xc0(sp)\n\t"
                   "ld s9, 0xc8(sp)\n\t"
                   "ld s10, 0xd0(sp)\n\t"
                   "ld s11, 0xd8(sp)\n\t"
                   "ld t5, 0x00(sp)\n\t"
                   "ld sp, 0x10(sp)\n\t"
                   "jr t5\n\t"
                   "4:\tli a0, 0xc0000000\n\t" /* STATUS_INVALID_PARAMETER */
                   "addi a0, a0, 0x000d\n\t"
                   "j " __ASM_LOCAL_LABEL("__wine_syscall_dispatcher_return") "\n\t"
                   ".globl " __ASM_NAME("__wine_syscall_dispatcher_return") "\n"
                   __ASM_NAME("__wine_syscall_dispatcher_return") ":\n\t"
                   "mv sp, a0\n\t"
                   "mv a0, a1\n\t"
                   "j " __ASM_LOCAL_LABEL("__wine_syscall_dispatcher_return") )


/***********************************************************************
 *           __wine_unix_call_dispatcher
 */
__ASM_GLOBAL_FUNC( __wine_unix_call_dispatcher,
                   "addi sp, sp, -0x30\n\t"
                   "sd ra, 0x28(sp)\n\t"
                   "sd fp, 0x20(sp)\n\t"
                   "sd a0, 0x00(sp)\n\t"
                   "sd a1, 0x08(sp)\n\t"
                   "sd a2, 0x10(sp)\n\t"
                   "jal " __ASM_NAME("NtCurrentTeb") "\n\t"
                   "ld t0, 0x2f8(a0)\n\t"   /* riscv64_thread_data()->syscall_frame */
                   "ld a0, 0x00(sp)\n\t"
                   "ld a1, 0x08(sp)\n\t"
                   "ld a2, 0x10(sp)\n\t"
                   "ld fp, 0x20(sp)\n"
                   "ld ra, 0x28(sp)\n"
                   "addi sp, sp, 0x30\n"

                   //"ld t0, 0x2f8(tp)\n\t"   /* riscv64_thread_data()->syscall_frame */
                   "sd ra, 0x08(t0)\n\t"
                   "sd sp, 0x10(t0)\n\t"
                   "sd gp, 0x18(t0)\n\t"
                   "sd tp, 0x20(t0)\n\t"
                   "sd t0, 0x28(t0)\n\t"
                   "sd t1, 0x30(t0)\n\t"
                   "sd t2, 0x38(t0)\n\t"
                   "sd s0, 0x40(t0)\n\t"
                   "sd s1, 0x48(t0)\n\t"
                   "sd a0, 0x50(t0)\n\t"
                   "sd a1, 0x58(t0)\n\t"
                   "sd a2, 0x60(t0)\n\t"
                   "sd a3, 0x68(t0)\n\t"
                   "sd a4, 0x70(t0)\n\t"
                   "sd a5, 0x78(t0)\n\t"
                   "sd a6, 0x80(t0)\n\t"
                   "sd a7, 0x88(t0)\n\t"
                   "sd s2, 0x90(t0)\n\t"
                   "sd s3, 0x98(t0)\n\t"
                   "sd s4, 0xa0(t0)\n\t"
                   "sd s5, 0xa8(t0)\n\t"
                   "sd s6, 0xb0(t0)\n\t"
                   "sd s7, 0xb8(t0)\n\t"
                   "sd s8, 0xc0(t0)\n\t"
                   "sd s9, 0xc8(t0)\n\t"
                   "sd s10, 0xd0(t0)\n\t"
                   "sd s11, 0xd8(t0)\n\t"
                   "sd t3, 0xe0(t0)\n\t"
                   "sd t4, 0xe8(t0)\n\t"
                   "sd t5, 0xf0(t0)\n\t"
                   "sd t6, 0xf8(t0)\n\t"
                   "sw zero, 0x100(t0)\n\t"   /* frame->restore_flags */ // fixme: not obv documented in arm64
                   "mv sp, t0\n\t"
                   //"ld tp, 0x118(t0)\n\t"
                   "slli t0, a1, 3\n\t"
                   "add t0, t0, a0\n\t"
                   "ld t1, 0(t0)\n\t"
                   "mv a0, a2\n\t"          /* args */
                   "jalr t1\n\t"
                   "lw t1, 0x100(sp)\n\t"   /* frame->restore_flags */
                   "bnez t1, " __ASM_LOCAL_LABEL("__wine_syscall_dispatcher_return") "\n\t"
                   //"ld tp, 0x20(sp)\n\t"
                   "ld ra, 0x08(sp)\n\t"
                   "ld sp, 0x10(sp)\n\t"
                   "ret" )

/***********************************************************************
 *           __wine_setjmpex
 */
__ASM_GLOBAL_FUNC( __wine_setjmpex,  // FIXME: fpcr fpsr
                   "sd a1, 0(a0)\n\t"       /* jmp_buf->Frame */
                   "sd ra, 0x10(a0)\n\t"    /* jmp_buf->__pc */
                   "sd s0, 0x18(a0)\n\t"
                   "sd s1, 0x20(a0)\n\t"
                   "sd s2, 0x28(a0)\n\t"
                   "sd s3, 0x30(a0)\n\t"
                   "sd s4, 0x38(a0)\n\t"
                   "sd s5, 0x40(a0)\n\t"
                   "sd s6, 0x48(a0)\n\t"
                   "sd s7, 0x50(a0)\n\t"
                   "sd s8, 0x58(a0)\n\t"
                   "sd s9, 0x60(a0)\n\t"
                   "sd s10, 0x68(a0)\n\t"
                   "sd s11, 0x70(a0)\n\t"
                   "sd sp, 0x78(a0)\n\t"
                   "fsd fs0, 0x80(a0)\n\t"
                   "fsd fs1, 0x88(a0)\n\t"
                   "fsd fs2, 0x90(a0)\n\t"
                   "fsd fs3, 0x98(a0)\n\t"
                   "fsd fs4, 0xa0(a0)\n\t"
                   "fsd fs5, 0xa8(a0)\n\t"
                   "fsd fs6, 0xb0(a0)\n\t"
                   "fsd fs7, 0xb8(a0)\n\t"
                   "fsd fs8, 0xc0(a0)\n\t"
                   "fsd fs9, 0xc8(a0)\n\t"
                   "fsd fs10, 0xd0(a0)\n\t"
                   "fsd fs11, 0xd8(a0)\n\t"
                   "li a0, 0\n\t"
                   "ret" )


/***********************************************************************
 *           __wine_longjmp
 */
__ASM_GLOBAL_FUNC( __wine_longjmp,  // FIXME: fpcr fpsr
                   "ld ra, 0x10(a0)\n\t"    /* jmp_buf->__pc */
                   "ld s0, 0x18(a0)\n\t"
                   "ld s1, 0x20(a0)\n\t"
                   "ld s2, 0x28(a0)\n\t"
                   "ld s3, 0x30(a0)\n\t"
                   "ld s4, 0x38(a0)\n\t"
                   "ld s5, 0x40(a0)\n\t"
                   "ld s6, 0x48(a0)\n\t"
                   "ld s7, 0x50(a0)\n\t"
                   "ld s8, 0x58(a0)\n\t"
                   "ld s9, 0x60(a0)\n\t"
                   "ld s10, 0x68(a0)\n\t"
                   "ld s11, 0x70(a0)\n\t"
                   "ld sp, 0x78(a0)\n\t"
                   "fld fs0, 0x80(a0)\n\t"
                   "fld fs1, 0x88(a0)\n\t"
                   "fld fs2, 0x90(a0)\n\t"
                   "fld fs3, 0x98(a0)\n\t"
                   "fld fs4, 0xa0(a0)\n\t"
                   "fld fs5, 0xa8(a0)\n\t"
                   "fld fs6, 0xb0(a0)\n\t"
                   "fld fs7, 0xb8(a0)\n\t"
                   "fld fs8, 0xc0(a0)\n\t"
                   "fld fs9, 0xc8(a0)\n\t"
                   "fld fs10, 0xd0(a0)\n\t"
                   "fld fs11, 0xd8(a0)\n\t"
                   "mv a0, a1\n\t"          /* retval */
                   "ret" )

#endif  /* __aarch64__ */
