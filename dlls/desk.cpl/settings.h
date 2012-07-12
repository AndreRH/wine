#ifndef __WINE_DESKCPL_SETTINGSH__
#define __WINE_DESKCPL_SETTINGSH__

#include <windows.h>

#define MINDPI 96
#define MAXDPI 480
#define DEFDPI 96
#define RES_MAXLEN 5 /* max number of digits in a screen dimension. 5 digits should be plenty */

#define disable(id) EnableWindow(GetDlgItem(dialog, id), 0);
#define enable(id) EnableWindow(GetDlgItem(dialog, id), 1);

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif
