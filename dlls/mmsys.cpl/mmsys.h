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

#ifndef __WINE_MMSYSCPL__
#define __WINE_MMSYSCPL__

#include <wine/debug.h>
#include "resources.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmsyscpl);
extern HMODULE hcpl;

static WCHAR g_drv_keyW[256] = {'S','o','f','t','w','a','r','e','\\','W','i','n','e','\\','D','r','i','v','e','r','s','\\',0};
static const WCHAR reg_out_nameW[] = {'D','e','f','a','u','l','t','O','u','t','p','u','t',0};
static const WCHAR reg_in_nameW[] = {'D','e','f','a','u','l','t','I','n','p','u','t',0};
static const WCHAR reg_vout_nameW[] = {'D','e','f','a','u','l','t','V','o','i','c','e','O','u','t','p','u','t',0};
static const WCHAR reg_vin_nameW[] = {'D','e','f','a','u','l','t','V','o','i','c','e','I','n','p','u','t',0};

#endif
