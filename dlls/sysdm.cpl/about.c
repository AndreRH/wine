/*
 * System Properties control panel applet
 * "About" page
 *
 * Copyright 2002 Jaco Greeff
 * Copyright 2003 Dimitrie O. Paun
 * Copyright 2003 Mike Hearn
 * Copyright 2012 Magdalena Nowak
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */


#include "config.h"
#include "sysdm.h"

#include <winbase.h>
#include <winuser.h>

#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

#include <wine/unicode.h> 

#include "about.h"
#include "helpers.h"
#include "resources.h"


extern HINSTANCE hInst;
extern HKEY config_key;
extern WCHAR* current_app;

static HICON logo = NULL;
static HFONT titleFont = NULL;

static const struct
{
    const char *szVersion;
    const char *szDescription;
    DWORD       dwMajorVersion;
    DWORD       dwMinorVersion;
    DWORD       dwBuildNumber;
    DWORD       dwPlatformId;
    const char *szCSDVersion;
    WORD        wServicePackMajor;
    WORD        wServicePackMinor;
    const char *szProductType;
} win_versions[] =
{
    { "win2008r2",   "Windows 2008 R2",   6,  1, 0x1DB1,VER_PLATFORM_WIN32_NT, "Service Pack 1", 1, 0, "ServerNT"},
    { "win7",        "Windows 7",         6,  1, 0x1DB1,VER_PLATFORM_WIN32_NT, "Service Pack 1", 1, 0, "WinNT"},
    { "win2008",     "Windows 2008",      6,  0, 0x1772,VER_PLATFORM_WIN32_NT, "Service Pack 2", 2, 0, "ServerNT"},
    { "vista",       "Windows Vista",     6,  0, 0x1772,VER_PLATFORM_WIN32_NT, "Service Pack 2", 2, 0, "WinNT"},
    { "win2003",     "Windows 2003",      5,  2, 0xECE, VER_PLATFORM_WIN32_NT, "Service Pack 2", 2, 0, "ServerNT"},
    { "winxp",       "Windows XP",        5,  1, 0xA28, VER_PLATFORM_WIN32_NT, "Service Pack 3", 3, 0, "WinNT"},
    { "win2k",       "Windows 2000",      5,  0, 0x893, VER_PLATFORM_WIN32_NT, "Service Pack 4", 4, 0, "WinNT"},
    { "winme",       "Windows ME",        4, 90, 0xBB8, VER_PLATFORM_WIN32_WINDOWS, " ", 0, 0, ""},
    { "win98",       "Windows 98",        4, 10, 0x8AE, VER_PLATFORM_WIN32_WINDOWS, " A ", 0, 0, ""},
    { "win95",       "Windows 95",        4,  0, 0x3B6, VER_PLATFORM_WIN32_WINDOWS, "", 0, 0, ""},
    { "nt40",        "Windows NT 4.0",    4,  0, 0x565, VER_PLATFORM_WIN32_NT, "Service Pack 6a", 6, 0, "WinNT"},
    { "nt351",       "Windows NT 3.51",   3, 51, 0x421, VER_PLATFORM_WIN32_NT, "Service Pack 5", 5, 0, "WinNT"},
    { "win31",       "Windows 3.1",       3, 10,     0, VER_PLATFORM_WIN32s, "Win32s 1.3", 0, 0, ""},
    { "win30",       "Windows 3.0",       3,  0,     0, VER_PLATFORM_WIN32s, "Win32s 1.3", 0, 0, ""},
    { "win20",       "Windows 2.0",       2,  0,     0, VER_PLATFORM_WIN32s, "Win32s 1.3", 0, 0, ""}
};

#define NB_VERSIONS (sizeof(win_versions)/sizeof(win_versions[0]))

static const char szKey9x[] = "Software\\Microsoft\\Windows\\CurrentVersion";
static const char szKeyNT[] = "Software\\Microsoft\\Windows NT\\CurrentVersion";
static const char szKeyProdNT[] = "System\\CurrentControlSet\\Control\\ProductOptions";

static int get_registry_version(void)
{
    int i, best = -1, platform, major, minor = 0, build = 0;
    char *p, *ver, *type = NULL;

    if ((ver = get_reg_key( HKEY_LOCAL_MACHINE, szKeyNT, "CurrentVersion", NULL )))
    {
        char *build_str;

        platform = VER_PLATFORM_WIN32_NT;

        build_str = get_reg_key( HKEY_LOCAL_MACHINE, szKeyNT, "CurrentBuildNumber", NULL );
        build = atoi(build_str);

        type = get_reg_key( HKEY_LOCAL_MACHINE, szKeyProdNT, "ProductType", NULL );
    }
    else if ((ver = get_reg_key( HKEY_LOCAL_MACHINE, szKey9x, "VersionNumber", NULL )))
        platform = VER_PLATFORM_WIN32_WINDOWS;
    else
        return -1;

    if ((p = strchr( ver, '.' )))
    {
        char *minor_str = p;
        *minor_str++ = 0;
        if ((p = strchr( minor_str, '.' )))
        {
            char *build_str = p;
            *build_str++ = 0;
            build = atoi(build_str);
        }
        minor = atoi(minor_str);
    }
    major = atoi(ver);

    for (i = 0; i < NB_VERSIONS; i++)
    {
        if (win_versions[i].dwPlatformId != platform) continue;
        if (win_versions[i].dwMajorVersion != major) continue;
        if (type && strcasecmp(win_versions[i].szProductType, type)) continue;
        best = i;
        if ((win_versions[i].dwMinorVersion == minor) &&
            (win_versions[i].dwBuildNumber == build))
            return i;
    }
    return best;
}

static void update_comboboxes(HWND dialog)
{
    int i, ver;
    char *winver;

    /* retrieve the registry values */
    winver = get_reg_key(config_key, keypath(""), "Version", "");
    ver = get_registry_version();

    if (!winver || !winver[0])
    {
        HeapFree(GetProcessHeap(), 0, winver);

        if (current_app) /* no explicit setting */
        {
            WINE_TRACE("setting winver combobox to default\n");
            SendDlgItemMessageW(dialog, IDC_WINVER, CB_SETCURSEL, 0, 0);
            return;
        }
        if (ver != -1) winver = strdupA( win_versions[ver].szVersion );
        else winver = strdupA("winxp");
    }
    WINE_TRACE("winver is %s\n", winver);

    /* normalize the version strings */
    for (i = 0; i < NB_VERSIONS; i++)
    {
        if (!strcasecmp (win_versions[i].szVersion, winver))
        {
            SendDlgItemMessageW(dialog, IDC_WINVER, CB_SETCURSEL,
                                i + (current_app?1:0), 0);
            WINE_TRACE("match with %s\n", win_versions[i].szVersion);
            break;
	}
    }

    HeapFree(GetProcessHeap(), 0, winver);
}

static void
init_comboboxes (HWND dialog)
{
    int i;

    SendDlgItemMessageW(dialog, IDC_WINVER, CB_RESETCONTENT, 0, 0);

    /* add the default entries (automatic) which correspond to no setting  */
    if (current_app)
    {
        WCHAR str[256];
        LoadStringW (GetModuleHandleW(NULL), IDS_USE_GLOBAL_SETTINGS, str,
            sizeof(str)/sizeof(str[0]));
        SendDlgItemMessageW (dialog, IDC_WINVER, CB_ADDSTRING, 0, (LPARAM)str);
    }

    for (i = 0; i < NB_VERSIONS; i++)
    {
      SendDlgItemMessageA(dialog, IDC_WINVER, CB_ADDSTRING,
                          0, (LPARAM) win_versions[i].szDescription);
    }
}

static void on_winver_change(HWND dialog)
{
    int selection = SendDlgItemMessageW(dialog, IDC_WINVER, CB_GETCURSEL, 0, 0);

    if (current_app)
    {
        if (!selection)
        {
            WINE_TRACE("default selected so removing current setting\n");
            set_reg_key(config_key, keypath(""), "Version", NULL);
        }
        else
        {
            WINE_TRACE("setting Version key to value '%s'\n", win_versions[selection-1].szVersion);
            set_reg_key(config_key, keypath(""), "Version", win_versions[selection-1].szVersion);
        }
    }
    else /* global version only */
    {
        static const char szKeyWindNT[] = "System\\CurrentControlSet\\Control\\Windows";
        static const char szKeyEnvNT[]  = "System\\CurrentControlSet\\Control\\Session Manager\\Environment";
        char Buffer[40];

        switch (win_versions[selection].dwPlatformId)
        {
        case VER_PLATFORM_WIN32_WINDOWS:
            snprintf(Buffer, sizeof(Buffer), "%d.%d.%d", win_versions[selection].dwMajorVersion,
                     win_versions[selection].dwMinorVersion, win_versions[selection].dwBuildNumber);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "VersionNumber", Buffer);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "SubVersionNumber", win_versions[selection].szCSDVersion);
            snprintf(Buffer, sizeof(Buffer), "Microsoft %s", win_versions[selection].szDescription);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "ProductName", Buffer);

            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CSDVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentBuildNumber", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "ProductName", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyProdNT, "ProductType", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyWindNT, "CSDVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyEnvNT, "OS", NULL);
            set_reg_key(config_key, keypath(""), "Version", NULL);
            break;

        case VER_PLATFORM_WIN32_NT:
            snprintf(Buffer, sizeof(Buffer), "%d.%d", win_versions[selection].dwMajorVersion,
                     win_versions[selection].dwMinorVersion);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentVersion", Buffer);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CSDVersion", win_versions[selection].szCSDVersion);
            snprintf(Buffer, sizeof(Buffer), "%d", win_versions[selection].dwBuildNumber);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentBuildNumber", Buffer);
            snprintf(Buffer, sizeof(Buffer), "Microsoft %s", win_versions[selection].szDescription);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "ProductName", Buffer);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyProdNT, "ProductType", win_versions[selection].szProductType);
            set_reg_key_dword(HKEY_LOCAL_MACHINE, szKeyWindNT, "CSDVersion", MAKEWORD(win_versions[selection].wServicePackMinor, win_versions[selection].wServicePackMajor));
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyEnvNT, "OS", "Windows_NT");

            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "VersionNumber", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "SubVersionNumber", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "ProductName", NULL);
            set_reg_key(config_key, keypath(""), "Version", NULL);
            break;

        case VER_PLATFORM_WIN32s:
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CSDVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentBuildNumber", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "ProductName", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyProdNT, "ProductType", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyWindNT, "CSDVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyEnvNT, "OS", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "VersionNumber", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "SubVersionNumber", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "ProductName", NULL);
            set_reg_key(config_key, keypath(""), "Version", win_versions[selection].szVersion);
            break;
        }
    }

    /* enable the apply button  */
    SendMessageW(GetParent(dialog), PSM_CHANGED, (WPARAM) dialog, 0);
}


INT_PTR CALLBACK AboutDlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	HWND hWnd;
	HDC hDC;
	RECT rcClient, rcRect;
	char *owner, *org;

	switch (uMsg) {
		case WM_NOTIFY:
			switch(((LPNMHDR)lParam)->code) {
				case PSN_APPLY:
					/*save registration info to registry */
					owner = get_text(hDlg, IDC_ABT_OWNER);
					org =   get_text(hDlg, IDC_ABT_ORG);

					set_reg_key(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "RegisteredOwner", owner ? owner : "");
					set_reg_key(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", "RegisteredOrganization", org ? org : "");
					set_reg_key(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", "RegisteredOwner", owner ? owner : "");
					set_reg_key(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", "RegisteredOrganization", org ? org : "");
					apply();

					HeapFree(GetProcessHeap(), 0, owner);
					HeapFree(GetProcessHeap(), 0, org);
					break;
				case NM_CLICK:
				case NM_RETURN:
					if(wParam == IDC_ABT_WEB_LINK)
						ShellExecuteA(NULL, "open", PACKAGE_URL, NULL, NULL, SW_SHOW);
					break;
			}
			break;

		case WM_INITDIALOG:
			init_comboboxes(hDlg);
			update_comboboxes(hDlg);
			
			hDC = GetDC(hDlg);

			/* read owner and organization info from registry, load it into text box */
			owner = get_reg_key(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", "RegisteredOwner", "");
			org =   get_reg_key(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", "RegisteredOrganization", "");

			SetDlgItemTextA(hDlg, IDC_ABT_OWNER, owner);
			SetDlgItemTextA(hDlg, IDC_ABT_ORG, org);

			SendMessageW(GetParent(hDlg), PSM_UNCHANGED, 0, 0);

			HeapFree(GetProcessHeap(), 0, owner);
			HeapFree(GetProcessHeap(), 0, org);

			/* prepare the panel */
			hWnd = GetDlgItem(hDlg, IDC_ABT_PANEL);
			if (hWnd) {
				GetClientRect(hDlg, &rcClient);
				GetClientRect(hWnd, &rcRect);
				MoveWindow(hWnd, 0, 0, rcClient.right, rcRect.bottom, FALSE);
				logo = LoadImageW((HINSTANCE)GetWindowLongPtrW(hDlg, GWLP_HINSTANCE), MAKEINTRESOURCEW(IDI_LOGO), IMAGE_ICON, 0, 0, LR_SHARED);
			}

			/* prepare the title text */
			hWnd = GetDlgItem(hDlg, IDC_ABT_TITLE_TEXT);
			if(hWnd) {
				static const WCHAR tahomaW[] = {'T','a','h','o','m','a',0};
				titleFont = CreateFontW(-MulDiv(24, GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0, 0, FALSE, 0, 0, 0, 0, 0, 0, 0, tahomaW);
				SendMessageW(hWnd, WM_SETFONT, (WPARAM)titleFont, TRUE);
				SetWindowTextA(hWnd, PACKAGE_NAME);
			}
			SetDlgItemTextA(hDlg, IDC_ABT_PANEL_TEXT, PACKAGE_VERSION);

			/* prepare the web link */
			SetDlgItemTextA(hDlg, IDC_ABT_WEB_LINK, "<a href=\"" PACKAGE_URL "\">" PACKAGE_URL "</a>");

			
			ReleaseDC(hDlg, hDC);

			break;

		case WM_DESTROY:
			if (logo) {
				DestroyIcon(logo);
				logo = NULL;
			}
			if (titleFont) {
				DeleteObject(titleFont);
				titleFont = NULL;
			}
			break;

		case WM_COMMAND:
			switch (HIWORD(wParam)) {
				case EN_CHANGE:
					/* enable apply button */
					SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
					break;
				case CBN_SELCHANGE:
          				switch (LOWORD(wParam)) {
						case IDC_WINVER:
							on_winver_change(hDlg);
							break;
					}
			}
			break;

		case WM_DRAWITEM:
			if(wParam == IDC_ABT_PANEL) {
				LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
				FillRect(pDIS->hDC, &pDIS->rcItem, (HBRUSH) (COLOR_WINDOW+1));
				DrawIconEx(pDIS->hDC, 0, 0, logo, 0, 0, 0, 0, DI_IMAGE);
				DrawEdge(pDIS->hDC, &pDIS->rcItem, EDGE_SUNKEN, BF_BOTTOM);
			}
			break;

		case WM_CTLCOLORSTATIC:
			switch(GetDlgCtrlID((HWND)lParam)) {
				case IDC_ABT_TITLE_TEXT:
					/* set the title to a wine color */
					SetTextColor((HDC)wParam, 0x0000007F);
				case IDC_ABT_PANEL_TEXT:
				case IDC_ABT_LICENSE_TEXT:
				case IDC_ABT_WEB_LINK:
					return (INT_PTR)CreateSolidBrush(GetSysColor(COLOR_WINDOW));
			}
			break;

		case WM_SHOWWINDOW:
			set_window_title(hDlg);
			break;
	}

	return FALSE;
}
