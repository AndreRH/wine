/*
 * Copyright 2022-2023 André Zwing
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

#include "config.h"

#include <stdarg.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "unixlib.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wow);

#define ExitProcess(x) exit(x)

static void (*phangover_fex_init)(void);
static void (*phangover_fex_run)(void* teb, I386_CONTEXT* ctx);
static void (*phangover_fex_invalidate_code_range)(uint64_t start, uint64_t length);

static void *emuapi_handle;

static NTSTATUS attach( void *args )
{
    static char default_lib[] = "/opt/libFEXCore.so";
    char *holib;

    MESSAGE("starting FEX based fexcore.dll\n");

    holib = getenv("HOLIB");
    if (!holib)
        holib = default_lib;

    if (!(emuapi_handle = dlopen( holib, RTLD_NOW )))
    {
        FIXME("%s\n", dlerror());
        return STATUS_DLL_NOT_FOUND;
    }

#define LOAD_FUNCPTR(f) if((p##f = dlsym(emuapi_handle, #f)) == NULL) {ERR(#f " %p\n", p##f);return STATUS_ENTRYPOINT_NOT_FOUND;}
    LOAD_FUNCPTR(hangover_fex_init);
    LOAD_FUNCPTR(hangover_fex_run);
    LOAD_FUNCPTR(hangover_fex_invalidate_code_range);
#undef LOAD_FUNCPTR

    phangover_fex_init();

    return STATUS_SUCCESS;
}

static NTSTATUS detach( void *args )
{
    dlclose( emuapi_handle );
    return STATUS_SUCCESS;
}

static inline void *get_wow_teb( TEB *teb )
{
    return teb->WowTebOffset ? (void *)((char *)teb + teb->WowTebOffset) : NULL;
}

static NTSTATUS emu_run( void *args )
{
    const struct emu_run_params *params = args;
    TEB *teb = NtCurrentTeb();
    void *wowteb = get_wow_teb(teb);

    if (!params->c->Eip)
    {
        ERR("Attempting to execute NULL.\n");
        ExitProcess(1);
    }

    phangover_fex_run(wowteb, params->c);
    return 0;
}

static void invalidate_code_range ( void *args )
{
    const struct invalidate_code_range_params *params = args;
    phangover_fex_invalidate_code_range(params->base, params->length);
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    attach,
    detach,
    emu_run,
    invalidate_code_range,
};
