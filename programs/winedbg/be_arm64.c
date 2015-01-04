/*
 * Debugger ARM64 specific functions
 *
 * Copyright 2010-2013 André Hentschel
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

#include "debugger.h"

#if defined(__mips__)

static BOOL be_arm64_get_addr(HANDLE hThread, const CONTEXT* ctx,
                              enum be_cpu_addr bca, ADDRESS64* addr)
{

    return FALSE;
}

static BOOL be_arm64_get_register_info(int regno, enum be_cpu_addr* kind)
{

    return FALSE;
}

static void be_arm64_single_step(CONTEXT* ctx, BOOL enable)
{
    dbg_printf("be_arm64_single_step: not done\n");
}

static void be_arm64_print_context(HANDLE hThread, const CONTEXT* ctx, int all_regs)
{
}

static void be_arm64_print_segment_info(HANDLE hThread, const CONTEXT* ctx)
{
}

static struct dbg_internal_var be_arm64_ctx[] =
{
    {0,                   NULL,     0,                                         dbg_itype_none}
};

static BOOL be_arm64_is_step_over_insn(const void* insn)
{
    dbg_printf("be_arm64_is_step_over_insn: not done\n");
    return FALSE;
}

static BOOL be_arm64_is_function_return(const void* insn)
{
    dbg_printf("be_arm64_is_function_return: not done\n");
    return FALSE;
}

static BOOL be_arm64_is_break_insn(const void* insn)
{
    dbg_printf("be_arm64_is_break_insn: not done\n");
    return FALSE;
}

static BOOL be_arm64_is_func_call(const void* insn, ADDRESS64* callee)
{
    return FALSE;
}

static BOOL be_arm64_is_jump(const void* insn, ADDRESS64* jumpee)
{
    return FALSE;
}

static BOOL be_arm64_insert_Xpoint(HANDLE hProcess, const struct be_process_io* pio,
                                   CONTEXT* ctx, enum be_xpoint_type type,
                                   void* addr, unsigned long* val, unsigned size)
{
    SIZE_T              sz;

    switch (type)
    {
    case be_xpoint_break:
        if (!size) return FALSE;
        if (!pio->read(hProcess, addr, val, 4, &sz) || sz != 4) return FALSE;
    default:
        dbg_printf("Unknown/unsupported bp type %c\n", type);
        return FALSE;
    }
    return TRUE;
}

static BOOL be_arm64_remove_Xpoint(HANDLE hProcess, const struct be_process_io* pio,
                                   CONTEXT* ctx, enum be_xpoint_type type,
                                   void* addr, unsigned long val, unsigned size)
{
    SIZE_T              sz;

    switch (type)
    {
    case be_xpoint_break:
        if (!size) return FALSE;
        if (!pio->write(hProcess, addr, &val, 4, &sz) || sz == 4) return FALSE;
        break;
    default:
        dbg_printf("Unknown/unsupported bp type %c\n", type);
        return FALSE;
    }
    return TRUE;
}

static BOOL be_arm64_is_watchpoint_set(const CONTEXT* ctx, unsigned idx)
{
    dbg_printf("be_arm64_is_watchpoint_set: not done\n");
    return FALSE;
}

static void be_arm64_clear_watchpoint(CONTEXT* ctx, unsigned idx)
{
    dbg_printf("be_arm64_clear_watchpoint: not done\n");
}

static int be_arm64_adjust_pc_for_break(CONTEXT* ctx, BOOL way)
{

}

static BOOL be_arm64_fetch_integer(const struct dbg_lvalue* lvalue, unsigned size,
                                   BOOL is_signed, LONGLONG* ret)
{
    if (size != 1 && size != 2 && size != 4 && size != 8) return FALSE;

    memset(ret, 0, sizeof(*ret)); /* clear unread bytes */
    /* FIXME: this assumes that debuggee and debugger use the same
     * integral representation
     */
    if (!memory_read_value(lvalue, size, ret)) return FALSE;

    /* propagate sign information */
    if (is_signed && size < 8 && (*ret >> (size * 8 - 1)) != 0)
    {
        ULONGLONG neg = -1;
        *ret |= neg << (size * 8);
    }
    return TRUE;
}

static BOOL be_arm64_fetch_float(const struct dbg_lvalue* lvalue, unsigned size,
                                 long double* ret)
{
    char tmp[sizeof(long double)];

    /* FIXME: this assumes that debuggee and debugger use the same
     * representation for reals
     */
    if (!memory_read_value(lvalue, size, tmp)) return FALSE;

    if (size == sizeof(float)) *ret = *(float*)tmp;
    else if (size == sizeof(double)) *ret = *(double*)tmp;
    else if (size == sizeof(long double)) *ret = *(long double*)tmp;
    else return FALSE;

    return TRUE;
}

static BOOL be_arm64_store_integer(const struct dbg_lvalue* lvalue, unsigned size,
                                   BOOL is_signed, LONGLONG val)
{
    /* this is simple if we're on a little endian CPU */
    return memory_write_value(lvalue, size, &val);
}

void be_arm64_disasm_one_insn(ADDRESS64 *addr, int display)
{
    dbg_printf("be_arm64_disasm_one_insn: not done\n");
}

struct backend_cpu be_arm64 =
{
    IMAGE_FILE_MACHINE_ARM64,
    8,
    be_cpu_linearize,
    be_cpu_build_addr,
    be_arm64_get_addr,
    be_arm64_get_register_info,
    be_arm64_single_step,
    be_arm64_print_context,
    be_arm64_print_segment_info,
    be_arm64_ctx,
    be_arm64_is_step_over_insn,
    be_arm64_is_function_return,
    be_arm64_is_break_insn,
    be_arm64_is_func_call,
    be_arm64_is_jump,
    be_arm64_disasm_one_insn,
    be_arm64_insert_Xpoint,
    be_arm64_remove_Xpoint,
    be_arm64_is_watchpoint_set,
    be_arm64_clear_watchpoint,
    be_arm64_adjust_pc_for_break,
    be_arm64_fetch_integer,
    be_arm64_fetch_float,
    be_arm64_store_integer,
};
#endif
