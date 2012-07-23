/*
 * Desktop Settings control panel applet
 * Helpers
 *
 * Copyright 2002 Jaco Greeff
 * Copyright 2003 Dimitrie O. Paun
 * Copyright 2004 Mike Hearn
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

#ifndef __WINE_DESKCPL_HELPERSH__
#define __WINE_DESKCPL_HELPERSH__

#include <windows.h>

#define MAXBUFLEN 256
#define WINE_KEY_ROOT "Software\\Wine"

#define IS_OPTION_TRUE(ch) \
    ((ch) == 'y' || (ch) == 'Y' || (ch) == 't' || (ch) == 'T' || (ch) == '1')
#define IS_OPTION_FALSE(ch) \
    ((ch) == 'n' || (ch) == 'N' || (ch) == 'f' || (ch) == 'F' || (ch) == '0')

void initialize_settings(void); 
void set_reg_key_ex(HKEY root, const WCHAR *path, const WCHAR *name, const void *value, DWORD type);
char *get_reg_key(HKEY root, const char *path, const char *name, const char *def);
void set_reg_key(HKEY root, const char *path, const char *name, const char *value);
void set_window_title(HWND dialog);
WCHAR *get_reg_keyW(HKEY root, const WCHAR *path, const WCHAR *name, const WCHAR *def);
WCHAR *get_config_key(HKEY root, const WCHAR *subkey, const WCHAR *name, const WCHAR *def);
void apply(void);
WCHAR *keypathW(const WCHAR *section);
char *keypath(const char *section);
BOOL reg_key_exists(HKEY root, const char *path, const char *name);


/* returns a string in the win32 heap  */
static inline char *strdupA(const char *s)
{
    char *r = HeapAlloc(GetProcessHeap(), 0, strlen(s)+1);
    return strcpy(r, s);
}
static inline WCHAR *strdupW(const WCHAR *s)
{
    WCHAR *r = HeapAlloc(GetProcessHeap(), 0, (lstrlenW(s)+1)*sizeof(WCHAR));
    return lstrcpyW(r, s);
}

static inline char *get_text(HWND dialog, WORD id)
{
    HWND item = GetDlgItem(dialog, id);
    int len = GetWindowTextLengthA(item) + 1;
    char *result = len ? HeapAlloc(GetProcessHeap(), 0, len) : NULL;
    if (!result || GetWindowTextA(item, result, len) == 0) return NULL;
    return result;
}
static inline WCHAR *get_textW(HWND dialog, WORD id)
{
    HWND item = GetDlgItem(dialog, id);
    int len = GetWindowTextLengthW(item) + 1;
    WCHAR *result = len ? HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR)) : NULL;
    if (!result || GetWindowTextW(item, result, len) == 0) return NULL;
    return result;
}

#endif
