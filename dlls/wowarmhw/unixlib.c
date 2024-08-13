/*
 * Copyright 2022-2023 André Zwing
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdarg.h>
#include <dlfcn.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "unixlib.h"
#include "wine/debug.h"
#include "qemu.h"

WINE_DEFAULT_DEBUG_CHANNEL(wow);

__thread CPUState *thread_cpu;

#define ExitProcess(x) exit(x)

static void (*pcpu_reset)(CPUState*);
static void (*pcpu_dump_state)(CPUState *cpu, FILE *f, int flags);
static void (*pcpu_exec_start)(CPUState *);
static int (*pcpu_exec)(CPUState *);
static void (*pcpu_exec_end)(CPUState *);
static void (*pprocess_queued_cpu_work)(CPUState *);
static void (*prcu_register_thread)(void);
static void (*ptcg_register_thread)(void);
static void (*pmodule_call_init)(module_init_type);
static void (*pqemu_init_cpu_list)(void);
static void (*ptcg_exec_init)(unsigned long);
static void (*ptcg_prologue_init)(void*/*TCGContext*/);
static void (*ptcg_region_init)(void);
static CPUState* (*pcpu_create)(const char *typename);
static int (*pqemu_str_to_log_mask)(const char *);
static void (*pqemu_log_needs_buffers)(void);
static void (*pqemu_set_log)(int);
static void (*perror_init)(const char *);
static void (*ppage_set_flags)(target_ulong start, target_ulong end, int flags);
static void (*ptb_invalidate_phys_page_range)(tb_page_addr_t start, tb_page_addr_t end);

static void* *ptcg_ctx;

static void *emuapi_handle;

static void init_thread_cpu(void)
{
    if (!thread_cpu)
        thread_cpu = pcpu_create(ARM_CPU_TYPE_NAME("max"));
    if (!thread_cpu)
    {
        fprintf(stderr, "Unable to find CPU definition\n");
    }
    pcpu_reset(thread_cpu);
}

static NTSTATUS attach( void *args )
{
    static char default_lib[] = "libqemu-arm.so";
    char *holib, *qemu_log;
    int log_mask;

    MESSAGE("starting Qemu based wowarmhw.dll\n");

    holib = getenv("HOLIB");
    if (!holib)
        holib = default_lib;
    if (!(emuapi_handle = dlopen( holib, RTLD_NOW ))) return STATUS_DLL_NOT_FOUND;

#define LOAD_FUNCPTR(f) if((p##f = dlsym(emuapi_handle, #f)) == NULL) {ERR(#f " %p\n", p##f);return STATUS_ENTRYPOINT_NOT_FOUND;}
#define LOAD_FUNCPTR_OPT(f) if((p##f = dlsym(emuapi_handle, #f)) == NULL) {ERR(#f " %p\n", p##f);}
    LOAD_FUNCPTR(cpu_reset);
    LOAD_FUNCPTR(cpu_dump_state);
    LOAD_FUNCPTR(cpu_exec_start);
    LOAD_FUNCPTR(cpu_exec);
    LOAD_FUNCPTR(cpu_exec_end);
    LOAD_FUNCPTR(process_queued_cpu_work);
    LOAD_FUNCPTR(rcu_register_thread);
    LOAD_FUNCPTR(tcg_register_thread);
    LOAD_FUNCPTR(module_call_init);
    LOAD_FUNCPTR(qemu_init_cpu_list);
    LOAD_FUNCPTR_OPT(tcg_exec_init);
    LOAD_FUNCPTR(tcg_prologue_init);
    LOAD_FUNCPTR(tcg_region_init);
    LOAD_FUNCPTR(cpu_create);
    LOAD_FUNCPTR(qemu_str_to_log_mask);
    LOAD_FUNCPTR_OPT(qemu_log_needs_buffers);
    LOAD_FUNCPTR(qemu_set_log);
    LOAD_FUNCPTR(error_init);
    LOAD_FUNCPTR(page_set_flags);
    LOAD_FUNCPTR(tcg_ctx);
    LOAD_FUNCPTR(tb_invalidate_phys_page_range);
#undef LOAD_FUNCPTR_OPT
#undef LOAD_FUNCPTR

    perror_init("HOqemu");
    pmodule_call_init(MODULE_INIT_TRACE);
    pqemu_init_cpu_list();
    pmodule_call_init(MODULE_INIT_QOM);

    qemu_log = getenv("QEMU_LOG");
    if (qemu_log && (log_mask = pqemu_str_to_log_mask(qemu_log)))
    {
        if (log_mask) {
            if (pqemu_log_needs_buffers)
                pqemu_log_needs_buffers();
            pqemu_set_log(log_mask);
        }
    }

    if (ptcg_exec_init)
        ptcg_exec_init(0);
    init_thread_cpu();

    ptcg_prologue_init(*ptcg_ctx);
    ptcg_region_init();
    ppage_set_flags(4096, 0x80000000, PAGE_VALID | PAGE_EXEC | PAGE_WRITE | PAGE_READ);

    return STATUS_SUCCESS;
}

static NTSTATUS detach( void *args )
{
    dlclose( emuapi_handle );
    return STATUS_SUCCESS;
}

static inline void *get_wow_teb( TEB *teb )
{
    return teb->WowTebOffset ? (void *)((char *)teb + teb->WowTebOffset) : NULL;
}

static void ctx_to_qemu(ARM_CONTEXT *c, CPUARMState *env)
{
    TEB *teb = NtCurrentTeb();
    void *wowteb = get_wow_teb(teb);
    env->regs[0] = c->R0;
    env->regs[1] = c->R1;
    env->regs[2] = c->R2;
    env->regs[3] = c->R3;
    env->regs[4] = c->R4;
    env->regs[5] = c->R5;
    env->regs[6] = c->R6;
    env->regs[7] = c->R7;
    env->regs[8] = c->R8;
    env->regs[9] = c->R9;
    env->regs[10] = c->R10;
    env->regs[11] = c->R11;
    env->regs[12] = c->R12;
    env->regs[13] = c->Sp;
    env->regs[14] = c->Lr;
    env->regs[15] = c->Pc;

    /* TODO: ARM_CONTEXT also has CPSR!!!! */
    env->uncached_cpsr = 0x10 /*user*/;
    env->cp15.tpidrurw_ns = (uint64_t)wowteb;
    if ((uint32_t)c->Pc & 1)
    {
        env->thumb = 1;
        env->uncached_cpsr |= 0x20 /* thumb? */;
        env->regs[15] = env->regs[15] & ~1;
    }
    /* TODO: Much more! */
}

static void qemu_to_ctx(CPUARMState *env, ARM_CONTEXT *c)
{
     c->R0 = env->regs[0];
     c->R1 = env->regs[1];
     c->R2 = env->regs[2];
     c->R3 = env->regs[3];
     c->R4 = env->regs[4];
     c->R5 = env->regs[5];
     c->R6 = env->regs[6];
     c->R7 = env->regs[7];
     c->R8 = env->regs[8];
     c->R9 = env->regs[9];
     c->R10 = env->regs[10];
     c->R11 = env->regs[11];
     c->R12 = env->regs[12];
     c->Sp = env->regs[13];
     c->Lr = env->regs[14];
     c->Pc = env->regs[15];
     c->Cpsr = env->uncached_cpsr;

    /* TODO: Much more! */
}

static NTSTATUS emu_run( void *args )
{
    const struct emu_run_params *params = args;
    CPUState *cs;
    CPUARMState *env;
    int trapnr;

    if (!params->c->Pc)
    {
        ERR("Attempting to execute NULL.\n");
        ExitProcess(1);
    }

    if (!thread_cpu)
    {
        FIXME("Initializing new CPU for thread %x.\n", GetCurrentThreadId());
        prcu_register_thread();
        ptcg_register_thread();
        init_thread_cpu();
        ppage_set_flags(4096, 0x80000000, 8|4|2|1);
    }
    cs = thread_cpu;
    env = cs->env_ptr;

    ctx_to_qemu(params->c, env);

    while (TRUE)
    {
        pcpu_exec_start(cs);
        trapnr = pcpu_exec(cs);
        pcpu_exec_end(cs);
        pprocess_queued_cpu_work(cs);

        switch (trapnr)
        {
            case EXCP_SWI:
                break;

            case EXCP_INTERRUPT:
                ERR("Unhandled interrupt %x, exiting.\n", trapnr);
                pcpu_dump_state(cs, stderr, 0);
                ExitProcess(255);

            case EXCP_YIELD:
                ERR("pause trap %x, continue.\n", trapnr);
                sched_yield();
                continue;

            default:
                ERR("Unhandled trap %x, exiting.\n", trapnr);
                pcpu_dump_state(cs, stderr, 0);
                ExitProcess(255);
        }

        qemu_to_ctx(env, params->c);
        return 0;
    }

    return 0;
}

static NTSTATUS set_prot( void *args )
{
    const struct set_prot_params *params = args;
    int flags = PAGE_VALID;
    DWORD64 end;

    if (params->prot & PAGE_EXECUTE)
        flags |= PAGE_EXEC;
    if (params->prot & PAGE_EXECUTE_READ)
        flags |= PAGE_EXEC | PAGE_READ;
    if (params->prot & PAGE_EXECUTE_READWRITE)
        flags |= PAGE_READ | PAGE_WRITE;
    if (params->prot & PAGE_READONLY)
        flags = PAGE_READ;
    if (params->prot & PAGE_NOACCESS)
        flags = 0;

    end = params->base + params->length;
    if (end <= params->base) end = params->base + 0x1000;

    ppage_set_flags(params->base, end, flags);
    return STATUS_SUCCESS;
}

static NTSTATUS invalidate_code_range( void *args )
{
    const struct invalidate_code_range_params *params = args;
    ptb_invalidate_phys_page_range(params->base, params->base + params->length);
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    attach,
    detach,
    emu_run,
    set_prot,
    invalidate_code_range
};
