/*
 * Audio Settings control panel applet
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

#ifndef __WINE_MMSYSCPL_RESOURCESH__
#define __WINE_MMSYSCPL_RESOURCESH__

/* strings */
#define IDS_CPL_NAME                 1
#define IDS_CPL_NAME_APP             2
#define IDS_CPL_INFO                 3
#define IDS_CPL_ICONNAME             4
#define IDS_TAB_AUDIO                5
#define IDS_TAB_VOICE                6
#define IDS_TAB_HARDWARE             7
#define IDS_AUDIO_DRIVER             8
#define IDS_AUDIO_DRIVER_NONE        9
#define IDS_AUDIO_TEST_FAILED       10
#define IDS_AUDIO_TEST_FAILED_TITLE 11
#define IDS_AUDIO_SYSDEFAULT        12

/* icons, sounds */
#define ICO_MAIN            100
#define IDW_TESTSOUND       101

/* dialogs */
#define IDC_STATIC  -1
#define IDD_AUDIO    1
#define IDD_VOICE    2
#define IDD_HARDWARE 3

/* widgets */
#define IDC_AUDIOOUT_DEVICE 1
#define IDC_AUDIOIN_DEVICE  2
#define IDC_VOICEOUT_DEVICE 3
#define IDC_VOICEIN_DEVICE  4
#define IDC_AUDIO_DRIVER    5
#define IDC_AUDIO_TEST      6

#endif
