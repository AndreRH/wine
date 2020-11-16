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

#if 0
#pragma makedep unix
#endif

#ifdef __powerpc64__

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
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

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "wine/exception.h"
#include "wine/asm.h"
#include "unix_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);

/***********************************************************************
 * signal context platform-specific definitions
 */
#ifdef linux

/* All Registers access - only for local access */
# define REG_sig(reg_name, context)     ((context)->uc_mcontext.regs->reg_name)


/* Gpr Registers access  */
# define GPR_sig(reg_num, context)      REG_sig(gpr[reg_num], context)

# define IAR_sig(context)               REG_sig(nip, context)    /* Program counter */
# define MSR_sig(context)               REG_sig(msr, context)   /* Machine State Register (Supervisor) */
# define CTR_sig(context)               REG_sig(ctr, context)   /* Count register */

# define XER_sig(context)               REG_sig(xer, context) /* User's integer exception register */
# define LR_sig(context)                REG_sig(link, context) /* Link register */
# define CR_sig(context)                REG_sig(ccr, context) /* Condition register */

/* Float Registers access  */
# define FLOAT_sig(reg_num, context)    ((context)->uc_mcontext.fp_regs[reg_num])

# define FPSCR_sig(context)             FLOAT_sig(32, context)

/* Exception Registers access */
# define DAR_sig(context)               REG_sig(dar, context)
# define DSISR_sig(context)             REG_sig(dsisr, context)
# define TRAP_sig(context)              REG_sig(trap, context)

#endif /* linux */

static pthread_key_t teb_key;

struct syscall_frame
{
    ULONG64 r1;
    ULONG64 cr; /* unused */
    ULONG64 pad0;
    ULONG64 r2;
    ULONG64 r3, r4, r5, r6, r7, r8, r9, r10;
    ULONG64 r14, r15, r16, r17, r18, r19, r20, r21, r22;
    ULONG64 r23, r24, r25, r26, r27, r28, r29, r30, r31;
    ULONG64 thunk_r1;
    ULONG64 pad2;
    ULONG64 thunk_addr;
    ULONG64 thunk_r2;
    ULONG64 ret_addr;
    ULONG64 r11;
    ULONG64 r12;
    ULONG64 pad3;
};

struct ppc64_thread_data
{
    void                 *exit_frame;    /* 02f0 exit frame pointer */
    struct syscall_frame *syscall_frame; /* 02f8 frame pointer on syscall entry */
};

C_ASSERT( sizeof(struct ppc64_thread_data) <= sizeof(((struct ntdll_thread_data *)0)->cpu_data) );
C_ASSERT( offsetof( TEB, GdiTebBatch ) + offsetof( struct ppc64_thread_data, exit_frame ) == 0x2f0 );
C_ASSERT( offsetof( TEB, GdiTebBatch ) + offsetof( struct ppc64_thread_data, syscall_frame ) == 0x2f8 );

static inline struct ppc64_thread_data *ppc64_thread_data(void)
{
    return (struct ppc64_thread_data *)ntdll_get_thread_data()->cpu_data;
}


/***********************************************************************
 *           unwind_builtin_dll
 *
 * Equivalent of RtlVirtualUnwind for builtin modules.
 */
NTSTATUS CDECL unwind_builtin_dll( ULONG type, struct _DISPATCHER_CONTEXT *dispatch, CONTEXT *context )
{
    FIXME("not implemented\n");
    return STATUS_UNSUCCESSFUL;
}


/***********************************************************************
 *           save_context
 *
 * Set the register values from a sigcontext.
 */
static void save_context( CONTEXT *context, const ucontext_t *sigcontext )
{
#define C(x) context->Gpr##x = GPR_sig(x,sigcontext)
    /* Save Gpr registers */
    C(0); C(1); C(2); C(3); C(4); C(5); C(6); C(7); C(8); C(9); C(10);
    C(11); C(12); C(13); C(14); C(15); C(16); C(17); C(18); C(19); C(20);
    C(21); C(22); C(23); C(24); C(25); C(26); C(27); C(28); C(29); C(30);
    C(31);
#undef C

    context->Iar = IAR_sig(sigcontext);  /* Program Counter */
    context->Msr = MSR_sig(sigcontext);  /* Machine State Register (Supervisor) */
    context->Ctr = CTR_sig(sigcontext);

    context->Xer = XER_sig(sigcontext);
    context->Lr  = LR_sig(sigcontext);
    context->Cr  = CR_sig(sigcontext);

    /* Saving Exception regs */
    context->Dar   = DAR_sig(sigcontext);
    context->Dsisr = DSISR_sig(sigcontext);
    context->Trap  = TRAP_sig(sigcontext);
}


/***********************************************************************
 *           restore_context
 *
 * Build a sigcontext from the register values.
 */
static void restore_context( const CONTEXT *context, ucontext_t *sigcontext )
{
#define C(x)  GPR_sig(x,sigcontext) = context->Gpr##x
    C(0); C(1); C(2); C(3); C(4); C(5); C(6); C(7); C(8); C(9); C(10);
    C(11); C(12); C(13); C(14); C(15); C(16); C(17); C(18); C(19); C(20);
    C(21); C(22); C(23); C(24); C(25); C(26); C(27); C(28); C(29); C(30);
    C(31);
#undef C

    IAR_sig(sigcontext) = context->Iar;  /* Program Counter */
    MSR_sig(sigcontext) = context->Msr;  /* Machine State Register (Supervisor) */
    CTR_sig(sigcontext) = context->Ctr;

    XER_sig(sigcontext) = context->Xer;
    LR_sig(sigcontext) = context->Lr;
    CR_sig(sigcontext) = context->Cr;

    /* Setting Exception regs */
    DAR_sig(sigcontext) = context->Dar;
    DSISR_sig(sigcontext) = context->Dsisr;
    TRAP_sig(sigcontext) = context->Trap;
}


/***********************************************************************
 *           save_fpu
 *
 * Set the FPU context from a sigcontext.
 */
static void save_fpu( CONTEXT *context, ucontext_t *sigcontext )
{
#define C(x)   context->Fpr##x = FLOAT_sig(x,sigcontext)
    C(0); C(1); C(2); C(3); C(4); C(5); C(6); C(7); C(8); C(9); C(10);
    C(11); C(12); C(13); C(14); C(15); C(16); C(17); C(18); C(19); C(20);
    C(21); C(22); C(23); C(24); C(25); C(26); C(27); C(28); C(29); C(30);
    C(31);
#undef C
    context->Fpscr = FPSCR_sig(sigcontext);
}


/***********************************************************************
 *           restore_fpu
 *
 * Restore the FPU context to a sigcontext.
 */
static void restore_fpu( CONTEXT *context, ucontext_t *sigcontext )
{
#define C(x)  FLOAT_sig(x,sigcontext) = context->Fpr##x
    C(0); C(1); C(2); C(3); C(4); C(5); C(6); C(7); C(8); C(9); C(10);
    C(11); C(12); C(13); C(14); C(15); C(16); C(17); C(18); C(19); C(20);
    C(21); C(22); C(23); C(24); C(25); C(26); C(27); C(28); C(29); C(30);
    C(31);
#undef C
    FPSCR_sig(sigcontext) = context->Fpscr;
}


/***********************************************************************
 *           set_cpu_context
 *
 * Set the new CPU context.
 */
void DECLSPEC_HIDDEN set_cpu_context( const CONTEXT *context );
__ASM_GLOBAL_FUNC( set_cpu_context,
                   "ld 1, 272(3)\n\t"
                   "ld 2, 280(3)\n\t"
                   "ld 4, 296(3)\n\t"
                   "ld 5, 304(3)\n\t"
                   "ld 6, 312(3)\n\t"
                   "ld 7, 320(3)\n\t"
                   "ld 8, 328(3)\n\t"
                   "ld 9, 336(3)\n\t"
                   "ld 10, 344(3)\n\t"
                   "ld 11, 352(3)\n\t"
                   "ld 14, 376(3)\n\t"
                   "ld 15, 384(3)\n\t"
                   "ld 16, 392(3)\n\t"
                   "ld 17, 400(3)\n\t"
                   "ld 18, 408(3)\n\t"
                   "ld 19, 416(3)\n\t"
                   "ld 20, 424(3)\n\t"
                   "ld 21, 432(3)\n\t"
                   "ld 22, 440(3)\n\t"
                   "ld 23, 448(3)\n\t"
                   "ld 24, 456(3)\n\t"
                   "ld 25, 464(3)\n\t"
                   "ld 26, 472(3)\n\t"
                   "ld 27, 480(3)\n\t"
                   "ld 28, 488(3)\n\t"
                   "ld 29, 496(3)\n\t"
                   "ld 30, 504(3)\n\t"
                   "ld 31, 512(3)\n\t"
                   "ld 12, 544(3)\n\t" /* iar */
                   "ld 0, 552(3)\n\t" /* lr */
                   "mtlr 0\n\t"
                   "ld 3, 288(3)\n\t" /* r3 */
                   "mtctr 12\n\t"
                   "bctr\n\t"
                   "ori 0, 0, 0" )


/***********************************************************************
 *           get_server_context_flags
 *
 * Convert CPU-specific flags to generic server flags
 */
static unsigned int get_server_context_flags( DWORD flags )
{
    unsigned int ret = 0;

    flags &= ~CONTEXT_POWERPC64;  /* get rid of CPU id */
    if (flags & CONTEXT_CONTROL) ret |= SERVER_CTX_CONTROL;
    if (flags & CONTEXT_INTEGER) ret |= SERVER_CTX_INTEGER;
    if (flags & CONTEXT_FLOATING_POINT) ret |= SERVER_CTX_FLOATING_POINT;
    if (flags & CONTEXT_DEBUG_REGISTERS) ret |= SERVER_CTX_DEBUG_REGISTERS;
    return ret;
}


/***********************************************************************
 *           context_to_server
 *
 * Convert a register context to the server format.
 */
NTSTATUS context_to_server( context_t *to, const CONTEXT *from )
{
    DWORD flags = from->ContextFlags & ~CONTEXT_POWERPC64;  /* get rid of CPU id */

    memset( to, 0, sizeof(*to) );
    to->cpu = CPU_POWERPC64;

    if (flags & CONTEXT_CONTROL)
    {
        to->flags |= SERVER_CTX_CONTROL;
        to->ctl.powerpc64_regs.iar   = from->Iar;
        to->ctl.powerpc64_regs.msr   = from->Msr;
        to->ctl.powerpc64_regs.ctr   = from->Ctr;
        to->ctl.powerpc64_regs.lr    = from->Lr;
        to->ctl.powerpc64_regs.dar   = from->Dar;
        to->ctl.powerpc64_regs.dsisr = from->Dsisr;
        to->ctl.powerpc64_regs.trap  = from->Trap;
    }
    if (flags & CONTEXT_INTEGER)
    {
        to->flags |= SERVER_CTX_INTEGER;
        to->integer.powerpc64_regs.gpr[0]  = from->Gpr0;
        to->integer.powerpc64_regs.gpr[1]  = from->Gpr1;
        to->integer.powerpc64_regs.gpr[2]  = from->Gpr2;
        to->integer.powerpc64_regs.gpr[3]  = from->Gpr3;
        to->integer.powerpc64_regs.gpr[4]  = from->Gpr4;
        to->integer.powerpc64_regs.gpr[5]  = from->Gpr5;
        to->integer.powerpc64_regs.gpr[6]  = from->Gpr6;
        to->integer.powerpc64_regs.gpr[7]  = from->Gpr7;
        to->integer.powerpc64_regs.gpr[8]  = from->Gpr8;
        to->integer.powerpc64_regs.gpr[9]  = from->Gpr9;
        to->integer.powerpc64_regs.gpr[10] = from->Gpr10;
        to->integer.powerpc64_regs.gpr[11] = from->Gpr11;
        to->integer.powerpc64_regs.gpr[12] = from->Gpr12;
        to->integer.powerpc64_regs.gpr[13] = from->Gpr13;
        to->integer.powerpc64_regs.gpr[14] = from->Gpr14;
        to->integer.powerpc64_regs.gpr[15] = from->Gpr15;
        to->integer.powerpc64_regs.gpr[16] = from->Gpr16;
        to->integer.powerpc64_regs.gpr[17] = from->Gpr17;
        to->integer.powerpc64_regs.gpr[18] = from->Gpr18;
        to->integer.powerpc64_regs.gpr[19] = from->Gpr19;
        to->integer.powerpc64_regs.gpr[20] = from->Gpr20;
        to->integer.powerpc64_regs.gpr[21] = from->Gpr21;
        to->integer.powerpc64_regs.gpr[22] = from->Gpr22;
        to->integer.powerpc64_regs.gpr[23] = from->Gpr23;
        to->integer.powerpc64_regs.gpr[24] = from->Gpr24;
        to->integer.powerpc64_regs.gpr[25] = from->Gpr25;
        to->integer.powerpc64_regs.gpr[26] = from->Gpr26;
        to->integer.powerpc64_regs.gpr[27] = from->Gpr27;
        to->integer.powerpc64_regs.gpr[28] = from->Gpr28;
        to->integer.powerpc64_regs.gpr[29] = from->Gpr29;
        to->integer.powerpc64_regs.gpr[30] = from->Gpr30;
        to->integer.powerpc64_regs.gpr[31] = from->Gpr31;
        to->integer.powerpc64_regs.xer     = from->Xer;
        to->integer.powerpc64_regs.cr      = from->Cr;
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        to->flags |= SERVER_CTX_FLOATING_POINT;
        to->fp.powerpc64_regs.fpr[0]  = from->Fpr0;
        to->fp.powerpc64_regs.fpr[1]  = from->Fpr1;
        to->fp.powerpc64_regs.fpr[2]  = from->Fpr2;
        to->fp.powerpc64_regs.fpr[3]  = from->Fpr3;
        to->fp.powerpc64_regs.fpr[4]  = from->Fpr4;
        to->fp.powerpc64_regs.fpr[5]  = from->Fpr5;
        to->fp.powerpc64_regs.fpr[6]  = from->Fpr6;
        to->fp.powerpc64_regs.fpr[7]  = from->Fpr7;
        to->fp.powerpc64_regs.fpr[8]  = from->Fpr8;
        to->fp.powerpc64_regs.fpr[9]  = from->Fpr9;
        to->fp.powerpc64_regs.fpr[10] = from->Fpr10;
        to->fp.powerpc64_regs.fpr[11] = from->Fpr11;
        to->fp.powerpc64_regs.fpr[12] = from->Fpr12;
        to->fp.powerpc64_regs.fpr[13] = from->Fpr13;
        to->fp.powerpc64_regs.fpr[14] = from->Fpr14;
        to->fp.powerpc64_regs.fpr[15] = from->Fpr15;
        to->fp.powerpc64_regs.fpr[16] = from->Fpr16;
        to->fp.powerpc64_regs.fpr[17] = from->Fpr17;
        to->fp.powerpc64_regs.fpr[18] = from->Fpr18;
        to->fp.powerpc64_regs.fpr[19] = from->Fpr19;
        to->fp.powerpc64_regs.fpr[20] = from->Fpr20;
        to->fp.powerpc64_regs.fpr[21] = from->Fpr21;
        to->fp.powerpc64_regs.fpr[22] = from->Fpr22;
        to->fp.powerpc64_regs.fpr[23] = from->Fpr23;
        to->fp.powerpc64_regs.fpr[24] = from->Fpr24;
        to->fp.powerpc64_regs.fpr[25] = from->Fpr25;
        to->fp.powerpc64_regs.fpr[26] = from->Fpr26;
        to->fp.powerpc64_regs.fpr[27] = from->Fpr27;
        to->fp.powerpc64_regs.fpr[28] = from->Fpr28;
        to->fp.powerpc64_regs.fpr[29] = from->Fpr29;
        to->fp.powerpc64_regs.fpr[30] = from->Fpr30;
        to->fp.powerpc64_regs.fpr[31] = from->Fpr31;
        to->fp.powerpc64_regs.fpscr   = from->Fpscr;
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           context_from_server
 *
 * Convert a register context from the server format.
 */
NTSTATUS context_from_server( CONTEXT *to, const context_t *from )
{
    if (from->cpu != CPU_POWERPC64) return STATUS_INVALID_PARAMETER;

    to->ContextFlags = CONTEXT_POWERPC64;
    if (from->flags & SERVER_CTX_CONTROL)
    {
        to->ContextFlags |= CONTEXT_CONTROL;
        to->Msr   = from->ctl.powerpc64_regs.msr;
        to->Ctr   = from->ctl.powerpc64_regs.ctr;
        to->Iar   = from->ctl.powerpc64_regs.iar;
        to->Lr    = from->ctl.powerpc64_regs.lr;
        to->Dar   = from->ctl.powerpc64_regs.dar;
        to->Dsisr = from->ctl.powerpc64_regs.dsisr;
        to->Trap  = from->ctl.powerpc64_regs.trap;
    }
    if (from->flags & SERVER_CTX_INTEGER)
    {
        to->ContextFlags |= CONTEXT_INTEGER;
        to->Gpr0  = from->integer.powerpc64_regs.gpr[0];
        to->Gpr1  = from->integer.powerpc64_regs.gpr[1];
        to->Gpr2  = from->integer.powerpc64_regs.gpr[2];
        to->Gpr3  = from->integer.powerpc64_regs.gpr[3];
        to->Gpr4  = from->integer.powerpc64_regs.gpr[4];
        to->Gpr5  = from->integer.powerpc64_regs.gpr[5];
        to->Gpr6  = from->integer.powerpc64_regs.gpr[6];
        to->Gpr7  = from->integer.powerpc64_regs.gpr[7];
        to->Gpr8  = from->integer.powerpc64_regs.gpr[8];
        to->Gpr9  = from->integer.powerpc64_regs.gpr[9];
        to->Gpr10 = from->integer.powerpc64_regs.gpr[10];
        to->Gpr11 = from->integer.powerpc64_regs.gpr[11];
        to->Gpr12 = from->integer.powerpc64_regs.gpr[12];
        to->Gpr13 = from->integer.powerpc64_regs.gpr[13];
        to->Gpr14 = from->integer.powerpc64_regs.gpr[14];
        to->Gpr15 = from->integer.powerpc64_regs.gpr[15];
        to->Gpr16 = from->integer.powerpc64_regs.gpr[16];
        to->Gpr17 = from->integer.powerpc64_regs.gpr[17];
        to->Gpr18 = from->integer.powerpc64_regs.gpr[18];
        to->Gpr19 = from->integer.powerpc64_regs.gpr[19];
        to->Gpr20 = from->integer.powerpc64_regs.gpr[20];
        to->Gpr21 = from->integer.powerpc64_regs.gpr[21];
        to->Gpr22 = from->integer.powerpc64_regs.gpr[22];
        to->Gpr23 = from->integer.powerpc64_regs.gpr[23];
        to->Gpr24 = from->integer.powerpc64_regs.gpr[24];
        to->Gpr25 = from->integer.powerpc64_regs.gpr[25];
        to->Gpr26 = from->integer.powerpc64_regs.gpr[26];
        to->Gpr27 = from->integer.powerpc64_regs.gpr[27];
        to->Gpr28 = from->integer.powerpc64_regs.gpr[28];
        to->Gpr29 = from->integer.powerpc64_regs.gpr[29];
        to->Gpr30 = from->integer.powerpc64_regs.gpr[30];
        to->Gpr31 = from->integer.powerpc64_regs.gpr[31];
        to->Xer   = from->integer.powerpc64_regs.xer;
        to->Cr    = from->integer.powerpc64_regs.cr;
    }
    if (from->flags & SERVER_CTX_FLOATING_POINT)
    {
        to->ContextFlags |= CONTEXT_FLOATING_POINT;
        to->Fpr0  = from->fp.powerpc64_regs.fpr[0];
        to->Fpr1  = from->fp.powerpc64_regs.fpr[1];
        to->Fpr2  = from->fp.powerpc64_regs.fpr[2];
        to->Fpr3  = from->fp.powerpc64_regs.fpr[3];
        to->Fpr4  = from->fp.powerpc64_regs.fpr[4];
        to->Fpr5  = from->fp.powerpc64_regs.fpr[5];
        to->Fpr6  = from->fp.powerpc64_regs.fpr[6];
        to->Fpr7  = from->fp.powerpc64_regs.fpr[7];
        to->Fpr8  = from->fp.powerpc64_regs.fpr[8];
        to->Fpr9  = from->fp.powerpc64_regs.fpr[9];
        to->Fpr10 = from->fp.powerpc64_regs.fpr[10];
        to->Fpr11 = from->fp.powerpc64_regs.fpr[11];
        to->Fpr12 = from->fp.powerpc64_regs.fpr[12];
        to->Fpr13 = from->fp.powerpc64_regs.fpr[13];
        to->Fpr14 = from->fp.powerpc64_regs.fpr[14];
        to->Fpr15 = from->fp.powerpc64_regs.fpr[15];
        to->Fpr16 = from->fp.powerpc64_regs.fpr[16];
        to->Fpr17 = from->fp.powerpc64_regs.fpr[17];
        to->Fpr18 = from->fp.powerpc64_regs.fpr[18];
        to->Fpr19 = from->fp.powerpc64_regs.fpr[19];
        to->Fpr20 = from->fp.powerpc64_regs.fpr[20];
        to->Fpr21 = from->fp.powerpc64_regs.fpr[21];
        to->Fpr22 = from->fp.powerpc64_regs.fpr[22];
        to->Fpr23 = from->fp.powerpc64_regs.fpr[23];
        to->Fpr24 = from->fp.powerpc64_regs.fpr[24];
        to->Fpr25 = from->fp.powerpc64_regs.fpr[25];
        to->Fpr26 = from->fp.powerpc64_regs.fpr[26];
        to->Fpr27 = from->fp.powerpc64_regs.fpr[27];
        to->Fpr28 = from->fp.powerpc64_regs.fpr[28];
        to->Fpr29 = from->fp.powerpc64_regs.fpr[29];
        to->Fpr30 = from->fp.powerpc64_regs.fpr[30];
        to->Fpr31 = from->fp.powerpc64_regs.fpr[31];
        to->Fpscr = from->fp.powerpc64_regs.fpscr;
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *              NtSetContextThread  (NTDLL.@)
 *              ZwSetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetContextThread( HANDLE handle, const CONTEXT *context )
{
    NTSTATUS ret;
    BOOL self;
    context_t server_context;

    context_to_server( &server_context, context );
    ret = set_thread_context( handle, &server_context, &self );
    if (self && ret == STATUS_SUCCESS)
    {
        ppc64_thread_data()->syscall_frame = NULL;
        set_cpu_context( context );
    }
    return ret;
}


/***********************************************************************
 *              NtGetContextThread  (NTDLL.@)
 *              ZwGetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtGetContextThread( HANDLE handle, CONTEXT *context )
{
    NTSTATUS ret;
    struct syscall_frame *frame = ppc64_thread_data()->syscall_frame;
    DWORD needed_flags = context->ContextFlags & ~CONTEXT_POWERPC64;
    BOOL self = (handle == GetCurrentThread());

    FIXME("semi stub\n");

    if (!self)
    {
        context_t server_context;
        unsigned int server_flags = get_server_context_flags( context->ContextFlags );

        if ((ret = get_thread_context( handle, &server_context, server_flags, &self ))) return ret;
        if ((ret = context_from_server( context, &server_context ))) return ret;
        needed_flags &= ~context->ContextFlags;
    }

    if (self)
    {
        if (needed_flags & CONTEXT_INTEGER)
        {
            context->Gpr0  = 0;
            context->Gpr3  = 0;
            context->Gpr4  = 0;
            context->Gpr5  = 0;
            context->Gpr6  = 0;
            context->Gpr7  = 0;
            context->Gpr8  = 0;
            context->Gpr9  = 0;
            context->Gpr10 = 0;
            context->Gpr11 = 0;
            context->Gpr12 = 0;
            context->Gpr13 = 0;
            context->Gpr14 = frame->r14;
            context->Gpr15 = frame->r15;
            context->Gpr16 = frame->r16;
            context->Gpr17 = frame->r17;
            context->Gpr18 = frame->r18;
            context->Gpr19 = frame->r19;
            context->Gpr20 = frame->r20;
            context->Gpr21 = frame->r21;
            context->Gpr22 = frame->r22;
            context->Gpr23 = frame->r23;
            context->Gpr24 = frame->r24;
            context->Gpr25 = frame->r25;
            context->Gpr26 = frame->r26;
            context->Gpr27 = frame->r27;
            context->Gpr28 = frame->r28;
            context->Gpr29 = frame->r29;
            context->Gpr30 = frame->r30;
            context->Gpr31 = frame->r31;
            context->ContextFlags |= CONTEXT_INTEGER;
        }
        if (needed_flags & CONTEXT_CONTROL)
        {
            context->Gpr1   = (DWORD64)&frame->thunk_r1;
            context->Gpr2   = (DWORD64)&frame->thunk_r2;
            context->Lr     = frame->ret_addr;
            context->Iar    = frame->thunk_addr;
            context->ContextFlags |= CONTEXT_CONTROL;
        }
        if (needed_flags & CONTEXT_FLOATING_POINT) FIXME( "floating point not supported\n" );
        if (needed_flags & CONTEXT_DEBUG_REGISTERS) FIXME( "debug registers not supported\n" );
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           setup_exception
 *
 * Modify the signal context to call the exception raise function.
 */
static void setup_exception( ucontext_t *sigcontext, EXCEPTION_RECORD *rec )
{
    struct
    {
        void   *r1;
        ULONG64 cr;
        ULONG64 lr;
        ULONG64 r2;
        CONTEXT          context;
        EXCEPTION_RECORD rec;
    } *stack;

    void *stack_ptr = (void *)(GPR_sig(1, sigcontext) & ~15);
    CONTEXT context;
    NTSTATUS status;

    rec->ExceptionAddress = (void *)IAR_sig(sigcontext);
    save_context( &context, sigcontext );
    save_fpu( &context, sigcontext );
    if (rec->ExceptionCode == EXCEPTION_BREAKPOINT) context.Iar += 4;

    status = send_debug_event( rec, &context, TRUE );
    if (status == DBG_CONTINUE || status == DBG_EXCEPTION_HANDLED)
    {
        restore_context( &context, sigcontext );
        restore_fpu( &context, sigcontext );
        return;
    }

    stack = virtual_setup_exception( stack_ptr, sizeof(*stack), rec );
    stack->r1 = stack_ptr;
    stack->cr = CR_sig(sigcontext);
    stack->lr = LR_sig(sigcontext);
    stack->r2 = GPR_sig(2, sigcontext);
    stack->context = context;
    stack->rec = *rec;

    /* now modify the sigcontext to return to the raise function */
    GPR_sig(1, sigcontext) = (ULONG_PTR)stack;
    GPR_sig(3, sigcontext) = (ULONG_PTR)&stack->rec;  /* first arg for KiUserExceptionDispatcher */
    GPR_sig(4, sigcontext) = (ULONG_PTR)&stack->context; /* second arg for KiUserExceptionDispatcher */
    GPR_sig(12, sigcontext) = (ULONG_PTR)pKiUserExceptionDispatcher; /* for ABIv2 */
    LR_sig(sigcontext) = IAR_sig(sigcontext);
    IAR_sig(sigcontext) = (ULONG_PTR)pKiUserExceptionDispatcher;
}

/***********************************************************************
 *           call_user_apc_dispatcher
 */
__ASM_GLOBAL_FUNC( call_user_apc_dispatcher,
                   "mr 14, 3\n\t"               /* context */
                   "mr 15, 4\n\t"               /* ctx */
                   "mr 16, 5\n\t"               /* arg1 */
                   "mr 17, 6\n\t"               /* arg2 */
                   "mr 18, 7\n\t"               /* func */
                   "mr 19, 8\n\t"               /* dispatcher */
                   "lis 12, " __ASM_NAME("NtCurrentTeb") "@highest\n\t"
                   "ori 12, 12, " __ASM_NAME("NtCurrentTeb") "@higher\n\t"
                   "rldicr 12, 12, 32, 31\n\t"
                   "oris 12, 12, " __ASM_NAME("NtCurrentTeb") "@high\n\t"
                   "ori 12, 12, " __ASM_NAME("NtCurrentTeb") "@l\n\t"
                   "mtctr 12\n\t"
                   "bctrl\n\t"
                   "ori 0, 0, 0\n\t"
                   "ld 20, 0x2f8(3)\n\t"        /* ppc64_thread_data()->syscall_frame */
                   "cmpdi cr7, 20, 0\n\t"
                   "beq cr7, 1f\n\t"
                   "ld 3, 272(14)\n\t"          /* context.Gpr1 */
                   "subi 3, 3, 920\n\t"         /* sizeof(CONTEXT)=664 + offsetof(frame,thunk_r1)=256 */
                   "li 9, 0\n\t"
                   "std 9, 0(20)\n\t"
                   "mr 1, 3\n\t"
                   "b 2f\n"
                   "1:\tld 3, 0(20)\n\t"
                   "subi 1, 3, 0x298\n\t"       /* sizeof(CONTEXT)=664 */
                   "lis 0, 0x80\n\t"            /* context.ContextFlags = CONTEXT_FULL */
                   "ori 0, 0, 7\n\t"
                   "std 0, 568(1)\n\t"
                   "li 3, -2\n\t"               /* -2 == ~1 == GetCurrentThread() */
                   "mr 4, 1\n\t"
                   "lis 12, " __ASM_NAME("NtGetContextThread") "@highest\n\t"
                   "ori 12, 12, " __ASM_NAME("NtGetContextThread") "@higher\n\t"
                   "rldicr 12, 12, 32, 31\n\t"
                   "oris 12, 12, " __ASM_NAME("NtGetContextThread") "@high\n\t"
                   "ori 12, 12, " __ASM_NAME("NtGetContextThread") "@l\n\t"
                   "mtctr 12\n\t"
                   "bctrl\n\t"
                   "ori 0, 0, 0\n\t"
                   "li 0, 0xc0\n\t"
                   "std 0, 288(1)\n\t"         /* context.Gpr3 = STATUS_USER_APC */
                   "li 0, 0\n\t"
                   "std 0, 0(20)\n\t"
                   "mr 3, 1\n"
                   "2:\tld 0, 552(3)\n\t"       /* context.Lr */
                   "mtlr 0\n\t"
                   "mr 4, 15\n\t"               /* ctx */
                   "mr 5, 16\n\t"               /* arg1 */
                   "mr 6, 17\n\t"               /* arg2 */
                   "mr 7, 18\n\t"               /* func */
                   "mr 12, 19\n\t"
                   "mtctr 12\n\t"
                   "bctr\n\t"
                   "ori 0, 0, 0" )

/***********************************************************************
 *           call_raise_user_exception_dispatcher
 */
__ASM_GLOBAL_FUNC( call_raise_user_exception_dispatcher,
                   "mr 5, 3\n\t"  /* dispatcher */
                   "lis 12, " __ASM_NAME("call_user_exception_dispatcher") "@highest\n\t"
                   "ori 12, 12, " __ASM_NAME("call_user_exception_dispatcher") "@higher\n\t"
                   "rldicr 12, 12, 32, 31\n\t"
                   "oris 12, 12, " __ASM_NAME("call_user_exception_dispatcher") "@high\n\t"
                   "ori 12, 12, " __ASM_NAME("call_user_exception_dispatcher") "@l\n\t"
                   "mtctr 12\n\t"
                   "bctr\n\t"
                   "ori 0, 0, 0" )


/***********************************************************************
 *           call_user_exception_dispatcher
 */
__ASM_GLOBAL_FUNC( call_user_exception_dispatcher,
                   "mr 14, 3\n\t"
                   "mr 15, 4\n\t"
                   "mr 16, 5\n\t"
                   "lis 12, " __ASM_NAME("NtCurrentTeb") "@highest\n\t"
                   "ori 12, 12, " __ASM_NAME("NtCurrentTeb") "@higher\n\t"
                   "rldicr 12, 12, 32, 31\n\t"
                   "oris 12, 12, " __ASM_NAME("NtCurrentTeb") "@high\n\t"
                   "ori 12, 12, " __ASM_NAME("NtCurrentTeb") "@l\n\t"
                   "mtctr 12\n\t"
                   "bctrl\n\t"
                   "ori 0, 0, 0\n\t"
                   "ld 8, 0x2f8(3)\n\t"             /* ppc64_thread_data()->syscall_frame */
                   "li 5, 0\n\t"
                   "std 5, 0x2f8(3)\n\t"
                   "mr 3, 14\n\t"
                   "mr 4, 15\n\t"
                   "mr 5, 16\n\t"
                   "ld 14,  96(8)\n\t"
                   "ld 15, 104(8)\n\t"
                   "ld 16, 112(8)\n\t"
                   "ld 17, 120(8)\n\t"
                   "ld 18, 128(8)\n\t"
                   "ld 19, 136(8)\n\t"
                   "ld 20, 144(8)\n\t"
                   "ld 21, 152(8)\n\t"
                   "ld 22, 160(8)\n\t"
                   "ld 23, 168(8)\n\t"
                   "ld 24, 176(8)\n\t"
                   "ld 25, 184(8)\n\t"
                   "ld 26, 192(8)\n\t"
                   "ld 27, 200(8)\n\t"
                   "ld 28, 208(8)\n\t"
                   "ld 29, 216(8)\n\t"
                   "ld 20, 224(8)\n\t"
                   "ld 31, 232(8)\n\t"
                   "addi 1, 8, 240\n\t"
                   "mr 12, 5\n\t"
                   "mtctr 12\n\t"
                   "bctr\n\t"
                   "ori 0, 0, 0" )

/***********************************************************************
 *           handle_syscall_fault
 *
 * Handle a page fault happening during a system call.
 */
static BOOL handle_syscall_fault( ucontext_t *context, EXCEPTION_RECORD *rec )
{
    struct syscall_frame *frame = ppc64_thread_data()->syscall_frame;
    __WINE_FRAME *wine_frame = (__WINE_FRAME *)NtCurrentTeb()->Tib.ExceptionList;
    DWORD i;

    if (!frame) return FALSE;

    TRACE( "code=%x flags=%x addr=%p pc=%p tid=%04x\n",
           rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress,
           (void *)IAR_sig(context), GetCurrentThreadId() );
    for (i = 0; i < rec->NumberParameters; i++)
        TRACE( " info[%d]=%016lx\n", i, rec->ExceptionInformation[i] );

    TRACE("  r0=%016lx  r1=%016lx  r2=%016lx  r3=%016lx\n",
          (DWORD64)GPR_sig(0, context), (DWORD64)GPR_sig(1, context),
          (DWORD64)GPR_sig(2, context), (DWORD64)GPR_sig(3, context) );
    TRACE("  r4=%016lx  r5=%016lx  r6=%016lx  r7=%016lx\n",
          (DWORD64)GPR_sig(4, context), (DWORD64)GPR_sig(5, context),
          (DWORD64)GPR_sig(6, context), (DWORD64)GPR_sig(7, context) );
    TRACE("  r8=%016lx  r9=%016lx r10=%016lx r11=%016lx\n",
          (DWORD64)GPR_sig(8, context), (DWORD64)GPR_sig(9, context),
          (DWORD64)GPR_sig(10, context), (DWORD64)GPR_sig(11, context) );
    TRACE(" r12=%016lx r13=%016lx r14=%016lx r15=%016lx\n",
          (DWORD64)GPR_sig(12, context), (DWORD64)GPR_sig(13, context),
          (DWORD64)GPR_sig(14, context), (DWORD64)GPR_sig(15, context) );
    TRACE(" r16=%016lx r17=%016lx r18=%016lx r19=%016lx\n",
          (DWORD64)GPR_sig(16, context), (DWORD64)GPR_sig(17, context),
          (DWORD64)GPR_sig(18, context), (DWORD64)GPR_sig(19, context) );
    TRACE(" r20=%016lx r21=%016lx r22=%016lx r23=%016lx\n",
          (DWORD64)GPR_sig(20, context), (DWORD64)GPR_sig(21, context),
          (DWORD64)GPR_sig(22, context), (DWORD64)GPR_sig(23, context) );
    TRACE(" r24=%016lx r25=%016lx r26=%016lx r27=%016lx\n",
          (DWORD64)GPR_sig(24, context), (DWORD64)GPR_sig(25, context),
          (DWORD64)GPR_sig(26, context), (DWORD64)GPR_sig(27, context) );
    TRACE(" r28=%016lx r29=%016lx r30=%016lx r31=%016lx\n",
          (DWORD64)GPR_sig(28, context), (DWORD64)GPR_sig(29, context),
          (DWORD64)GPR_sig(30, context), (DWORD64)GPR_sig(31, context) );
    TRACE(" ctr=%016lx  fpscr=%016lx  lr=%016lx\n",
          (DWORD64)CTR_sig(context), (DWORD64)FPSCR_sig(context), (DWORD64)LR_sig(context) );

    if ((char *)wine_frame < (char *)frame)
    {
        TRACE( "returning to handler\n" );
        GPR_sig(3, context) = (ULONG_PTR)&wine_frame->jmp;
        GPR_sig(4, context) = 1;
        GPR_sig(12, context) = (ULONG_PTR)__wine_longjmp; /* for ABIv2 */
        IAR_sig(context)     = (ULONG_PTR)__wine_longjmp;
    }
    else
    {
        TRACE( "returning to user mode ip=%p ret=%08x\n", (void *)frame->ret_addr, rec->ExceptionCode );
        GPR_sig(1, context)  = (ULONG_PTR)&frame->thunk_r1;
        GPR_sig(2, context)  = frame->thunk_r2;
        GPR_sig(3, context)  = rec->ExceptionCode;
        GPR_sig(14, context) = frame->r14;
        GPR_sig(15, context) = frame->r15;
        GPR_sig(16, context) = frame->r16;
        GPR_sig(17, context) = frame->r17;
        GPR_sig(18, context) = frame->r18;
        GPR_sig(19, context) = frame->r19;
        GPR_sig(20, context) = frame->r20;
        GPR_sig(21, context) = frame->r21;
        GPR_sig(22, context) = frame->r22;
        GPR_sig(23, context) = frame->r23;
        GPR_sig(24, context) = frame->r24;
        GPR_sig(25, context) = frame->r25;
        GPR_sig(26, context) = frame->r26;
        GPR_sig(27, context) = frame->r27;
        GPR_sig(28, context) = frame->r28;
        GPR_sig(29, context) = frame->r29;
        GPR_sig(30, context) = frame->r30;
        GPR_sig(31, context) = frame->r31;
        LR_sig(context)      = frame->ret_addr;
        IAR_sig(context)     = frame->thunk_addr;
        ppc64_thread_data()->syscall_frame = NULL;
    }
    return TRUE;
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

    rec.NumberParameters = 2;
    rec.ExceptionInformation[0] = (REG_sig(dsisr, context) & 0x02000000) != 0;
    rec.ExceptionInformation[1] = (ULONG_PTR)siginfo->si_addr;
    rec.ExceptionCode = virtual_handle_fault( siginfo->si_addr, rec.ExceptionInformation[0],
                                              (void *)GPR_sig(1, context) );
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

    switch (siginfo->si_code)
    {
    case TRAP_TRACE:
        rec.ExceptionCode = EXCEPTION_SINGLE_STEP;
        break;
    case TRAP_BRKPT:
    default:
        rec.ExceptionCode = EXCEPTION_BREAKPOINT;
        rec.NumberParameters = 1;
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
    EXCEPTION_RECORD rec = { CONTROL_C_EXIT };

    setup_exception( sigcontext, &rec );
}


/**********************************************************************
 *		abrt_handler
 *
 * Handler for SIGABRT.
 */
static void abrt_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { EXCEPTION_WINE_ASSERTION, EH_NONCONTINUABLE };

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

    save_context( &context, sigcontext );
    wait_suspend( &context );
    restore_context( &context, sigcontext );
    restore_fpu( &context, sigcontext );
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
    pthread_key_create( &teb_key, NULL );
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
 *		signal_init_thread
 */
void signal_init_thread( TEB *teb )
{
    pthread_setspecific( teb_key, teb );
}


/**********************************************************************
 *		signal_init_process
 */
void signal_init_process(void)
{
    struct sigaction sig_act;

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
 *           init_thread_context
 */
static void init_thread_context( CONTEXT *context, LPTHREAD_START_ROUTINE entry, void *arg, TEB *teb )
{
    context->Gpr1   = (DWORD64)teb->Tib.StackBase - 32; /* add initial frame size */
    context->Gpr3   = (DWORD64)entry;
    context->Gpr4   = (DWORD64)arg;
    context->Gpr12  = (DWORD64)pRtlUserThreadStart;
    context->Iar    = (DWORD64)pRtlUserThreadStart;
}

/***********************************************************************
 *           get_initial_context
 */
PCONTEXT DECLSPEC_HIDDEN get_initial_context( LPTHREAD_START_ROUTINE entry, void *arg,
                                              BOOL suspend, TEB *teb )
{
    CONTEXT *ctx;

    if (suspend)
    {
        CONTEXT context = { CONTEXT_ALL };

        init_thread_context( &context, entry, arg, teb );
        wait_suspend( &context );
        ctx = (CONTEXT *)((ULONG_PTR)context.Gpr1 & ~15) - 1;
        *ctx = context;
    }
    else
    {
        ctx = (CONTEXT *)teb->Tib.StackBase - 1;
        init_thread_context( ctx, entry, arg, teb );
    }
    pthread_sigmask( SIG_UNBLOCK, &server_block_set, NULL );
    ctx->ContextFlags = CONTEXT_FULL;
    return ctx;
}


/***********************************************************************
 *           signal_start_thread
 */
/* extern void DECLSPEC_NORETURN signal_start_thread( PRTL_THREAD_START_ROUTINE entry, void *arg,
-                                                   BOOL suspend, void *relay, void *thunk, TEB *teb ) DECLSPEC_HIDDEN;
+                                                   BOOL suspend, void *thunk, TEB *teb ) DECLSPEC_HIDDEN;*/
__ASM_GLOBAL_FUNC( signal_start_thread,
                   "mflr 0\n\t"
                   "std 0, 16(1)\n\t"
                   /* store exit frame */
                   "addi 9, 7, 0x200\n\t"
                   "addi 9, 9, 0x0f0\n\t"
                   "std 1, 0(9)\n\t" /* ppc64_thread_data()->exit_frame */
                   /* switch to thread stack */
                   "ld 9, 8(7)\n\t"  /* teb->Tib.StackBase */
                   "subi 1, 9, 0x1000\n\t"
                   /* attach dlls */
                   "std 6, 32(1)\n\t"
                   "mr 6, 7\n\t"
                   "lis 12, " __ASM_NAME("get_initial_context") "@highest\n\t"
                   "ori 12, 12, " __ASM_NAME("get_initial_context") "@higher\n\t"
                   "rldicr 12, 12, 32, 31\n\t"
                   "oris 12, 12, " __ASM_NAME("get_initial_context") "@high\n\t"
                   "ori 12, 12, " __ASM_NAME("get_initial_context") "@l\n\t"
                   "mtctr 12\n\t"
                   "bctrl\n\t"
                   "ori 0, 0, 0\n\t"
                   "ld 12, 32(1)\n\t"
                   "mtctr 12\n\t"
                   "li 4, 0\n\t"
                   "mtlr 4\n\t"
                   "bctr\n\t"
                   "ori 0, 0, 0" )

extern void DECLSPEC_NORETURN call_thread_exit_func( int status, void (*func)(int), TEB *teb );
__ASM_GLOBAL_FUNC( call_thread_exit_func,
                   "li 7, 0\n\t"
                   "addi 8, 5, 0x200\n\t"
                   "addi 8, 8, 0x0f0\n\t"
                   "ld 6, 0(8)\n\t"
                   "std 7, 0(8)\n\t"
                   "cmpdi cr7, 6, 0\n\t"
                   "beq cr7, 1f\n\t"
                   "mr 1, 6\n"
                   "1:\tmr 12, 4\n\t"
                   "mtctr 12\n\t"
                   "bctr\n\t"
                   "ori 0, 0, 0" )

/***********************************************************************
 *           signal_exit_thread
 */
void signal_exit_thread( int status, void (*func)(int) )
{
    call_thread_exit_func( status, func, NtCurrentTeb() );
}


/**********************************************************************
 *           NtCurrentTeb   (NTDLL.@)
 */
TEB * WINAPI NtCurrentTeb(void)
{
    return pthread_getspecific( teb_key );
}

#endif  /* __aarch64__ */
