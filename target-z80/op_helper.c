/*
 * Z80 helpers
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
#include "exec.h"
#include "helper.h"

const uint8_t parity_table[256] = {
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
};

/* thread support */

spinlock_t global_cpu_lock = SPIN_LOCK_UNLOCKED;

void cpu_lock(void)
{
    spin_lock(&global_cpu_lock);
}

void cpu_unlock(void)
{
    spin_unlock(&global_cpu_lock);
}

void do_interrupt(CPUZ80State *env)
{
// printf("z80: do_interrupt()\n");

    if (!env->iff1) {
        return;
    }

    env->iff1 = 0;
    env->iff2 = 0; /* XXX: Unchanged for NMI */

    {
        target_ulong sp;
        sp = (uint16_t)(env->regs[R_SP] - 2);
        env->regs[R_SP] = sp;
        stw_kernel(sp, env->pc);
    }

    /* IM0 = execute data on bus (0xff == rst $38) */
    /* IM1 = execute rst $38 (ROM uses this)*/
    /* IM2 = indirect jump -- address is held at (I << 8) | DATA */

    /* value on data bus is 0xff for the zx spectrum */

    /* when an interrupt occurs, iff1 and iff2 are reset, disabling interrupts */
    /* when an NMI occurs, iff1 is reset. iff2 is left unchanged */

    uint8_t d;
    switch (env->imode) {
    case 0:
        /* XXX: assuming 0xff on data bus */
    case 1:
        env->pc = 0x0038;
        break;
    case 2:
        /* XXX: assuming 0xff on data bus */
        d = 0xff;
        env->pc = lduw_kernel((env->regs[R_I] << 8) | d);
        break;
    }
}

/*
 * Signal an interruption. It is executed in the main CPU loop.
 * is_int is TRUE if coming from the int instruction. next_eip is the
 * EIP value AFTER the interrupt instruction. It is only relevant if
 * is_int is TRUE.
 */
void raise_interrupt(int intno, int is_int, int error_code,
                     int next_eip_addend)
{
    env->exception_index = intno;
    env->error_code = error_code;
    env->exception_is_int = is_int;
    env->exception_next_pc = env->pc + next_eip_addend;
    cpu_loop_exit();
}

/* same as raise_exception_err, but do not restore global registers */
static void raise_exception_err_norestore(int exception_index, int error_code)
{
    env->exception_index = exception_index;
    env->error_code = error_code;
    env->exception_is_int = 0;
    env->exception_next_pc = 0;
    longjmp(env->jmp_env, 1);
}

/* shortcuts to generate exceptions */

void (raise_exception_err)(int exception_index, int error_code)
{
    raise_interrupt(exception_index, 0, error_code, 0);
}

void raise_exception(int exception_index)
{
    raise_interrupt(exception_index, 0, 0, 0);
}

void HELPER(debug)(void)
{
    env->exception_index = EXCP_DEBUG;
    cpu_loop_exit();
}

void HELPER(raise_exception)(int exception_index)
{
    raise_exception(exception_index);
}

void HELPER(set_inhibit_irq)(void)
{
    env->hflags |= HF_INHIBIT_IRQ_MASK;
}

void HELPER(reset_inhibit_irq)(void)
{
    env->hflags &= ~HF_INHIBIT_IRQ_MASK;
}

/* instruction-specific helpers */

void HELPER(halt)(void)
{
    //printf("halting at PC 0x%x\n",env->pc);
    env->halted = 1;
    env->hflags &= ~HF_INHIBIT_IRQ_MASK; /* needed if sti is just before */
    env->exception_index = EXCP_HLT;
    cpu_loop_exit();
}

void HELPER(in_T0_im)(uint32_t val)
{
    T0 = cpu_inb(env, (A << 8) | val);
}

void HELPER(in_T0_bc_cc)(void)
{
    int sf, zf, pf;

    T0 = cpu_inb(env, BC);

    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    pf = parity_table[(uint8_t)T0];
    F = (F & CC_C) | sf | zf | pf;
}

void HELPER(out_T0_im)(uint32_t val)
{
    cpu_outb(env, (A << 8) | val, T0);
}

void HELPER(out_T0_bc)(void)
{
    cpu_outb(env, BC, T0);
}

#if !defined(CONFIG_USER_ONLY)

#define MMUSUFFIX _mmu

#define SHIFT 0
#include "softmmu_template.h"

#define SHIFT 1
#include "softmmu_template.h"

#define SHIFT 2
#include "softmmu_template.h"

#define SHIFT 3
#include "softmmu_template.h"

#endif

/* try to fill the TLB and return an exception if error. If retaddr is
   NULL, it means that the function was called in C code (i.e. not
   from generated code or from helper.c) */
/* XXX: fix it to restore all registers */
void tlb_fill(target_ulong addr, int is_write, int is_user, void *retaddr)
{
    TranslationBlock *tb;
    int ret;
    unsigned long pc;
    CPUZ80State *saved_env;

    /* XXX: hack to restore env in all cases, even if not called from
       generated code */
    saved_env = env;
    env = cpu_single_env;

    ret = cpu_z80_handle_mmu_fault(env, addr, is_write, is_user, 1);
    if (ret) {
        if (retaddr) {
            /* now we have a real cpu fault */
            pc = (unsigned long)retaddr;
            tb = tb_find_pc(pc);
            if (tb) {
                /* the PC is inside the translated code. It means that we have
                   a virtual CPU fault */
                cpu_restore_state(tb, env, pc, NULL);
            }
        }
        if (retaddr) {
            raise_exception_err(env->exception_index, env->error_code);
        } else {
            raise_exception_err_norestore(env->exception_index, env->error_code);
        }
    }
    env = saved_env;
}
