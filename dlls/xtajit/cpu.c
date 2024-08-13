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

#include <string.h>
#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winnls.h"
#include "unixlib.h"
#include "wine/debug.h"
#include "wine/exception.h"

WINE_DEFAULT_DEBUG_CHANNEL(wow);

static unixlib_handle_t emuapi_handle;

static const UINT_PTR page_mask = 0xfff;

#define ROUND_ADDR(addr,mask) ((void *)((UINT_PTR)(addr) & ~(UINT_PTR)(mask)))
#define ROUND_SIZE(addr,size) (((SIZE_T)(size) + ((UINT_PTR)(addr) & page_mask) + page_mask) & ~page_mask)

#define EMUAPI_CALL( func, params ) __wine_unix_call( emuapi_handle, unix_ ## func, params )

static char gdt[1024*9], *idt, *ldt;

NTSTATUS WINAPI emu_run(I386_CONTEXT *c)
{
    struct emu_run_params params = { c, gdt, idt, ldt };
    return EMUAPI_CALL( emu_run, &params );
}

NTSTATUS WINAPI set_prot( DWORD64 base, DWORD64 length, DWORD64 prot )
{
    struct set_prot_params params = { base, length, prot };
    NTSTATUS ret;
    ret = EMUAPI_CALL( set_prot, &params );
    return ret;
}

NTSTATUS WINAPI invalidate_code_range( DWORD64 base, DWORD64 length )
{
    struct invalidate_code_range_params params = { base, length };
    NTSTATUS ret;
    ret = EMUAPI_CALL( invalidate_code_range, &params );
    return ret;
}

NTSTATUS WINAPI Wow64SystemServiceEx( UINT num, UINT *args );

static const UINT16 bopcode = 0x040f;
static const UINT16 unxcode = 0x070f;

static NTSTATUS (WINAPI *p__wine_unix_call_dispatcher)( unixlib_handle_t, unsigned int, void * );

void WINAPI handle_unix_call(I386_CONTEXT *c)
{
    unixlib_handle_t *handle;
    UINT32 *p, p0;
    NTSTATUS ret;

    p = ULongToPtr(c->Esp);
    handle = (void*)&p[1];

    p0 = p[0];
    ret = p__wine_unix_call_dispatcher(*handle, p[3], ULongToPtr(p[4]));
    c->Eip = p0;
    c->Esp += 4+8+4+4; /* ret + args */
    c->Eax = ret;
}

void WINAPI handle_syscall(I386_CONTEXT *c)
{
    NTSTATUS ret;

    ret = Wow64SystemServiceEx(c->Eax, ULongToPtr(c->Esp+8));

    if (ULongToPtr(c->Eip) == &bopcode)
    {
        TRACE("ADAPTING\n");
        c->Eip =* (DWORD*)ULongToPtr(c->Esp);
        c->Esp += 4;
        c->Eax = ret;
    }
    else
    {
        TRACE("NOT ADAPTING !!!\n");
    }
}


/**********************************************************************
 *           BTCpuProcessInit  (xtajit.@)
 */
void WINAPI BTCpuSimulate(void)
{
    I386_CONTEXT *wow_context;
    NTSTATUS ret;

    RtlWow64GetCurrentCpuArea(NULL, (void **)&wow_context, NULL);

    TRACE( "START %p %08lx\n", wow_context, wow_context->Eip );

    ret = emu_run(wow_context);
    if (ret)
    {
        EXCEPTION_RECORD rec;

        ERR("emu_run returned %08lx\n", ret);
        rec.ExceptionCode    = ret;
        rec.ExceptionFlags   = 0;
        rec.ExceptionRecord  = NULL;
        rec.ExceptionAddress = ULongToPtr(wow_context->Eip);
        rec.NumberParameters = 0;
        RtlRaiseException( &rec );
    }

    if (ULongToPtr(wow_context->Eip) == &unxcode)
    {
        /* unix call */
        handle_unix_call(wow_context);
    }
    else if (ULongToPtr(wow_context->Eip) == &bopcode)
    {
        /* sys call */
        handle_syscall(wow_context);
    }
    else
    {
        DWORD arg[] = {0xffffffff, 1};

        ERR("Client crashed\n");

        /* NtTerminateProcess */
        Wow64SystemServiceEx(212, (UINT*)&arg);
        return;
    }
}


/**********************************************************************
 *           BTCpuProcessInit  (xtajit.@)
 */
NTSTATUS WINAPI BTCpuProcessInit(void)
{
    if ((ULONG_PTR)BTCpuProcessInit >> 32)
    {
        ERR( "xtajit loaded above 4G, disabling\n" );
        return STATUS_INVALID_ADDRESS;
    }

    if (!p__wine_unix_call_dispatcher)
    {
        void **pp__wine_unix_call_dispatcher;
        UNICODE_STRING str;
        HMODULE module;

        RtlInitUnicodeString( &str, L"ntdll.dll" );
        LdrGetDllHandle( NULL, 0, &str, &module );
        pp__wine_unix_call_dispatcher = RtlFindExportedRoutineByName( module, "__wine_unix_call_dispatcher" );
        p__wine_unix_call_dispatcher = *pp__wine_unix_call_dispatcher;
    }

    return STATUS_SUCCESS;
}

/**********************************************************************
 *           BTCpuThreadInit  (xtajit.@)
 */
NTSTATUS WINAPI BTCpuThreadInit(void)
{
    idt = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, 255*8 );
    ldt = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, 256*8 );

    return STATUS_SUCCESS;
}

/**********************************************************************
 *           BTCpuGetBopCode  (xtajit.@)
 */
void * WINAPI BTCpuGetBopCode(void)
{
    return (UINT32*)&bopcode;
}

void * WINAPI __wine_get_unix_opcode(void)
{
    return (UINT32*)&unxcode;
}

/**********************************************************************
 *           BTCpuGetContext  (xtajit.@)
 */
NTSTATUS WINAPI BTCpuGetContext( HANDLE thread, HANDLE process, void *unknown, I386_CONTEXT *ctx )
{
    return NtQueryInformationThread( thread, ThreadWow64Context, ctx, sizeof(*ctx), NULL );
}


/**********************************************************************
 *           BTCpuSetContext  (xtajit.@)
 */
NTSTATUS WINAPI BTCpuSetContext( HANDLE thread, HANDLE process, void *unknown, I386_CONTEXT *ctx )
{
    return NtSetInformationThread( thread, ThreadWow64Context, ctx, sizeof(*ctx) );
}


/**********************************************************************
 *           BTCpuResetToConsistentState  (xtajit.@)
 */
NTSTATUS WINAPI BTCpuResetToConsistentState( EXCEPTION_POINTERS *ptrs )
{
    FIXME( "NYI\n" );
    return STATUS_SUCCESS;
}


/**********************************************************************
 *           BTCpuTurboThunkControl  (xtajit.@)
 */
NTSTATUS WINAPI BTCpuTurboThunkControl( ULONG enable )
{
    FIXME( "NYI\n" );
    if (enable) return STATUS_NOT_SUPPORTED;
    /* we don't have turbo thunks yet */
    return STATUS_SUCCESS;
}

/**********************************************************************
 *           invalidate_mapped_section
 *
 * Invalidates all code in the entire memory section containing 'addr'
 */
static NTSTATUS invalidate_mapped_section( PVOID addr )
{
    MEMORY_BASIC_INFORMATION mem_info;
    NTSTATUS ret = NtQueryVirtualMemory( NtCurrentProcess(), addr, MemoryBasicInformation, &mem_info,
                                         sizeof(mem_info), NULL );

    if (!NT_SUCCESS(ret))
        return ret;

    return invalidate_code_range( (DWORD64)mem_info.AllocationBase,
                                  (DWORD64)(mem_info.BaseAddress + mem_info.RegionSize - mem_info.AllocationBase) );
}

/**********************************************************************
 *           BTCpuNotifyUnmapViewOfSection  (xtajit.@)
 */
void WINAPI BTCpuNotifyUnmapViewOfSection( PVOID addr, ULONG flags )
{
    NTSTATUS ret;
    TRACE( "DBG %p %ld\n", addr, flags );
    ret = invalidate_mapped_section( addr );
    if (!NT_SUCCESS(ret))
        WARN( "Failed to invalidate code memory: 0x%x\n", ret );
}

/**********************************************************************
 *           BTCpuNotifyMemoryFree  (xtajit.@)
 */
void WINAPI BTCpuNotifyMemoryFree( PVOID addr, SIZE_T size, ULONG free_type )
{
    NTSTATUS ret;

    TRACE( "DBG %p %lld %ld\n", addr, size, free_type );
    if (!size)
        ret = invalidate_mapped_section( addr );
    else if (free_type & MEM_DECOMMIT)
        /* Invalidate all pages touched by the region, even if they are just straddled */
        ret = invalidate_code_range( (DWORD64)ROUND_ADDR( addr, page_mask ), (DWORD64)ROUND_SIZE( addr, size ) );

    if (!NT_SUCCESS(ret))
        WARN( "Failed to invalidate code memory: 0x%x\n", ret );
    ret = set_prot( (DWORD64)ROUND_ADDR( addr, page_mask ), (DWORD64)ROUND_SIZE( addr, size ), PAGE_NOACCESS );
    if (!NT_SUCCESS(ret))
        WARN( "Failed to set protection: 0x%x\n", ret );
}

/**********************************************************************
 *           BTCpuNotifyMemoryProtect  (xtajit.@)
 */
void WINAPI BTCpuNotifyMemoryProtect( PVOID addr, SIZE_T size, DWORD new_protect )
{
    NTSTATUS ret;
    if (!(new_protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        return;
    TRACE( "DBG %p %lld %ld\n", addr, size, new_protect );

    /* Invalidate all pages touched by the region, even if they are just straddled */
    ret = invalidate_code_range( (DWORD64)ROUND_ADDR( addr, page_mask ), (DWORD64)ROUND_SIZE( addr, size ) );
    if (!NT_SUCCESS(ret))
        WARN( "Failed to invalidate code memory: 0x%x\n", ret );
    ret = set_prot( (DWORD64)ROUND_ADDR( addr, page_mask ), (DWORD64)ROUND_SIZE( addr, size ), new_protect );
    if (!NT_SUCCESS(ret))
        WARN( "Failed to set protection: 0x%x\n", ret );
}

/**********************************************************************
 *           BTCpuFlushInstructionCache2  (xtajit.@)
 */
void WINAPI BTCpuFlushInstructionCache2( LPCVOID addr, SIZE_T size)
{
    NTSTATUS ret;

    /* Invalidate all pages touched by the region, even if they are just straddled */
    ret = invalidate_code_range( (DWORD64)addr, (DWORD64)size );
    if (!NT_SUCCESS(ret))
        WARN( "Failed to invalidate code memory: 0x%x\n", ret );
}

BOOL WINAPI DllMain (HINSTANCE inst, DWORD reason, void *reserved )
{
    TRACE("%p,%lx,%p\n", inst, reason, reserved);

    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            LdrDisableThreadCalloutsForDll(inst);
            if (NtQueryVirtualMemory( GetCurrentProcess(), inst, MemoryWineUnixFuncs,
                                      &emuapi_handle, sizeof(emuapi_handle), NULL ))
                return FALSE;
            if (EMUAPI_CALL( attach, NULL ))
                return FALSE;
            break;
        case DLL_PROCESS_DETACH:
            if (reserved) break;
            EMUAPI_CALL( detach, NULL );
            break;
    }

    return TRUE;
}
