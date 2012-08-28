/*
 * CE coredll.dll
 *
 * Copyright 2010 André Hentschel
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windows.h"
#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(coredll);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}


/***********************************************************************
 *           GetProcAddressW    (COREDLL.530)
 *
 * Find the address of an exported symbol in a loaded dll.
 *
 * PARAMS
 *  hModule  [I] Handle to the dll returned by LoadLibraryW().
 *  function [I] Name of the symbol, or an integer ordinal number < 16384
 *
 * RETURNS
 *  Success: A pointer to the symbol in the process address space.
 *  Failure: NULL. Use GetLastError() to determine the cause.
 */
FARPROC WINAPI COREDLL_GetProcAddressW( HMODULE hModule, LPCWSTR function )
{
    NTSTATUS    nts;
    FARPROC     fp;

    if (!hModule) hModule = NtCurrentTeb()->Peb->ImageBaseAddress;

    /* FIXME: untested hack */

    if ((ULONG_PTR)function >> 16)
    {
        ANSI_STRING     str;
        char functionA[MAX_PATH];
        int len;

        len = WideCharToMultiByte(CP_ACP, 0, function, -1, functionA, MAX_PATH, NULL, NULL);

        RtlInitAnsiString( &str, functionA );
        nts = LdrGetProcedureAddress( hModule, &str, 0, (void**)&fp );
    }
    else
        nts = LdrGetProcedureAddress( hModule, NULL, LOWORD(function), (void**)&fp );

    if (nts != STATUS_SUCCESS)
    {
        SetLastError( RtlNtStatusToDosError( nts ) );
        fp = NULL;
    }

    return fp;
}
