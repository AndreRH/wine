/*
 * File cpu_risc64.c
 *
 * Copyright (C) 2009 Eric Pouech
 * Copyright (C) 2018 André Hentschel
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

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
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
#ifdef __risc
    case cpu_addr_pc:    addr->Offset = ctx->Pc;  return TRUE;
    case cpu_addr_stack: addr->Offset = ctx->Sp;  return TRUE;
    case cpu_addr_frame: addr->Offset = ctx->Fp; return TRUE;
#endif
    default: addr->Mode = -1;
        return FALSE;
    }
}

static BOOL riscv64_stack_walk(struct cpu_stack_walk* csw, LPSTACKFRAME64 frame, CONTEXT* context)
{
    FIXME("not done\n");
    return FALSE;
}

static unsigned riscv64_map_dwarf_register(unsigned regno, BOOL eh_frame)
{
    FIXME("not done\n");
    return 0;
}

static void* riscv64_fetch_context_reg(CONTEXT* ctx, unsigned regno, unsigned* size)
{
    FIXME("NIY\n");
    return NULL;
}

static const char* riscv64_fetch_regname(unsigned regno)
{
    FIXME("Unknown register %x\n", regno);
    return NULL;
}

static BOOL riscv64_fetch_minidump_thread(struct dump_context* dc, unsigned index, unsigned flags, const CONTEXT* ctx)
{
    FIXME("NIY\n");
    return FALSE;
}

static BOOL riscv64_fetch_minidump_module(struct dump_context* dc, unsigned index, unsigned flags)
{
    FIXME("NIY\n");
    return FALSE;
}

DECLSPEC_HIDDEN struct cpu cpu_riscv64 = {
    IMAGE_FILE_MACHINE_ARM64,
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
