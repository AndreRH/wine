/*
 * Copyright 2022 André Zwing
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/unixlib.h"


struct emu_run_params
{
    ARM_CONTEXT *c;
};

struct set_prot_params
{
    DWORD64 base;
    DWORD64 length;
    DWORD64 prot;
};

struct invalidate_code_range_params
{
    DWORD64 base;
    DWORD64 length;
};

enum wowarmhw_funcs
{
    unix_attach,
    unix_detach,
    unix_emu_run,
    unix_set_prot,
    unix_invalidate_code_range
};
