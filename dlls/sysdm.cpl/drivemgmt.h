/*
 * Drive management code
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

#ifndef __WINE_SYSDMCPL_DRIVEMGMTH__
#define __WINE_SYSDMCPL_DRIVEMGMTH__

#define DRIVE_MASK_BIT(B) (1 << (toupper(B) - 'A'))

struct drive {
        char letter;
        char *unixpath;
        char *device;
        WCHAR *label;
        DWORD serial;
        DWORD type; /* one of the DRIVE_ constants from winbase.h  */
        BOOL in_use;
        BOOL modified;
};

ULONG drive_available_mask(char letter);
BOOL add_drive(char letter, const char *targetpath, const char *device, const WCHAR *label, DWORD serial, DWORD type);
void delete_drive(struct drive *d);
static DWORD get_drive_type(char letter);
static void set_drive_label(char letter, const WCHAR *label);
static void set_drive_serial(WCHAR letter, DWORD serial);
static HANDLE open_mountmgr(void);
BOOL load_drives(void);
void apply_drive_changes(void);

#endif
