/*
 * Audio Settings control panel applet
 * "Hardware" page
 * 
 * Copyright 2004 Chris Morgan 
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


#include "mmsys.h"

#include <winbase.h>
#include <winuser.h>

#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>

#include "helpers.h"
#include "hardware.h"

extern HINSTANCE hInst;
extern WCHAR display_str[256];

static void test_sound(void) {
	WINE_ERR("\n");
	if (!PlaySoundW(MAKEINTRESOURCEW(IDW_TESTSOUND), hInst, SND_RESOURCE | SND_ASYNC)) {
		WCHAR error_str[256], title_str[256];
		LoadStringW(hInst, IDS_AUDIO_TEST_FAILED, error_str, sizeof(error_str) / sizeof(*error_str));
		LoadStringW(hInst, IDS_AUDIO_TEST_FAILED_TITLE, title_str, sizeof(title_str) / sizeof(*title_str));
		MessageBoxW(NULL, error_str, title_str, MB_OK | MB_ICONERROR);
		WINE_ERR("%s: %s\n", wine_dbgstr_w(title_str), wine_dbgstr_w(error_str));
	}
}

INT_PTR CALLBACK HardwareDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_AUDIO_TEST:
					test_sound();
					break;
			}
			break;
		case WM_SHOWWINDOW:
			set_window_title(hDlg);
			break;
		case WM_NOTIFY:
			switch (((LPNMHDR)lParam)->code) {
				case PSN_KILLACTIVE:
					SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, FALSE);
					break;
				case PSN_APPLY:
					apply();
					SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
					break;
			}
			break;
		case WM_INITDIALOG:
			find_devices();
			SetDlgItemTextW(hDlg, IDC_AUDIO_DRIVER, display_str);
			break;

	}
	return FALSE;
}

