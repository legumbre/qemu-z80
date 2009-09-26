/*
 * Z80 virtual CPU header
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
#ifndef CPU_Z80_H
#define CPU_Z80_H

#include "config.h"

#define TARGET_LONG_BITS 32

/* target supports implicit self modifying code */
#define TARGET_HAS_SMC
/* support for self modifying code even if the modified instruction is
   close to the modifying instruction */
#define TARGET_HAS_PRECISE_SMC

#define TARGET_HAS_ICE 1

#define ELF_MACHINE	EM_NONE

#define CPUState struct CPUZ80State

#include "cpu-defs.h"

#include "softfloat.h"

/* Z80 registers */

#define R_A     0
#define R_F     1

#define R_BC    2
#define R_DE    3
#define R_HL    4
#define R_IX    5
#define R_IY    6
#define R_SP    7

#define R_I     8
#define R_R     9

#define R_AX    10
#define R_FX    11
#define R_BCX   12
#define R_DEX   13
#define R_HLX   14

#define CPU_NB_REGS 15

/* flags masks */
#define CC_C   	0x0001
#define CC_N    0x0002
#define CC_P 	0x0004
#define CC_X 	0x0008
#define CC_H	0x0010
#define CC_Y 	0x0020
#define CC_Z	0x0040
#define CC_S    0x0080

/* hidden flags - used internally by qemu to represent additionnal cpu
   states. Only the CPL and INHIBIT_IRQ are not redundant. We avoid
   using the IOPL_MASK, TF_MASK and VM_MASK bit position to ease oring
   with eflags. */
/* current cpl */
#define HF_CPL_SHIFT         0
/* true if soft mmu is being used */
#define HF_SOFTMMU_SHIFT     2
/* true if hardware interrupts must be disabled for next instruction */
#define HF_INHIBIT_IRQ_SHIFT 3
/* 16 or 32 segments */
#define HF_CS32_SHIFT        4
#define HF_SS32_SHIFT        5
/* zero base for DS, ES and SS : can be '0' only in 32 bit CS segment */
#define HF_ADDSEG_SHIFT      6
/* copy of CR0.PE (protected mode) */
#define HF_PE_SHIFT          7
#define HF_TF_SHIFT          8 /* must be same as eflags */
#define HF_MP_SHIFT          9 /* the order must be MP, EM, TS */
#define HF_EM_SHIFT         10
#define HF_TS_SHIFT         11
#define HF_IOPL_SHIFT       12 /* must be same as eflags */
#define HF_LMA_SHIFT        14 /* only used on x86_64: long mode active */
#define HF_CS64_SHIFT       15 /* only used on x86_64: 64 bit code segment  */
#define HF_OSFXSR_SHIFT     16 /* CR4.OSFXSR */
#define HF_VM_SHIFT         17 /* must be same as eflags */
#define HF_SMM_SHIFT        19 /* CPU in SMM mode */

#define HF_CPL_MASK          (3 << HF_CPL_SHIFT)
#define HF_SOFTMMU_MASK      (1 << HF_SOFTMMU_SHIFT)
#define HF_INHIBIT_IRQ_MASK  (1 << HF_INHIBIT_IRQ_SHIFT)
#define HF_CS32_MASK         (1 << HF_CS32_SHIFT)
#define HF_SS32_MASK         (1 << HF_SS32_SHIFT)
#define HF_ADDSEG_MASK       (1 << HF_ADDSEG_SHIFT)
#define HF_PE_MASK           (1 << HF_PE_SHIFT)
#define HF_TF_MASK           (1 << HF_TF_SHIFT)
#define HF_MP_MASK           (1 << HF_MP_SHIFT)
#define HF_EM_MASK           (1 << HF_EM_SHIFT)
#define HF_TS_MASK           (1 << HF_TS_SHIFT)
#define HF_LMA_MASK          (1 << HF_LMA_SHIFT)
#define HF_CS64_MASK         (1 << HF_CS64_SHIFT)
#define HF_OSFXSR_MASK       (1 << HF_OSFXSR_SHIFT)
#define HF_SMM_MASK          (1 << HF_SMM_SHIFT)

#define EXCP00_DIVZ	0
#define EXCP01_SSTP	1
#define EXCP02_NMI	2
#define EXCP03_INT3	3
#define EXCP04_INTO	4
#define EXCP05_BOUND	5
#define EXCP06_ILLOP	6
#define EXCP07_PREX	7
#define EXCP08_DBLE	8
#define EXCP09_XERR	9
#define EXCP0A_TSS	10
#define EXCP0B_NOSEG	11
#define EXCP0C_STACK	12
#define EXCP0D_GPF	13
#define EXCP0E_PAGE	14
#define EXCP10_COPR	16
#define EXCP11_ALGN	17
#define EXCP12_MCHK	18

#define NB_MMU_MODES 2

typedef struct CPUZ80State {
#if TARGET_LONG_BITS > HOST_LONG_BITS
    /* temporaries if we cannot store them in host registers */
    target_ulong t0, t1;
#endif
    target_ulong a0;

    /* Z80 registers */
    uint16_t pc;
    /* not sure if this is messy: */
    target_ulong regs[CPU_NB_REGS];

    int iff1;
    int iff2;
    int imode;

    int ir;

    target_ulong (*mapaddr)(target_ulong addr);

    /* standard registers */
    target_ulong eflags; /* eflags register. During CPU emulation, CC
                        flags are set to zero because they are
                        stored elsewhere */

    /* emulator internal eflags handling */
    uint32_t hflags; /* hidden flags, see HF_xxx constants */

    target_ulong cr[5]; /* NOTE: cr1 is unused */

    /* sysenter registers */
    uint64_t efer;
    uint64_t star;

    uint64_t pat;

    /* exception/interrupt handling */
    int error_code;
    int exception_is_int;
    target_ulong exception_next_pc;
    target_ulong dr[8]; /* debug registers */
    uint32_t smbase;

    CPU_COMMON

    int model;

    /* in order to simplify APIC support, we leave this pointer to the
       user */
    struct APICState *apic_state;
} CPUZ80State;

CPUZ80State *cpu_z80_init(const char *cpu_model);
void z80_translate_init(void);
int cpu_z80_exec(CPUZ80State *s);
void cpu_z80_close(CPUZ80State *s);
int cpu_get_pic_interrupt(CPUZ80State *s);

/* wrapper, just in case memory mappings must be changed */
static inline void cpu_z80_set_cpl(CPUZ80State *s, int cpl)
{
#if HF_CPL_MASK == 3
    s->hflags = (s->hflags & ~HF_CPL_MASK) | cpl;
#else
#error HF_CPL_MASK is hardcoded
#endif
}

/* you can call this signal handler from your SIGBUS and SIGSEGV
   signal handlers to inform the virtual CPU of exceptions. non zero
   is returned if the signal was handled by the virtual CPU.  */
struct siginfo;
int cpu_z80_signal_handler(int host_signum, struct siginfo *info,
                           void *puc);

uint64_t cpu_get_tsc(CPUZ80State *env);

int cpu_z80_handle_mmu_fault(CPUZ80State *env1, target_ulong address, int rw,
                             int mmu_idx, int is_softmmu);

void z80_cpu_list(FILE *f, int (*cpu_fprintf)(FILE *f, const char *fmt, ...));

#define Z80_CPU_Z80  1
#define Z80_CPU_R800 2

#define TARGET_PAGE_BITS 12

#define cpu_init cpu_z80_init
#define cpu_exec cpu_z80_exec
#define cpu_gen_code cpu_z80_gen_code
#define cpu_signal_handler cpu_z80_signal_handler
#define cpu_list z80_cpu_list

/* MMU modes definitions */
#define MMU_MODE0_SUFFIX _kernel
#define MMU_MODE1_SUFFIX _user
#define MMU_USER_IDX 1
static inline int cpu_mmu_index (CPUState *env)
{
    /* return (env->hflags & HF_CPL_MASK) == 3 ? 1 : 0; */
    return 0;
}

#include "cpu-all.h"
#include "exec-all.h"

static inline void cpu_pc_from_tb(CPUState *env, TranslationBlock *tb)
{
    env->pc = tb->pc;
    env->hflags = tb->flags;
}

static inline void cpu_get_tb_cpu_state(CPUState *env, target_ulong *pc,
                                        target_ulong *cs_base, int *flags)
{
    *pc = env->pc;
    *cs_base = 0;
    *flags = env->hflags;
}

#endif /* CPU_Z80_H */
