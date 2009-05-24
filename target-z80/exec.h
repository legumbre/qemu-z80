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
#define T2 (env->t2)

#else

/* XXX: use unsigned long instead of target_ulong - better code will
   be generated for 64 bit CPUs */
register target_ulong T0 asm(AREG1);
register target_ulong T1 asm(AREG2);
register target_ulong T2 asm(AREG3);

#endif /* ! (TARGET_LONG_BITS > HOST_LONG_BITS) */

#define A0 T2

extern FILE *logfile;
extern int loglevel;

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

#define CC_SRC (env->cc_src)
#define CC_DST (env->cc_dst)
#define CC_OP  (env->cc_op)

/* float macros */
#if 0
#define FT0    (env->ft0)
#define ST0    (env->fpregs[env->fpstt].d)
#define ST(n)  (env->fpregs[(env->fpstt + (n)) & 7].d)
#define ST1    ST(1)

#ifdef USE_FP_CONVERT
#define FP_CONVERT  (env->fp_convert)
#endif
#endif

#include "cpu.h"
#include "exec-all.h"

typedef struct CCTable {
    int (*compute_all)(void); /* return all the flags */
    int (*compute_c)(void);  /* return the C flag */
} CCTable;

extern CCTable cc_table[];

void load_seg(int seg_reg, int selector);
void helper_ljmp_protected_T0_T1(int next_eip);
void helper_lcall_real_T0_T1(int shift, int next_eip);
void helper_lcall_protected_T0_T1(int shift, int next_eip);
void helper_iret_real(int shift);
void helper_iret_protected(int shift, int next_eip);
void helper_lret_protected(int shift, int addend);
void helper_lldt_T0(void);
void helper_ltr_T0(void);
void helper_movl_crN_T0(int reg);
void helper_movl_drN_T0(int reg);
void helper_invlpg(target_ulong addr);

int cpu_z80_handle_mmu_fault(CPUZ80State *env, target_ulong addr,
                             int is_write, int is_user, int is_softmmu);
void tlb_fill(target_ulong addr, int is_write, int is_user,
              void *retaddr);
void __hidden cpu_lock(void);
void __hidden cpu_unlock(void);
void do_interrupt(CPUZ80State *env);
void raise_interrupt(int intno, int is_int, int error_code,
                     int next_eip_addend);
void raise_exception_err(int exception_index, int error_code);
void raise_exception(int exception_index);
void do_smm_enter(void);
void __hidden cpu_loop_exit(void);

void OPPROTO op_movl_eflags_T0(void);
void OPPROTO op_movl_T0_eflags(void);

#if !defined(CONFIG_USER_ONLY)

#include "softmmu_exec.h"

static inline double ldfq(target_ulong ptr)
{
    union {
        double d;
        uint64_t i;
    } u;
    u.i = ldq(ptr);
    return u.d;
}

static inline void stfq(target_ulong ptr, double v)
{
    union {
        double d;
        uint64_t i;
    } u;
    u.d = v;
    stq(ptr, u.i);
}

static inline float ldfl(target_ulong ptr)
{
    union {
        float f;
        uint32_t i;
    } u;
    u.i = ldl(ptr);
    return u.f;
}

static inline void stfl(target_ulong ptr, float v)
{
    union {
        float f;
        uint32_t i;
    } u;
    u.f = v;
    stl(ptr, u.i);
}

#endif /* !defined(CONFIG_USER_ONLY) */

#define RC_MASK         0xc00
#define RC_NEAR		0x000
#define RC_DOWN		0x400
#define RC_UP		0x800
#define RC_CHOP		0xc00

void helper_halt(void);
void helper_monitor(void);
void helper_mwait(void);

extern const uint8_t parity_table[256];
extern const uint8_t rclw_table[32];
extern const uint8_t rclb_table[32];

static inline uint32_t compute_eflags(void)
{
    return env->eflags | cc_table[CC_OP].compute_all();
}

/* NOTE: CC_OP must be modified manually to CC_OP_EFLAGS */
static inline void load_eflags(int eflags, int update_mask)
{
    CC_SRC = eflags & (CC_S | CC_Z | CC_P | CC_C);
    env->eflags = (env->eflags & ~update_mask) |
        (eflags & update_mask);
}

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

static inline int cpu_halted(CPUState* env)
{
    if (!env->halted) {
        return 0;
    }
    //printf("%s: at PC 0x%x halted == %d, irq %d\n",__FUNCTION__, env->pc, env->halted,env->interrupt_request);
    if (env->interrupt_request & CPU_INTERRUPT_HARD) {
        env->halted = 0;
        return 0;
    }
    return EXCP_HALTED;
}
