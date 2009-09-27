/*
 * Z80 helpers (without register variable usage)
 *
 *  Copyright (c) 2007 Stuart Brady <stuart.brady@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301 USA
 */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#include "cpu.h"
#include "exec-all.h"
#include "qemu-common.h"

//#define DEBUG_MMU

static int cpu_z80_find_by_name(const char *name);

CPUZ80State *cpu_z80_init(const char *model)
{
    CPUZ80State *env;
    static int inited;
    int id;

    id = cpu_z80_find_by_name(model);
    if (id == 0) {
        return NULL;
    }
    env = qemu_mallocz(sizeof(CPUZ80State));
    cpu_exec_init(env);

    /* init various static tables */
    if (!inited) {
        inited = 1;
        z80_translate_init();
    }
    env->model = id;
    cpu_reset(env);
    qemu_init_vcpu(env);
    return env;
}

typedef struct {
    int id;
    const char *name;
} Z80CPUModel;

static const Z80CPUModel z80_cpu_names[] = {
    { Z80_CPU_Z80,  "z80" },
    { Z80_CPU_R800, "r800" },
    { 0, NULL }
};

void z80_cpu_list(FILE *f, int (*cpu_fprintf)(FILE *f, const char *fmt, ...))
{
    int i;

    (*cpu_fprintf)(f, "Available CPUs:\n");
    for (i = 0; z80_cpu_names[i].name; i++) {
        (*cpu_fprintf)(f, "  %s\n", z80_cpu_names[i].name);
    }
}

/* return 0 if not found */
static int cpu_z80_find_by_name(const char *name)
{
    int i;
    int id;

    id = 0;
    for (i = 0; z80_cpu_names[i].name; i++) {
        if (strcmp(name, z80_cpu_names[i].name) == 0) {
            id = z80_cpu_names[i].id;
            break;
        }
    }
    return id;
}

/* NOTE: must be called outside the CPU execute loop */
void cpu_reset(CPUZ80State *env)
{
    if (qemu_loglevel_mask(CPU_LOG_RESET)) {
        qemu_log("CPU Reset (CPU %d)\n", env->cpu_index);
        log_cpu_state(env, 0);
    }

    memset(env, 0, offsetof(CPUZ80State, breakpoints));

    tlb_flush(env, 1);

    /* init to reset state */

#ifdef CONFIG_SOFTMMU
    env->hflags |= HF_SOFTMMU_MASK;
#endif

    env->pc = 0x0000;
    env->iff1 = 0;
    env->iff2 = 0;
    env->imode = 0;
    env->regs[R_A] = 0xff;
    env->regs[R_F] = 0xff;
    env->regs[R_SP] = 0xffff;
}

void cpu_z80_close(CPUZ80State *env)
{
    free(env);
}

/***********************************************************/
/* x86 debug */

void cpu_dump_state(CPUState *env, FILE *f,
                    int (*cpu_fprintf)(FILE *f, const char *fmt, ...),
                    int flags)
{
    int fl = env->regs[R_F];

    cpu_fprintf(f, "AF =%04x BC =%04x DE =%04x HL =%04x IX=%04x\n"
                   "AF'=%04x BC'=%04x DE'=%04x HL'=%04x IY=%04x\n"
                   "PC =%04x SP =%04x F=[%c%c%c%c%c%c%c%c]\n"
                   "IM=%i IFF1=%i IFF2=%i I=%02x R=%02x\n",
                   (env->regs[R_A] << 8) | env->regs[R_F],
                   env->regs[R_BC],
                   env->regs[R_DE],
                   env->regs[R_HL],
                   env->regs[R_IX],
                   (env->regs[R_AX] << 8) | env->regs[R_FX],
                   env->regs[R_BCX],
                   env->regs[R_DEX],
                   env->regs[R_HLX],
                   env->regs[R_IY],
                   env->pc, /* pc == -1 ? env->pc : pc, */
                   env->regs[R_SP],
                   fl & 0x80 ? 'S' : '-',
                   fl & 0x40 ? 'Z' : '-',
                   fl & 0x20 ? 'Y' : '-',
                   fl & 0x10 ? 'H' : '-',
                   fl & 0x08 ? 'X' : '-',
                   fl & 0x04 ? 'P' : '-',
                   fl & 0x02 ? 'N' : '-',
                   fl & 0x01 ? 'C' : '-',
                   env->imode, env->iff1, env->iff2, env->regs[R_I], env->regs[R_R]);
}

/***********************************************************/
static void cpu_z80_flush_tlb(CPUZ80State *env, target_ulong addr)
{
    tlb_flush_page(env, addr);
}

/* return value:
   -1 = cannot handle fault
   0  = nothing more to do
   1  = generate PF fault
   2  = soft MMU activation required for this block
*/
int cpu_z80_handle_mmu_fault(CPUZ80State *env, target_ulong addr,
                             int is_write1, int mmu_idx, int is_softmmu)
{
    int prot, page_size, ret, is_write;
    unsigned long paddr, page_offset;
    target_ulong vaddr, virt_addr;
    int is_user = 0;

#if defined(DEBUG_MMU)
    printf("MMU fault: addr=" TARGET_FMT_lx " w=%d u=%d pc=" TARGET_FMT_lx "\n",
           addr, is_write1, mmu_idx, env->pc);
#endif
    is_write = is_write1 & 1;

    virt_addr = addr & TARGET_PAGE_MASK;
    prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    page_size = TARGET_PAGE_SIZE;

    if (env->mapaddr) {
        addr = env->mapaddr(addr);
    }
    page_offset = (addr & TARGET_PAGE_MASK) & (page_size - 1);
    paddr = (addr & TARGET_PAGE_MASK) + page_offset;
    vaddr = virt_addr + page_offset;

    ret = tlb_set_page_exec(env, vaddr, paddr, prot, is_user, is_softmmu);
    return ret;
}

target_phys_addr_t cpu_get_phys_page_debug(CPUState *env, target_ulong addr)
{
    uint32_t pte, paddr, page_offset, page_size;

    pte = addr;
    page_size = TARGET_PAGE_SIZE;

    if (env->mapaddr) {
        addr = env->mapaddr(addr);
    }
    page_offset = (addr & TARGET_PAGE_MASK) & (page_size - 1);
    paddr = (pte & TARGET_PAGE_MASK) + page_offset;
    return paddr;
}
