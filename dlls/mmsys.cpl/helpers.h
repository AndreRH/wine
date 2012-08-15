/*
 * Audio Settings control panel applet
 * Helpers
 *
 * Copyright 2012 Magdalena Nowak
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
 *
 */

#ifndef __WINE_MMSYSCPL_HELPERSH__
#define __WINE_MMSYSCPL_HELPERSH__

#include <windows.h>
#include <propidl.h>

#define WINE_KEY_ROOT "Software\\Wine"

struct DeviceInfo {
    WCHAR *id;
    PROPVARIANT name;
};

void initialize_settings(void);
void set_reg_device(HWND hDlg, int dlgitem, const WCHAR *key_name);
void set_window_title(HWND dialog);
void apply(void);
WCHAR *get_reg_keyW(HKEY root, const WCHAR *path, const WCHAR *name, const WCHAR *def);
void find_devices(void);

/* returns a string in the win32 heap  */
static inline WCHAR *strdupW(const WCHAR *s) {
    WCHAR *r = HeapAlloc(GetProcessHeap(), 0, (lstrlenW(s)+1)*sizeof(WCHAR));
    return lstrcpyW(r, s);
}

#endif
