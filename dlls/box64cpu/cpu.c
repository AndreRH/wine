/*
 * Copyright 2022-2023 André Zwing
 * Copyright 2023 Alexandre Julliard
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

#define DYNAREC

#if defined(__aarch64__)
#define ARM64
#elif defined(__riscv64__)
#define RV64
#endif

#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <signal.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "winbase.h"
#include "winreg.h"
#include "winnls.h"
#include "wine/asm.h"
#include "wine/debug.h"
#include "wine/exception.h"
#include "wine/unixlib.h"
#include "x64emu.h"
#include "x64run.h"
#include "x64trace.h"
#include "box64context.h"
#include "custommem.h"
#include "dynablock.h"
#include "emu/x64emu_private.h"
#include "emu/x64run_private.h"
#include "emu/x87emu_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(wow);
WINE_DECLARE_DEBUG_CHANNEL(box64dump);

NTSTATUS WINAPI Wow64SystemServiceEx( UINT num, UINT *args );

int box64_dynarec_log = 0;
int box64_dynarec = 1;
int box64_dynarec_dump = 0;
int box64_dynarec_forced = 0;
int box64_dynarec_bigblock = 1;
int box64_dynarec_forward = 128;
int box64_dynarec_strongmem = 0;
int box64_dynarec_x87double = 0;
int box64_dynarec_fastnan = 1;
int box64_dynarec_fastround = 1;
int box64_dynarec_safeflags = 1;
int box64_dynarec_callret = 0;
int box64_dynarec_hotpage = 0;
int box64_dynarec_fastpage = 0;
int box64_dynarec_bleeding_edge = 1;
int box64_dynarec_jvm = 1;
int box64_dynarec_wait = 1;
int box64_dynarec_test = 0;
int box64_dynarec_missing = 0;
int box64_dynarec_trace = 0;
int box64_dynarec_aligned_atomics = 0;
int box64_dynarec_div0 = 0;

int arm64_atomics = 0;
int arm64_crc32 = 0;
int arm64_flagm = 0;
int arm64_aes = 0;
int arm64_pmull = 0;
int arm64_sha1 = 0;
int arm64_sha2 = 0;
int arm64_uscat = 0;
int arm64_frintts = 0;
int arm64_rndr = 0;
int box64_log = 1;
uintptr_t box64_pagesize = 4096;
uint32_t default_gs = 0x2b;
uintptr_t box64_nodynarec_start = 0;
uintptr_t box64_nodynarec_end = 0;
int box64_sse_flushto0 = 0;
int box64_x87_no80bits = 0;
int box64_ignoreint3 = 0;
int box64_rdtsc = 0;
uint8_t box64_rdtsc_shift = 0;

static UINT16 DECLSPEC_ALIGN(4096) bopcode[4096/sizeof(UINT16)];
static UINT16 DECLSPEC_ALIGN(4096) unxcode[4096/sizeof(UINT16)];

#define ROUND_ADDR(addr,mask) ((void *)((UINT_PTR)(addr) & ~(UINT_PTR)(mask)))
#define ROUND_SIZE(addr,size) (((SIZE_T)(size) + ((UINT_PTR)(addr) & page_mask) + page_mask) & ~page_mask)
static const UINT_PTR page_mask = 0xfff;

int box64_wine = 0;

ULONG_PTR default_zero_bits = 0x7fffffff;

void UnimpOpcode(x64emu_t* emu, int is32bits)
{
    emu->quit=1;
    emu->error |= ERR_UNIMPL;
}

int my_getcontext(x64emu_t* emu, void* ucp)
{
    ERR( "NYI\n" );
    return 0;
}

int my_setcontext(x64emu_t* emu, void* ucp)
{
    ERR( "NYI\n" );
    return R_EAX;
}

void emit_interruption(x64emu_t* emu, int num, void* addr)
{
    ERR( "got interrupt %i @ %p\n", num, addr );
}

void emit_div0(x64emu_t* emu, void* addr, int code)
{
    EXCEPTION_RECORD rec;

    ERR( "got division by 0\n" );
    rec.ExceptionCode = EXCEPTION_INT_DIVIDE_BY_ZERO; // FIXME: Might be EXCEPTION_FLT_DIVIDE_BY_ZERO
    rec.ExceptionFlags   = EXCEPTION_NONCONTINUABLE;
    rec.ExceptionRecord  = NULL;
    rec.ExceptionAddress = addr;
    rec.NumberParameters = 0;
    RtlRaiseException( &rec );
}

void emit_signal( x64emu_t *emu, int sig, void *addr, int code )
{
    EXCEPTION_RECORD rec;

    ERR( "got signal %u\n", sig );
    switch (sig)
    {
    case SIGILL:
        rec.ExceptionCode = STATUS_ILLEGAL_INSTRUCTION;
        break;
    case SIGSEGV:
        rec.ExceptionCode = STATUS_ACCESS_VIOLATION;
        break;
    default:
        ERR( "unknown signal %u\n", sig );
        rec.ExceptionCode = STATUS_ACCESS_VIOLATION;
        break;
    }
    rec.ExceptionFlags   = EXCEPTION_NONCONTINUABLE;
    rec.ExceptionRecord  = NULL;
    rec.ExceptionAddress = addr;
    rec.NumberParameters = 0;
    RtlRaiseException( &rec );
}

void fpu_to_box(I386_CONTEXT *ctx, x64emu_t *emu)
{
    XMM_SAVE_AREA32 *fpu = (XMM_SAVE_AREA32 *)ctx->ExtendedRegisters;
    //ERR("start\n");
    emu->mxcsr.x32 = fpu->MxCsr;
    emu->cw.x16 = fpu->ControlWord;
    emu->sw.x16 = fpu->StatusWord;

    LD2D(&ctx->FloatSave.RegisterArea[ 0], &emu->x87[0]);
    LD2D(&ctx->FloatSave.RegisterArea[10], &emu->x87[1]);
    LD2D(&ctx->FloatSave.RegisterArea[20], &emu->x87[2]);
    LD2D(&ctx->FloatSave.RegisterArea[30], &emu->x87[3]);
    LD2D(&ctx->FloatSave.RegisterArea[40], &emu->x87[4]);
    LD2D(&ctx->FloatSave.RegisterArea[50], &emu->x87[5]);
    LD2D(&ctx->FloatSave.RegisterArea[60], &emu->x87[6]);
    LD2D(&ctx->FloatSave.RegisterArea[70], &emu->x87[7]);
    memcpy(emu->xmm, fpu->XmmRegisters, sizeof(emu->xmm));
    //memcpy(emu->fpu_ld, ctx->FloatSave.RegisterArea, sizeof(emu->fpu_ld));
    //ERR("done\n");
}

void box_to_fpu(I386_CONTEXT *ctx, x64emu_t *emu)
{
    XMM_SAVE_AREA32 *fpu = (XMM_SAVE_AREA32 *)ctx->ExtendedRegisters;
    //ERR("start\n");
    fpu->MxCsr = emu->mxcsr.x32;
    fpu->ControlWord = emu->cw.x16;
    fpu->StatusWord  = emu->sw.x16;

    D2LD(&emu->x87[0], &ctx->FloatSave.RegisterArea[ 0]);
    D2LD(&emu->x87[1], &ctx->FloatSave.RegisterArea[10]);
    D2LD(&emu->x87[2], &ctx->FloatSave.RegisterArea[20]);
    D2LD(&emu->x87[3], &ctx->FloatSave.RegisterArea[30]);
    D2LD(&emu->x87[4], &ctx->FloatSave.RegisterArea[40]);
    D2LD(&emu->x87[5], &ctx->FloatSave.RegisterArea[50]);
    D2LD(&emu->x87[6], &ctx->FloatSave.RegisterArea[60]);
    D2LD(&emu->x87[7], &ctx->FloatSave.RegisterArea[70]);
    memcpy(fpu->XmmRegisters, emu->xmm, sizeof(emu->xmm));
    //memcpy(ctx->FloatSave.RegisterArea, emu->fpu_ld, sizeof(emu->fpu_ld));
    //ERR("done\n");
}

void x64Int3( x64emu_t *emu, uintptr_t* addr )
{
    EXCEPTION_RECORD rec;

    ERR( "got int3 at %llx\n", R_RIP );
    rec.ExceptionCode    = STATUS_BREAKPOINT;
    rec.ExceptionFlags   = 0;
    rec.ExceptionRecord  = NULL;
    rec.ExceptionAddress = (void *)(uintptr_t)R_RIP;
    rec.NumberParameters = 0;
    RtlRaiseException( &rec );
}

#define PRINT_STACK 4
void x86Int(x64emu_t *emu, int code)
{
    int inst_off = box64_dynarec ? 2 : 0;
    TRACE("%x\n", R_ESP);
    #if PRINT_STACK
    {
        int i;
        DWORD *p = ULongToPtr(R_RSP);
        for (i = 0; i < PRINT_STACK; i++)
            TRACE("stack+%i: %08lx\n", i, p[i]);
    }
    #endif
    if (code == 0x2e)  /* NT syscall */
    {
        WOW64_CPURESERVED *cpu = NtCurrentTeb()->TlsSlots[WOW64_TLS_CPURESERVED];
        I386_CONTEXT *ctx = (I386_CONTEXT *)(cpu + 1);
        int id = R_EAX;
        BOOL is_unix_call = FALSE;

        TRACE("syscall !!! %p %p\n", ULongToPtr(R_RIP-inst_off), &bopcode);

        if (ULongToPtr(R_RIP-inst_off) == &bopcode)
            TRACE("SYSCALL\n");
        else if (ULongToPtr(R_RIP-inst_off) == &unxcode)
        {
            TRACE("UNIXCALL\n");
            is_unix_call = TRUE;
        }
        else
        {
            ERR("ERRCALL\n");
        }
    
        R_RIP = Pop32(emu);
        ctx->Eip = R_RIP;
        ctx->Esp = R_ESP;
        ctx->Ebx = R_EBX;
        ctx->Esi = R_ESI;
        ctx->Edi = R_EDI;
        ctx->Ebp = R_EBP;
        ctx->EFlags = emu->eflags.x64;
        cpu->Flags = 0;

        box_to_fpu(ctx, emu);

        if (is_unix_call)
        {
            uintptr_t handle_low = Pop32(emu);
            uintptr_t handle_high = Pop32(emu);
            unsigned int code = Pop32(emu);
            uintptr_t args = Pop32(emu);

            ctx->Esp = R_ESP;
            R_EAX = __wine_unix_call_dispatcher( handle_low | (handle_high << 32), code, (void *)args );
            TRACE("unix ret %x\n", R_EAX);
        }
        else
        {
            R_EAX = Wow64SystemServiceEx( id, ULongToPtr(ctx->Esp+4) );
        }

        fpu_to_box(ctx, emu);

        R_EBX = ctx->Ebx;
        R_ESI = ctx->Esi;
        R_EDI = ctx->Edi;
        R_EBP = ctx->Ebp;
        R_ESP = ctx->Esp;
        R_RIP = ctx->Eip;
        if (cpu->Flags & WOW64_CPURESERVED_FLAG_RESET_STATE)
        {
            cpu->Flags &= ~WOW64_CPURESERVED_FLAG_RESET_STATE;
            R_EAX = ctx->Eax;
            R_ECX = ctx->Ecx;
            R_EDX = ctx->Edx;
            R_FS  = ctx->SegFs & 0xffff;
            emu->segs_offs[_FS] = (uintptr_t)NtCurrentTeb() + NtCurrentTeb()->WowTebOffset;
            emu->eflags.x64 = ctx->EFlags;
        }
    }
    else
    {
        ERR( "%x not supported\n", code );
        RtlRaiseStatus( STATUS_ACCESS_VIOLATION );
    }
    #if PRINT_STACK
    {
        int i;
        DWORD *p = ULongToPtr(R_RSP);
        for (i = 0; i < PRINT_STACK; i++)
            TRACE("stack+%i: %08lx\n", i, p[i]);
    }
    #endif
    TRACE("EXIT %x\n", R_ESP);
}

void applyFlushTo0(x64emu_t* emu)
{
    ERR( "not supported yet: applyFlushTo0\n" );
}


void x64Syscall(x64emu_t *emu)
{
    ERR( "not supported yet: x64Syscall\n" );
}

int printFunctionAddr(uintptr_t nextaddr, const char* text)
{
    //ERR( "not supported yet: %p\n", (void*)nextaddr );
    return 0;
}


void *box_malloc(size_t size)
{
    void * ret;
    ret = RtlAllocateHeap(GetProcessHeap(), 0, size);
    if ((ULONG_PTR)ret >> 32)
        ERR( "ret above 4G, disabling\n" );
    TRACE( "ret: %p\n", ret );
    return ret;
}

void *box_realloc(void *ptr, size_t size)
{
    void * ret;
    if (!ptr)
        return box_malloc(size);
    ret = RtlReAllocateHeap(GetProcessHeap(), HEAP_ZERO_MEMORY, ptr, size);
    TRACE( "ret: %p\n", ret );
    return ret;
}

void *box_calloc(size_t nmemb, size_t size)
{
    void * ret;
    ret = RtlAllocateHeap(GetProcessHeap(), HEAP_ZERO_MEMORY, nmemb * size);
    if ((ULONG_PTR)ret >> 32)
        ERR( "ret above 4G, disabling\n" );
    TRACE( "ret: %p\n", ret );
    return ret;
}

void box_free(void *ptr)
{
    if ((ULONG_PTR)ptr >> 32)
        ERR( "ptr above 4G, disabling\n" );
    TRACE( "ptr: %p\n", ptr );
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

void *box_memalign(size_t alignment, size_t size)
{
    NTSTATUS ntstatus;
    SIZE_T sz = size;
    void *ret = NULL;
    if (alignment != 0x1000)
        ERR( "box_memalign with alignment %zx\n", alignment );
    ntstatus = NtAllocateVirtualMemory(NtCurrentProcess(), &ret, default_zero_bits, &sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if ((ULONG_PTR)ret >> 32)
        ERR( "ret above 4G, disabling\n" );
    TRACE( "ntstatus %lx ret: %p\n", ntstatus, ret );
    return ret;
}

int hasAlternate(void* addr)
{
    return 0;
}

void* getAlternate(void* addr)
{
    return addr;
}

void* GetNativeFnc(uintptr_t fnc)
{
    return NULL;
}

const char* GetNativeName(void* p)
{
    return "Hangover_GetNativeName";
}

int GetTID(void)
{
    return GetCurrentThreadId();
}

uint64_t ReadTSC( x64emu_t *emu )
{
    LARGE_INTEGER counter;
    NtQueryPerformanceCounter( &counter, NULL );
    return counter.QuadPart;  /* FIXME */
}

uint32_t get_random32()
{
    ULONG seed = NtGetTickCount();
    return RtlRandom(&seed);
}

uint64_t get_random64()
{
    ULONG seed = NtGetTickCount();
    return RtlRandom(&seed) | (RtlRandom(&seed) << 32);
}

uint32_t helper_getcpu(x64emu_t* emu) {
    return 0;
}

void my_cpuid(x64emu_t* emu, uint32_t tmp32u)
{
    static char branding[3*4*4+1] = "Box64 on Hangover";
    int ncpu = NtCurrentTeb()->Peb->NumberOfProcessors;
    emu->regs[_AX].dword[1] = emu->regs[_DX].dword[1] = emu->regs[_CX].dword[1] = emu->regs[_BX].dword[1] = 0;
    if(ncpu>255) ncpu = 255;
    if(!ncpu) ncpu = 1;
    switch(tmp32u) {
        case 0x0:
            // emulate a P4. TODO: Emulate a Core2?
            R_EAX = 0x0000000D;//0x80000004;
            // return GenuineIntel
            R_EBX = 0x756E6547;
            R_EDX = 0x49656E69;
            R_ECX = 0x6C65746E;
            break;
        case 0x1:
            R_EAX = 0x00000601; // family and all
            R_EBX = 0 | (8<<0x8) | (ncpu<<16);          // Brand index, CLFlush (8), Max APIC ID (16-23), Local APIC ID (24-31)
            {
                int cpu = 0; // FIXME sched_getcpu();
                if(cpu<0) cpu=0;
                R_EAX |= cpu<<24;
            }
            R_EDX =   1         // fpu 
                    | 1<<4      // rdtsc
                    | 1<<8      // cmpxchg8
                    | 1<<11     // sep (sysenter & sysexit)
                    | 1<<15     // cmov
                    | 1<<19     // clflush (seems to be with SSE2)
                    | 1<<23     // mmx
                    | 1<<24     // fxsr (fxsave, fxrestore)
                    | 1<<25     // SSE
                    | 1<<26     // SSE2
                    ;
            R_ECX =   1<<0      // SSE3
                    | 1<<1      // PCLMULQDQ
                    | 1<<9      // SSSE3
                    | 1<<12     // fma
                    | 1<<13     // cx16 (cmpxchg16)
                    | 1<<19     // SSE4_1
                    | 1<<22     // MOVBE
                    | 1<<23     // POPCOUNT
                    | 1<<25     // aesni
                    ; 
            break;
        case 0x2:   // TLB and Cache info. Sending 1st gen P4 info...
            R_EAX = 0x665B5001;
            R_EBX = 0x00000000;
            R_ECX = 0x00000000;
            R_EDX = 0x007A7000;
            break;
        
        case 0x4:   // Cache info
            switch (R_ECX) {
                case 0: // L1 data cache
                    R_EAX = (1 | (1<<5) | (1<<8) | ((ncpu-1)<<26));   //type + (26-31):max cores per packages-1
                    R_EBX = (63 | (7<<22)); // size
                    R_ECX = 63;
                    R_EDX = 1;
                    break;
                case 1: // L1 inst cache
                    R_EAX = (2 | (1<<5) | (1<<8)); //type
                    R_EBX = (63 | (7<<22)); // size
                    R_ECX = 63;
                    R_EDX = 1;
                    break;
                case 2: // L2 cache
                    R_EAX = (3 | (2<<5) | (1<<8)); //type
                    R_EBX = (63 | (15<<22));    // size
                    R_ECX = 4095;
                    R_EDX = 1;
                    break;

                default:
                    R_EAX = 0x00000000;
                    R_EBX = 0x00000000;
                    R_ECX = 0x00000000;
                    R_EDX = 0x00000000;
                    break;
            }
            break;
        case 0x5:   //mwait info
            R_EAX = 0;
            R_EBX = 0;
            R_ECX = 1 | 2;
            R_EDX = 0;
            break;
        case 0x3:   // PSN
        case 0x6:   // thermal
        case 0x8:   // more extended capabilities
        case 0x9:   // direct cache access
        case 0xA:   // Architecture performance monitor
            R_EAX = 0;
            R_EBX = 0;
            R_ECX = 0;
            R_EDX = 0;
            break;
        case 0x7:   // extended bits...
            if(R_ECX==1) {
                R_EAX = 0; // Bit 5 is avx512_bf16
            } else 
                R_EAX = R_ECX = R_EBX = R_EDX = 0; // TODO
            break;
        case 0xB:   // Extended Topology Enumeration Leaf
            //TODO!
            R_EAX = 0;
            R_EBX = 0;
            break;
        case 0xC:   //?
            R_EAX = 0;
            break;
        case 0xD:   // Processor Extended State Enumeration Main Leaf / Sub Leaf
            if(R_CX==0) {
                R_EAX = 1 | 2;  // x87 SSE saved
                R_EBX = 512;    // size of xsave/xrstor
                R_ECX = 512;    // same
                R_EDX = 0;      // more bits
            } else if(R_CX==1){
                R_EAX = R_ECX = R_EBX = R_EDX = 0;  // XSAVEOPT and co are not available
            } else {
                R_EAX = R_ECX = R_EBX = R_EDX = 0;
            }
            break;
            
        case 0x80000000:        // max extended
            R_EAX = 0x80000005;
            break;
        case 0x80000001:        //Extended Processor Signature and Feature Bits
            R_EAX = 0;  // reserved
            R_EBX = 0;  // reserved
            R_ECX = (1<<5) | (1<<8); // LZCNT | PREFETCHW
            R_EDX = 1 | (1<<29); // x87 FPU? bit 29 is 64bits available
            //AMD flags?
            //R_EDX = 1 | (1<<8) | (1<<11) | (1<<15) | (1<<23) | (1<<29); // fpu+cmov+cx8+syscall+mmx+lm (mmxext=22, 3dnow=31, 3dnowext=30)
            break;
        case 0x80000002:    // Brand part 1 (branding signature)
            R_EAX = ((uint32_t*)branding)[0];
            R_EBX = ((uint32_t*)branding)[1];
            R_ECX = ((uint32_t*)branding)[2];
            R_EDX = ((uint32_t*)branding)[3];
            break;
        case 0x80000003:    // Brand part 2
            R_EAX = ((uint32_t*)branding)[4];
            R_EBX = ((uint32_t*)branding)[5];
            R_ECX = ((uint32_t*)branding)[6];
            R_EDX = ((uint32_t*)branding)[7];
            break;
        case 0x80000004:    // Brand part 3, with frequency
            R_EAX = ((uint32_t*)branding)[8];
            R_EBX = ((uint32_t*)branding)[9];
            R_ECX = ((uint32_t*)branding)[10];
            R_EDX = ((uint32_t*)branding)[11];
            break;  
        case 0x80000005:
            R_EAX = 0;
            R_EBX = 0;
            R_ECX = 0;
            R_EDX = 0;
            break;
        default:
            printf_log(LOG_INFO, "Warning, CPUID command %X unsupported (ECX=%08x)\n", tmp32u, R_ECX);
            R_EAX = 0;
            R_EBX = 0;
            R_ECX = 0;
            R_EDX = 0;
    }   
}

void *GetSegmentBase( uint32_t desc )
{
    FIXME( "%x: stub\n", desc );
    return NULL;
}

uint32_t default_fs = 0; /* FIXME */

static box64context_t box64_context;
box64context_t *my_context = &box64_context;

typedef struct instsize_s {
    unsigned char x64:4;
    unsigned char nat:4;
} instsize_t;

typedef struct dynablock_s {
    void*           block;  // block-sizeof(void*) == self
    void*           actual_block;   // the actual start of the block (so block-sizeof(void*))
    struct dynablock_s*    previous;   // a previous block that might need to be freed
    int             size;
    void*           x64_addr;
    uintptr_t       x64_size;
    uint32_t        hash;
    uint8_t         done;
    uint8_t         gone;
    uint8_t         always_test;
    uint8_t         dirty;      // if need to be tested as soon as it's created
    int             isize;
    instsize_t*     instsize;
    void*           jmpnext;    // a branch jmpnext code when block is marked
} dynablock_t;

/**********************************************************************
 *           BTCpuProcessInit  (box64cpu.@)
 */
void WINAPI BTCpuSimulate(void)
{
    WOW64_CPURESERVED *cpu = NtCurrentTeb()->TlsSlots[WOW64_TLS_CPURESERVED];
    CONTEXT *tlsctx = NtCurrentTeb()->TlsSlots[WOW64_TLS_MAX_NUMBER]; // FIXME
    x64emu_t *emu = NtCurrentTeb()->TlsSlots[0]; // FIXME
    I386_CONTEXT *ctx = (I386_CONTEXT *)(cpu + 1);
    CONTEXT entry_context;

    TRACE( "START %lx\n", ctx->Eip );

    RtlCaptureContext(&entry_context);
    if (!tlsctx || tlsctx->Sp <= entry_context.Sp)
        NtCurrentTeb()->TlsSlots[WOW64_TLS_MAX_NUMBER] = &entry_context;

    R_EAX = ctx->Eax;
    R_EBX = ctx->Ebx;
    R_ECX = ctx->Ecx;
    R_EDX = ctx->Edx;
    R_ESI = ctx->Esi;
    R_EDI = ctx->Edi;
    R_EBP = ctx->Ebp;
    R_RIP = ctx->Eip;
    R_ESP = ctx->Esp;
    R_CS  = ctx->SegCs & 0xffff;
    R_DS  = ctx->SegDs & 0xffff;
    R_ES  = ctx->SegEs & 0xffff;
    R_FS  = ctx->SegFs & 0xffff;
    R_GS  = ctx->SegGs & 0xffff;
    R_SS  = ctx->SegSs & 0xffff;
    emu->eflags.x64 = ctx->EFlags;
    emu->segs_offs[_FS] = (uintptr_t)NtCurrentTeb() + NtCurrentTeb()->WowTebOffset;
    emu->win64_teb = (uint64_t)NtCurrentTeb();

    TRACE("WIN32 TEB %08llx\n", emu->segs_offs[_FS]);

    fpu_to_box(ctx, emu);

    if (box64_dynarec)
        DynaRun(emu);
    else
        Run(emu, 0);
    TRACE("finished eip %llx stack %x\n", R_RIP, R_ESP);
}

static uint8_t box64_is_addr_in_jit(void* addr)
{
    if (!addr)
        return FALSE;
    return !!FindDynablockFromNativeAddress(addr);
}

uintptr_t getX64Address(dynablock_t* db, uintptr_t arm_addr)
{
    uintptr_t x64addr = (uintptr_t)db->x64_addr;
    uintptr_t armaddr = (uintptr_t)db->block;
    int i = 0;
    do {
        int x64sz = 0;
        int armsz = 0;
        do {
            x64sz+=db->instsize[i].x64;
            armsz+=db->instsize[i].nat*4;
            ++i;
        } while((db->instsize[i-1].x64==15) || (db->instsize[i-1].nat==15));
        // if the opcode is a NOP on ARM side (so armsz==0), it cannot be an address to find
        if(armsz) {
            if((arm_addr>=armaddr) && (arm_addr<(armaddr+armsz)))
                return x64addr;
            armaddr+=armsz;
            x64addr+=x64sz;
        } else
            x64addr+=x64sz;
    } while(db->instsize[i].x64 || db->instsize[i].nat);
    return x64addr;
}

/* Note: This works on Linux by emulating the access to the register,
 * other platforms might have issues here
 * https://www.kernel.org/doc/html/latest/arch/arm64/cpu-feature-registers.html
 * https://developer.arm.com/documentation/ddi0601/latest/
*/
static void box64_detect_cpu_features(void)
{
    uint64_t isar0, isar1, mmfr2, feat;

    /* First try without privileged instructions that need to be emulated by the kernel */
    arm64_aes = RtlIsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
    arm64_crc32 = RtlIsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);
    arm64_atomics = RtlIsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE);

    asm("mrs %0, ID_AA64ISAR0_EL1" : "=r" (isar0));
    TRACE("ID_AA64ISAR0_EL1: 0x%016llx\n", isar0);
    asm("mrs %0, ID_AA64ISAR1_EL1" : "=r" (isar1));
    TRACE("ID_AA64ISAR1_EL1: 0x%016llx\n", isar1);
    asm("mrs %0, ID_AA64MMFR2_EL1" : "=r" (mmfr2));
    TRACE("ID_AA64MMFR2_EL1: 0x%016llx\n", mmfr2);

    /* AES & PMULL */
    feat = (isar0 >> 4) & 0x03;
    if (feat > 0)
    {
        TRACE("AES supported\n");
        arm64_aes = TRUE;
    }
    if (feat > 1)
    {
        TRACE("PMULL supported\n");
        arm64_pmull = TRUE;
    }

    /* SHA1 */
    feat = (isar0 >> 8) & 0x01;
    if (feat > 0)
    {
        TRACE("SHA1 supported\n");
        arm64_sha1 = TRUE;
    }

    /* SHA2 */
    feat = (isar0 >> 12) & 0x03;
    if (feat > 0)
    {
        TRACE("SHA2 supported\n");
        arm64_sha2 = TRUE;
    }

    /* CRC32 */
    feat = (isar0 >> 16) & 0x01;
    if (feat > 0)
    {
        TRACE("CRC32 supported\n");
        arm64_crc32 = TRUE;
    }

    /* Atomic */
    feat = (isar0 >> 20) & 0x03;
    if (feat > 0)
    {
        TRACE("Atomics supported\n");
        arm64_atomics = TRUE;
    }

    /* TS */
    feat = (isar0 >> 52) & 0x03;
    if (feat > 0)
    {
        TRACE("FlagM supported\n");
        arm64_flagm = TRUE;
    }

    /* RNDR */
    feat = (isar0 >> 60) & 0x0f;
    if (feat > 0)
    {
        TRACE("RNDR supported\n");
        arm64_rndr = TRUE;
    }

    /* USCAT */
    feat = (mmfr2 >> 32) & 0x0f;
    if (feat > 0)
    {
        TRACE("USCAT supported\n");
        arm64_uscat = TRUE;
    }

    /* FRINTTS */
    feat = (isar1 >> 32) & 0x0f;
    if (feat > 0)
    {
        TRACE("FRINTTS supported\n");
        arm64_frintts = TRUE;
    }
}

/**********************************************************************
 *           BTCpuProcessInit  (box64cpu.@)
 */
NTSTATUS WINAPI BTCpuProcessInit(void)
{
    HMODULE module;
    UNICODE_STRING str;
    void **p__wine_unix_call_dispatcher;

    MESSAGE("starting Box64 based box64cpu.dll\n");

    __TRY
    {
        box64_detect_cpu_features();
    }
    __EXCEPT_ALL
    {
        ERR("Failed to detect CPU features!\n");
    }
    __ENDTRY

    if (TRACE_ON(box64dump))
    {
        box64_dynarec_log = 1;
        box64_dynarec_dump = 1;
    }

    memset(bopcode, 0xc3, sizeof(bopcode));
    memset(unxcode, 0xc3, sizeof(unxcode));
    bopcode[0] = 0x2ecd;
    unxcode[0] = 0x2ecd;

    init_custommem_helper(&box64_context);

    if ((ULONG_PTR)bopcode >> 32 || (ULONG_PTR)unxcode >> 32)
    {
        ERR( "box64cpu loaded above 4G, disabling\n" );
        return STATUS_INVALID_ADDRESS;
    }

    RtlInitUnicodeString( &str, L"ntdll.dll" );
    LdrGetDllHandle( NULL, 0, &str, &module );
    p__wine_unix_call_dispatcher = RtlFindExportedRoutineByName( module, "__wine_unix_call_dispatcher" );
    __wine_unix_call_dispatcher = *p__wine_unix_call_dispatcher;

    RtlInitializeCriticalSection(&box64_context.mutex_dyndump);
    RtlInitializeCriticalSection(&box64_context.mutex_trace);
    RtlInitializeCriticalSection(&box64_context.mutex_tls);
    RtlInitializeCriticalSection(&box64_context.mutex_thread);
    RtlInitializeCriticalSection(&box64_context.mutex_bridge);
    RtlInitializeCriticalSection(&box64_context.mutex_lock);

    InitX64Trace(&box64_context);

    return STATUS_SUCCESS;
}

static uint32_t x86emu_parity_tab[8] =
{
    0x96696996,
    0x69969669,
    0x69969669,
    0x96696996,
    0x69969669,
    0x96696996,
    0x96696996,
    0x69969669,
};

/**********************************************************************
 *           BTCpuThreadInit  (box64cpu.@)
 */
NTSTATUS WINAPI BTCpuThreadInit(void)
{
    I386_CONTEXT *ctx;
    x64emu_t *emu = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*emu) );

    RtlWow64GetCurrentCpuArea( NULL, (void **)&ctx, NULL );
    emu->context = &box64_context;

    // setup cpu helpers
    for (int i=0; i<16; ++i)
        emu->sbiidx[i] = &emu->regs[i];
    emu->sbiidx[4] = &emu->zero;
    emu->x64emu_parity_tab = x86emu_parity_tab;

    reset_fpu(emu);

    NtCurrentTeb()->TlsSlots[0] = emu;  // FIXME
    return STATUS_SUCCESS;
}

/**********************************************************************
 *           BTCpuGetBopCode  (box64cpu.@)
 */
void * WINAPI BTCpuGetBopCode(void)
{
    return (UINT32*)&bopcode;
}

void * WINAPI __wine_get_unix_opcode(void)
{
    return (UINT32*)&unxcode;
}

/**********************************************************************
 *           BTCpuGetContext  (box64cpu.@)
 */
NTSTATUS WINAPI BTCpuGetContext( HANDLE thread, HANDLE process, void *unknown, I386_CONTEXT *ctx )
{
    return NtQueryInformationThread( thread, ThreadWow64Context, ctx, sizeof(*ctx), NULL );
}


/**********************************************************************
 *           BTCpuSetContext  (box64cpu.@)
 */
NTSTATUS WINAPI BTCpuSetContext( HANDLE thread, HANDLE process, void *unknown, I386_CONTEXT *ctx )
{
    return NtSetInformationThread( thread, ThreadWow64Context, ctx, sizeof(*ctx) );
}


/**********************************************************************
 *           BTCpuResetToConsistentState  (box64cpu.@)
 */
NTSTATUS WINAPI BTCpuResetToConsistentState( EXCEPTION_POINTERS *ptrs )
{
    x64emu_t *emu = NtCurrentTeb()->TlsSlots[0];  // FIXME
    EXCEPTION_RECORD *rec = ptrs->ExceptionRecord;
    CONTEXT *ctx = ptrs->ContextRecord;

    TRACE("\n%s\n", DumpCPURegs(emu, ctx->Pc, 1));

    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    {
        dynablock_t *db = NULL, *db2 = NULL;
        void* addr = NULL;
        uint32_t prot;

        if (rec->NumberParameters == 2 && rec->ExceptionInformation[0] == 1)
            addr = ULongToPtr(rec->ExceptionInformation[1]);

        db = FindDynablockFromNativeAddress(rec->ExceptionAddress);
        ERR("db ExceptionAddress %p\n", db);
        if (db)
        {
            ERR("db block %p\n", db->block);
            ERR("db actual_block %p\n", db->actual_block);
            ERR("db x64_addr %p\n", db->x64_addr);
            ERR("db dirty %x\n", db->dirty);
        }
        if (addr)
        {
            int db_need_test;
            prot = getProtection((uintptr_t)addr);
            db2 = FindDynablockFromNativeAddress((void*)addr);
            ERR("db addr %p prot %x\n", db2, prot);
            if (db2)
            {
                ERR("db block %p\n", db2->block);
                ERR("db actual_block %p\n", db2->actual_block);
                ERR("db x64_addr %p\n", db2->x64_addr);
                ERR("db dirty %x\n", db2->dirty);
            }
            unprotectDB((uintptr_t)addr, 1, 1);    // unprotect 1 byte... But then, the whole page will be unprotected
            db_need_test = (db && !box64_dynarec_fastpage)?getNeedTest((uintptr_t)db->x64_addr):0;
            prot = getProtection((uintptr_t)addr);
            ERR("addr prot %x\n", prot);
            ERR("db_need_test %x\n", db_need_test);
            if(db && ((addr>=db->x64_addr && addr<(db->x64_addr+db->x64_size)) || db_need_test))
            {
                x64emu_t *emu = NtCurrentTeb()->TlsSlots[0];  // FIXME
                ERR("emu->jmpbuf %p\n", emu->jmpbuf);
            }

            ERR(" ctx->Sp   %16llx\n",  ctx->Sp  );
            ERR(" ctx->Pc   %16llx\n",  ctx->Pc  );
            NtContinue(ctx, FALSE);
        }
    }

    if (!box64_is_addr_in_jit(ULongToPtr(ctx->Pc)))
        return STATUS_SUCCESS;

    /* Replace the host context with one captured before JIT entry so host code can unwind */
    memcpy(ctx, NtCurrentTeb()->TlsSlots[WOW64_TLS_MAX_NUMBER], sizeof(*ctx));
    return STATUS_SUCCESS;
}


/**********************************************************************
 *           BTCpuTurboThunkControl  (box64cpu.@)
 */
NTSTATUS WINAPI BTCpuTurboThunkControl( ULONG enable )
{
    FIXME( "NYI\n" );
    if (enable) return STATUS_NOT_SUPPORTED;
    /* we don't have turbo thunks yet */
    return STATUS_SUCCESS;
}

static NTSTATUS invalidate_mapped_section( PVOID addr )
{
    MEMORY_BASIC_INFORMATION mem_info;
    NTSTATUS ret = NtQueryVirtualMemory( NtCurrentProcess(), addr, MemoryBasicInformation, &mem_info,
                                         sizeof(mem_info), NULL );

    if (!NT_SUCCESS(ret))
    {
        ERR( "NtQueryVirtualMemory failed at %p\n", addr );
        return ret;
    }

    unprotectDB((uintptr_t)mem_info.AllocationBase, (DWORD64)(mem_info.BaseAddress + mem_info.RegionSize - mem_info.AllocationBase), 1);
    // or cleanDBFromAddressRange ?
    return STATUS_SUCCESS;
}

/**********************************************************************
 *           BTCpuNotifyUnmapViewOfSection  (xtajit.@)
 */
void WINAPI BTCpuNotifyUnmapViewOfSection( PVOID addr, ULONG flags )
{
    invalidate_mapped_section( addr );
}

/**********************************************************************
 *           BTCpuNotifyMemoryFree  (xtajit.@)
 */
void WINAPI BTCpuNotifyMemoryFree( PVOID addr, SIZE_T size, ULONG free_type )
{
    if (!size)
        invalidate_mapped_section( addr );
    else if (free_type & MEM_DECOMMIT)
        /* Invalidate all pages touched by the region, even if they are just straddled */
        unprotectDB((uintptr_t)ROUND_ADDR( addr, page_mask ), (DWORD64)ROUND_SIZE( addr, size ), 1);
    // or cleanDBFromAddressRange ?
}

/**********************************************************************
 *           BTCpuNotifyMemoryProtect  (xtajit.@)
 */
void WINAPI BTCpuNotifyMemoryProtect( PVOID addr, SIZE_T size, DWORD new_protect )
{
    if (!(new_protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        return;
    unprotectDB((uintptr_t)addr, size, 1);
    // or cleanDBFromAddressRange ?
}

/**********************************************************************
 *           BTCpuFlushInstructionCache2  (xtajit.@)
 */
void WINAPI BTCpuFlushInstructionCache2( LPCVOID addr, SIZE_T size)
{
    /* Invalidate all pages touched by the region, even if they are just straddled */
    unprotectDB((uintptr_t)addr, (DWORD64)size, 1);
    // or cleanDBFromAddressRange ?
}

BOOL WINAPI DllMain (HINSTANCE inst, DWORD reason, void *reserved )
{
    if (reason == DLL_PROCESS_ATTACH) LdrDisableThreadCalloutsForDll( inst );
    return TRUE;
}
