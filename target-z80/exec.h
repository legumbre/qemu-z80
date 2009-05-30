/*
 * Z80 execution defines
 *
 *  Copyright (c) 2007-2009 Stuart Brady <stuart.brady@gmail.com>
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
#include "config.h"
#include "dyngen-exec.h"

#define TARGET_LONG_BITS 32

#include "cpu-defs.h"

/* at least 4 register variables are defined */
register struct CPUZ80State *env asm(AREG0);

#if TARGET_LONG_BITS > HOST_LONG_BITS

/* no registers can be used */
#define T0 (env->t0)
#define T1 (env->t1)

#else

/* XXX: use unsigned long instead of target_ulong - better code will
   be generated for 64 bit CPUs */
register target_ulong T0 asm(AREG1);
register target_ulong T1 asm(AREG2);

#endif /* ! (TARGET_LONG_BITS > HOST_LONG_BITS) */

#define A0 (env->a0)

#define A   (env->regs[R_A])
#define F   (env->regs[R_F])
#define BC  (env->regs[R_BC])
#define DE  (env->regs[R_DE])
#define HL  (env->regs[R_HL])
#define IX  (env->regs[R_IX])
#define IY  (env->regs[R_IY])
#define SP  (env->regs[R_SP])
#define I   (env->regs[R_I])
#define R   (env->regs[R_R])
#define AX  (env->regs[R_AX])
#define FX  (env->regs[R_FX])
#define BCX (env->regs[R_BCX])
#define DEX (env->regs[R_DEX])
#define HLX (env->regs[R_HLX])

#define PC  (env->pc)

#include "cpu.h"
#include "exec-all.h"

void do_interrupt(CPUZ80State *env);
void raise_interrupt(int intno, int is_int, int error_code,
                     int next_eip_addend);
void raise_exception_err(int exception_index, int error_code);
void raise_exception(int exception_index);

#if !defined(CONFIG_USER_ONLY)

#include "softmmu_exec.h"

#endif /* !defined(CONFIG_USER_ONLY) */

extern const uint8_t parity_table[256];

static inline void env_to_regs(void)
{
#ifdef reg_A
    A = env->regs[R_A];
#endif
#ifdef reg_F
    F = env->regs[R_F];
#endif
#ifdef reg_BC
    BC = env->regs[R_BC];
#endif
#ifdef reg_DE
    DE = env->regs[R_DE];
#endif
#ifdef reg_HL
    HL = env->regs[R_HL];
#endif
#ifdef reg_IX
    IX = env->regs[R_IX];
#endif
#ifdef reg_IY
    IY = env->regs[R_IY];
#endif
#ifdef reg_SP
    SP = env->regs[R_SP];
#endif
#ifdef reg_I
    I = env->regs[R_I];
#endif
#ifdef reg_R
    R = env->regs[R_R];
#endif
#ifdef reg_AX
    AX = env->regs[R_AX];
#endif
#ifdef reg_FX
    FX = env->regs[R_FX];
#endif
#ifdef reg_BCX
    BCX = env->regs[R_BCX];
#endif
#ifdef reg_DEX
    DEX = env->regs[R_DEX];
#endif
#ifdef reg_HLX
    HLX = env->regs[R_HLX];
#endif
}

static inline void regs_to_env(void)
{
#ifdef reg_A
    env->regs[R_A] = A;
#endif
#ifdef reg_F
    env->regs[R_F] = F;
#endif
#ifdef reg_BC
    env->regs[R_BC] = BC;
#endif
#ifdef reg_DE
    env->regs[R_DE] = DE;
#endif
#ifdef reg_HL
    env->regs[R_HL] = HL;
#endif
#ifdef reg_IX
    env->regs[R_IX] = IX;
#endif
#ifdef reg_IY
    env->regs[R_IY] = IY;
#endif
#ifdef reg_SP
    env->regs[R_SP] = SP;
#endif
#ifdef reg_I
    env->regs[R_I] = I;
#endif
#ifdef reg_R
    env->regs[R_R] = R;
#endif
#ifdef reg_AX
    env->regs[R_AX] = AX;
#endif
#ifdef reg_FX
    env->regs[R_FX] = FX;
#endif
#ifdef reg_BCX
    env->regs[R_BCX] = BCX;
#endif
#ifdef reg_DEX
    env->regs[R_DEX] = DEX;
#endif
#ifdef reg_HLX
    env->regs[R_HLX] = HLX;
#endif
}

static inline int cpu_has_work(CPUState *env)
{
    return env->interrupt_request & CPU_INTERRUPT_HARD;
}

static inline int cpu_halted(CPUState *env)
{
    if (!env->halted) {
        return 0;
    }
    //printf("%s: at PC 0x%x halted == %d, irq %d\n",__FUNCTION__, env->pc, env->halted,env->interrupt_request);
    if (cpu_has_work(env)) {
        env->halted = 0;
        return 0;
    }
    return EXCP_HALTED;
}
