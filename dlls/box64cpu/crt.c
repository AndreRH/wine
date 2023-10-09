/*
 * Avoid issues with linking box64cpu by adding some crt functions
 *
 * Copyright 2023 André Zwing
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
 */

#include <stdarg.h>

#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wow);

int __cdecl __stdio_common_vsprintf(unsigned __int64 a,char* b,size_t c,const char* d,_locale_t e,va_list f)
{
    return vsprintf(b, d, f);
}

void* CDECL calloc(size_t count, size_t size)
{
    void *ret = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, size*count );
    if ((ULONG_PTR)ret >> 32)
        ERR( "ret above 4G, disabling\n" );
    return ret;
}

void CDECL free(void* ptr)
{
    if ((ULONG_PTR)ptr >> 32)
        ERR( "ptr above 4G, disabling\n" );
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

void CDECL _assert (const char *_Message, const char *_File, unsigned _Line)
{
    ERR( "_assert not supported yet\n" );
    ERR( "%s:%d: %s\n", _File, _Line, _Message);
}

double math_error(int type, const char *name, double arg1, double arg2, double retval)
{
    TRACE("(%d, %s, %g, %g, %g)\n", type, debugstr_a(name), arg1, arg2, retval);
    return retval;
}


typedef struct _div_t {
    int quot;
    int rem;
} div_t;

div_t __cdecl div(int num, int denom)
{
    div_t ret;

    ret.quot = num / denom;
    ret.rem = num % denom;
    return ret;
}

typedef struct _ldiv_t {
    long quot;
    long rem;
} ldiv_t;

ldiv_t __cdecl ldiv(long num, long denom)
{
    ldiv_t ret;

    ret.quot = num / denom;
    ret.rem = num % denom;
    return ret;
}
