/*
 * CE commctrl.dll
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

#include "windows.h"

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

HWND WINAPI CommandBar_Create(HINSTANCE hInst, HWND hwndParent, int idCmdBar)
{
    /* FIXME: hack */
    return hwndParent;
}

BOOL WINAPI CommandBar_InsertMenubar(HWND hwndCB, HINSTANCE hInst, WORD idMenu, WORD iButton)
{
    /* FIXME: hack */
    return TRUE;
}

BOOL WINAPI CommandBar_AddAdornments(HINSTANCE hInst, HWND hwndParent, int idCmdBar)
{
    /* FIXME: hack */
    return TRUE;
}
