/*
 * Audio Settings control panel applet
 * Helpers
 * 
 * Copyright 2002 Jaco Greeff
 * Copyright 2003 Dimitrie O. Paun
 * Copyright 2003-2004 Mike Hearn
 * Copyright 2004 Chris Morgan 
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

#define WIN32_LEAN_AND_MEAN
#define NONAMELESSSTRUCT
#define NONAMELESSUNION
#define COBJMACROS

#include "mmsys.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <initguid.h>
#include <devpkey.h>

#include <shellapi.h>
#include <objbase.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <mmdeviceapi.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <mmddk.h>
#include <wine/list.h>
#include <wine/unicode.h>

#include "helpers.h"

extern HINSTANCE hInst;
extern WCHAR* current_app;
static const int is_win64 = (sizeof(void *) > sizeof(int));

static BOOL devices_found = FALSE;

UINT num_render_devs, num_capture_devs, selected_out_dev, selected_vout_dev, selected_in_dev, selected_vin_dev;
struct DeviceInfo *render_devs, *capture_devs;
WCHAR display_str[256], sysdefault_str[256];


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

void set_reg_device(HWND hDlg, int dlgitem, const WCHAR *key_name) {
	UINT idx;
	struct DeviceInfo *info;

	idx = SendDlgItemMessageW(hDlg, dlgitem, CB_GETCURSEL, 0, 0);

	info = (struct DeviceInfo *)SendDlgItemMessageW(hDlg, dlgitem, CB_GETITEMDATA, idx, 0);

	if (!info || info == (void*)CB_ERR)
		set_reg_key_ex(HKEY_CURRENT_USER, g_drv_keyW, key_name, NULL, REG_SZ);
	else
		set_reg_key_ex(HKEY_CURRENT_USER, g_drv_keyW, key_name, info->id, REG_SZ);
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

static void process_setting(struct setting *s) {
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

static BOOL load_device(IMMDevice *dev, struct DeviceInfo *info) {
	IPropertyStore *ps;
	HRESULT hr;

	hr = IMMDevice_GetId(dev, &info->id);
	if (FAILED(hr)) {
		info->id = NULL;
		return FALSE;
	}

	hr = IMMDevice_OpenPropertyStore(dev, STGM_READ, &ps);
	if (FAILED(hr)) {
		CoTaskMemFree(info->id);
		info->id = NULL;
		return FALSE;
	}

	PropVariantInit(&info->name);

	hr = IPropertyStore_GetValue(ps, (PROPERTYKEY*)&DEVPKEY_Device_FriendlyName, &info->name);
	IPropertyStore_Release(ps);
	if (FAILED(hr)) {
		CoTaskMemFree(info->id);
		info->id = NULL;
		return FALSE;
	}

	return TRUE;
}

static BOOL load_devices(IMMDeviceEnumerator *devenum, EDataFlow dataflow, UINT *ndevs, struct DeviceInfo **out) {
	IMMDeviceCollection *coll;
	UINT i;
	HRESULT hr;

	hr = IMMDeviceEnumerator_EnumAudioEndpoints(devenum, dataflow, DEVICE_STATE_ACTIVE, &coll);
	if (FAILED(hr))
		return FALSE;

	hr = IMMDeviceCollection_GetCount(coll, ndevs);
	if (FAILED(hr)) {
		IMMDeviceCollection_Release(coll);
		return FALSE;
	}

	if (*ndevs > 0) {
		*out = HeapAlloc(GetProcessHeap(), 0, sizeof(struct DeviceInfo) * (*ndevs));
		if (!*out) {
			IMMDeviceCollection_Release(coll);
			return FALSE;
		}

		for (i = 0; i < *ndevs; ++i) {
			IMMDevice *dev;

			hr = IMMDeviceCollection_Item(coll, i, &dev);
			if (FAILED(hr)) {
				(*out)[i].id = NULL;
				continue;
			}

			load_device(dev, &(*out)[i]);

			IMMDevice_Release(dev);
		}
	} else
		*out = NULL;

	IMMDeviceCollection_Release(coll);

	return TRUE;
}

static BOOL get_driver_name(IMMDeviceEnumerator *devenum, PROPVARIANT *pv) {
	IMMDevice *device;
	IPropertyStore *ps;
	HRESULT hr;

	static const WCHAR wine_info_deviceW[] = {'W','i','n','e',' ','i','n','f','o',' ','d','e','v','i','c','e',0};

	hr = IMMDeviceEnumerator_GetDevice(devenum, wine_info_deviceW, &device);
	if (FAILED(hr))
		return FALSE;

	hr = IMMDevice_OpenPropertyStore(device, STGM_READ, &ps);
	if (FAILED(hr)) {
		IMMDevice_Release(device);
		return FALSE;
	}

	hr = IPropertyStore_GetValue(ps, (const PROPERTYKEY *)&DEVPKEY_Device_Driver, pv);
	IPropertyStore_Release(ps);
	IMMDevice_Release(device);
	if(FAILED(hr))
		return FALSE;

	return TRUE;
}

/* Prepares strings for the combo boxes on the property sheets */
void find_devices(void) {
	WCHAR format_str[256], disabled_str[64];
	IMMDeviceEnumerator *devenum;
	BOOL have_driver = FALSE;
	HRESULT hr;

	WINE_ERR("devices_found=%i\n", devices_found);

	if (devices_found) return;

	devices_found = TRUE;

	LoadStringW(hInst, IDS_AUDIO_DRIVER, format_str, sizeof(format_str) / sizeof(*format_str));
	LoadStringW(hInst, IDS_AUDIO_DRIVER_NONE, disabled_str, sizeof(disabled_str) / sizeof(*disabled_str));
	LoadStringW(hInst, IDS_AUDIO_SYSDEFAULT, sysdefault_str, sizeof(sysdefault_str) / sizeof(*sysdefault_str));

	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void**)&devenum);
	if (SUCCEEDED(hr)) {
		WINE_ERR("succeeded\n");
		PROPVARIANT pv;

		load_devices(devenum, eRender, &num_render_devs, &render_devs);
		load_devices(devenum, eCapture, &num_capture_devs, &capture_devs);

		printf("num_render_devs=%i\n", num_render_devs);

		PropVariantInit(&pv);
		if (get_driver_name(devenum, &pv) && pv.u.pwszVal[0] != '\0') {
			have_driver = TRUE;
			wnsprintfW(display_str, sizeof(display_str) / sizeof(*display_str), format_str, pv.u.pwszVal);
			lstrcatW(g_drv_keyW, pv.u.pwszVal);
		}
		PropVariantClear(&pv);

		IMMDeviceEnumerator_Release(devenum);
	}

	if (have_driver) {
		WCHAR *reg_out_dev, *reg_vout_dev, *reg_in_dev, *reg_vin_dev;
		UINT i;

		reg_out_dev = get_reg_keyW(HKEY_CURRENT_USER, g_drv_keyW, reg_out_nameW, NULL);
		reg_vout_dev = get_reg_keyW(HKEY_CURRENT_USER, g_drv_keyW, reg_vout_nameW, NULL);
		reg_in_dev = get_reg_keyW(HKEY_CURRENT_USER, g_drv_keyW, reg_in_nameW, NULL);
		reg_vin_dev = get_reg_keyW(HKEY_CURRENT_USER, g_drv_keyW, reg_vin_nameW, NULL);

		for (i = 0; i < num_render_devs; ++i) {
			if (!render_devs[i].id)
				continue;

			if (reg_out_dev && !lstrcmpW(render_devs[i].id, reg_out_dev))
				selected_out_dev = i + 1;

			if (reg_vout_dev && !lstrcmpW(render_devs[i].id, reg_vout_dev))
				selected_vout_dev = i + 1;
		}

		for (i = 0; i < num_capture_devs; ++i) {
			if (!capture_devs[i].id)
				continue;

			if (reg_in_dev && !lstrcmpW(capture_devs[i].id, reg_in_dev))
				selected_in_dev = i + 1;

			if (reg_vin_dev && !lstrcmpW(capture_devs[i].id, reg_vin_dev))
				selected_vin_dev = i + 1;
		}

		HeapFree(GetProcessHeap(), 0, reg_out_dev);
		HeapFree(GetProcessHeap(), 0, reg_vout_dev);
		HeapFree(GetProcessHeap(), 0, reg_in_dev);
		HeapFree(GetProcessHeap(), 0, reg_vin_dev);
	} else
		wnsprintfW(display_str, sizeof(display_str) / sizeof(*display_str), format_str, disabled_str);
}

