/*
 * Audio Settings control panel applet
 * "Audio" page
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

#define WIN32_LEAN_AND_MEAN
#define NONAMELESSSTRUCT
#define NONAMELESSUNION
#define COBJMACROS

#include "mmsys.h"

#include <winbase.h>
#include <winuser.h>

#include <commctrl.h>
#include <shellapi.h>

#include "helpers.h"
#include "audio.h"

extern UINT num_render_devs, num_capture_devs, selected_out_dev, selected_in_dev;
extern struct DeviceInfo *render_devs, *capture_devs;
extern WCHAR sysdefault_str[256];

INT_PTR CALLBACK AudioDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_AUDIOOUT_DEVICE:
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						set_reg_device(hDlg, IDC_AUDIOOUT_DEVICE, reg_out_nameW);
						SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
					}
					break;
				case IDC_AUDIOIN_DEVICE:
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						set_reg_device(hDlg, IDC_AUDIOIN_DEVICE, reg_in_nameW);
						SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
					}
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
		case WM_INITDIALOG: {
				UINT i;

				find_devices();
				SendDlgItemMessageW(hDlg, IDC_AUDIOOUT_DEVICE, CB_ADDSTRING, 0, (LPARAM)sysdefault_str);
				SendDlgItemMessageW(hDlg, IDC_AUDIOOUT_DEVICE, CB_SETCURSEL, 0, 0);
				SendDlgItemMessageW(hDlg, IDC_AUDIOIN_DEVICE, CB_ADDSTRING, 0, (LPARAM)sysdefault_str);
				SendDlgItemMessageW(hDlg, IDC_AUDIOIN_DEVICE, CB_SETCURSEL, 0, 0);

				for (i = 0; i < num_render_devs; ++i) {
					if (!render_devs[i].id) continue;
					SendDlgItemMessageW(hDlg, IDC_AUDIOOUT_DEVICE, CB_ADDSTRING, 0, (LPARAM)render_devs[i].name.u.pwszVal);
					SendDlgItemMessageW(hDlg, IDC_AUDIOOUT_DEVICE, CB_SETITEMDATA, i + 1, (LPARAM)&render_devs[i]);
				}
				SendDlgItemMessageW(hDlg, IDC_AUDIOOUT_DEVICE, CB_SETCURSEL, selected_out_dev, 0);

				for (i = 0; i < num_capture_devs; ++i) {
					if (!capture_devs[i].id) continue;
					SendDlgItemMessageW(hDlg, IDC_AUDIOIN_DEVICE, CB_ADDSTRING, 0, (LPARAM)capture_devs[i].name.u.pwszVal);
					SendDlgItemMessageW(hDlg, IDC_AUDIOIN_DEVICE, CB_SETITEMDATA, i + 1, (LPARAM)&capture_devs[i]);
				}
				SendDlgItemMessageW(hDlg, IDC_AUDIOIN_DEVICE, CB_SETCURSEL, selected_in_dev, 0);
			}
			break;
	}
	return FALSE;
}

