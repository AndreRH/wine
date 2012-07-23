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

#ifndef __WINE_DESKCPL_RESOURCESH__
#define __WINE_DESKCPL_RESOURCESH__

/* strings */
#define IDS_CPL_NAME          1
#define IDS_CPL_INFO          2
#define IDS_CHOOSE_PATH       5
#define IDS_CPL_NAME_APP      6
#define IDS_NOTHEME           8
#define IDS_THEMEFILE        14
#define IDS_THEMEFILE_SELECT 15
#define IDS_SHELL_FOLDER     16
#define IDS_LINKS_TO         17
#define IDS_TAB_APPEARANCE   19
#define IDS_TAB_SETTINGS     20

/* icons */
#define ICO_MAIN            100

/* dialogs */
#define IDC_STATIC            -1
#define IDD_APPEARANCE       110
#define IDD_SETTINGS         115

/* widgets */
#define IDC_DESKTOP_WIDTH               1023
#define IDC_DESKTOP_HEIGHT              1024
#define IDC_DESKTOP_SIZE                1025
#define IDC_DESKTOP_BY                  1026
#define IDC_ENABLE_DESKTOP              1074
#define IDC_ENABLE_MANAGED              1100
#define IDC_ENABLE_DECORATED            1101
#define IDC_FULLSCREEN_GRAB             1102
#define IDC_RES_TRACKBAR                1107
#define IDC_RES_DPIEDIT                 1108
#define IDC_RES_FONT_PREVIEW            1109
#define IDC_THEME_COLORCOMBO            1401
#define IDC_THEME_COLORTEXT             1402
#define IDC_THEME_SIZECOMBO             1403
#define IDC_THEME_SIZETEXT              1404
#define IDC_THEME_THEMECOMBO            1405
#define IDC_THEME_INSTALL               1406
#define IDC_LIST_SFPATHS                1407
#define IDC_LINK_SFPATH                 1408
#define IDC_EDIT_SFPATH                 1409
#define IDC_BROWSE_SFPATH               1410
#define IDC_SYSPARAM_COMBO              1411
#define IDC_SYSPARAM_SIZE_TEXT          1412
#define IDC_SYSPARAM_SIZE               1413
#define IDC_SYSPARAM_SIZE_UD            1414
#define IDC_SYSPARAM_COLOR_TEXT         1415
#define IDC_SYSPARAM_COLOR              1416
#define IDC_SYSPARAM_FONT               1417
#define IDC_SYSPARAMS_BUTTON            8400
#define IDC_SYSPARAMS_BUTTON_TEXT       8401
#define IDC_SYSPARAMS_DESKTOP           8402
#define IDC_SYSPARAMS_MENU              8403
#define IDC_SYSPARAMS_MENU_TEXT         8404
#define IDC_SYSPARAMS_SCROLLBAR         8405
#define IDC_SYSPARAMS_SELECTION         8406
#define IDC_SYSPARAMS_SELECTION_TEXT    8407
#define IDC_SYSPARAMS_TOOLTIP           8408
#define IDC_SYSPARAMS_TOOLTIP_TEXT      8409
#define IDC_SYSPARAMS_WINDOW            8410
#define IDC_SYSPARAMS_WINDOW_TEXT       8411
#define IDC_SYSPARAMS_ACTIVE_TITLE      8412
#define IDC_SYSPARAMS_ACTIVE_TITLE_TEXT 8413
#define IDC_SYSPARAMS_INACTIVE_TITLE    8414
#define IDC_SYSPARAMS_INACTIVE_TITLE_TEXT 8415
#define IDC_SYSPARAMS_MSGBOX_TEXT       8416
#define IDC_SYSPARAMS_APPWORKSPACE      8417
#define IDC_SYSPARAMS_WINDOW_FRAME      8418
#define IDC_SYSPARAMS_ACTIVE_BORDER     8419
#define IDC_SYSPARAMS_INACTIVE_BORDER   8420
#define IDC_SYSPARAMS_BUTTON_SHADOW     8421
#define IDC_SYSPARAMS_GRAY_TEXT         8422
#define IDC_SYSPARAMS_BUTTON_HILIGHT    8423
#define IDC_SYSPARAMS_BUTTON_DARK_SHADOW 8424
#define IDC_SYSPARAMS_BUTTON_LIGHT      8425
#define IDC_SYSPARAMS_BUTTON_ALTERNATE 8426
#define IDC_SYSPARAMS_HOT_TRACKING      8427
#define IDC_SYSPARAMS_ACTIVE_TITLE_GRADIENT 8428
#define IDC_SYSPARAMS_INACTIVE_TITLE_GRADIENT 8429
#define IDC_SYSPARAMS_MENU_HILIGHT      8430
#define IDC_SYSPARAMS_MENUBAR           8431

#define IDT_DPIEDIT 0x1234

#endif
