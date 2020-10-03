/*
 * File cpu_ppc64.c
 *
 * Copyright (C) 2009 Eric Pouech
 * Copyright (C) 2010-2013 André Hentschel
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

static BOOL ppc64_get_addr(HANDLE hThread, const CONTEXT* ctx,
                           enum cpu_addr ca, ADDRESS64* addr)
{
    return FALSE;
}

#ifdef __powerpc64__

/* indexes in Reserved array */
#define __CurrentModeCount      0

#define curr_mode   (frame->Reserved[__CurrentModeCount] & 0x0F)
#define curr_count  (frame->Reserved[__CurrentModeCount] >> 4)

#define set_curr_mode(m) {frame->Reserved[__CurrentModeCount] &= ~0x0F; frame->Reserved[__CurrentModeCount] |= (m & 0x0F);}
#define inc_curr_count() (frame->Reserved[__CurrentModeCount] += 0x10)

/* fetch_next_frame()
 *
 * modify (at least) context.Pc using unwind information
 * either out of debug info (dwarf), or simple Lr trace
 */
static BOOL fetch_next_frame(struct cpu_stack_walk* csw, union ctx *pcontext,
    DWORD_PTR curr_pc)
{
    FIXME("stub\n");
    return TRUE;
}

static BOOL ppc64_stack_walk(struct cpu_stack_walk *csw, STACKFRAME64 *frame,
    union ctx *context)
{
    FIXME("stub\n");
    return FALSE;
}
#else
static BOOL ppc64_stack_walk(struct cpu_stack_walk* csw, STACKFRAME64 *frame,
    union ctx *ctx)
{
    return FALSE;
}
#endif

static unsigned ppc64_map_dwarf_register(unsigned regno, const struct module* module, BOOL eh_frame)
{
    FIXME("stub\n");
    return 0;
}

static void *ppc64_fetch_context_reg(union ctx *pctx, unsigned regno, unsigned *size)
{
    FIXME("Unknown register %x\n", regno);
    return NULL;
}

static const char* ppc64_fetch_regname(unsigned regno)
{
    FIXME("Unknown register %x\n", regno);
    return NULL;
}

static BOOL ppc64_fetch_minidump_thread(struct dump_context* dc, unsigned index, unsigned flags, const CONTEXT* ctx)
{
    FIXME("stub\n");
    return TRUE;
}

static BOOL ppc64_fetch_minidump_module(struct dump_context* dc, unsigned index, unsigned flags)
{
    FIXME("stub\n");
    return FALSE;
}

DECLSPEC_HIDDEN struct cpu cpu_ppc64 = {
    IMAGE_FILE_MACHINE_POWERPC64,
    8,
    0,
    ppc64_get_addr,
    ppc64_stack_walk,
    NULL,
    ppc64_map_dwarf_register,
    ppc64_fetch_context_reg,
    ppc64_fetch_regname,
    ppc64_fetch_minidump_thread,
    ppc64_fetch_minidump_module,
};
