/*
 * Internet control panel applet
 *
 * Copyright 2010 Detlef Riekenberg
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

#ifndef __WINE_DESKCPL__
#define __WINE_DESKCPL__

#include <windef.h>
#include <winuser.h>
#include <commctrl.h>

extern HMODULE hcpl;
INT_PTR CALLBACK content_dlgproc(HWND, UINT, WPARAM, LPARAM) DECLSPEC_HIDDEN;
INT_PTR CALLBACK general_dlgproc(HWND, UINT, WPARAM, LPARAM) DECLSPEC_HIDDEN;
INT_PTR CALLBACK security_dlgproc(HWND, UINT, WPARAM, LPARAM) DECLSPEC_HIDDEN;

/* ## Memory allocation functions ## */

static inline void * __WINE_ALLOC_SIZE(1) heap_alloc( size_t len )
{
    return HeapAlloc( GetProcessHeap(), 0, len );
}

static inline void * __WINE_ALLOC_SIZE(1) heap_alloc_zero( size_t len )
{
    return HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, len );
}

static inline BOOL heap_free( void *mem )
{
    return HeapFree( GetProcessHeap(), 0, mem );
}

/* ######### */

#define NUM_PROPERTY_PAGES 2

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


/* icons */
#define ICO_MAIN            100

/* strings */
#define IDS_CPL_NAME        1
#define IDS_CPL_INFO        2
#define IDS_NOTHEME         8

#define IDS_WINECFG_TITLE               13
#define IDS_THEMEFILE                   14
#define IDS_THEMEFILE_SELECT            15
#define IDS_SHELL_FOLDER                16
#define IDS_LINKS_TO                    17
#define IDS_WINECFG_TITLE_APP           18


/* dialogs */
#define IDC_STATIC          -1



#endif
