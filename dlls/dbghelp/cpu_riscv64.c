/*
 * File cpu_riscv64.c
 *
 * Copyright (C) 2023 André Zwing
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

#include <assert.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "dbghelp_private.h"
#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp);

static BOOL riscv64_get_addr(HANDLE hThread, const CONTEXT* ctx,
                           enum cpu_addr ca, ADDRESS64* addr)
{
    addr->Mode    = AddrModeFlat;
    addr->Segment = 0; /* don't need segment */
    switch (ca)
    {
#ifdef __riscv64__
    case cpu_addr_pc:    addr->Offset = ctx->Pc; return TRUE;
    case cpu_addr_stack: addr->Offset = ctx->Sp; return TRUE;
    case cpu_addr_frame: addr->Offset = ctx->Fp; return TRUE;
#endif
    default: addr->Mode = -1;
        return FALSE;
    }
}

#ifdef __riscv64__
enum st_mode {stm_start, stm_riscv64, stm_done};

/* indexes in Reserved array */
#define __CurrentModeCount      0

#define curr_mode   (frame->Reserved[__CurrentModeCount] & 0x0F)
#define curr_count  (frame->Reserved[__CurrentModeCount] >> 4)

#define set_curr_mode(m) {frame->Reserved[__CurrentModeCount] &= ~0x0F; frame->Reserved[__CurrentModeCount] |= (m & 0x0F);}
#define inc_curr_count() (frame->Reserved[__CurrentModeCount] += 0x10)

/* fetch_next_frame()
 *
 * modify (at least) context.Pc using unwind information
 * either out of debug info (dwarf), or simple X1 trace
 */
static BOOL fetch_next_frame(struct cpu_stack_walk* csw, union ctx *pcontext,
    DWORD_PTR curr_pc)
{
    DWORD64 xframe;
    CONTEXT *context = &pcontext->ctx;
    DWORD_PTR oldReturn = context->Ra;

    if (dwarf2_virtual_unwind(csw, curr_pc, pcontext, &xframe))
    {
        context->Fp = xframe;
        context->Pc = oldReturn;
        return TRUE;
    }

    if (context->Pc == context->Ra) return FALSE;
    context->Pc = oldReturn;

    return TRUE;
}

static BOOL riscv64_stack_walk(struct cpu_stack_walk *csw, STACKFRAME64 *frame,
    union ctx *context)
{
    unsigned deltapc = curr_count <= 1 ? 0 : 4;

    /* sanity check */
    if (curr_mode >= stm_done) return FALSE;

    TRACE("Enter: PC=%s Frame=%s Return=%s Stack=%s Mode=%s Count=%I64u\n",
          wine_dbgstr_addr(&frame->AddrPC),
          wine_dbgstr_addr(&frame->AddrFrame),
          wine_dbgstr_addr(&frame->AddrReturn),
          wine_dbgstr_addr(&frame->AddrStack),
          curr_mode == stm_start ? "start" : "RISCV64",
          curr_count);

    if (curr_mode == stm_start)
    {
        /* Init done */
        set_curr_mode(stm_riscv64);
        frame->AddrReturn.Mode = frame->AddrStack.Mode = AddrModeFlat;
        /* don't set up AddrStack on first call. Either the caller has set it up, or
         * we will get it in the next frame
         */
        memset(&frame->AddrBStore, 0, sizeof(frame->AddrBStore));
    }
    else
    {
        if (context->ctx.Sp != frame->AddrStack.Offset) FIXME("inconsistent Stack Pointer\n");
        if (context->ctx.Pc != frame->AddrPC.Offset) FIXME("inconsistent Program Counter\n");

        if (frame->AddrReturn.Offset == 0) goto done_err;
        if (!fetch_next_frame(csw, context, frame->AddrPC.Offset - deltapc))
            goto done_err;
    }

    memset(&frame->Params, 0, sizeof(frame->Params));

    /* set frame information */
    frame->AddrStack.Offset = context->ctx.Sp;
    frame->AddrReturn.Offset = context->ctx.Ra;
    frame->AddrFrame.Offset = context->ctx.Fp;
    frame->AddrPC.Offset = context->ctx.Pc;

    frame->Far = TRUE;
    frame->Virtual = TRUE;
    inc_curr_count();

    TRACE("Leave: PC=%s Frame=%s Return=%s Stack=%s Mode=%s Count=%I64u FuncTable=%p\n",
          wine_dbgstr_addr(&frame->AddrPC),
          wine_dbgstr_addr(&frame->AddrFrame),
          wine_dbgstr_addr(&frame->AddrReturn),
          wine_dbgstr_addr(&frame->AddrStack),
          curr_mode == stm_start ? "start" : "RISCV64",
          curr_count,
          frame->FuncTableEntry);

    return TRUE;
done_err:
    set_curr_mode(stm_done);
    return FALSE;
}
#else
static BOOL riscv64_stack_walk(struct cpu_stack_walk* csw, STACKFRAME64 *frame,
    union ctx *ctx)
{
    return FALSE;
}
#endif

static unsigned riscv64_map_dwarf_register(unsigned regno, const struct module* module, BOOL eh_frame)
{
    FIXME("NIY\n");
    return 0;
}

static void *riscv64_fetch_context_reg(union ctx *pctx, unsigned regno, unsigned *size)
{
    FIXME("NIY\n");
    return NULL;
}

static const char* riscv64_fetch_regname(unsigned regno)
{
    FIXME("NIY\n");
    return NULL;
}

static BOOL riscv64_fetch_minidump_thread(struct dump_context* dc, unsigned index, unsigned flags, const CONTEXT* ctx)
{
    if (ctx->ContextFlags && (flags & ThreadWriteInstructionWindow))
    {
        /* FIXME: crop values across module boundaries, */
#ifdef __aarch64__
        ULONG64 base = ctx->pc <= 0x80 ? 0 : ctx->pc - 0x80;
        minidump_add_memory_block(dc, base, ctx->pc + 0x80 - base, 0);
#endif
    }

    return TRUE;
}

static BOOL riscv64_fetch_minidump_module(struct dump_context* dc, unsigned index, unsigned flags)
{
    /* FIXME: actually, we should probably take care of FPO data, unless it's stored in
     * function table minidump stream
     */
    return FALSE;
}

DECLSPEC_HIDDEN struct cpu cpu_riscv64 = {
    IMAGE_FILE_MACHINE_RISCV64,
    8,
    CV_REG_NONE, /* FIXME */
    riscv64_get_addr,
    riscv64_stack_walk,
    NULL,
    riscv64_map_dwarf_register,
    riscv64_fetch_context_reg,
    riscv64_fetch_regname,
    riscv64_fetch_minidump_thread,
    riscv64_fetch_minidump_module,
};
