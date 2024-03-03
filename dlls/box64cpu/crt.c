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

/* Control word masks for unMask */
#define _MCW_EM 0x0008001f
#define _MCW_IC 0x00040000
#define _MCW_RC 0x00000300
#define _MCW_PC 0x00030000
#define _MCW_DN 0x03000000

/* Control word values for unNew (use with related unMask above) */
#define _EM_INVALID    0x00000010
#define _EM_DENORMAL   0x00080000
#define _EM_ZERODIVIDE 0x00000008
#define _EM_OVERFLOW   0x00000004
#define _EM_UNDERFLOW  0x00000002
#define _EM_INEXACT    0x00000001
#define _IC_AFFINE     0x00040000
#define _IC_PROJECTIVE 0x00000000
#define _RC_CHOP       0x00000300
#define _RC_UP         0x00000200
#define _RC_DOWN       0x00000100
#define _RC_NEAR       0x00000000
#define _PC_24         0x00020000
#define _PC_53         0x00010000
#define _PC_64         0x00000000
#define _DN_SAVE       0x00000000
#define _DN_FLUSH      0x01000000
#define _DN_FLUSH_OPERANDS_SAVE_RESULTS 0x02000000
#define _DN_SAVE_OPERANDS_FLUSH_RESULTS 0x03000000
#define _EM_AMBIGUOUS  0x80000000

/* _statusfp bit flags */
#define _SW_INEXACT    0x00000001 /* inexact (precision) */
#define _SW_UNDERFLOW  0x00000002 /* underflow */
#define _SW_OVERFLOW   0x00000004 /* overflow */
#define _SW_ZERODIVIDE 0x00000008 /* zero divide */
#define _SW_INVALID    0x00000010 /* invalid */

#define _SW_UNEMULATED     0x00000040  /* unemulated instruction */
#define _SW_SQRTNEG        0x00000080  /* square root of a neg number */
#define _SW_STACKOVERFLOW  0x00000200  /* FP stack overflow */
#define _SW_STACKUNDERFLOW 0x00000400  /* FP stack underflow */

#define _SW_DENORMAL 0x00080000 /* denormal status bit */

static void _setfp( unsigned int *cw, unsigned int cw_mask,
        unsigned int *sw, unsigned int sw_mask )
{
    ULONG_PTR old_fpsr = 0, fpsr = 0, old_fpcr = 0, fpcr = 0;
    unsigned int flags;

    cw_mask &= _MCW_EM | _MCW_RC;
    sw_mask &= _MCW_EM;

    if (sw)
    {
        __asm__ __volatile__( "mrs %0, fpsr" : "=r" (fpsr) );
        old_fpsr = fpsr;

        flags = 0;
        if (fpsr & 0x1) flags |= _SW_INVALID;
        if (fpsr & 0x2) flags |= _SW_ZERODIVIDE;
        if (fpsr & 0x4) flags |= _SW_OVERFLOW;
        if (fpsr & 0x8) flags |= _SW_UNDERFLOW;
        if (fpsr & 0x10) flags |= _SW_INEXACT;
        if (fpsr & 0x80) flags |= _SW_DENORMAL;

        *sw = (flags & ~sw_mask) | (*sw & sw_mask);
        TRACE("aarch64 update sw %08x to %08x\n", flags, *sw);
        fpsr &= ~0x9f;
        if (*sw & _SW_INVALID) fpsr |= 0x1;
        if (*sw & _SW_ZERODIVIDE) fpsr |= 0x2;
        if (*sw & _SW_OVERFLOW) fpsr |= 0x4;
        if (*sw & _SW_UNDERFLOW) fpsr |= 0x8;
        if (*sw & _SW_INEXACT) fpsr |= 0x10;
        if (*sw & _SW_DENORMAL) fpsr |= 0x80;
        *sw = flags;
    }

    if (cw)
    {
        __asm__ __volatile__( "mrs %0, fpcr" : "=r" (fpcr) );
        old_fpcr = fpcr;

        flags = 0;
        if (!(fpcr & 0x100)) flags |= _EM_INVALID;
        if (!(fpcr & 0x200)) flags |= _EM_ZERODIVIDE;
        if (!(fpcr & 0x400)) flags |= _EM_OVERFLOW;
        if (!(fpcr & 0x800)) flags |= _EM_UNDERFLOW;
        if (!(fpcr & 0x1000)) flags |= _EM_INEXACT;
        if (!(fpcr & 0x8000)) flags |= _EM_DENORMAL;
        switch (fpcr & 0xc00000)
        {
        case 0x400000: flags |= _RC_UP; break;
        case 0x800000: flags |= _RC_DOWN; break;
        case 0xc00000: flags |= _RC_CHOP; break;
        }

        *cw = (flags & ~cw_mask) | (*cw & cw_mask);
        TRACE("aarch64 update cw %08x to %08x\n", flags, *cw);
        fpcr &= ~0xc09f00ul;
        if (!(*cw & _EM_INVALID)) fpcr |= 0x100;
        if (!(*cw & _EM_ZERODIVIDE)) fpcr |= 0x200;
        if (!(*cw & _EM_OVERFLOW)) fpcr |= 0x400;
        if (!(*cw & _EM_UNDERFLOW)) fpcr |= 0x800;
        if (!(*cw & _EM_INEXACT)) fpcr |= 0x1000;
        if (!(*cw & _EM_DENORMAL)) fpcr |= 0x8000;
        switch (*cw & _MCW_RC)
        {
        case _RC_CHOP: fpcr |= 0xc00000; break;
        case _RC_UP: fpcr |= 0x400000; break;
        case _RC_DOWN: fpcr |= 0x800000; break;
        }
    }

    /* mask exceptions if needed */
    if (old_fpcr != fpcr && ~(old_fpcr >> 8) & fpsr & 0x9f != fpsr & 0x9f)
    {
        ULONG_PTR mask = fpcr & ~0x9f00;
        __asm__ __volatile__( "msr fpcr, %0" :: "r" (mask) );
    }

    if (old_fpsr != fpsr)
        __asm__ __volatile__( "msr fpsr, %0" :: "r" (fpsr) );
    if (old_fpcr != fpcr)
        __asm__ __volatile__( "msr fpcr, %0" :: "r" (fpcr) );
}

unsigned int CDECL _control87(unsigned int newval, unsigned int mask)
{
    unsigned int flags = 0;
    flags = newval;
    _setfp(&flags, mask, NULL, 0);
    return flags;
}

unsigned int _controlfp(unsigned int newval, unsigned int mask)
{
  return _control87( newval, mask & ~_EM_DENORMAL );
}

int CDECL fegetround(void)
{
    return _controlfp(0, 0) & _MCW_RC;
}

int CDECL fesetround(int round_mode)
{
    if (round_mode & (~_MCW_RC))
        return 1;
    _controlfp(round_mode, _MCW_RC);
    return 0;
}
