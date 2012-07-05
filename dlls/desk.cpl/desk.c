/*
 * Desktop control panel applet
 * 
 * Copyright 2002 Jaco Greeff
 * Copyright 2003 Dimitrie O. Paun
 * Copyright 2003 Mark Westcott
 * Copyright 2003-2004 Mike Hearn
 * Copyright 2004 Chris Morgan 
 * Copyright 2005 Raphael Junqueira
 * Copyright (c) 2005 by Frank Richter
 * Copyright (c) 2006 by Michael Jung
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



#define NONAMELESSUNION
#define COBJMACROS
#define CONST_VTABLE

#include "config.h"
#include "wine/port.h"


#include <assert.h>
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include <commctrl.h>
#include <cpl.h>
#include <wine/unicode.h>
#include <wine/list.h>
#include <winreg.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>

#include <windows.h>
#include <wine/debug.h>

#define RES_MAXLEN 5 /* max number of digits in a screen dimension. 5 digits should be plenty */
#define MINDPI 96
#define MAXDPI 480
#define DEFDPI 96

#include <shlobj.h>
#include <shlwapi.h>

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif

#define COBJMACROS

#include <commdlg.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <tmschema.h>


#define IDT_DPIEDIT 0x1234
#include "wine/debug.h"

#include "desk.h"

#define WINE_KEY_ROOT "Software\\Wine"


WINE_DEFAULT_DEBUG_CHANNEL(deskcpl);

HINSTANCE hInst;

WCHAR* current_app = NULL;

static const int is_win64 = (sizeof(void *) > sizeof(int));


/********************************************** from winecfg.h *****************************************/


#define disable(id) EnableWindow(GetDlgItem(dialog, id), 0);
#define enable(id) EnableWindow(GetDlgItem(dialog, id), 1);

#define MAXBUFLEN 256

#define IS_OPTION_TRUE(ch) \
    ((ch) == 'y' || (ch) == 'Y' || (ch) == 't' || (ch) == 'T' || (ch) == '1')
#define IS_OPTION_FALSE(ch) \
    ((ch) == 'n' || (ch) == 'N' || (ch) == 'f' || (ch) == 'F' || (ch) == '0')


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

/* create a unicode string from a string in Unix locale */
static inline WCHAR *strdupU2W(const char *unix_str)
{
    WCHAR *unicode_str;
    int lenW;

    lenW = MultiByteToWideChar(CP_UNIXCP, 0, unix_str, -1, NULL, 0);
    unicode_str = HeapAlloc(GetProcessHeap(), 0, lenW * sizeof(WCHAR));
    if (unicode_str)
        MultiByteToWideChar(CP_UNIXCP, 0, unix_str, -1, unicode_str, lenW);
    return unicode_str;
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

static inline void set_text(HWND dialog, WORD id, const char *text)
{
    SetWindowTextA(GetDlgItem(dialog, id), text);
}

static inline void set_textW(HWND dialog, WORD id, const WCHAR *text)
{
    SetWindowTextW(GetDlgItem(dialog, id), text);
}



/********************************************* from winecfg.c ****************************************************/

HKEY config_key = NULL;


/**
 * get_config_key: Retrieves a configuration value from the registry
 *
 * char *subkey : the name of the config section
 * char *name : the name of the config value
 * char *default : if the key isn't found, return this value instead
 *
 * Returns a buffer holding the value if successful, NULL if
 * not. Caller is responsible for releasing the result.
 *
 */
static WCHAR *get_config_key (HKEY root, const WCHAR *subkey, const WCHAR *name, const WCHAR *def)
{
    LPWSTR buffer = NULL;
    DWORD len;
    HKEY hSubKey = NULL;
    DWORD res;

    WINE_TRACE("subkey=%s, name=%s, def=%s\n", wine_dbgstr_w(subkey),
               wine_dbgstr_w(name), wine_dbgstr_w(def));

    res = RegOpenKeyExW(root, subkey, 0, MAXIMUM_ALLOWED, &hSubKey);
    if (res != ERROR_SUCCESS)
    {
        if (res == ERROR_FILE_NOT_FOUND)
        {
            WINE_TRACE("Section key not present - using default\n");
            return def ? strdupW(def) : NULL;
        }
        else
        {
            WINE_ERR("RegOpenKey failed on wine config key (res=%d)\n", res);
        }
        goto end;
    }

    res = RegQueryValueExW(hSubKey, name, NULL, NULL, NULL, &len);
    if (res == ERROR_FILE_NOT_FOUND)
    {
        WINE_TRACE("Value not present - using default\n");
        buffer = def ? strdupW(def) : NULL;
	goto end;
    } else if (res != ERROR_SUCCESS)
    {
        WINE_ERR("Couldn't query value's length (res=%d)\n", res);
        goto end;
    }

    buffer = HeapAlloc(GetProcessHeap(), 0, len + sizeof(WCHAR));

    RegQueryValueExW(hSubKey, name, NULL, NULL, (LPBYTE) buffer, &len);

    WINE_TRACE("buffer=%s\n", wine_dbgstr_w(buffer));
end:
    RegCloseKey(hSubKey);

    return buffer;
}

/**
 * set_config_key: convenience wrapper to set a key/value pair
 *
 * const char *subKey : the name of the config section
 * const char *valueName : the name of the config value
 * const char *value : the value to set the configuration key to
 *
 * Returns 0 on success, non-zero otherwise
 *
 * If valueName or value is NULL, an empty section will be created
 */
static int set_config_key(HKEY root, const WCHAR *subkey, REGSAM access, const WCHAR *name, const void *value, DWORD type)
{
    DWORD res = 1;
    HKEY key = NULL;

    WINE_TRACE("subkey=%s: name=%s, value=%p, type=%d\n", wine_dbgstr_w(subkey),
               wine_dbgstr_w(name), value, type);

    assert( subkey != NULL );

    if (subkey[0])
    {
        res = RegCreateKeyExW( root, subkey, 0, NULL, REG_OPTION_NON_VOLATILE,
                               access, NULL, &key, NULL );
        if (res != ERROR_SUCCESS) goto end;
    }
    else key = root;
    if (name == NULL || value == NULL) goto end;

    switch (type)
    {
        case REG_SZ: res = RegSetValueExW(key, name, 0, REG_SZ, value, (lstrlenW(value)+1)*sizeof(WCHAR)); break;
        case REG_DWORD: res = RegSetValueExW(key, name, 0, REG_DWORD, value, sizeof(DWORD)); break;
    }
    if (res != ERROR_SUCCESS) goto end;

    res = 0;
end:
    if (key && key != root) RegCloseKey(key);
    if (res != 0)
        WINE_ERR("Unable to set configuration key %s in section %s, res=%d\n",
                 wine_dbgstr_w(name), wine_dbgstr_w(subkey), res);
    return res;
}

/* ========================================================================= */

/* This code exists for the following reasons:
 *
 * - It makes working with the registry easier
 * - By storing a mini cache of the registry, we can more easily implement
 *   cancel/revert and apply. The 'settings list' is an overlay on top of
 *   the actual registry data that we can write out at will.
 *
 * Rather than model a tree in memory, we simply store each absolute (rooted
 * at the config key) path.
 *
 */

struct setting
{
    struct list entry;
    HKEY root;    /* the key on which path is rooted */
    WCHAR *path;   /* path in the registry rooted at root  */
    WCHAR *name;   /* name of the registry value. if null, this means delete the key  */
    WCHAR *value;  /* contents of the registry value. if null, this means delete the value  */
    DWORD type;   /* type of registry value. REG_SZ or REG_DWORD for now */
};

struct list *settings;

static void free_setting(struct setting *setting)
{
    assert( setting != NULL );
    assert( setting->path );

    WINE_TRACE("destroying %p: %s\n", setting,
               wine_dbgstr_w(setting->path));

    HeapFree(GetProcessHeap(), 0, setting->path);
    HeapFree(GetProcessHeap(), 0, setting->name);
    HeapFree(GetProcessHeap(), 0, setting->value);

    list_remove(&setting->entry);

    HeapFree(GetProcessHeap(), 0, setting);
}



int initialize(HINSTANCE hInstance)
{
    DWORD res = RegCreateKeyA(HKEY_CURRENT_USER, WINE_KEY_ROOT, &config_key);

    if (res != ERROR_SUCCESS) {
	WINE_ERR("RegOpenKey failed on wine config key (%d)\n", res);
	return 1;
    }

    /* we could probably just have the list as static data  */
    settings = HeapAlloc(GetProcessHeap(), 0, sizeof(struct list));
    list_init(settings);

    return 0;
}



/**
 * Returns the contents of the value at path. If not in the settings
 * list, it will be fetched from the registry - failing that, the
 * default will be used.
 *
 * If already in the list, the contents as given there will be
 * returned. You are expected to HeapFree the result.
 */
WCHAR *get_reg_keyW(HKEY root, const WCHAR *path, const WCHAR *name, const WCHAR *def)
{
    struct list *cursor;
    struct setting *s;
    WCHAR *val;

    WINE_TRACE("path=%s, name=%s, def=%s\n", wine_dbgstr_w(path),
               wine_dbgstr_w(name), wine_dbgstr_w(def));

    /* check if it's in the list */
    LIST_FOR_EACH( cursor, settings )
    {
        s = LIST_ENTRY(cursor, struct setting, entry);

        if (root != s->root) continue;
        if (lstrcmpiW(path, s->path) != 0) continue;
        if (!s->name) continue;
        if (lstrcmpiW(name, s->name) != 0) continue;

        WINE_TRACE("found %s:%s in settings list, returning %s\n",
                   wine_dbgstr_w(path), wine_dbgstr_w(name),
                   wine_dbgstr_w(s->value));
        return s->value ? strdupW(s->value) : NULL;
    }

    /* no, so get from the registry */
    val = get_config_key(root, path, name, def);

    WINE_TRACE("returning %s\n", wine_dbgstr_w(val));

    return val;
}


char *get_reg_key(HKEY root, const char *path, const char *name, const char *def)
{
    WCHAR *wpath, *wname, *wdef = NULL, *wRet = NULL;
    char *szRet = NULL;
    int len;

    WINE_TRACE("path=%s, name=%s, def=%s\n", path, name, def);

    wpath = HeapAlloc(GetProcessHeap(), 0, (strlen(path)+1)*sizeof(WCHAR));
    wname = HeapAlloc(GetProcessHeap(), 0, (strlen(name)+1)*sizeof(WCHAR));

    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, strlen(path)+1);
    MultiByteToWideChar(CP_ACP, 0, name, -1, wname, strlen(name)+1);

    if (def)
    {
        wdef = HeapAlloc(GetProcessHeap(), 0, (strlen(def)+1)*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, def, -1, wdef, strlen(def)+1);
    }

    wRet = get_reg_keyW(root, wpath, wname, wdef);

    len = WideCharToMultiByte(CP_ACP, 0, wRet, -1, NULL, 0, NULL, NULL);
    if (len)
    {
        szRet = HeapAlloc(GetProcessHeap(), 0, len);
        WideCharToMultiByte(CP_ACP, 0, wRet, -1, szRet, len, NULL, NULL);
    }

    HeapFree(GetProcessHeap(), 0, wpath);
    HeapFree(GetProcessHeap(), 0, wname);
    HeapFree(GetProcessHeap(), 0, wdef);
    HeapFree(GetProcessHeap(), 0, wRet);

    return szRet;
}

/**
 * Used to set a registry key.
 *
 * path is rooted at the config key, ie use "Version" or
 * "AppDefaults\\fooapp.exe\\Version". You can use keypath()
 * to get such a string.
 *
 * name is the value name, or NULL to delete the path.
 *
 * value is what to set the value to, or NULL to delete it.
 *
 * type is REG_SZ or REG_DWORD.
 *
 * These values will be copied when necessary.
 */
static void set_reg_key_ex(HKEY root, const WCHAR *path, const WCHAR *name, const void *value, DWORD type)
{
    struct list *cursor;
    struct setting *s;

    assert( path != NULL );

    WINE_TRACE("path=%s, name=%s, value=%s\n", wine_dbgstr_w(path),
               wine_dbgstr_w(name), wine_dbgstr_w(value));

    /* firstly, see if we already set this setting  */
    LIST_FOR_EACH( cursor, settings )
    {
        struct setting *s = LIST_ENTRY(cursor, struct setting, entry);

        if (root != s->root) continue;
        if (lstrcmpiW(s->path, path) != 0) continue;
        if ((s->name && name) && lstrcmpiW(s->name, name) != 0) continue;

        /* are we attempting a double delete? */
        if (!s->name && !name) return;

        /* do we want to undelete this key? */
        if (!s->name && name) s->name = strdupW(name);

        /* yes, we have already set it, so just replace the content and return  */
        HeapFree(GetProcessHeap(), 0, s->value);
        s->type = type;
        switch (type)
        {
            case REG_SZ:
                s->value = value ? strdupW(value) : NULL;
                break;
            case REG_DWORD:
                s->value = HeapAlloc(GetProcessHeap(), 0, sizeof(DWORD));
                memcpy( s->value, value, sizeof(DWORD) );
                break;
        }

        /* are we deleting this key? this won't remove any of the
         * children from the overlay so if the user adds it again in
         * that session it will appear to undelete the settings, but
         * in reality only the settings actually modified by the user
         * in that session will be restored. we might want to fix this
         * corner case in future by actually deleting all the children
         * here so that once it's gone, it's gone.
         */
        if (!name) s->name = NULL;

        return;
    }

    /* otherwise add a new setting for it  */
    s = HeapAlloc(GetProcessHeap(), 0, sizeof(struct setting));
    s->root  = root;
    s->path  = strdupW(path);
    s->name  = name  ? strdupW(name)  : NULL;
    s->type  = type;
    switch (type)
    {
        case REG_SZ:
            s->value = value ? strdupW(value) : NULL;
            break;
        case REG_DWORD:
            s->value = HeapAlloc(GetProcessHeap(), 0, sizeof(DWORD));
            memcpy( s->value, value, sizeof(DWORD) );
            break;
    }

    list_add_tail(settings, &s->entry);
}

void set_reg_key(HKEY root, const char *path, const char *name, const char *value)
{
    WCHAR *wpath, *wname = NULL, *wvalue = NULL;

    wpath = HeapAlloc(GetProcessHeap(), 0, (strlen(path)+1)*sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, strlen(path)+1);

    if (name)
    {
        wname = HeapAlloc(GetProcessHeap(), 0, (strlen(name)+1)*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, name, -1, wname, strlen(name)+1);
    }

    if (value)
    {
        wvalue = HeapAlloc(GetProcessHeap(), 0, (strlen(value)+1)*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, value, -1, wvalue, strlen(value)+1);
    }

    set_reg_key_ex(root, wpath, wname, wvalue, REG_SZ);

    HeapFree(GetProcessHeap(), 0, wpath);
    HeapFree(GetProcessHeap(), 0, wname);
    HeapFree(GetProcessHeap(), 0, wvalue);
}

void set_reg_key_dword(HKEY root, const char *path, const char *name, DWORD value)
{
    WCHAR *wpath, *wname;

    wpath = HeapAlloc(GetProcessHeap(), 0, (strlen(path)+1)*sizeof(WCHAR));
    wname = HeapAlloc(GetProcessHeap(), 0, (strlen(name)+1)*sizeof(WCHAR));

    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, strlen(path)+1);
    MultiByteToWideChar(CP_ACP, 0, name, -1, wname, strlen(name)+1);

    set_reg_key_ex(root, wpath, wname, &value, REG_DWORD);

    HeapFree(GetProcessHeap(), 0, wpath);
    HeapFree(GetProcessHeap(), 0, wname);
}

void set_reg_keyW(HKEY root, const WCHAR *path, const WCHAR *name, const WCHAR *value)
{
    set_reg_key_ex(root, path, name, value, REG_SZ);
}

void set_reg_key_dwordW(HKEY root, const WCHAR *path, const WCHAR *name, DWORD value)
{
    set_reg_key_ex(root, path, name, &value, REG_DWORD);
}

/**
 * returns true if the given key/value pair exists in the registry or
 * has been written to.
 */
BOOL reg_key_exists(HKEY root, const char *path, const char *name)
{
    char *val = get_reg_key(root, path, name, NULL);

    if (val)
    {
        HeapFree(GetProcessHeap(), 0, val);
        return TRUE;
    }

    return FALSE;
}

/* this is called from the WM_SHOWWINDOW handlers of each tab page.
 *
 * it's a nasty hack, necessary because the property sheet insists on resetting the window title
 * to the title of the tab, which is utterly useless. dropping the property sheet is on the todo list.
 */
void set_window_title(HWND dialog)
{
    WCHAR newtitle[256];

    /* update the window title  */
    if (current_app)
    {
        WCHAR apptitle[256];
        LoadStringW (hInst, IDS_CPL_NAME_APP, apptitle,
            sizeof(apptitle)/sizeof(apptitle[0]));
        wsprintfW (newtitle, apptitle, current_app);
    }
    else
    {
        LoadStringW (hInst, IDS_CPL_NAME, newtitle,
            sizeof(newtitle)/sizeof(newtitle[0]));
    }

    WINE_TRACE("setting title to %s\n", wine_dbgstr_w (newtitle));
    SendMessageW (GetParent(dialog), PSM_SETTITLEW, 0, (LPARAM) newtitle);
}

static void process_setting(struct setting *s)
{
    static const WCHAR softwareW[] = {'S','o','f','t','w','a','r','e','\\'};
    HKEY key;
    BOOL needs_wow64 = (is_win64 && s->root == HKEY_LOCAL_MACHINE && s->path &&
                        !strncmpiW( s->path, softwareW, sizeof(softwareW)/sizeof(WCHAR) ));

    if (s->value)
    {
	WINE_TRACE("Setting %s:%s to '%s'\n", wine_dbgstr_w(s->path),
                   wine_dbgstr_w(s->name), wine_dbgstr_w(s->value));
        set_config_key(s->root, s->path, MAXIMUM_ALLOWED, s->name, s->value, s->type);
        if (needs_wow64)
        {
            WINE_TRACE("Setting 32-bit %s:%s to '%s'\n", wine_dbgstr_w(s->path),
                       wine_dbgstr_w(s->name), wine_dbgstr_w(s->value));
            set_config_key(s->root, s->path, MAXIMUM_ALLOWED | KEY_WOW64_32KEY, s->name, s->value, s->type);
        }
    }
    else
    {
	WINE_TRACE("Removing %s:%s\n", wine_dbgstr_w(s->path), wine_dbgstr_w(s->name));
        if (!RegOpenKeyExW( s->root, s->path, 0, MAXIMUM_ALLOWED, &key ))
        {
            /* NULL name means remove that path/section entirely */
            if (s->name) RegDeleteValueW( key, s->name );
            else
            {
                RegDeleteTreeW( key, NULL );
                RegDeleteKeyW( s->root, s->path );
            }
            RegCloseKey( key );
        }
        if (needs_wow64)
        {
            WINE_TRACE("Removing 32-bit %s:%s\n", wine_dbgstr_w(s->path), wine_dbgstr_w(s->name));
            if (!RegOpenKeyExW( s->root, s->path, 0, MAXIMUM_ALLOWED | KEY_WOW64_32KEY, &key ))
            {
                if (s->name) RegDeleteValueW( key, s->name );
                else
                {
                    RegDeleteTreeW( key, NULL );
                    RegDeleteKeyExW( s->root, s->path, KEY_WOW64_32KEY, 0 );
                }
                RegCloseKey( key );
            }
        }
    }
}


void apply(void)
{
    if (list_empty(settings)) return; /* we will be called for each page when the user clicks OK */

    WINE_TRACE("()\n");

    while (!list_empty(settings))
    {
        struct setting *s = (struct setting *) list_head(settings);
        process_setting(s);
        free_setting(s);
    }
}


/* ================================== utility functions ============================ */

/* returns a registry key path suitable for passing to addTransaction  */
char *keypath(const char *section)
{
    static char *result = NULL;

    HeapFree(GetProcessHeap(), 0, result);

    if (current_app)
    {
        result = HeapAlloc(GetProcessHeap(), 0, strlen("AppDefaults\\") + lstrlenW(current_app)*2 + 2 /* \\ */ + strlen(section) + 1 /* terminator */);
        wsprintfA(result, "AppDefaults\\%ls", current_app);
        if (section[0]) sprintf( result + strlen(result), "\\%s", section );
    }
    else
    {
        result = strdupA(section);
    }

    return result;
}

WCHAR *keypathW(const WCHAR *section)
{
    static const WCHAR appdefaultsW[] = {'A','p','p','D','e','f','a','u','l','t','s','\\',0};
    static WCHAR *result = NULL;

    HeapFree(GetProcessHeap(), 0, result);

    if (current_app)
    {
        DWORD len = sizeof(appdefaultsW) + (lstrlenW(current_app) + lstrlenW(section) + 1) * sizeof(WCHAR);
        result = HeapAlloc(GetProcessHeap(), 0, len );
        lstrcpyW( result, appdefaultsW );
        lstrcatW( result, current_app );
        if (section[0])
        {
            len = lstrlenW(result);
            result[len++] = '\\';
            lstrcpyW( result + len, section );
        }
    }
    else
    {
        result = strdupW(section);
    }

    return result;
}

/********************************************* from driveui.c *****************************************************/

BOOL browse_for_unix_folder(HWND dialog, WCHAR *pszPath)
{
    static WCHAR wszUnixRootDisplayName[] = 
        { ':',':','{','C','C','7','0','2','E','B','2','-','7','D','C','5','-','1','1','D','9','-',
          'C','6','8','7','-','0','0','0','4','2','3','8','A','0','1','C','D','}', 0 };
    WCHAR pszChoosePath[FILENAME_MAX];
    BROWSEINFOW bi = {
        dialog,
        NULL,
        NULL,
        pszChoosePath,
        0,
        NULL,
        0,
        0
    };
    IShellFolder *pDesktop;
    LPITEMIDLIST pidlUnixRoot, pidlSelectedPath;
    HRESULT hr;

    LoadStringW(hInst, IDS_CHOOSE_PATH, pszChoosePath, FILENAME_MAX);

    hr = SHGetDesktopFolder(&pDesktop);
    if (FAILED(hr)) return FALSE;

    hr = IShellFolder_ParseDisplayName(pDesktop, NULL, NULL, wszUnixRootDisplayName, NULL, 
                                       &pidlUnixRoot, NULL);
    if (FAILED(hr)) {
        IShellFolder_Release(pDesktop);
        return FALSE;
    }

    bi.pidlRoot = pidlUnixRoot;
    pidlSelectedPath = SHBrowseForFolderW(&bi);
    SHFree(pidlUnixRoot);
    
    if (pidlSelectedPath) {
        STRRET strSelectedPath;
        WCHAR *pszSelectedPath;
        HRESULT hr;
        
        hr = IShellFolder_GetDisplayNameOf(pDesktop, pidlSelectedPath, SHGDN_FORPARSING, 
                                           &strSelectedPath);
        IShellFolder_Release(pDesktop);
        if (FAILED(hr)) {
            SHFree(pidlSelectedPath);
            return FALSE;
        }

        hr = StrRetToStrW(&strSelectedPath, pidlSelectedPath, &pszSelectedPath);
        SHFree(pidlSelectedPath);
        if (FAILED(hr)) return FALSE;

        lstrcpyW(pszPath, pszSelectedPath);
        
        CoTaskMemFree(pszSelectedPath);
        return TRUE;
    }
    return FALSE;
}

/******************************************** from x11drvdlg.c ****************************************************/



static const WCHAR logpixels_reg[] = {'S','y','s','t','e','m','\\','C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\','H','a','r','d','w','a','r','e',' ','P','r','o','f','i','l','e','s','\\','C','u','r','r','e','n','t','\\','S','o','f','t','w','a','r','e','\\','F','o','n','t','s',0};
static const WCHAR logpixels[] = {'L','o','g','P','i','x','e','l','s',0};

static const WCHAR desktopW[] = {'D','e','s','k','t','o','p',0};
static const WCHAR defaultW[] = {'D','e','f','a','u','l','t',0};
static const WCHAR explorerW[] = {'E','x','p','l','o','r','e','r',0};
static const WCHAR explorer_desktopsW[] = {'E','x','p','l','o','r','e','r','\\',
                                           'D','e','s','k','t','o','p','s',0};
static const WCHAR x11_driverW[] = {'X','1','1',' ','D','r','i','v','e','r',0};
static const WCHAR default_resW[] = {'8','0','0','x','6','0','0',0};


static int updating_ui;

/* convert the x11 desktop key to the new explorer config */
static void convert_x11_desktop_key(void)
{
    char *buf;

    if (!(buf = get_reg_key(config_key, "X11 Driver", "Desktop", NULL))) return;
    set_reg_key(config_key, "Explorer\\Desktops", "Default", buf);
    set_reg_key(config_key, "Explorer", "Desktop", "Default");
    set_reg_key(config_key, "X11 Driver", "Desktop", NULL);
    HeapFree(GetProcessHeap(), 0, buf);
}

static void update_gui_for_desktop_mode(HWND dialog)
{
    WCHAR *buf, *bufindex;
    const WCHAR *desktop_name = current_app ? current_app : defaultW;

    WINE_TRACE("\n");
    updating_ui = TRUE;

    buf = get_reg_keyW(config_key, explorer_desktopsW, desktop_name, NULL);
    if (buf && (bufindex = strchrW(buf, 'x')))
    {
        *bufindex = 0;
        ++bufindex;
        SetWindowTextW(GetDlgItem(dialog, IDC_DESKTOP_WIDTH), buf);
        SetWindowTextW(GetDlgItem(dialog, IDC_DESKTOP_HEIGHT), bufindex);
    } else {
        SetWindowTextA(GetDlgItem(dialog, IDC_DESKTOP_WIDTH), "800");
        SetWindowTextA(GetDlgItem(dialog, IDC_DESKTOP_HEIGHT), "600");
    }
    HeapFree(GetProcessHeap(), 0, buf);

    /* do we have desktop mode enabled? */
    if (reg_key_exists(config_key, keypath("Explorer"), "Desktop"))
    {
	CheckDlgButton(dialog, IDC_ENABLE_DESKTOP, BST_CHECKED);
        enable(IDC_DESKTOP_WIDTH);
        enable(IDC_DESKTOP_HEIGHT);
        enable(IDC_DESKTOP_SIZE);
        enable(IDC_DESKTOP_BY);
    }
    else
    {
	CheckDlgButton(dialog, IDC_ENABLE_DESKTOP, BST_UNCHECKED);
	disable(IDC_DESKTOP_WIDTH);
	disable(IDC_DESKTOP_HEIGHT);
	disable(IDC_DESKTOP_SIZE);
	disable(IDC_DESKTOP_BY);
    }

    updating_ui = FALSE;
}

static void x11drvdlg_init_dialog(HWND dialog)
{
    char* buf;

    convert_x11_desktop_key();
    update_gui_for_desktop_mode(dialog);

    updating_ui = TRUE;

    SendDlgItemMessageW(dialog, IDC_DESKTOP_WIDTH, EM_LIMITTEXT, RES_MAXLEN, 0);
    SendDlgItemMessageW(dialog, IDC_DESKTOP_HEIGHT, EM_LIMITTEXT, RES_MAXLEN, 0);

    buf = get_reg_key(config_key, keypath("X11 Driver"), "GrabFullscreen", "N");
    if (IS_OPTION_TRUE(*buf))
	CheckDlgButton(dialog, IDC_FULLSCREEN_GRAB, BST_CHECKED);
    else
	CheckDlgButton(dialog, IDC_FULLSCREEN_GRAB, BST_UNCHECKED);
    HeapFree(GetProcessHeap(), 0, buf);

    buf = get_reg_key(config_key, keypath("X11 Driver"), "Managed", "Y");
    if (IS_OPTION_TRUE(*buf))
	CheckDlgButton(dialog, IDC_ENABLE_MANAGED, BST_CHECKED);
    else
	CheckDlgButton(dialog, IDC_ENABLE_MANAGED, BST_UNCHECKED);
    HeapFree(GetProcessHeap(), 0, buf);

    buf = get_reg_key(config_key, keypath("X11 Driver"), "Decorated", "Y");
    if (IS_OPTION_TRUE(*buf))
	CheckDlgButton(dialog, IDC_ENABLE_DECORATED, BST_CHECKED);
    else
	CheckDlgButton(dialog, IDC_ENABLE_DECORATED, BST_UNCHECKED);
    HeapFree(GetProcessHeap(), 0, buf);

    updating_ui = FALSE;
}

static void set_from_desktop_edits(HWND dialog)
{
    static const WCHAR x[] = {'x',0};
    static const WCHAR def_width[]  = {'8','0','0',0};
    static const WCHAR def_height[] = {'6','0','0',0};
    static const WCHAR min_width[]  = {'6','4','0',0};
    static const WCHAR min_height[] = {'4','8','0',0};
    WCHAR *width, *height, *new;
    const WCHAR *desktop_name = current_app ? current_app : defaultW;

    if (updating_ui) return;
    
    WINE_TRACE("\n");

    width = get_textW(dialog, IDC_DESKTOP_WIDTH);
    height = get_textW(dialog, IDC_DESKTOP_HEIGHT);

    if (!width || !width[0]) {
        HeapFree(GetProcessHeap(), 0, width);
        width = strdupW(def_width);
    }
    else if (atoiW(width) < atoiW(min_width))
    {
        HeapFree(GetProcessHeap(), 0, width);
        width = strdupW(min_width);
    }
    if (!height || !height[0]) {
        HeapFree(GetProcessHeap(), 0, height);
        height = strdupW(def_height);
    }
    else if (atoiW(height) < atoiW(min_height))
    {
        HeapFree(GetProcessHeap(), 0, height);
        height = strdupW(min_height);
    }

    new = HeapAlloc(GetProcessHeap(), 0, (strlenW(width) + strlenW(height) + 2) * sizeof(WCHAR));
    strcpyW( new, width );
    strcatW( new, x );
    strcatW( new, height );
    set_reg_keyW(config_key, explorer_desktopsW, desktop_name, new);
    set_reg_keyW(config_key, keypathW(explorerW), desktopW, desktop_name);

    HeapFree(GetProcessHeap(), 0, width);
    HeapFree(GetProcessHeap(), 0, height);
    HeapFree(GetProcessHeap(), 0, new);
}

static void on_enable_desktop_clicked(HWND dialog) {
    WINE_TRACE("\n");
    
    if (IsDlgButtonChecked(dialog, IDC_ENABLE_DESKTOP) == BST_CHECKED) {
        set_from_desktop_edits(dialog);
    } else {
        set_reg_key(config_key, keypath("Explorer"), "Desktop", NULL);
    }
    
    update_gui_for_desktop_mode(dialog);
}

static void on_enable_managed_clicked(HWND dialog) {
    WINE_TRACE("\n");
    
    if (IsDlgButtonChecked(dialog, IDC_ENABLE_MANAGED) == BST_CHECKED) {
        set_reg_key(config_key, keypath("X11 Driver"), "Managed", "Y");
    } else {
        set_reg_key(config_key, keypath("X11 Driver"), "Managed", "N");
    }
}

static void on_enable_decorated_clicked(HWND dialog) {
    WINE_TRACE("\n");

    if (IsDlgButtonChecked(dialog, IDC_ENABLE_DECORATED) == BST_CHECKED) {
        set_reg_key(config_key, keypath("X11 Driver"), "Decorated", "Y");
    } else {
        set_reg_key(config_key, keypath("X11 Driver"), "Decorated", "N");
    }
}

static void on_fullscreen_grab_clicked(HWND dialog)
{
    if (IsDlgButtonChecked(dialog, IDC_FULLSCREEN_GRAB) == BST_CHECKED)
        set_reg_key(config_key, keypath("X11 Driver"), "GrabFullscreen", "Y");
    else
        set_reg_key(config_key, keypath("X11 Driver"), "GrabFullscreen", "N");
}

static INT read_logpixels_reg(void)
{
    DWORD dwLogPixels;
    WCHAR *buf = get_reg_keyW(HKEY_LOCAL_MACHINE, logpixels_reg, logpixels, NULL);
    dwLogPixels = buf ? *buf : DEFDPI;
    HeapFree(GetProcessHeap(), 0, buf);
    return dwLogPixels;
}

static void init_dpi_editbox(HWND hDlg)
{
    static const WCHAR fmtW[] = {'%','u',0};
    DWORD dwLogpixels;
    WCHAR szLogpixels[MAXBUFLEN];

    updating_ui = TRUE;

    dwLogpixels = read_logpixels_reg();

    sprintfW(szLogpixels, fmtW, dwLogpixels);
    SetDlgItemTextW(hDlg, IDC_RES_DPIEDIT, szLogpixels);

    updating_ui = FALSE;
}

static void init_trackbar(HWND hDlg)
{
    HWND hTrackBar = GetDlgItem(hDlg, IDC_RES_TRACKBAR);
    DWORD dwLogpixels;

    updating_ui = TRUE;

    dwLogpixels = read_logpixels_reg();

    SendMessageW(hTrackBar, TBM_SETRANGE, TRUE, MAKELONG(MINDPI, MAXDPI));
    SendMessageW(hTrackBar, TBM_SETPOS, TRUE, dwLogpixels);

    updating_ui = FALSE;
}

static void update_dpi_trackbar_from_edit(HWND hDlg, BOOL fix)
{
    static const WCHAR fmtW[] = {'%','u',0};
    DWORD dpi;

    updating_ui = TRUE;

    dpi = GetDlgItemInt(hDlg, IDC_RES_DPIEDIT, NULL, FALSE);

    if (fix)
    {
        DWORD fixed_dpi = dpi;

        if (dpi < MINDPI) fixed_dpi = MINDPI;
        if (dpi > MAXDPI) fixed_dpi = MAXDPI;

        if (fixed_dpi != dpi)
        {
            WCHAR buf[16];

            dpi = fixed_dpi;
            sprintfW(buf, fmtW, dpi);
            SetDlgItemTextW(hDlg, IDC_RES_DPIEDIT, buf);
        }
    }

    if (dpi >= MINDPI && dpi <= MAXDPI)
    {
        SendDlgItemMessageW(hDlg, IDC_RES_TRACKBAR, TBM_SETPOS, TRUE, dpi);
        set_reg_key_dwordW(HKEY_LOCAL_MACHINE, logpixels_reg, logpixels, dpi);
    }

    updating_ui = FALSE;
}

static void update_font_preview(HWND hDlg)
{
    DWORD dpi;

    updating_ui = TRUE;

    dpi = GetDlgItemInt(hDlg, IDC_RES_DPIEDIT, NULL, FALSE);

    if (dpi >= MINDPI && dpi <= MAXDPI)
    {
        static const WCHAR tahomaW[] = {'T','a','h','o','m','a',0};
        LOGFONTW lf;
        HFONT hfont;

        hfont = (HFONT)SendDlgItemMessageW(hDlg, IDC_RES_FONT_PREVIEW, WM_GETFONT, 0, 0);

        GetObjectW(hfont, sizeof(lf), &lf);

        if (strcmpW(lf.lfFaceName, tahomaW) != 0)
            strcpyW(lf.lfFaceName, tahomaW);
        else
            DeleteObject(hfont);
        lf.lfHeight = MulDiv(-10, dpi, 72);
        hfont = CreateFontIndirectW(&lf);
        SendDlgItemMessageW(hDlg, IDC_RES_FONT_PREVIEW, WM_SETFONT, (WPARAM)hfont, 1);
    }

    updating_ui = FALSE;
}

INT_PTR CALLBACK
GraphDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
	case WM_INITDIALOG:
	    init_dpi_editbox(hDlg);
	    init_trackbar(hDlg);
            update_font_preview(hDlg);
	    break;

        case WM_SHOWWINDOW:
            set_window_title(hDlg);
            break;

        case WM_TIMER:
            if (wParam == IDT_DPIEDIT)
            {
                KillTimer(hDlg, IDT_DPIEDIT);
                update_dpi_trackbar_from_edit(hDlg, TRUE);
                update_font_preview(hDlg);
            }
            break;
            
	case WM_COMMAND:
	    switch(HIWORD(wParam)) {
		case EN_CHANGE: {
		    if (updating_ui) break;
		    SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
		    if ( ((LOWORD(wParam) == IDC_DESKTOP_WIDTH) || (LOWORD(wParam) == IDC_DESKTOP_HEIGHT)) && !updating_ui )
			set_from_desktop_edits(hDlg);
                    else if (LOWORD(wParam) == IDC_RES_DPIEDIT)
                    {
                        update_dpi_trackbar_from_edit(hDlg, FALSE);
                        update_font_preview(hDlg);
                        SetTimer(hDlg, IDT_DPIEDIT, 1500, NULL);
                    }
		    break;
		}
		case BN_CLICKED: {
		    if (updating_ui) break;
		    SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
		    switch(LOWORD(wParam)) {
			case IDC_ENABLE_DESKTOP: on_enable_desktop_clicked(hDlg); break;
                        case IDC_ENABLE_MANAGED: on_enable_managed_clicked(hDlg); break;
                        case IDC_ENABLE_DECORATED: on_enable_decorated_clicked(hDlg); break;
			case IDC_FULLSCREEN_GRAB:  on_fullscreen_grab_clicked(hDlg); break;
		    }
		    break;
		}
		case CBN_SELCHANGE: {
		    SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
		    break;
		}
		    
		default:
		    break;
	    }
	    break;
	
	
	case WM_NOTIFY:
	    switch (((LPNMHDR)lParam)->code) {
		case PSN_KILLACTIVE: {
		    SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, FALSE);
		    break;
		}
		case PSN_APPLY: {
                    apply();
		    SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
		    break;
		}
		case PSN_SETACTIVE: {
		    x11drvdlg_init_dialog (hDlg);
		    break;
		}
	    }
	    break;

	case WM_HSCROLL:
	    switch (wParam) {
		default: {
                    static const WCHAR fmtW[] = {'%','d',0};
		    WCHAR buf[MAXBUFLEN];
		    int i = SendMessageW(GetDlgItem(hDlg, IDC_RES_TRACKBAR), TBM_GETPOS, 0, 0);
		    buf[0] = 0;
		    sprintfW(buf, fmtW, i);
		    SendMessageW(GetDlgItem(hDlg, IDC_RES_DPIEDIT), WM_SETTEXT, 0, (LPARAM) buf);
                    update_font_preview(hDlg);
		    set_reg_key_dwordW(HKEY_LOCAL_MACHINE, logpixels_reg, logpixels, i);
		    break;
		}
	    }
	    break;

	default:
	    break;
    }
    return FALSE;
}

/**************************************************** from theme.c *************************************************/



/* UXTHEME functions not in the headers */

typedef struct tagTHEMENAMES
{
    WCHAR szName[MAX_PATH+1];
    WCHAR szDisplayName[MAX_PATH+1];
    WCHAR szTooltip[MAX_PATH+1];
} THEMENAMES, *PTHEMENAMES;

typedef void* HTHEMEFILE;
typedef BOOL (CALLBACK *EnumThemeProc)(LPVOID lpReserved, 
				       LPCWSTR pszThemeFileName,
                                       LPCWSTR pszThemeName, 
				       LPCWSTR pszToolTip, LPVOID lpReserved2,
                                       LPVOID lpData);

HRESULT WINAPI EnumThemeColors (LPCWSTR pszThemeFileName, LPWSTR pszSizeName,
				DWORD dwColorNum, PTHEMENAMES pszColorNames);
HRESULT WINAPI EnumThemeSizes (LPCWSTR pszThemeFileName, LPWSTR pszColorName,
			       DWORD dwSizeNum, PTHEMENAMES pszSizeNames);
HRESULT WINAPI ApplyTheme (HTHEMEFILE hThemeFile, char* unknown, HWND hWnd);
HRESULT WINAPI OpenThemeFile (LPCWSTR pszThemeFileName, LPCWSTR pszColorName,
			      LPCWSTR pszSizeName, HTHEMEFILE* hThemeFile,
			      DWORD unknown);
HRESULT WINAPI CloseThemeFile (HTHEMEFILE hThemeFile);
HRESULT WINAPI EnumThemes (LPCWSTR pszThemePath, EnumThemeProc callback,
                           LPVOID lpData);

/* A struct to keep both the internal and "fancy" name of a color or size */
typedef struct
{
  WCHAR* name;
  WCHAR* fancyName;
} ThemeColorOrSize;

/* wrapper around DSA that also keeps an item count */
typedef struct
{
  HDSA dsa;
  int count;
} WrappedDsa;

/* Some helper functions to deal with ThemeColorOrSize structs in WrappedDSAs */

static void color_or_size_dsa_add (WrappedDsa* wdsa, const WCHAR* name,
				   const WCHAR* fancyName)
{
    ThemeColorOrSize item;
    
    item.name = HeapAlloc (GetProcessHeap(), 0, 
	(lstrlenW (name) + 1) * sizeof(WCHAR));
    lstrcpyW (item.name, name);

    item.fancyName = HeapAlloc (GetProcessHeap(), 0, 
	(lstrlenW (fancyName) + 1) * sizeof(WCHAR));
    lstrcpyW (item.fancyName, fancyName);

    DSA_InsertItem (wdsa->dsa, wdsa->count, &item);
    wdsa->count++;
}

static int CALLBACK dsa_destroy_callback (LPVOID p, LPVOID pData)
{
    ThemeColorOrSize* item = p;
    HeapFree (GetProcessHeap(), 0, item->name);
    HeapFree (GetProcessHeap(), 0, item->fancyName);
    return 1;
}

static void free_color_or_size_dsa (WrappedDsa* wdsa)
{
    DSA_DestroyCallback (wdsa->dsa, dsa_destroy_callback, NULL);
}

static void create_color_or_size_dsa (WrappedDsa* wdsa)
{
    wdsa->dsa = DSA_Create (sizeof (ThemeColorOrSize), 1);
    wdsa->count = 0;
}

static ThemeColorOrSize* color_or_size_dsa_get (WrappedDsa* wdsa, int index)
{
    return DSA_GetItemPtr (wdsa->dsa, index);
}

static int color_or_size_dsa_find (WrappedDsa* wdsa, const WCHAR* name)
{
    int i = 0;
    for (; i < wdsa->count; i++)
    {
	ThemeColorOrSize* item = color_or_size_dsa_get (wdsa, i);
	if (lstrcmpiW (item->name, name) == 0) break;
    }
    return i;
}

/* A theme file, contains file name, display name, color and size scheme names */
typedef struct
{
    WCHAR* themeFileName;
    WCHAR* fancyName;
    WrappedDsa colors;
    WrappedDsa sizes;
} ThemeFile;

static HDSA themeFiles = NULL;
static int themeFilesCount = 0;

static int CALLBACK theme_dsa_destroy_callback (LPVOID p, LPVOID pData)
{
    ThemeFile* item = p;
    HeapFree (GetProcessHeap(), 0, item->themeFileName);
    HeapFree (GetProcessHeap(), 0, item->fancyName);
    free_color_or_size_dsa (&item->colors);
    free_color_or_size_dsa (&item->sizes);
    return 1;
}

/* Free memory occupied by the theme list */
static void free_theme_files(void)
{
    if (themeFiles == NULL) return;
      
    DSA_DestroyCallback (themeFiles , theme_dsa_destroy_callback, NULL);
    themeFiles = NULL;
    themeFilesCount = 0;
}

typedef HRESULT (WINAPI * EnumTheme) (LPCWSTR, LPWSTR, DWORD, PTHEMENAMES);

/* fill a string list with either colors or sizes of a theme */
static void fill_theme_string_array (const WCHAR* filename, 
				     WrappedDsa* wdsa,
				     EnumTheme enumTheme)
{
    DWORD index = 0;
    THEMENAMES names;

    WINE_TRACE ("%s %p %p\n", wine_dbgstr_w (filename), wdsa, enumTheme);

    while (SUCCEEDED (enumTheme (filename, NULL, index++, &names)))
    {
	WINE_TRACE ("%s: %s\n", wine_dbgstr_w (names.szName), 
            wine_dbgstr_w (names.szDisplayName));
	color_or_size_dsa_add (wdsa, names.szName, names.szDisplayName);
    }
}

/* Theme enumeration callback, adds theme to theme list */
static BOOL CALLBACK myEnumThemeProc (LPVOID lpReserved, 
				      LPCWSTR pszThemeFileName,
				      LPCWSTR pszThemeName, 
				      LPCWSTR pszToolTip, 
				      LPVOID lpReserved2, LPVOID lpData)
{
    ThemeFile newEntry;

    /* fill size/color lists */
    create_color_or_size_dsa (&newEntry.colors);
    fill_theme_string_array (pszThemeFileName, &newEntry.colors, EnumThemeColors);
    create_color_or_size_dsa (&newEntry.sizes);
    fill_theme_string_array (pszThemeFileName, &newEntry.sizes, EnumThemeSizes);

    newEntry.themeFileName = HeapAlloc (GetProcessHeap(), 0, 
	(lstrlenW (pszThemeFileName) + 1) * sizeof(WCHAR));
    lstrcpyW (newEntry.themeFileName, pszThemeFileName);
  
    newEntry.fancyName = HeapAlloc (GetProcessHeap(), 0, 
	(lstrlenW (pszThemeName) + 1) * sizeof(WCHAR));
    lstrcpyW (newEntry.fancyName, pszThemeName);
  
    /*list_add_tail (&themeFiles, &newEntry->entry);*/
    DSA_InsertItem (themeFiles, themeFilesCount, &newEntry);
    themeFilesCount++;

    return TRUE;
}

/* Scan for themes */
static void scan_theme_files(void)
{
    static const WCHAR themesSubdir[] = { '\\','T','h','e','m','e','s',0 };
    WCHAR themesPath[MAX_PATH];

    free_theme_files();

    if (FAILED (SHGetFolderPathW (NULL, CSIDL_RESOURCES, NULL, 
        SHGFP_TYPE_CURRENT, themesPath))) return;

    themeFiles = DSA_Create (sizeof (ThemeFile), 1);
    lstrcatW (themesPath, themesSubdir);

    EnumThemes (themesPath, myEnumThemeProc, 0);
}

/* fill the color & size combo boxes for a given theme */
static void fill_color_size_combos (ThemeFile* theme, HWND comboColor, 
                                    HWND comboSize)
{
    int i;

    SendMessageW (comboColor, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < theme->colors.count; i++)
    {
	ThemeColorOrSize* item = color_or_size_dsa_get (&theme->colors, i);
	SendMessageW (comboColor, CB_ADDSTRING, 0, (LPARAM)item->fancyName);
    }

    SendMessageW (comboSize, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < theme->sizes.count; i++)
    {
	ThemeColorOrSize* item = color_or_size_dsa_get (&theme->sizes, i);
	SendMessageW (comboSize, CB_ADDSTRING, 0, (LPARAM)item->fancyName);
    }
}

/* Select the item of a combo box that matches a theme's color and size 
 * scheme. */
static void select_color_and_size (ThemeFile* theme, 
			    const WCHAR* colorName, HWND comboColor, 
			    const WCHAR* sizeName, HWND comboSize)
{
    SendMessageW (comboColor, CB_SETCURSEL, 
	color_or_size_dsa_find (&theme->colors, colorName), 0);
    SendMessageW (comboSize, CB_SETCURSEL, 
	color_or_size_dsa_find (&theme->sizes, sizeName), 0);
}

/* Fill theme, color and sizes combo boxes with the know themes and select
 * the entries matching the currently active theme. */
static BOOL fill_theme_list (HWND comboTheme, HWND comboColor, HWND comboSize)
{
    WCHAR textNoTheme[256];
    int themeIndex = 0;
    BOOL ret = TRUE;
    int i;
    WCHAR currentTheme[MAX_PATH];
    WCHAR currentColor[MAX_PATH];
    WCHAR currentSize[MAX_PATH];
    ThemeFile* theme = NULL;

    LoadStringW (hInst, IDS_NOTHEME, textNoTheme,
	sizeof(textNoTheme) / sizeof(WCHAR));

    SendMessageW (comboTheme, CB_RESETCONTENT, 0, 0);
    SendMessageW (comboTheme, CB_ADDSTRING, 0, (LPARAM)textNoTheme);

    for (i = 0; i < themeFilesCount; i++)
    {
        ThemeFile* item = DSA_GetItemPtr (themeFiles, i);
	SendMessageW (comboTheme, CB_ADDSTRING, 0, 
	    (LPARAM)item->fancyName);
    }
  
    if (IsThemeActive () && SUCCEEDED (GetCurrentThemeName (currentTheme, 
	    sizeof(currentTheme) / sizeof(WCHAR),
	    currentColor, sizeof(currentColor) / sizeof(WCHAR),
	    currentSize, sizeof(currentSize) / sizeof(WCHAR))))
    {
	/* Determine the index of the currently active theme. */
	BOOL found = FALSE;
	for (i = 0; i < themeFilesCount; i++)
	{
            theme = DSA_GetItemPtr (themeFiles, i);
	    if (lstrcmpiW (theme->themeFileName, currentTheme) == 0)
	    {
		found = TRUE;
		themeIndex = i+1;
		break;
	    }
	}
	if (!found)
	{
	    /* Current theme not found?... add to the list, then... */
	    WINE_TRACE("Theme %s not in list of enumerated themes\n",
		wine_dbgstr_w (currentTheme));
	    myEnumThemeProc (NULL, currentTheme, currentTheme, 
		currentTheme, NULL, NULL);
	    themeIndex = themeFilesCount;
            theme = DSA_GetItemPtr (themeFiles, themeFilesCount-1);
	}
	fill_color_size_combos (theme, comboColor, comboSize);
	select_color_and_size (theme, currentColor, comboColor,
	    currentSize, comboSize);
    }
    else
    {
	/* No theme selected */
	ret = FALSE;
    }

    SendMessageW (comboTheme, CB_SETCURSEL, themeIndex, 0);
    return ret;
}

/* Update the color & size combo boxes when the selection of the theme
 * combo changed. Selects the current color and size scheme if the theme
 * is currently active, otherwise the first color and size. */
static BOOL update_color_and_size (int themeIndex, HWND comboColor, 
				   HWND comboSize)
{
    if (themeIndex == 0)
    {
	return FALSE;
    }
    else
    {
	WCHAR currentTheme[MAX_PATH];
	WCHAR currentColor[MAX_PATH];
	WCHAR currentSize[MAX_PATH];
	ThemeFile* theme = DSA_GetItemPtr (themeFiles, themeIndex - 1);
    
	fill_color_size_combos (theme, comboColor, comboSize);
      
	if ((SUCCEEDED (GetCurrentThemeName (currentTheme, 
	    sizeof(currentTheme) / sizeof(WCHAR),
	    currentColor, sizeof(currentColor) / sizeof(WCHAR),
	    currentSize, sizeof(currentSize) / sizeof(WCHAR))))
	    && (lstrcmpiW (currentTheme, theme->themeFileName) == 0))
	{
	    select_color_and_size (theme, currentColor, comboColor,
		currentSize, comboSize);
	}
	else
	{
	    SendMessageW (comboColor, CB_SETCURSEL, 0, 0);
	    SendMessageW (comboSize, CB_SETCURSEL, 0, 0);
	}
    }
    return TRUE;
}

/* Apply a theme from a given theme, color and size combo box item index. */
static void do_apply_theme (int themeIndex, int colorIndex, int sizeIndex)
{
    static char b[] = "\0";

    if (themeIndex == 0)
    {
	/* no theme */
	ApplyTheme (NULL, b, NULL);
    }
    else
    {
        ThemeFile* theme = DSA_GetItemPtr (themeFiles, themeIndex-1);
	const WCHAR* themeFileName = theme->themeFileName;
	const WCHAR* colorName = NULL;
	const WCHAR* sizeName = NULL;
	HTHEMEFILE hTheme;
	ThemeColorOrSize* item;
    
	item = color_or_size_dsa_get (&theme->colors, colorIndex);
	colorName = item->name;
	
	item = color_or_size_dsa_get (&theme->sizes, sizeIndex);
	sizeName = item->name;
	
	if (SUCCEEDED (OpenThemeFile (themeFileName, colorName, sizeName,
	    &hTheme, 0)))
	{
	    ApplyTheme (hTheme, b, NULL);
	    CloseThemeFile (hTheme);
	}
	else
	{
	    ApplyTheme (NULL, b, NULL);
	}
    }
}

static BOOL theme_dirty;

static void enable_size_and_color_controls (HWND dialog, BOOL enable)
{
    EnableWindow (GetDlgItem (dialog, IDC_THEME_COLORCOMBO), enable);
    EnableWindow (GetDlgItem (dialog, IDC_THEME_COLORTEXT), enable);
    EnableWindow (GetDlgItem (dialog, IDC_THEME_SIZECOMBO), enable);
    EnableWindow (GetDlgItem (dialog, IDC_THEME_SIZETEXT), enable);
}
  
static void theme_init_dialog (HWND dialog)
{
    updating_ui = TRUE;
    
    scan_theme_files();
    if (!fill_theme_list (GetDlgItem (dialog, IDC_THEME_THEMECOMBO),
        GetDlgItem (dialog, IDC_THEME_COLORCOMBO),
        GetDlgItem (dialog, IDC_THEME_SIZECOMBO)))
    {
        SendMessageW (GetDlgItem (dialog, IDC_THEME_COLORCOMBO), CB_SETCURSEL, (WPARAM)-1, 0);
        SendMessageW (GetDlgItem (dialog, IDC_THEME_SIZECOMBO), CB_SETCURSEL, (WPARAM)-1, 0);
        enable_size_and_color_controls (dialog, FALSE);
    }
    else
    {
        enable_size_and_color_controls (dialog, TRUE);
    }
    theme_dirty = FALSE;

    SendDlgItemMessageW(dialog, IDC_SYSPARAM_SIZE_UD, UDM_SETBUDDY, (WPARAM)GetDlgItem(dialog, IDC_SYSPARAM_SIZE), 0);
    SendDlgItemMessageW(dialog, IDC_SYSPARAM_SIZE_UD, UDM_SETRANGE, 0, MAKELONG(100, 8));

    updating_ui = FALSE;
}

static void on_theme_changed(HWND dialog) {
    int index = SendMessageW (GetDlgItem (dialog, IDC_THEME_THEMECOMBO),
        CB_GETCURSEL, 0, 0);
    if (!update_color_and_size (index, GetDlgItem (dialog, IDC_THEME_COLORCOMBO),
        GetDlgItem (dialog, IDC_THEME_SIZECOMBO)))
    {
        SendMessageW (GetDlgItem (dialog, IDC_THEME_COLORCOMBO), CB_SETCURSEL, -1, 0);
        SendMessageW (GetDlgItem (dialog, IDC_THEME_SIZECOMBO), CB_SETCURSEL, -1, 0);
        enable_size_and_color_controls (dialog, FALSE);
    }
    else
    {
        enable_size_and_color_controls (dialog, TRUE);
    }
    theme_dirty = TRUE;
}

static void apply_theme(HWND dialog)
{
    int themeIndex, colorIndex, sizeIndex;

    if (!theme_dirty) return;

    themeIndex = SendMessageW (GetDlgItem (dialog, IDC_THEME_THEMECOMBO),
        CB_GETCURSEL, 0, 0);
    colorIndex = SendMessageW (GetDlgItem (dialog, IDC_THEME_COLORCOMBO),
        CB_GETCURSEL, 0, 0);
    sizeIndex = SendMessageW (GetDlgItem (dialog, IDC_THEME_SIZECOMBO),
        CB_GETCURSEL, 0, 0);

    do_apply_theme (themeIndex, colorIndex, sizeIndex);
    theme_dirty = FALSE;
}

static struct
{
    int sm_idx, color_idx;
    const char *color_reg;
    int size;
    COLORREF color;
    LOGFONTW lf;
} metrics[] =
{
    {-1,                COLOR_BTNFACE,          "ButtonFace"    }, /* IDC_SYSPARAMS_BUTTON */
    {-1,                COLOR_BTNTEXT,          "ButtonText"    }, /* IDC_SYSPARAMS_BUTTON_TEXT */
    {-1,                COLOR_BACKGROUND,       "Background"    }, /* IDC_SYSPARAMS_DESKTOP */
    {SM_CXMENUSIZE,     COLOR_MENU,             "Menu"          }, /* IDC_SYSPARAMS_MENU */
    {-1,                COLOR_MENUTEXT,         "MenuText"      }, /* IDC_SYSPARAMS_MENU_TEXT */
    {SM_CXVSCROLL,      COLOR_SCROLLBAR,        "Scrollbar"     }, /* IDC_SYSPARAMS_SCROLLBAR */
    {-1,                COLOR_HIGHLIGHT,        "Hilight"       }, /* IDC_SYSPARAMS_SELECTION */
    {-1,                COLOR_HIGHLIGHTTEXT,    "HilightText"   }, /* IDC_SYSPARAMS_SELECTION_TEXT */
    {-1,                COLOR_INFOBK,           "InfoWindow"    }, /* IDC_SYSPARAMS_TOOLTIP */
    {-1,                COLOR_INFOTEXT,         "InfoText"      }, /* IDC_SYSPARAMS_TOOLTIP_TEXT */
    {-1,                COLOR_WINDOW,           "Window"        }, /* IDC_SYSPARAMS_WINDOW */
    {-1,                COLOR_WINDOWTEXT,       "WindowText"    }, /* IDC_SYSPARAMS_WINDOW_TEXT */
    {SM_CXSIZE,         COLOR_ACTIVECAPTION,    "ActiveTitle"   }, /* IDC_SYSPARAMS_ACTIVE_TITLE */
    {-1,                COLOR_CAPTIONTEXT,      "TitleText"     }, /* IDC_SYSPARAMS_ACTIVE_TITLE_TEXT */
    {-1,                COLOR_INACTIVECAPTION,  "InactiveTitle" }, /* IDC_SYSPARAMS_INACTIVE_TITLE */
    {-1,                COLOR_INACTIVECAPTIONTEXT,"InactiveTitleText" }, /* IDC_SYSPARAMS_INACTIVE_TITLE_TEXT */
    {-1,                -1,                     "MsgBoxText"    }, /* IDC_SYSPARAMS_MSGBOX_TEXT */
    {-1,                COLOR_APPWORKSPACE,     "AppWorkSpace"  }, /* IDC_SYSPARAMS_APPWORKSPACE */
    {-1,                COLOR_WINDOWFRAME,      "WindowFrame"   }, /* IDC_SYSPARAMS_WINDOW_FRAME */
    {-1,                COLOR_ACTIVEBORDER,     "ActiveBorder"  }, /* IDC_SYSPARAMS_ACTIVE_BORDER */
    {-1,                COLOR_INACTIVEBORDER,   "InactiveBorder" }, /* IDC_SYSPARAMS_INACTIVE_BORDER */
    {-1,                COLOR_BTNSHADOW,        "ButtonShadow"  }, /* IDC_SYSPARAMS_BUTTON_SHADOW */
    {-1,                COLOR_GRAYTEXT,         "GrayText"      }, /* IDC_SYSPARAMS_GRAY_TEXT */
    {-1,                COLOR_BTNHILIGHT,       "ButtonHilight" }, /* IDC_SYSPARAMS_BUTTON_HILIGHT */
    {-1,                COLOR_3DDKSHADOW,       "ButtonDkShadow" }, /* IDC_SYSPARAMS_BUTTON_DARK_SHADOW */
    {-1,                COLOR_3DLIGHT,          "ButtonLight"   }, /* IDC_SYSPARAMS_BUTTON_LIGHT */
    {-1,                COLOR_ALTERNATEBTNFACE, "ButtonAlternateFace" }, /* IDC_SYSPARAMS_BUTTON_ALTERNATE */
    {-1,                COLOR_HOTLIGHT,         "HotTrackingColor" }, /* IDC_SYSPARAMS_HOT_TRACKING */
    {-1,                COLOR_GRADIENTACTIVECAPTION, "GradientActiveTitle" }, /* IDC_SYSPARAMS_ACTIVE_TITLE_GRADIENT */
    {-1,                COLOR_GRADIENTINACTIVECAPTION, "GradientInactiveTitle" }, /* IDC_SYSPARAMS_INACTIVE_TITLE_GRADIENT */
    {-1,                COLOR_MENUHILIGHT,      "MenuHilight"   }, /* IDC_SYSPARAMS_MENU_HILIGHT */
    {-1,                COLOR_MENUBAR,          "MenuBar"       }, /* IDC_SYSPARAMS_MENUBAR */
};

static void save_sys_color(int idx, COLORREF clr)
{
    char buffer[13];

    sprintf(buffer, "%d %d %d",  GetRValue (clr), GetGValue (clr), GetBValue (clr));
    set_reg_key(HKEY_CURRENT_USER, "Control Panel\\Colors", metrics[idx].color_reg, buffer);
}

static void set_color_from_theme(WCHAR *keyName, COLORREF color)
{
    char *keyNameA = NULL;
    int keyNameSize=0, i=0;

    keyNameSize = WideCharToMultiByte(CP_ACP, 0, keyName, -1, keyNameA, 0, NULL, NULL);
    keyNameA = HeapAlloc(GetProcessHeap(), 0, keyNameSize);
    WideCharToMultiByte(CP_ACP, 0, keyName, -1, keyNameA, keyNameSize, NULL, NULL);

    for (i=0; i<sizeof(metrics)/sizeof(metrics[0]); i++)
    {
        if (lstrcmpiA(metrics[i].color_reg, keyNameA)==0)
        {
            metrics[i].color = color;
            save_sys_color(i, color);
            break;
        }
    }
    HeapFree(GetProcessHeap(), 0, keyNameA);
}

static void do_parse_theme(WCHAR *file)
{
    static const WCHAR colorSect[] = {
        'C','o','n','t','r','o','l',' ','P','a','n','e','l','\\',
        'C','o','l','o','r','s',0};
    WCHAR keyName[MAX_PATH], keyNameValue[MAX_PATH];
    WCHAR *keyNamePtr = NULL;
    char *keyNameValueA = NULL;
    int keyNameValueSize = 0;
    int red = 0, green = 0, blue = 0;
    COLORREF color;

    WINE_TRACE("%s\n", wine_dbgstr_w(file));

    GetPrivateProfileStringW(colorSect, NULL, NULL, keyName,
                             MAX_PATH, file);

    keyNamePtr = keyName;
    while (*keyNamePtr!=0) {
        GetPrivateProfileStringW(colorSect, keyNamePtr, NULL, keyNameValue,
                                 MAX_PATH, file);

        keyNameValueSize = WideCharToMultiByte(CP_ACP, 0, keyNameValue, -1,
                                               keyNameValueA, 0, NULL, NULL);
        keyNameValueA = HeapAlloc(GetProcessHeap(), 0, keyNameValueSize);
        WideCharToMultiByte(CP_ACP, 0, keyNameValue, -1, keyNameValueA, keyNameValueSize, NULL, NULL);

        WINE_TRACE("parsing key: %s with value: %s\n",
                   wine_dbgstr_w(keyNamePtr), wine_dbgstr_w(keyNameValue));

        sscanf(keyNameValueA, "%d %d %d", &red, &green, &blue);

        color = RGB((BYTE)red, (BYTE)green, (BYTE)blue);

        HeapFree(GetProcessHeap(), 0, keyNameValueA);

        set_color_from_theme(keyNamePtr, color);

        keyNamePtr+=lstrlenW(keyNamePtr);
        keyNamePtr++;
    }
}

static void on_theme_install(HWND dialog)
{
  static const WCHAR filterMask[] = {0,'*','.','m','s','s','t','y','l','e','s',';',
      '*','.','t','h','e','m','e',0,0};
  static const WCHAR themeExt[] = {'.','T','h','e','m','e',0};
  const int filterMaskLen = sizeof(filterMask)/sizeof(filterMask[0]);
  OPENFILENAMEW ofn;
  WCHAR filetitle[MAX_PATH];
  WCHAR file[MAX_PATH];
  WCHAR filter[100];
  WCHAR title[100];

  LoadStringW (hInst, IDS_THEMEFILE,
      filter, sizeof (filter) / sizeof (filter[0]) - filterMaskLen);
  memcpy (filter + lstrlenW (filter), filterMask, 
      filterMaskLen * sizeof (WCHAR));
  LoadStringW (hInst, IDS_THEMEFILE_SELECT,
      title, sizeof (title) / sizeof (title[0]));

  ofn.lStructSize = sizeof(OPENFILENAMEW);
  ofn.hwndOwner = 0;
  ofn.hInstance = 0;
  ofn.lpstrFilter = filter;
  ofn.lpstrCustomFilter = NULL;
  ofn.nMaxCustFilter = 0;
  ofn.nFilterIndex = 0;
  ofn.lpstrFile = file;
  ofn.lpstrFile[0] = '\0';
  ofn.nMaxFile = sizeof(file)/sizeof(filetitle[0]);
  ofn.lpstrFileTitle = filetitle;
  ofn.lpstrFileTitle[0] = '\0';
  ofn.nMaxFileTitle = sizeof(filetitle)/sizeof(filetitle[0]);
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = title;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
  ofn.nFileOffset = 0;
  ofn.nFileExtension = 0;
  ofn.lpstrDefExt = NULL;
  ofn.lCustData = 0;
  ofn.lpfnHook = NULL;
  ofn.lpTemplateName = NULL;

  if (GetOpenFileNameW(&ofn))
  {
      static const WCHAR themesSubdir[] = { '\\','T','h','e','m','e','s',0 };
      static const WCHAR backslash[] = { '\\',0 };
      WCHAR themeFilePath[MAX_PATH];
      SHFILEOPSTRUCTW shfop;

      if (FAILED (SHGetFolderPathW (NULL, CSIDL_RESOURCES|CSIDL_FLAG_CREATE, NULL, 
          SHGFP_TYPE_CURRENT, themeFilePath))) return;

      if (lstrcmpiW(PathFindExtensionW(filetitle), themeExt)==0)
      {
          do_parse_theme(file);
          SendMessageW(GetParent(dialog), PSM_CHANGED, 0, 0);
          return;
      }

      PathRemoveExtensionW (filetitle);

      /* Construct path into which the theme file goes */
      lstrcatW (themeFilePath, themesSubdir);
      lstrcatW (themeFilePath, backslash);
      lstrcatW (themeFilePath, filetitle);

      /* Create the directory */
      SHCreateDirectoryExW (dialog, themeFilePath, NULL);

      /* Append theme file name itself */
      lstrcatW (themeFilePath, backslash);
      lstrcatW (themeFilePath, PathFindFileNameW (file));
      /* SHFileOperation() takes lists as input, so double-nullterminate */
      themeFilePath[lstrlenW (themeFilePath)+1] = 0;
      file[lstrlenW (file)+1] = 0;

      /* Do the copying */
      WINE_TRACE("copying: %s -> %s\n", wine_dbgstr_w (file), 
          wine_dbgstr_w (themeFilePath));
      shfop.hwnd = dialog;
      shfop.wFunc = FO_COPY;
      shfop.pFrom = file;
      shfop.pTo = themeFilePath;
      shfop.fFlags = FOF_NOCONFIRMMKDIR;
      if (SHFileOperationW (&shfop) == 0)
      {
          scan_theme_files();
          if (!fill_theme_list (GetDlgItem (dialog, IDC_THEME_THEMECOMBO),
              GetDlgItem (dialog, IDC_THEME_COLORCOMBO),
              GetDlgItem (dialog, IDC_THEME_SIZECOMBO)))
          {
              SendMessageW (GetDlgItem (dialog, IDC_THEME_COLORCOMBO), CB_SETCURSEL, -1, 0);
              SendMessageW (GetDlgItem (dialog, IDC_THEME_SIZECOMBO), CB_SETCURSEL, -1, 0);
              enable_size_and_color_controls (dialog, FALSE);
          }
          else
          {
              enable_size_and_color_controls (dialog, TRUE);
          }
      }
      else
          WINE_TRACE("copy operation failed\n");
  }
  else WINE_TRACE("user cancelled\n");
}

static void read_sysparams(HWND hDlg)
{
    WCHAR buffer[256];
    HWND list = GetDlgItem(hDlg, IDC_SYSPARAM_COMBO);
    NONCLIENTMETRICSW nonclient_metrics;
    int i, idx;

    for (i = 0; i < sizeof(metrics) / sizeof(metrics[0]); i++)
    {
        LoadStringW(hInst, i + IDC_SYSPARAMS_BUTTON, buffer,
                    sizeof(buffer) / sizeof(buffer[0]));
        idx = SendMessageW(list, CB_ADDSTRING, 0, (LPARAM)buffer);
        if (idx != CB_ERR) SendMessageW(list, CB_SETITEMDATA, idx, i);

        if (metrics[i].sm_idx != -1)
            metrics[i].size = GetSystemMetrics(metrics[i].sm_idx);
        if (metrics[i].color_idx != -1)
            metrics[i].color = GetSysColor(metrics[i].color_idx);
    }

    nonclient_metrics.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &nonclient_metrics, 0);

    memcpy(&(metrics[IDC_SYSPARAMS_MENU_TEXT - IDC_SYSPARAMS_BUTTON].lf),
           &(nonclient_metrics.lfMenuFont), sizeof(LOGFONTW));
    memcpy(&(metrics[IDC_SYSPARAMS_ACTIVE_TITLE_TEXT - IDC_SYSPARAMS_BUTTON].lf),
           &(nonclient_metrics.lfCaptionFont), sizeof(LOGFONTW));
    memcpy(&(metrics[IDC_SYSPARAMS_TOOLTIP_TEXT - IDC_SYSPARAMS_BUTTON].lf),
           &(nonclient_metrics.lfStatusFont), sizeof(LOGFONTW));
    memcpy(&(metrics[IDC_SYSPARAMS_MSGBOX_TEXT - IDC_SYSPARAMS_BUTTON].lf),
           &(nonclient_metrics.lfMessageFont), sizeof(LOGFONTW));
}

static void apply_sysparams(void)
{
    NONCLIENTMETRICSW nonclient_metrics;
    int i, cnt = 0;
    int colors_idx[sizeof(metrics) / sizeof(metrics[0])];
    COLORREF colors[sizeof(metrics) / sizeof(metrics[0])];

    nonclient_metrics.cbSize = sizeof(nonclient_metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(nonclient_metrics), &nonclient_metrics, 0);

    nonclient_metrics.iMenuWidth = nonclient_metrics.iMenuHeight =
            metrics[IDC_SYSPARAMS_MENU - IDC_SYSPARAMS_BUTTON].size;
    nonclient_metrics.iCaptionWidth = nonclient_metrics.iCaptionHeight =
            metrics[IDC_SYSPARAMS_ACTIVE_TITLE - IDC_SYSPARAMS_BUTTON].size;
    nonclient_metrics.iScrollWidth = nonclient_metrics.iScrollHeight =
            metrics[IDC_SYSPARAMS_SCROLLBAR - IDC_SYSPARAMS_BUTTON].size;

    memcpy(&(nonclient_metrics.lfMenuFont),
           &(metrics[IDC_SYSPARAMS_MENU_TEXT - IDC_SYSPARAMS_BUTTON].lf),
           sizeof(LOGFONTW));
    memcpy(&(nonclient_metrics.lfCaptionFont),
           &(metrics[IDC_SYSPARAMS_ACTIVE_TITLE_TEXT - IDC_SYSPARAMS_BUTTON].lf),
           sizeof(LOGFONTW));
    memcpy(&(nonclient_metrics.lfStatusFont),
           &(metrics[IDC_SYSPARAMS_TOOLTIP_TEXT - IDC_SYSPARAMS_BUTTON].lf),
           sizeof(LOGFONTW));
    memcpy(&(nonclient_metrics.lfMessageFont),
           &(metrics[IDC_SYSPARAMS_MSGBOX_TEXT - IDC_SYSPARAMS_BUTTON].lf),
           sizeof(LOGFONTW));

    SystemParametersInfoW(SPI_SETNONCLIENTMETRICS, sizeof(nonclient_metrics), &nonclient_metrics,
                          SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    for (i = 0; i < sizeof(metrics) / sizeof(metrics[0]); i++)
        if (metrics[i].color_idx != -1)
        {
            colors_idx[cnt] = metrics[i].color_idx;
            colors[cnt++] = metrics[i].color;
        }
    SetSysColors(cnt, colors_idx, colors);
}

static void on_sysparam_change(HWND hDlg)
{
    int index = SendDlgItemMessageW(hDlg, IDC_SYSPARAM_COMBO, CB_GETCURSEL, 0, 0);

    index = SendDlgItemMessageW(hDlg, IDC_SYSPARAM_COMBO, CB_GETITEMDATA, index, 0);

    updating_ui = TRUE;

    EnableWindow(GetDlgItem(hDlg, IDC_SYSPARAM_COLOR_TEXT), metrics[index].color_idx != -1);
    EnableWindow(GetDlgItem(hDlg, IDC_SYSPARAM_COLOR), metrics[index].color_idx != -1);
    InvalidateRect(GetDlgItem(hDlg, IDC_SYSPARAM_COLOR), NULL, TRUE);

    EnableWindow(GetDlgItem(hDlg, IDC_SYSPARAM_SIZE_TEXT), metrics[index].sm_idx != -1);
    EnableWindow(GetDlgItem(hDlg, IDC_SYSPARAM_SIZE), metrics[index].sm_idx != -1);
    EnableWindow(GetDlgItem(hDlg, IDC_SYSPARAM_SIZE_UD), metrics[index].sm_idx != -1);
    if (metrics[index].sm_idx != -1)
        SendDlgItemMessageW(hDlg, IDC_SYSPARAM_SIZE_UD, UDM_SETPOS, 0, MAKELONG(metrics[index].size, 0));
    else
        set_text(hDlg, IDC_SYSPARAM_SIZE, "");

    EnableWindow(GetDlgItem(hDlg, IDC_SYSPARAM_FONT),
        index == IDC_SYSPARAMS_MENU_TEXT-IDC_SYSPARAMS_BUTTON ||
        index == IDC_SYSPARAMS_ACTIVE_TITLE_TEXT-IDC_SYSPARAMS_BUTTON ||
        index == IDC_SYSPARAMS_TOOLTIP_TEXT-IDC_SYSPARAMS_BUTTON ||
        index == IDC_SYSPARAMS_MSGBOX_TEXT-IDC_SYSPARAMS_BUTTON
    );

    updating_ui = FALSE;
}

static void on_draw_item(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
    static HBRUSH black_brush = 0;
    LPDRAWITEMSTRUCT draw_info = (LPDRAWITEMSTRUCT)lParam;

    if (!black_brush) black_brush = CreateSolidBrush(0);

    if (draw_info->CtlID == IDC_SYSPARAM_COLOR)
    {
        UINT state = DFCS_ADJUSTRECT | DFCS_BUTTONPUSH;

        if (draw_info->itemState & ODS_DISABLED)
            state |= DFCS_INACTIVE;
        else
            state |= draw_info->itemState & ODS_SELECTED ? DFCS_PUSHED : 0;

        DrawFrameControl(draw_info->hDC, &draw_info->rcItem, DFC_BUTTON, state);

        if (!(draw_info->itemState & ODS_DISABLED))
        {
            HBRUSH brush;
            int index = SendDlgItemMessageW(hDlg, IDC_SYSPARAM_COMBO, CB_GETCURSEL, 0, 0);

            index = SendDlgItemMessageW(hDlg, IDC_SYSPARAM_COMBO, CB_GETITEMDATA, index, 0);
            brush = CreateSolidBrush(metrics[index].color);

            InflateRect(&draw_info->rcItem, -1, -1);
            FrameRect(draw_info->hDC, &draw_info->rcItem, black_brush);
            InflateRect(&draw_info->rcItem, -1, -1);
            FillRect(draw_info->hDC, &draw_info->rcItem, brush);
            DeleteObject(brush);
        }
    }
}

static void on_select_font(HWND hDlg)
{
    CHOOSEFONTW cf;
    int index = SendDlgItemMessageW(hDlg, IDC_SYSPARAM_COMBO, CB_GETCURSEL, 0, 0);
    index = SendDlgItemMessageW(hDlg, IDC_SYSPARAM_COMBO, CB_GETITEMDATA, index, 0);

    ZeroMemory(&cf, sizeof(cf));
    cf.lStructSize = sizeof(CHOOSEFONTW);
    cf.hwndOwner = hDlg;
    cf.lpLogFont = &(metrics[index].lf);
    cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL;

    ChooseFontW(&cf);
}

INT_PTR CALLBACK
ThemeDlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG:
            read_sysparams(hDlg);
            break;
        
        case WM_DESTROY:
            free_theme_files();
            break;

        case WM_SHOWWINDOW:
            set_window_title(hDlg);
            break;
            
        case WM_COMMAND:
            switch(HIWORD(wParam)) {
                case CBN_SELCHANGE: {
                    if (updating_ui) break;
                    switch (LOWORD(wParam))
                    {
                        case IDC_THEME_THEMECOMBO: on_theme_changed(hDlg); break;
                        case IDC_THEME_COLORCOMBO: /* fall through */
                        case IDC_THEME_SIZECOMBO: theme_dirty = TRUE; break;
                        case IDC_SYSPARAM_COMBO: on_sysparam_change(hDlg); return FALSE;
                    }
                    SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
                    break;
                }
                case EN_CHANGE: {
                    if (updating_ui) break;
                    switch (LOWORD(wParam))
                    {
                        case IDC_SYSPARAM_SIZE:
                        {
                            char *text = get_text(hDlg, IDC_SYSPARAM_SIZE);
                            int index = SendDlgItemMessageW(hDlg, IDC_SYSPARAM_COMBO, CB_GETCURSEL, 0, 0);

                            index = SendDlgItemMessageW(hDlg, IDC_SYSPARAM_COMBO, CB_GETITEMDATA, index, 0);
                            metrics[index].size = atoi(text);
                            HeapFree(GetProcessHeap(), 0, text);

                            SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
                            break;
                        }
                    }
                    break;
                }
                case BN_CLICKED:
                    switch (LOWORD(wParam))
                    {
                        case IDC_THEME_INSTALL:
                            on_theme_install (hDlg);
                            break;

                        case IDC_SYSPARAM_FONT:
                            on_select_font(hDlg);
                            break;

                        case IDC_SYSPARAM_COLOR:
                        {
                            static COLORREF user_colors[16];
                            CHOOSECOLORW c_color;
                            int index = SendDlgItemMessageW(hDlg, IDC_SYSPARAM_COMBO, CB_GETCURSEL, 0, 0);

                            index = SendDlgItemMessageW(hDlg, IDC_SYSPARAM_COMBO, CB_GETITEMDATA, index, 0);

                            memset(&c_color, 0, sizeof(c_color));
                            c_color.lStructSize = sizeof(c_color);
                            c_color.lpCustColors = user_colors;
                            c_color.rgbResult = metrics[index].color;
                            c_color.Flags = CC_ANYCOLOR | CC_RGBINIT;
                            c_color.hwndOwner = hDlg;
                            if (ChooseColorW(&c_color))
                            {
                                metrics[index].color = c_color.rgbResult;
                                save_sys_color(index, metrics[index].color);
                                InvalidateRect(GetDlgItem(hDlg, IDC_SYSPARAM_COLOR), NULL, TRUE);
                                SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
                            }
                            break;
                        }
                    }
                    break;
            }
            break;
        
        case WM_NOTIFY:
            switch (((LPNMHDR)lParam)->code) {
                case PSN_KILLACTIVE: {
                    SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, FALSE);
                    break;
                }
                case PSN_APPLY: {
                    apply();
                    apply_theme(hDlg);
                    apply_sysparams();
                    SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                    break;
                }
                case PSN_SETACTIVE: {
                    theme_init_dialog (hDlg);
                    break;
                }
            }
            break;

        case WM_DRAWITEM:
            on_draw_item(hDlg, wParam, lParam);
            break;

        default:
            break;
    }
    return FALSE;
}

/**********************************************************************************************************************/


/******************************************************************************
 * Name       : DllMain
 * Description: Entry point for DLL file
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("(%p, %d, %p)\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            hInst = hinstDLL;
	    break;
    }
    return TRUE;
}


/******************************************************************************
 * propsheet_callback [internal]
 *
 */
static int CALLBACK propsheet_callback(HWND hwnd, UINT msg, LPARAM lparam)
{

    TRACE("(%p, 0x%08x/%d, 0x%lx)\n", hwnd, msg, msg, lparam);
    switch (msg)
    {
        case PSCB_INITIALIZED:
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM) LoadIconW(hInst, MAKEINTRESOURCEW(ICO_MAIN)));
            break;
    }
    return 0;
}

static void start(HWND hWnd)
{

    initialize(hInst);

    INITCOMMONCONTROLSEX icex;
    PROPSHEETPAGEW psp[NUM_PROPERTY_PAGES];
    PROPSHEETHEADERW psh;

    OleInitialize(NULL);  
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    ZeroMemory(&psh, sizeof(psh));
    ZeroMemory(psp, sizeof(psp));

    /* Fill out all PROPSHEETPAGEs */
    psp[0].dwSize = sizeof (PROPSHEETPAGEW);
    psp[0].dwFlags = PSP_USETITLE; 
    psp[0].hInstance = hInst;
    psp[0].u.pszTemplate = MAKEINTRESOURCEW(IDD_APPEARANCE);
    psp[0].pszTitle = MAKEINTRESOURCEW(IDS_TAB_APPEARANCE);
    psp[0].pfnDlgProc = ThemeDlgProc;

    psp[1].dwSize = sizeof (PROPSHEETPAGEW);
    psp[1].dwFlags = PSP_USETITLE;     
    psp[1].hInstance = hInst;
    psp[1].u.pszTemplate = MAKEINTRESOURCEW(IDD_SETTINGS);
    psp[1].pszTitle = MAKEINTRESOURCEW(IDS_TAB_SETTINGS);
    psp[1].pfnDlgProc = GraphDlgProc;


    /* Fill out the PROPSHEETHEADER */
    psh.dwSize = sizeof (PROPSHEETHEADERW);
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_USECALLBACK;
    psh.hwndParent = hWnd;
    psh.hInstance = hInst;
    psh.u.pszIcon = MAKEINTRESOURCEW(ICO_MAIN);
    psh.pszCaption = MAKEINTRESOURCEW(IDS_CPL_NAME);
    psh.nPages = 2;
    psh.u3.ppsp = psp;
    psh.pfnCallback = propsheet_callback;

    /* display the dialog */
    PropertySheetW(&psh);

    OleUninitialize();

}

/*********************************************************************
 * CPlApplet (desk.@)
 *
 * Control Panel entry point
 *
 * PARAMS
 *  hWnd    [I] Handle for the Control Panel Window
 *  command [I] CPL_* Command
 *  lParam1 [I] first extra Parameter
 *  lParam2 [I] second extra Parameter
 *
 * RETURNS
 *  Depends on the command
 *
 */
LONG CALLBACK CPlApplet(HWND hWnd, UINT command, LPARAM lParam1, LPARAM lParam2)
{
    TRACE("(%p, %u, 0x%lx, 0x%lx)\n", hWnd, command, lParam1, lParam2);

    switch (command)
    {
        case CPL_INIT:
            return TRUE;

        case CPL_GETCOUNT:
            return 1;

        case CPL_INQUIRE:
        {
            CPLINFO *appletInfo = (CPLINFO *) lParam2;

            appletInfo->idIcon = ICO_MAIN;
            appletInfo->idName = IDS_CPL_NAME;
            appletInfo->idInfo = IDS_CPL_INFO;
            appletInfo->lData = 0;
            return TRUE;
        }

        case CPL_DBLCLK:
            start(hWnd);
            break;
    }

    return FALSE;
}

