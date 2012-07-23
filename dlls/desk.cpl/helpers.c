/*
 * Desktop Settings control panel applet
 * Helpers
 * 
 * Copyright 2002 Jaco Greeff
 * Copyright 2003 Dimitrie O. Paun
 * Copyright 2003-2004 Mike Hearn
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


#include "desk.h"

#include <winbase.h>
#include <winuser.h>

#include <assert.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <wine/list.h>
#include <wine/unicode.h>

#include "helpers.h"

extern HINSTANCE hInst;
extern WCHAR* current_app;
static const int is_win64 = (sizeof(void *) > sizeof(int));

/* The code below exists for the following reasons:
 *
 * - It makes working with the registry easier
 * - By storing a mini cache of the registry, we can more easily implement
 *   cancel/revert and apply. The 'settings list' is an overlay on top of
 *   the actual registry data that we can write out at will.
 *
 * Rather than model a tree in memory, we simply store each absolute (rooted
 * at the config key) path.
 */
HKEY config_key = NULL;

struct setting {
	struct list entry;
	HKEY root;    /* the key on which path is rooted */
	WCHAR *path;  /* path in the registry rooted at root  */
	WCHAR *name;  /* name of the registry value. if null, this means delete the key  */
	WCHAR *value; /* contents of the registry value. if null, this means delete the value  */
	DWORD type;   /* type of registry value. REG_SZ or REG_DWORD for now */
};

struct list *settings;

void initialize_settings(void) {
	DWORD res = RegCreateKeyA(HKEY_CURRENT_USER, WINE_KEY_ROOT, &config_key);

	if (res != ERROR_SUCCESS) {
		WINE_ERR("RegOpenKey failed on wine config key (%d)\n", res);
		exit(1);
	}

	/* we could probably just have the list as static data  */
	settings = HeapAlloc(GetProcessHeap(), 0, sizeof(struct list));
	list_init(settings);
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
void set_reg_key_ex(HKEY root, const WCHAR *path, const WCHAR *name, const void *value, DWORD type) {
	struct list *cursor;
	struct setting *s;

	assert(path != NULL);

	WINE_TRACE("path=%s, name=%s, value=%s\n", wine_dbgstr_w(path), wine_dbgstr_w(name), wine_dbgstr_w(value));

	/* firstly, see if we already set this setting  */
	LIST_FOR_EACH(cursor, settings) {
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
		switch (type) {
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
	s->name  = name ? strdupW(name) : NULL;
	s->type  = type;
	switch (type) {
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

void set_reg_key(HKEY root, const char *path, const char *name, const char *value) {
	WCHAR *wpath, *wname = NULL, *wvalue = NULL;

	wpath = HeapAlloc(GetProcessHeap(), 0, (strlen(path)+1)*sizeof(WCHAR));
	MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, strlen(path)+1);

	if (name) {
		wname = HeapAlloc(GetProcessHeap(), 0, (strlen(name)+1)*sizeof(WCHAR));
		MultiByteToWideChar(CP_ACP, 0, name, -1, wname, strlen(name)+1);
	}

	if (value) {
		wvalue = HeapAlloc(GetProcessHeap(), 0, (strlen(value)+1)*sizeof(WCHAR));
		MultiByteToWideChar(CP_ACP, 0, value, -1, wvalue, strlen(value)+1);
	}

	set_reg_key_ex(root, wpath, wname, wvalue, REG_SZ);

	HeapFree(GetProcessHeap(), 0, wpath);
	HeapFree(GetProcessHeap(), 0, wname);
	HeapFree(GetProcessHeap(), 0, wvalue);
}


/* this is called from the WM_SHOWWINDOW handlers of each tab page.
 *
 * it's a nasty hack, necessary because the property sheet insists on resetting the window title
 * to the title of the tab, which is utterly useless. dropping the property sheet is on the todo list.
 */
void set_window_title(HWND dialog) {
	WCHAR newtitle[256];

	/* update the window title  */
	if (current_app) {
		WCHAR apptitle[256];
		LoadStringW (hInst, IDS_CPL_NAME_APP, apptitle, sizeof(apptitle)/sizeof(apptitle[0]));
		wsprintfW (newtitle, apptitle, current_app);
	} else {
		LoadStringW (hInst, IDS_CPL_NAME, newtitle, sizeof(newtitle)/sizeof(newtitle[0]));
	}

	SendMessageW (GetParent(dialog), PSM_SETTITLEW, 0, (LPARAM) newtitle);
}

/**
 * Returns the contents of the value at path. If not in the settings
 * list, it will be fetched from the registry - failing that, the
 * default will be used.
 *
 * If already in the list, the contents as given there will be
 * returned. You are expected to HeapFree the result.
 */
char *get_reg_key(HKEY root, const char *path, const char *name, const char *def) {
	WCHAR *wpath, *wname, *wdef = NULL, *wRet = NULL;
	char *szRet = NULL;
	int len;

	WINE_TRACE("path=%s, name=%s, def=%s\n", path, name, def);

	wpath = HeapAlloc(GetProcessHeap(), 0, (strlen(path)+1)*sizeof(WCHAR));
	wname = HeapAlloc(GetProcessHeap(), 0, (strlen(name)+1)*sizeof(WCHAR));

	MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, strlen(path)+1);
	MultiByteToWideChar(CP_ACP, 0, name, -1, wname, strlen(name)+1);

	if (def) {
		wdef = HeapAlloc(GetProcessHeap(), 0, (strlen(def)+1)*sizeof(WCHAR));
		MultiByteToWideChar(CP_ACP, 0, def, -1, wdef, strlen(def)+1);
	}

	wRet = get_reg_keyW(root, wpath, wname, wdef);

	len = WideCharToMultiByte(CP_ACP, 0, wRet, -1, NULL, 0, NULL, NULL);
	if (len) {
		szRet = HeapAlloc(GetProcessHeap(), 0, len);
		WideCharToMultiByte(CP_ACP, 0, wRet, -1, szRet, len, NULL, NULL);
	}

	HeapFree(GetProcessHeap(), 0, wpath);
	HeapFree(GetProcessHeap(), 0, wname);
	HeapFree(GetProcessHeap(), 0, wdef);
	HeapFree(GetProcessHeap(), 0, wRet);

	return szRet;
}
WCHAR *get_reg_keyW(HKEY root, const WCHAR *path, const WCHAR *name, const WCHAR *def) {
	struct list *cursor;
	struct setting *s;
	WCHAR *val;

	WINE_TRACE("path=%s, name=%s, def=%s\n", wine_dbgstr_w(path), wine_dbgstr_w(name), wine_dbgstr_w(def));

	/* check if it's in the list */
	LIST_FOR_EACH(cursor, settings) {
		s = LIST_ENTRY(cursor, struct setting, entry);

		if (root != s->root) continue;
		if (lstrcmpiW(path, s->path) != 0) continue;
		if (!s->name) continue;
		if (lstrcmpiW(name, s->name) != 0) continue;

		WINE_TRACE("found %s:%s in settings list, returning %s\n", wine_dbgstr_w(path), wine_dbgstr_w(name), wine_dbgstr_w(s->value));
		return s->value ? strdupW(s->value) : NULL;
	}

	/* no, so get from the registry */
	val = get_config_key(root, path, name, def);

	WINE_TRACE("returning %s\n", wine_dbgstr_w(val));

	return val;
}

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
WCHAR *get_config_key (HKEY root, const WCHAR *subkey, const WCHAR *name, const WCHAR *def) {
	LPWSTR buffer = NULL;
	DWORD len;
	HKEY hSubKey = NULL;
	DWORD res;

	WINE_TRACE("subkey=%s, name=%s, def=%s\n", wine_dbgstr_w(subkey), wine_dbgstr_w(name), wine_dbgstr_w(def));

	res = RegOpenKeyExW(root, subkey, 0, MAXIMUM_ALLOWED, &hSubKey);
	if (res != ERROR_SUCCESS) {
		if (res == ERROR_FILE_NOT_FOUND) {
			WINE_TRACE("Section key not present - using default\n");
			return def ? strdupW(def) : NULL;
		} else {
			WINE_ERR("RegOpenKey failed on wine config key (res=%d)\n", res);
		}
		goto end;
	}

	res = RegQueryValueExW(hSubKey, name, NULL, NULL, NULL, &len);
	if (res == ERROR_FILE_NOT_FOUND) {
		WINE_TRACE("Value not present - using default\n");
		buffer = def ? strdupW(def) : NULL;
	goto end;
	} else if (res != ERROR_SUCCESS) {
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
static int set_config_key(HKEY root, const WCHAR *subkey, REGSAM access, const WCHAR *name, const void *value, DWORD type) {
	DWORD res = 1;
	HKEY key = NULL;

	WINE_TRACE("subkey=%s: name=%s, value=%p, type=%d\n", wine_dbgstr_w(subkey), wine_dbgstr_w(name), value, type);

	assert(subkey != NULL);

	if (subkey[0]) {
		res = RegCreateKeyExW( root, subkey, 0, NULL, REG_OPTION_NON_VOLATILE, access, NULL, &key, NULL );
		if (res != ERROR_SUCCESS) goto end;
	}
	else key = root;
	if (name == NULL || value == NULL) goto end;

	switch (type) {
		case REG_SZ: res = RegSetValueExW(key, name, 0, REG_SZ, value, (lstrlenW(value)+1)*sizeof(WCHAR)); break;
		case REG_DWORD: res = RegSetValueExW(key, name, 0, REG_DWORD, value, sizeof(DWORD)); break;
	}
	if (res != ERROR_SUCCESS) goto end;

	res = 0;
end:
	if (key && key != root) RegCloseKey(key);
	if (res != 0)
		WINE_ERR("Unable to set configuration key %s in section %s, res=%d\n", wine_dbgstr_w(name), wine_dbgstr_w(subkey), res);
	return res;
}


static void process_setting(struct setting *s)
{
	static const WCHAR softwareW[] = {'S','o','f','t','w','a','r','e','\\'};
	HKEY key;
	BOOL needs_wow64 = (is_win64 && s->root == HKEY_LOCAL_MACHINE && s->path && !strncmpiW(s->path, softwareW, sizeof(softwareW)/sizeof(WCHAR)));

	if (s->value) {
		WINE_TRACE("Setting %s:%s to '%s'\n", wine_dbgstr_w(s->path), wine_dbgstr_w(s->name), wine_dbgstr_w(s->value));
		set_config_key(s->root, s->path, MAXIMUM_ALLOWED, s->name, s->value, s->type);
		if (needs_wow64) {
			WINE_TRACE("Setting 32-bit %s:%s to '%s'\n", wine_dbgstr_w(s->path), wine_dbgstr_w(s->name), wine_dbgstr_w(s->value));
			set_config_key(s->root, s->path, MAXIMUM_ALLOWED | KEY_WOW64_32KEY, s->name, s->value, s->type);
		}
	} else {
	WINE_TRACE("Removing %s:%s\n", wine_dbgstr_w(s->path), wine_dbgstr_w(s->name));
		if (!RegOpenKeyExW(s->root, s->path, 0, MAXIMUM_ALLOWED, &key)) {
			/* NULL name means remove that path/section entirely */
			if (s->name) {
				RegDeleteValueW(key, s->name);
			}else {
				RegDeleteTreeW(key, NULL);
				RegDeleteKeyW(s->root, s->path);
			}
			RegCloseKey(key);
		}
		if (needs_wow64) {
			WINE_TRACE("Removing 32-bit %s:%s\n", wine_dbgstr_w(s->path), wine_dbgstr_w(s->name));
			if (!RegOpenKeyExW(s->root, s->path, 0, MAXIMUM_ALLOWED | KEY_WOW64_32KEY, &key)) {
				if (s->name) {
					RegDeleteValueW( key, s->name );
				} else {
					RegDeleteTreeW( key, NULL );
					RegDeleteKeyExW( s->root, s->path, KEY_WOW64_32KEY, 0 );
				}
				RegCloseKey( key );
			}
		}
	}
}

static void free_setting(struct setting *setting) {
	assert(setting != NULL);
	assert(setting->path);

	WINE_TRACE("destroying %p: %s\n", setting, wine_dbgstr_w(setting->path));

	HeapFree(GetProcessHeap(), 0, setting->path);
	HeapFree(GetProcessHeap(), 0, setting->name);
	HeapFree(GetProcessHeap(), 0, setting->value);

	list_remove(&setting->entry);

	HeapFree(GetProcessHeap(), 0, setting);
}

void apply(void) {
	if (list_empty(settings)) return; /* we will be called for each page when the user clicks OK */

	WINE_TRACE("()\n");

	while (!list_empty(settings)) {
		struct setting *s = (struct setting *) list_head(settings);
		process_setting(s);
		free_setting(s);
	}
}

WCHAR *keypathW(const WCHAR *section) {
	static const WCHAR appdefaultsW[] = {'A','p','p','D','e','f','a','u','l','t','s','\\',0};
	static WCHAR *result = NULL;

	HeapFree(GetProcessHeap(), 0, result);

	if (current_app) {
		DWORD len = sizeof(appdefaultsW) + (lstrlenW(current_app) + lstrlenW(section) + 1) * sizeof(WCHAR);
		result = HeapAlloc(GetProcessHeap(), 0, len );
		lstrcpyW( result, appdefaultsW );
		lstrcatW( result, current_app );
		if (section[0]) {
			len = lstrlenW(result);
			result[len++] = '\\';
			lstrcpyW( result + len, section );
		}
	} else {
		result = strdupW(section);
	}

	return result;
}


/* returns a registry key path suitable for passing to addTransaction  */
char *keypath(const char *section) {
	static char *result = NULL;

	HeapFree(GetProcessHeap(), 0, result);

	if (current_app) {
		result = HeapAlloc(GetProcessHeap(), 0, strlen("AppDefaults\\") + lstrlenW(current_app)*2 + 2 /* \\ */ + strlen(section) + 1 /* terminator */);
		wsprintfA(result, "AppDefaults\\%ls", current_app);
		if (section[0]) sprintf( result + strlen(result), "\\%s", section );
	} else {
		result = strdupA(section);
	}

	return result;
}

/**
 * returns true if the given key/value pair exists in the registry or
 * has been written to.
 */
BOOL reg_key_exists(HKEY root, const char *path, const char *name) {
	char *val = get_reg_key(root, path, name, NULL);
	if (val) {
		HeapFree(GetProcessHeap(), 0, val);
		return TRUE;
	}
	return FALSE;
}
