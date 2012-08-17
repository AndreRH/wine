/*
 * Desktop Settings control panel applet
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

#define NONAMELESSUNION

#include "config.h"
#include "wine/port.h"

#include <windef.h>
#include <cpl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <wine/unicode.h>

#include "desk.h"
#include "helpers.h"
#include "appearance.h"
#include "settings.h"

HINSTANCE hInst;

WCHAR* current_app = NULL;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	TRACE("DllMain(hInstDLL=%p, fdwReason=%d, lpvReserved=%p)\n", hinstDLL, fdwReason, lpvReserved);
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			hInst = hinstDLL;
			break;
	}
	return TRUE;
}

static int CALLBACK propsheet_callback(HWND hwnd, UINT msg, LPARAM lparam) {
	TRACE("propsheet_callback(hwnd=%p, msg=0x%08x/%d, lparam=0x%lx)\n", hwnd, msg, msg, lparam);
	switch (msg) {
		case PSCB_INITIALIZED:
			SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM) LoadIconW(hInst, MAKEINTRESOURCEW(ICO_MAIN)));
			break;
	}
	return 0;
}

static void start(HWND hWnd) {
	INITCOMMONCONTROLSEX icex;
	PROPSHEETPAGEW psp[NUM_PROPERTY_PAGES];
	PROPSHEETHEADERW psh;

	initialize_settings();

	OleInitialize(NULL);  
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
	InitCommonControlsEx(&icex);

	ZeroMemory(&psh, sizeof(psh));
	ZeroMemory(psp, sizeof(psp));

	/* Fill out the Appearance page */
	psp[0].dwSize = sizeof(PROPSHEETPAGEW);
	psp[0].dwFlags = PSP_USETITLE; 
	psp[0].hInstance = hInst;
	psp[0].u.pszTemplate = MAKEINTRESOURCEW(IDD_APPEARANCE);
	psp[0].pszTitle = MAKEINTRESOURCEW(IDS_TAB_APPEARANCE);
	psp[0].pfnDlgProc = AppearanceDlgProc;

	/* Fill out the Settings page */
	psp[1].dwSize = sizeof(PROPSHEETPAGEW);
	psp[1].dwFlags = PSP_USETITLE;	 
	psp[1].hInstance = hInst;
	psp[1].u.pszTemplate = MAKEINTRESOURCEW(IDD_SETTINGS);
	psp[1].pszTitle = MAKEINTRESOURCEW(IDS_TAB_SETTINGS);
	psp[1].pfnDlgProc = SettingsDlgProc;

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

LONG CALLBACK CPlApplet(HWND hWnd, UINT command, LPARAM lParam1, LPARAM lParam2) {
	TRACE("CPlApplet(hWnd=%p, command=%u, lParam1=0x%lx, lParam2=0x%lx)\n", hWnd, command, lParam1, lParam2);

	switch (command) {
		case CPL_INIT:
			return TRUE;

		case CPL_GETCOUNT:
			return 2;

		case CPL_INQUIRE: {
			CPLINFO *appletInfo = (CPLINFO *) lParam2;
			appletInfo->idIcon = ICO_MAIN;
			appletInfo->idName = IDS_CPL_NAME;
			appletInfo->idInfo = IDS_CPL_INFO;
			appletInfo->lData = 0;
			return TRUE;
		}

		case CPL_STARTWPARMSW:
			/* Get app name */
			current_app = strchrW((WCHAR*)lParam2, ':');
			/* Check if there was an app name */
			if (current_app && current_app[1])
				++current_app; /* Ignore the colon character (current_app is WCHAR* so ++ will move to the next WCHAR) */
			else
				current_app = NULL;
			TRACE("AppDefaults current_app=%s\n", wine_dbgstr_w(current_app));
			break;

		case CPL_DBLCLK:
			start(hWnd);
			break;
	}

	return FALSE;
}

