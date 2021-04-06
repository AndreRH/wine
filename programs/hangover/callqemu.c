/*
 * Automatic start of Qemu for Hangover
 * (with parts from start.exe and winevdm.exe)
 *
 * Copyright 2021 André Hentschel
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

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(hangover);

static WCHAR* str_chr_w(const WCHAR *str, WCHAR ch)
{
    do { if (*str == ch) return (WCHAR *)(ULONG_PTR)str; } while (*str++);
    return NULL;
}

static void fatal_string(const char *what)
{
    WINE_ERR("Hangover failed: %s\n", what);

    ExitProcess(1);
}

static WCHAR *build_args( int argc, WCHAR **argvW )
{
    int i, wlen = 1;
    WCHAR *ret, *p;

    for (i = 0; i < argc; i++ )
    {
        wlen += lstrlenW(argvW[i]) + 1;
        if (str_chr_w(argvW[i], ' '))
            wlen += 2;
    }
    ret = HeapAlloc( GetProcessHeap(), 0, wlen*sizeof(WCHAR) );
    ret[0] = 0;

    for (i = 0, p = ret; i < argc; i++ )
    {
        int len = lstrlenW(argvW[i]);
        int bytes = len * sizeof(WCHAR);
        if (str_chr_w(argvW[i], ' '))
        {
            //p += swprintf(p, wlen - (p - ret), quotedW, argvW[i]);
            *p++ = ' ';
            *p++ = '"';
            memcpy(p, argvW[i], bytes);
            p += len;
            *p++ = '"';
        }
        else
        {
            //p += swprintf(p, wlen - (p - ret), spacedW, argvW[i]);
            *p++ = ' ';
            memcpy(p, argvW[i], bytes);
            p += len;
        }
    }
    *p = '\0';
    return ret;
}

/***********************************************************************
 *           usage
 */
static void usage(void)
{
    WINE_MESSAGE( "Usage: hangover.exe command line\n\n" );
    ExitProcess(1);
}

/***********************************************************************
 *           main
 */
int __cdecl wmain (int argc, WCHAR *argv[])
{
    LPWSTR (*CDECL wine_get_dos_file_name_ptr)(LPCSTR);
    PROCESS_INFORMATION process_information;
    WCHAR *dos_filename_emu = NULL;
    WCHAR *dos_filename_app = NULL;
    int multibyte_unixpath_len;
    STARTUPINFOW startup_info;
    char* multibyte_unixpath;
    WCHAR *commandline, *p;
    WCHAR *args = NULL;
    char *hoqemu;
    DWORD len;

    if (!argv[1]) usage();

    wine_get_dos_file_name_ptr = (void*)GetProcAddress(GetModuleHandleA("KERNEL32"), "wine_get_dos_file_name");
    if (!wine_get_dos_file_name_ptr)
        fatal_string("can't find wine_get_dos_file_name");

    hoqemu = getenv( "HOQEMU" );
    if (!hoqemu)
        fatal_string("You need to set HOQEMU");
    dos_filename_emu = wine_get_dos_file_name_ptr(hoqemu);
    if (!dos_filename_emu)
        fatal_string("can't convert HOQEMU");

    multibyte_unixpath_len = WideCharToMultiByte(CP_UNIXCP, 0, argv[1], -1, NULL, 0, NULL, NULL);
    multibyte_unixpath = HeapAlloc(GetProcessHeap(), 0, multibyte_unixpath_len);
    WideCharToMultiByte(CP_UNIXCP, 0, argv[1], -1, multibyte_unixpath, multibyte_unixpath_len, NULL, NULL);
    dos_filename_app = wine_get_dos_file_name_ptr(multibyte_unixpath);
    HeapFree(GetProcessHeap(), 0, multibyte_unixpath);
    if (!dos_filename_app)
        fatal_string("can't convert first arg");

    args = build_args( argc - 2, &argv[2] );

    WINE_TRACE( "GetCommandLine = %s\n", GetCommandLineA() );
    WINE_TRACE( "dos_filename_emu = %s\n", wine_dbgstr_w(dos_filename_emu) );
    WINE_TRACE( "dos_filename_app = %s\n", wine_dbgstr_w(dos_filename_app) );
    WINE_TRACE( "args = %s\n", wine_dbgstr_w(args) );

    len = lstrlenW(dos_filename_emu) + lstrlenW(dos_filename_app) + 3 + lstrlenW(args);

    commandline = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
    p = commandline;
    *p++ = '"';
    memcpy(p, dos_filename_emu, lstrlenW(dos_filename_emu) * sizeof(WCHAR));
    p += lstrlenW(dos_filename_emu);
    *p++ = '"';
    *p++ = ' ';
    memcpy(p, dos_filename_app, lstrlenW(dos_filename_app) * sizeof(WCHAR));
    p += lstrlenW(dos_filename_app);
    memcpy(p, args, lstrlenW(args) * sizeof(WCHAR));
    p += lstrlenW(args);
    *p = '\0';
    WINE_TRACE( "commandline = %s\n", wine_dbgstr_w(commandline) );

    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    startup_info.wShowWindow = SW_SHOWNORMAL;
    startup_info.dwFlags |= STARTF_USESHOWWINDOW;

    if (!CreateProcessW(
            dos_filename_emu,       /* lpApplicationName */
            commandline,            /* lpCommandLine */
            NULL,                   /* lpProcessAttributes */
            NULL,                   /* lpThreadAttributes */
            FALSE,                  /* bInheritHandles */
            0,                      /* dwCreationFlags */
            NULL,                   /* lpEnvironment */
            NULL,                   /* lpCurrentDirectory */
            &startup_info,          /* lpStartupInfo */
            &process_information    /* lpProcessInformation */ ))
    {
        fatal_string("CreateProcess failed");
    }

    return 0;
}
