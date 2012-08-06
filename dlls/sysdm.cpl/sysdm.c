/*
 * System Properties control panel applet
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
#include <stdlib.h>

#include "sysdm.h"
#include "about.h"
#include "dlls.h"
#include "drives.h"
#include "folders.h"
#include "helpers.h"

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
	PROPSHEETPAGEW psp[4];
	PROPSHEETHEADERW psh;

	initialize_settings();

	OleInitialize(NULL);  
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
	InitCommonControlsEx(&icex);

	ZeroMemory(&psh, sizeof(psh));
	ZeroMemory(psp, sizeof(psp));

	/* Fill out the About page */
	psp[0].dwSize = sizeof(PROPSHEETPAGEW);
	psp[0].dwFlags = PSP_USETITLE; 
	psp[0].hInstance = hInst;
	psp[0].u.pszTemplate = MAKEINTRESOURCEW(IDD_ABOUT);
	psp[0].pszTitle = MAKEINTRESOURCEW(IDS_TAB_ABOUT);
	psp[0].pfnDlgProc = AboutDlgProc;

	/* Fill out the Libraries page */
	psp[1].dwSize = sizeof(PROPSHEETPAGEW);
	psp[1].dwFlags = PSP_USETITLE;	 
	psp[1].hInstance = hInst;
	psp[1].u.pszTemplate = MAKEINTRESOURCEW(IDD_DLLS);
	psp[1].pszTitle = MAKEINTRESOURCEW(IDS_TAB_DLLS);
	psp[1].pfnDlgProc = DllsDlgProc;

	/* Fill out the Drives page */
	psp[2].dwSize = sizeof(PROPSHEETPAGEW);
	psp[2].dwFlags = PSP_USETITLE;	 
	psp[2].hInstance = hInst;
	psp[2].u.pszTemplate = MAKEINTRESOURCEW(IDD_DRIVES);
	psp[2].pszTitle = MAKEINTRESOURCEW(IDS_TAB_DRIVES);
	psp[2].pfnDlgProc = DrivesDlgProc;

	/* Fill out the Folders page */
	psp[3].dwSize = sizeof(PROPSHEETPAGEW);
	psp[3].dwFlags = PSP_USETITLE;	 
	psp[3].hInstance = hInst;
	psp[3].u.pszTemplate = MAKEINTRESOURCEW(IDD_FOLDERS);
	psp[3].pszTitle = MAKEINTRESOURCEW(IDS_TAB_FOLDERS);
	psp[3].pfnDlgProc = FoldersDlgProc;

	/* Fill out the PROPSHEETHEADER */
	psh.dwSize = sizeof (PROPSHEETHEADERW);
	psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_USECALLBACK;
	psh.hwndParent = hWnd;
	psh.hInstance = hInst;
	psh.u.pszIcon = MAKEINTRESOURCEW(ICO_MAIN);
	psh.pszCaption = MAKEINTRESOURCEW(IDS_CPL_NAME);
	psh.nPages = 4;
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
			return 1;

		case CPL_INQUIRE: {
			CPLINFO *appletInfo = (CPLINFO *) lParam2;
			appletInfo->idIcon = ICO_MAIN;
			appletInfo->idName = IDS_CPL_ICONNAME;
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

