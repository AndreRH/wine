/*
 * msvcrt C++ exception handling
 *
 * Copyright 2011 Alexandre Julliard
 * Copyright 2013 André Hentschel
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

#ifdef __riscv64__

#include <setjmp.h>
#include <stdarg.h>
#include <fpieee.h>

#include "ntstatus.h"
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "msvcrt.h"
#include "excpt.h"
#include "wine/debug.h"

#include "cppexcept.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);


extern void *call_exc_handler( void *handler, ULONG_PTR frame, UINT flags, BYTE *nonvol_regs );
__ASM_GLOBAL_FUNC( call_exc_handler,
                   "addi sp, sp, -112\n\t"
                   "sd ra, 104(sp)\n\t"
                   "sd s0, 0(sp)\n\t"
                   "sd s1, 8(sp)\n\t"
                   "sd s2, 16(sp)\n\t"
                   "sd s3, 24(sp)\n\t"
                   "sd s4, 32(sp)\n\t"
                   "sd s5, 40(sp)\n\t"
                   "sd s6, 48(sp)\n\t"
                   "sd s7, 56(sp)\n\t"
                   "sd s8, 64(sp)\n\t"
                   "sd s9, 72(sp)\n\t"
                   "sd s10, 80(sp)\n\t"
                   "sd s11, 88(sp)\n\t"
                   "ld s0, 0(a3)\n\t"        /* nonvolatile regs */
                   "ld s1, 8(a3)\n\t"
                   "ld s2, 16(a3)\n\t"
                   "ld s3, 24(a3)\n\t"
                   "ld s4, 32(a3)\n\t"
                   "ld s5, 40(a3)\n\t"
                   "ld s6, 48(a3)\n\t"
                   "ld s7, 56(a3)\n\t"
                   "ld s8, 64(a3)\n\t"
                   "ld s9, 72(a3)\n\t"
                   "ld s10, 80(a3)\n\t"
                   "ld s11, 88(a3)\n\t"
                   "fld fs0, 96(a3)\n\t"
                   "fld fs1, 104(a3)\n\t"
                   "fld fs2, 112(a3)\n\t"
                   "fld fs3, 120(a3)\n\t"
                   "fld fs4, 128(a3)\n\t"
                   "fld fs5, 136(a3)\n\t"
                   "fld fs6, 144(a3)\n\t"
                   "fld fs7, 152(a3)\n\t"
                   "fld fs8, 160(a3)\n\t"
                   "fld fs9, 168(a3)\n\t"
                   "fld fs10, 176(a3)\n\t"
                   "fld fs11, 184(a3)\n\t"
                   "jalr a0\n\t"
                   "ld ra, 104(sp)\n\t"
                   "ld s0, 0(sp)\n\t"
                   "ld s1, 8(sp)\n\t"
                   "ld s2, 16(sp)\n\t"
                   "ld s3, 24(sp)\n\t"
                   "ld s4, 32(sp)\n\t"
                   "ld s5, 40(sp)\n\t"
                   "ld s6, 48(sp)\n\t"
                   "ld s7, 56(sp)\n\t"
                   "ld s8, 64(sp)\n\t"
                   "ld s9, 72(sp)\n\t"
                   "ld s10, 80(sp)\n\t"
                   "ld s11, 88(sp)\n\t"
                   "addi sp, sp, 112\n\t"
                   "ret" )


/*******************************************************************
 *		call_catch_handler
 */
void *call_catch_handler( EXCEPTION_RECORD *rec )
{
    ULONG_PTR frame = rec->ExceptionInformation[1];
    void *handler = (void *)rec->ExceptionInformation[5];
    BYTE *nonvol_regs = (BYTE *)rec->ExceptionInformation[10];

    TRACE( "calling %p frame %Ix\n", handler, frame );
    return call_exc_handler( handler, frame, 0x100, nonvol_regs );
}


/*******************************************************************
 *		call_unwind_handler
 */
void *call_unwind_handler( void *handler, ULONG_PTR frame, DISPATCHER_CONTEXT *dispatch )
{
    TRACE( "calling %p frame %Ix\n", handler, frame );
    return call_exc_handler( handler, frame, 0x100, dispatch->NonVolatileRegisters );
}


/*******************************************************************
 *		get_exception_pc
 */
ULONG_PTR get_exception_pc( DISPATCHER_CONTEXT *dispatch )
{
    ULONG_PTR pc = dispatch->ControlPc;
    if (dispatch->ControlPcIsUnwound) pc -= 4;
    return pc;
}


/*******************************************************************
 *		_setjmp (MSVCRT.@)
 */
__ASM_GLOBAL_FUNC( _setjmp, "j _setjmpex" );


/*********************************************************************
 *              handle_fpieee_flt
 */
int handle_fpieee_flt( __msvcrt_ulong exception_code, EXCEPTION_POINTERS *ep,
                       int (__cdecl *handler)(_FPIEEE_RECORD*) )
{
    FIXME("(%lx %p %p)\n", exception_code, ep, handler);
    return EXCEPTION_CONTINUE_SEARCH;
}

__ASM_GLOBAL_FUNC( __C_ExecuteExceptionFilter,
    "addi sp, sp, -112\n\t"
    "sd ra, 104(sp)\n\t"
    "sd s0, 0(sp)\n\t"
    "sd s1, 8(sp)\n\t"
    "sd s2, 16(sp)\n\t"
    "sd s3, 24(sp)\n\t"
    "sd s4, 32(sp)\n\t"
    "sd s5, 40(sp)\n\t"
    "sd s6, 48(sp)\n\t"
    "sd s7, 56(sp)\n\t"
    "sd s8, 64(sp)\n\t"
    "sd s9, 72(sp)\n\t"
    "sd s10, 80(sp)\n\t"
    "sd s11, 88(sp)\n\t"
    "ld s1, 8(a3)\n\t"        /* nonvolatile regs */
    "ld s2, 16(a3)\n\t"
    "ld s3, 24(a3)\n\t"
    "ld s4, 32(a3)\n\t"
    "ld s5, 40(a3)\n\t"
    "ld s6, 48(a3)\n\t"
    "ld s7, 56(a3)\n\t"
    "ld s8, 64(a3)\n\t"
    "ld s9, 72(a3)\n\t"
    "ld s10, 80(a3)\n\t"
    "ld s11, 88(a3)\n\t"
    "ld a1, 0(a3)\n\t"        /* fp = frame */
    "jalr a2\n\t"             /* filter */
    "ld ra, 104(sp)\n\t"
    "ld s0, 0(sp)\n\t"
    "ld s1, 8(sp)\n\t"
    "ld s2, 16(sp)\n\t"
    "ld s3, 24(sp)\n\t"
    "ld s4, 32(sp)\n\t"
    "ld s5, 40(sp)\n\t"
    "ld s6, 48(sp)\n\t"
    "ld s7, 56(sp)\n\t"
    "ld s8, 64(sp)\n\t"
    "ld s9, 72(sp)\n\t"
    "ld s10, 80(sp)\n\t"
    "ld s11, 88(sp)\n\t"
    "addi sp, sp, 112\n\t"
    "ret" );

#endif  /* __riscv64__ */
