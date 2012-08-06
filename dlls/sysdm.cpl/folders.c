/*
 * System Properties control panel applet
 * "Folders" page
 *
 * Copyright (c) 2005 by Frank Richter
 * Copyright (c) 2006 by Michael Jung
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
#define COBJMACROS

#include "config.h"
#include <wine/port.h>
#include "sysdm.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <sys/stat.h>
#include <wine/unicode.h>

#include "folders.h"
#include "helpers.h"
#include "drives.h"
#include "resources.h"


extern HINSTANCE hInst;

static BOOL updating_ui = FALSE;

/* Information about symbolic link targets of certain User Shell Folders. */
struct ShellFolderInfo {
    int nFolder;
    char szLinkTarget[FILENAME_MAX]; /* in unix locale */
};

static struct ShellFolderInfo asfiInfo[] = {
    { CSIDL_DESKTOP,  "" },
    { CSIDL_PERSONAL, "" },
    { CSIDL_MYPICTURES, "" },
    { CSIDL_MYMUSIC, "" },
    { CSIDL_MYVIDEO, "" }
};

static struct ShellFolderInfo *psfiSelected = NULL;

#define NUM_ELEMS(x) (sizeof(x)/sizeof(*(x)))

static void init_shell_folder_listview_headers(HWND dialog) {
    LVCOLUMNW listColumn;
    RECT viewRect;
    WCHAR szShellFolder[64] = {'S','h','e','l','l',' ','F','o','l','d','e','r',0};
    WCHAR szLinksTo[64] = {'L','i','n','k','s',' ','t','o',0};
    int width;

    LoadStringW(GetModuleHandleW(NULL), IDS_SHELL_FOLDER, szShellFolder, sizeof(szShellFolder)/sizeof(WCHAR));
    LoadStringW(GetModuleHandleW(NULL), IDS_LINKS_TO, szLinksTo, sizeof(szLinksTo)/sizeof(WCHAR));

    GetClientRect(GetDlgItem(dialog, IDC_LIST_SFPATHS), &viewRect);
    width = (viewRect.right - viewRect.left) / 4;

    listColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    listColumn.pszText = szShellFolder;
    listColumn.cchTextMax = strlenW(listColumn.pszText);
    listColumn.cx = width;

    SendDlgItemMessageW(dialog, IDC_LIST_SFPATHS, LVM_INSERTCOLUMNW, 0, (LPARAM) &listColumn);

    listColumn.pszText = szLinksTo;
    listColumn.cchTextMax = strlenW(listColumn.pszText);
    listColumn.cx = viewRect.right - viewRect.left - width - 1;

    SendDlgItemMessageW(dialog, IDC_LIST_SFPATHS, LVM_INSERTCOLUMNW, 1, (LPARAM) &listColumn);
}

/* Reads the currently set shell folder symbol link targets into asfiInfo. */
static void read_shell_folder_link_targets(void) {
    WCHAR wszPath[MAX_PATH];
    HRESULT hr;
    int i;
   
    for (i=0; i<NUM_ELEMS(asfiInfo); i++) {
        asfiInfo[i].szLinkTarget[0] = '\0';
        hr = SHGetFolderPathW(NULL, asfiInfo[i].nFolder|CSIDL_FLAG_DONT_VERIFY, NULL, 
                              SHGFP_TYPE_CURRENT, wszPath);
        if (SUCCEEDED(hr)) {
            char *pszUnixPath = wine_get_unix_file_name(wszPath);
            if (pszUnixPath) {
                struct stat statPath;
                if (!lstat(pszUnixPath, &statPath) && S_ISLNK(statPath.st_mode)) {
                    int cLen = readlink(pszUnixPath, asfiInfo[i].szLinkTarget, FILENAME_MAX-1);
                    if (cLen >= 0) asfiInfo[i].szLinkTarget[cLen] = '\0';
                }
                HeapFree(GetProcessHeap(), 0, pszUnixPath);
            }
        } 
    }    
}

static void update_shell_folder_listview(HWND dialog) {
    int i;
    LVITEMW item;
    LONG lSelected = SendDlgItemMessageW(dialog, IDC_LIST_SFPATHS, LVM_GETNEXTITEM, -1,
                                        MAKELPARAM(LVNI_SELECTED,0));

    SendDlgItemMessageW(dialog, IDC_LIST_SFPATHS, LVM_DELETEALLITEMS, 0, 0);

    for (i=0; i<NUM_ELEMS(asfiInfo); i++) {
        WCHAR buffer[MAX_PATH];
        HRESULT hr;
        LPITEMIDLIST pidlCurrent;

        /* Some acrobatic to get the localized name of the shell folder */
        hr = SHGetFolderLocation(dialog, asfiInfo[i].nFolder, NULL, 0, &pidlCurrent);
        if (SUCCEEDED(hr)) { 
            LPSHELLFOLDER psfParent;
            LPCITEMIDLIST pidlLast;
            hr = SHBindToParent(pidlCurrent, &IID_IShellFolder, (LPVOID*)&psfParent, &pidlLast);
            if (SUCCEEDED(hr)) {
                STRRET strRet;
                hr = IShellFolder_GetDisplayNameOf(psfParent, pidlLast, SHGDN_FORADDRESSBAR, &strRet);
                if (SUCCEEDED(hr)) {
                    hr = StrRetToBufW(&strRet, pidlLast, buffer, MAX_PATH);
                }
                IShellFolder_Release(psfParent);
            }
            ILFree(pidlCurrent);
        }

        /* If there's a dangling symlink for the current shell folder, SHGetFolderLocation
         * will fail above. We fall back to the (non-verified) path of the shell folder. */
        if (FAILED(hr)) {
            hr = SHGetFolderPathW(dialog, asfiInfo[i].nFolder|CSIDL_FLAG_DONT_VERIFY, NULL,
                                 SHGFP_TYPE_CURRENT, buffer);
        }
    
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = buffer;
        item.lParam = (LPARAM)&asfiInfo[i];
        SendDlgItemMessageW(dialog, IDC_LIST_SFPATHS, LVM_INSERTITEMW, 0, (LPARAM)&item);

        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 1;
        item.pszText = strdupU2W(asfiInfo[i].szLinkTarget);
        SendDlgItemMessageW(dialog, IDC_LIST_SFPATHS, LVM_SETITEMW, 0, (LPARAM)&item);
        HeapFree(GetProcessHeap(), 0, item.pszText);
    }

    /* Ensure that the previously selected item is selected again. */
    if (lSelected >= 0) {
        item.mask = LVIF_STATE;
        item.state = LVIS_SELECTED;
        item.stateMask = LVIS_SELECTED;
        SendDlgItemMessageW(dialog, IDC_LIST_SFPATHS, LVM_SETITEMSTATE, lSelected, (LPARAM)&item);
    }
}

static void on_shell_folder_selection_changed(HWND hDlg, LPNMLISTVIEW lpnm) {
    if (lpnm->uNewState & LVIS_SELECTED) {
        psfiSelected = (struct ShellFolderInfo *)lpnm->lParam;
        EnableWindow(GetDlgItem(hDlg, IDC_LINK_SFPATH), 1);
        if (strlen(psfiSelected->szLinkTarget)) {
            WCHAR *link;
            CheckDlgButton(hDlg, IDC_LINK_SFPATH, BST_CHECKED);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_SFPATH), 1);
            EnableWindow(GetDlgItem(hDlg, IDC_BROWSE_SFPATH), 1);
            link = strdupU2W(psfiSelected->szLinkTarget);
            SetWindowTextW(GetDlgItem(hDlg, IDC_EDIT_SFPATH), link);
            HeapFree(GetProcessHeap(), 0, link);
        } else {
            CheckDlgButton(hDlg, IDC_LINK_SFPATH, BST_UNCHECKED);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_SFPATH), 0);
            EnableWindow(GetDlgItem(hDlg, IDC_BROWSE_SFPATH), 0);
            SetWindowTextA(GetDlgItem(hDlg, IDC_EDIT_SFPATH), "");
        }
    } else {
        psfiSelected = NULL;
        CheckDlgButton(hDlg, IDC_LINK_SFPATH, BST_UNCHECKED);
        SetWindowTextA(GetDlgItem(hDlg, IDC_EDIT_SFPATH), "");
        EnableWindow(GetDlgItem(hDlg, IDC_LINK_SFPATH), 0);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_SFPATH), 0);
        EnableWindow(GetDlgItem(hDlg, IDC_BROWSE_SFPATH), 0);
    }
}

/* Keep the contents of the edit control, the listview control and the symlink 
 * information in sync. */
static void on_shell_folder_edit_changed(HWND hDlg) {
    LVITEMW item;
    WCHAR *text = get_textW(hDlg, IDC_EDIT_SFPATH);
    LONG iSel = SendDlgItemMessageW(hDlg, IDC_LIST_SFPATHS, LVM_GETNEXTITEM, -1,
                                    MAKELPARAM(LVNI_SELECTED,0));

    if (!text || !psfiSelected || iSel < 0) {
        HeapFree(GetProcessHeap(), 0, text);
        return;
    }

    WideCharToMultiByte(CP_UNIXCP, 0, text, -1,
                        psfiSelected->szLinkTarget, FILENAME_MAX, NULL, NULL);

    item.mask = LVIF_TEXT;
    item.iItem = iSel;
    item.iSubItem = 1;
    item.pszText = text;
    SendDlgItemMessageW(hDlg, IDC_LIST_SFPATHS, LVM_SETITEMW, 0, (LPARAM)&item);

    HeapFree(GetProcessHeap(), 0, text);

    SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
}

static void apply_shell_folder_changes(void) {
    WCHAR wszPath[MAX_PATH];
    char szBackupPath[FILENAME_MAX], szUnixPath[FILENAME_MAX], *pszUnixPath = NULL;
    int i;
    struct stat statPath;
    HRESULT hr;

    for (i=0; i<NUM_ELEMS(asfiInfo); i++) {
        /* Ignore nonexistent link targets */
        if (asfiInfo[i].szLinkTarget[0] && stat(asfiInfo[i].szLinkTarget, &statPath))
            continue;
        
        hr = SHGetFolderPathW(NULL, asfiInfo[i].nFolder|CSIDL_FLAG_CREATE, NULL, 
                              SHGFP_TYPE_CURRENT, wszPath);
        if (FAILED(hr)) continue;

        /* Retrieve the corresponding unix path. */
        pszUnixPath = wine_get_unix_file_name(wszPath);
        if (!pszUnixPath) continue;
        lstrcpyA(szUnixPath, pszUnixPath);
        HeapFree(GetProcessHeap(), 0, pszUnixPath);
            
        /* Derive name for folder backup. */
        lstrcpyA(szBackupPath, szUnixPath);
        lstrcatA(szBackupPath, ".winecfg");
        
        if (lstat(szUnixPath, &statPath)) continue;
    
        /* Move old folder/link out of the way. */
        if (S_ISLNK(statPath.st_mode)) {
            if (unlink(szUnixPath)) continue; /* Unable to remove link. */
        } else { 
            if (!*asfiInfo[i].szLinkTarget) {
                continue; /* We are done. Old was real folder, as new shall be. */
            } else { 
                if (rename(szUnixPath, szBackupPath)) { /* Move folder out of the way. */
                    continue; /* Unable to move old folder. */
                }
            }
        }
    
        /* Create new link/folder. */
        if (*asfiInfo[i].szLinkTarget) {
            symlink(asfiInfo[i].szLinkTarget, szUnixPath);
        } else {
            /* If there's a backup folder, restore it. Else create new folder. */
            if (!lstat(szBackupPath, &statPath) && S_ISDIR(statPath.st_mode)) {
                rename(szBackupPath, szUnixPath);
            } else {
                mkdir(szUnixPath, 0777);
            }
        }
    }
}

INT_PTR CALLBACK FoldersDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG:
            read_shell_folder_link_targets();
            init_shell_folder_listview_headers(hDlg);
            update_shell_folder_listview(hDlg);
            break;

        case WM_SHOWWINDOW:
            set_window_title(hDlg);
            break;
            
        case WM_COMMAND:
            switch(HIWORD(wParam)) {
                case EN_CHANGE: {
                    if (updating_ui) break;
                    switch (LOWORD(wParam)) {
			case IDC_EDIT_SFPATH: on_shell_folder_edit_changed(hDlg); break;
                    }
                    break;
                }
                case BN_CLICKED:
                    switch (LOWORD(wParam))
                    {
                        case IDC_BROWSE_SFPATH:
                        {
                            WCHAR link[FILENAME_MAX];
                            if (browse_for_unix_folder(hDlg, link)) {
                                WideCharToMultiByte(CP_UNIXCP, 0, link, -1,
                                                    psfiSelected->szLinkTarget, FILENAME_MAX,
                                                    NULL, NULL);
                                update_shell_folder_listview(hDlg);
                                SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
                            }
                            break;
                        }

                        case IDC_LINK_SFPATH:
                            if (IsDlgButtonChecked(hDlg, IDC_LINK_SFPATH)) {
                                WCHAR link[FILENAME_MAX];
                                if (browse_for_unix_folder(hDlg, link)) {
                                    WideCharToMultiByte(CP_UNIXCP, 0, link, -1,
                                                        psfiSelected->szLinkTarget, FILENAME_MAX,
                                                        NULL, NULL);
                                    update_shell_folder_listview(hDlg);
                                    SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
                                } else {
                                    CheckDlgButton(hDlg, IDC_LINK_SFPATH, BST_UNCHECKED);
                                }
                            } else {
                                psfiSelected->szLinkTarget[0] = '\0';
                                update_shell_folder_listview(hDlg);
                                SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
                            }
                            break;    
                    }
                    break;
            }
            break;
        
        case WM_NOTIFY:
            switch (((LPNMHDR)lParam)->code) {
                case PSN_KILLACTIVE: {
                    SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, FALSE);
                    break;
                }
                case PSN_APPLY: {
                    apply();
                    apply_shell_folder_changes();
                    read_shell_folder_link_targets();
                    update_shell_folder_listview(hDlg);
                    SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                    break;
                }
                case LVN_ITEMCHANGED: { 
                    if (wParam == IDC_LIST_SFPATHS)  
                        on_shell_folder_selection_changed(hDlg, (LPNMLISTVIEW)lParam);
                    break;
                }
            }
            break;

        default:
            break;
    }
    return FALSE;
}

