/*
 * Z80 translation
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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#include "cpu.h"
#include "exec-all.h"
#include "disas.h"
#include "tcg-op.h"

#include "helper.h"
#define GEN_HELPER 1
#include "helper.h"

#define PREFIX_CB  0x01
#define PREFIX_DD  0x02
#define PREFIX_ED  0x04
#define PREFIX_FD  0x08

#define MODE_NORMAL 0
#define MODE_DD     1
#define MODE_FD     2

#define zprintf(...)
//#define zprintf printf

/* global register indexes */
static TCGv cpu_env, cpu_T[3], cpu_A0;

#include "gen-icount.h"

#define MEM_INDEX 0

typedef struct DisasContext {
    /* current insn context */
    int override; /* -1 if no override */
    int prefix;
    uint16_t pc; /* pc = pc + cs_base */
    int is_jmp; /* 1 = means jump (stop translation), 2 means CPU
                   static state change (stop translation) */
    int model;
    /* current block context */
    target_ulong cs_base; /* base of CS segment */
    int singlestep_enabled; /* "hardware" single step enabled */
    int jmp_opt; /* use direct block chaining for direct jumps */
    int flags; /* all execution flags */
    struct TranslationBlock *tb;
} DisasContext;

static void gen_eob(DisasContext *s);
static void gen_jmp(DisasContext *s, target_ulong pc);
static void gen_jmp_tb(DisasContext *s, target_ulong pc, int tb_num);

enum {
    /* 8-bit registers */
    OR_B,
    OR_C,
    OR_D,
    OR_E,
    OR_H,
    OR_L,
    OR_HLmem,
    OR_A,

    OR_IXh,
    OR_IXl,

    OR_IYh,
    OR_IYl,

    OR_IXmem,
    OR_IYmem,
};

static const char *const regnames[] = {
    [OR_B]     = "b",
    [OR_C]     = "c",
    [OR_D]     = "d",
    [OR_E]     = "e",
    [OR_H]     = "h",
    [OR_L]     = "l",
    [OR_HLmem] = "(hl)",
    [OR_A]     = "a",

    [OR_IXh]   = "ixh",
    [OR_IXl]   = "ixl",

    [OR_IYh]   = "iyh",
    [OR_IYl]   = "iyl",

    [OR_IXmem] = "(ix+d)",
    [OR_IYmem] = "(iy+d)",
};

static const char *const idxnames[] = {
    [OR_IXmem] = "ix",
    [OR_IYmem] = "iy",
};

/* signed hex byte value for printf */
#define shexb(val) (val < 0 ? '-' : '+'), (abs(val))

/* Register accessor functions */

#if defined(WORDS_BIGENDIAN)
#define UNIT_OFFSET(type, units, num) (sizeof(type) - ((num + 1) * units))
#else
#define UNIT_OFFSET(type, units, num) (num * units)
#endif

#define BYTE_OFFSET(type, num) UNIT_OFFSET(type, 1, num)
#define WORD_OFFSET(type, num) UNIT_OFFSET(type, 2, num)

#define REGPAIR AF
#define REGHIGH A
#define REGLOW  F
#include "genreg_template_af.h"
#undef REGPAIR
#undef REGHIGH
#undef REGLOW

#define REGPAIR BC
#define REGHIGH B
#define REGLOW  C
#include "genreg_template.h"
#undef REGPAIR
#undef REGHIGH
#undef REGLOW

#define REGPAIR DE
#define REGHIGH D
#define REGLOW  E
#include "genreg_template.h"
#undef REGPAIR
#undef REGHIGH
#undef REGLOW

#define REGPAIR HL
#define REGHIGH H
#define REGLOW  L
#include "genreg_template.h"
#undef REGPAIR
#undef REGHIGH
#undef REGLOW

#define REGPAIR IX
#define REGHIGH IXh
#define REGLOW  IXl
#include "genreg_template.h"
#undef REGPAIR
#undef REGHIGH
#undef REGLOW

#define REGPAIR IY
#define REGHIGH IYh
#define REGLOW  IYl
#include "genreg_template.h"
#undef REGPAIR
#undef REGHIGH
#undef REGLOW

#define REGPAIR AFX
#define REGHIGH AX
#define REGLOW  FX
#include "genreg_template_af.h"
#undef REGPAIR
#undef REGHIGH
#undef REGLOW

#define REGPAIR BCX
#define REGHIGH BX
#define REGLOW  CX
#include "genreg_template.h"
#undef REGPAIR
#undef REGHIGH
#undef REGLOW

#define REGPAIR DEX
#define REGHIGH DX
#define REGLOW  EX
#include "genreg_template.h"
#undef REGPAIR
#undef REGHIGH
#undef REGLOW

#define REGPAIR HLX
#define REGHIGH HX
#define REGLOW  LX
#include "genreg_template.h"
#undef REGPAIR
#undef REGHIGH
#undef REGLOW

#define REGPAIR SP
#include "genreg_template.h"
#undef REGPAIR

typedef void (gen_mov_func)(TCGv v);
typedef void (gen_mov_func_idx)(TCGv v, uint16_t ofs);

static inline void gen_movb_v_HLmem(TCGv v)
{
    TCGv addr = tcg_temp_new();
    gen_movw_v_HL(addr);
    tcg_gen_qemu_ld8u(v, addr, MEM_INDEX);
    tcg_temp_free(addr);
}

static inline void gen_movb_HLmem_v(TCGv v)
{
    TCGv addr = tcg_temp_new();
    gen_movw_v_HL(addr);
    tcg_gen_qemu_st8(v, addr, MEM_INDEX);
    tcg_temp_free(addr);
}

static inline void gen_movb_v_IXmem(TCGv v, uint16_t ofs)
{
    TCGv addr = tcg_temp_new();
    gen_movw_v_IX(addr);
    tcg_gen_addi_tl(addr, addr, ofs);
    tcg_gen_ext16u_tl(addr, addr);
    tcg_gen_qemu_ld8u(v, addr, MEM_INDEX);
    tcg_temp_free(addr);
}

static inline void gen_movb_v_IYmem(TCGv v, uint16_t ofs)
{
    TCGv addr = tcg_temp_new();
    gen_movw_v_IY(addr);
    tcg_gen_addi_tl(addr, addr, ofs);
    tcg_gen_ext16u_tl(addr, addr);
    tcg_gen_qemu_ld8u(v, addr, MEM_INDEX);
    tcg_temp_free(addr);
}

static inline void gen_movb_IXmem_v(TCGv v, uint16_t ofs)
{
    TCGv addr = tcg_temp_new();
    gen_movw_v_IX(addr);
    tcg_gen_addi_tl(addr, addr, ofs);
    tcg_gen_ext16u_tl(addr, addr);
    tcg_gen_qemu_st8(v, addr, MEM_INDEX);
    tcg_temp_free(addr);
}

static inline void gen_movb_IYmem_v(TCGv v, uint16_t ofs)
{
    TCGv addr = tcg_temp_new();
    gen_movw_v_IY(addr);
    tcg_gen_addi_tl(addr, addr, ofs);
    tcg_gen_ext16u_tl(addr, addr);
    tcg_gen_qemu_st8(v, addr, MEM_INDEX);
    tcg_temp_free(addr);
}

static inline void gen_pushw(TCGv v)
{
    TCGv addr = tcg_temp_new();
    gen_movw_v_SP(addr);
    tcg_gen_subi_i32(addr, addr, 2);
    tcg_gen_ext16u_i32(addr, addr);
    gen_movw_SP_v(addr);
    tcg_gen_qemu_st16(v, addr, MEM_INDEX);
    tcg_temp_free(addr);
}

static inline void gen_popw(TCGv v)
{
    TCGv addr = tcg_temp_new();
    gen_movw_v_SP(addr);
    tcg_gen_qemu_ld16u(v, addr, MEM_INDEX);
    tcg_gen_addi_i32(addr, addr, 2);
    tcg_gen_ext16u_i32(addr, addr);
    gen_movw_SP_v(addr);
    tcg_temp_free(addr);
}

static gen_mov_func *const gen_movb_v_reg_tbl[] = {
    [OR_B]     = gen_movb_v_B,
    [OR_C]     = gen_movb_v_C,
    [OR_D]     = gen_movb_v_D,
    [OR_E]     = gen_movb_v_E,
    [OR_H]     = gen_movb_v_H,
    [OR_L]     = gen_movb_v_L,
    [OR_HLmem] = gen_movb_v_HLmem,
    [OR_A]     = gen_movb_v_A,

    [OR_IXh]   = gen_movb_v_IXh,
    [OR_IXl]   = gen_movb_v_IXl,

    [OR_IYh]   = gen_movb_v_IYh,
    [OR_IYl]   = gen_movb_v_IYl,
};

static inline void gen_movb_v_reg(TCGv v, int reg)
{
    gen_movb_v_reg_tbl[reg](v);
}

static gen_mov_func_idx *const gen_movb_v_idx_tbl[] = {
    [OR_IXmem] = gen_movb_v_IXmem,
    [OR_IYmem] = gen_movb_v_IYmem,
};

static inline void gen_movb_v_idx(TCGv v, int idx, int ofs)
{
    gen_movb_v_idx_tbl[idx](v, ofs);
}

static gen_mov_func *const gen_movb_reg_v_tbl[] = {
    [OR_B]     = gen_movb_B_v,
    [OR_C]     = gen_movb_C_v,
    [OR_D]     = gen_movb_D_v,
    [OR_E]     = gen_movb_E_v,
    [OR_H]     = gen_movb_H_v,
    [OR_L]     = gen_movb_L_v,
    [OR_HLmem] = gen_movb_HLmem_v,
    [OR_A]     = gen_movb_A_v,

    [OR_IXh]   = gen_movb_IXh_v,
    [OR_IXl]   = gen_movb_IXl_v,

    [OR_IYh]   = gen_movb_IYh_v,
    [OR_IYl]   = gen_movb_IYl_v,
};

static inline void gen_movb_reg_v(int reg, TCGv v)
{
    gen_movb_reg_v_tbl[reg](v);
}

static gen_mov_func_idx *const gen_movb_idx_v_tbl[] = {
    [OR_IXmem] = gen_movb_IXmem_v,
    [OR_IYmem] = gen_movb_IYmem_v,
};

static inline void gen_movb_idx_v(int idx, TCGv v, int ofs)
{
    gen_movb_idx_v_tbl[idx](v, ofs);
}

static inline int regmap(int reg, int m)
{
    switch (m) {
    case MODE_DD:
        switch (reg) {
        case OR_H:
            return OR_IXh;
        case OR_L:
            return OR_IXl;
        case OR_HLmem:
            return OR_IXmem;
        default:
            return reg;
        }
    case MODE_FD:
        switch (reg) {
        case OR_H:
            return OR_IYh;
        case OR_L:
            return OR_IYl;
        case OR_HLmem:
            return OR_IYmem;
        default:
            return reg;
        }
    case MODE_NORMAL:
    default:
        return reg;
    }
}

static inline int is_indexed(int reg)
{
    if (reg == OR_IXmem || reg == OR_IYmem) {
        return 1;
    } else {
        return 0;
    }
}

static const int reg[8] = {
    OR_B,
    OR_C,
    OR_D,
    OR_E,
    OR_H,
    OR_L,
    OR_HLmem,
    OR_A,
};

enum {
    /* 16-bit registers and register pairs */
    OR2_AF,
    OR2_BC,
    OR2_DE,
    OR2_HL,

    OR2_IX,
    OR2_IY,
    OR2_SP,

    OR2_AFX,
    OR2_BCX,
    OR2_DEX,
    OR2_HLX,
};

static const char *const regpairnames[] = {
    [OR2_AF]  = "af",
    [OR2_BC]  = "bc",
    [OR2_DE]  = "de",
    [OR2_HL]  = "hl",

    [OR2_IX]  = "ix",
    [OR2_IY]  = "iy",
    [OR2_SP]  = "sp",

    [OR2_AFX] = "afx",
    [OR2_BCX] = "bcx",
    [OR2_DEX] = "dex",
    [OR2_HLX] = "hlx",
};

static gen_mov_func *const gen_movw_v_reg_tbl[] = {
    [OR2_AF]  = gen_movw_v_AF,
    [OR2_BC]  = gen_movw_v_BC,
    [OR2_DE]  = gen_movw_v_DE,
    [OR2_HL]  = gen_movw_v_HL,

    [OR2_IX]  = gen_movw_v_IX,
    [OR2_IY]  = gen_movw_v_IY,
    [OR2_SP]  = gen_movw_v_SP,

    [OR2_AFX] = gen_movw_v_AFX,
    [OR2_BCX] = gen_movw_v_BCX,
    [OR2_DEX] = gen_movw_v_DEX,
    [OR2_HLX] = gen_movw_v_HLX,
};

static inline void gen_movw_v_reg(TCGv v, int regpair)
{
    gen_movw_v_reg_tbl[regpair](v);
}

static gen_mov_func *const gen_movw_reg_v_tbl[] = {
    [OR2_AF]  = gen_movw_AF_v,
    [OR2_BC]  = gen_movw_BC_v,
    [OR2_DE]  = gen_movw_DE_v,
    [OR2_HL]  = gen_movw_HL_v,

    [OR2_IX]  = gen_movw_IX_v,
    [OR2_IY]  = gen_movw_IY_v,
    [OR2_SP]  = gen_movw_SP_v,

    [OR2_AFX] = gen_movw_AFX_v,
    [OR2_BCX] = gen_movw_BCX_v,
    [OR2_DEX] = gen_movw_DEX_v,
    [OR2_HLX] = gen_movw_HLX_v,
};

static inline void gen_movw_reg_v(int regpair, TCGv v)
{
    gen_movw_reg_v_tbl[regpair](v);
}

static inline int regpairmap(int regpair, int m)
{
    switch (regpair) {
    case OR2_HL:
        switch (m) {
        case MODE_DD:
            return OR2_IX;
        case MODE_FD:
            return OR2_IY;
        case MODE_NORMAL:
        default:
            return OR2_HL;
        }
    default:
        return regpair;
    }
}

static const int regpair[4] = {
    OR2_BC,
    OR2_DE,
    OR2_HL,
    OR2_SP,
};

static const int regpair2[4] = {
    OR2_BC,
    OR2_DE,
    OR2_HL,
    OR2_AF,
};

static inline void gen_jmp_im(target_ulong pc)
{
    gen_helper_movl_pc_im(tcg_const_tl(pc));
}

static void gen_debug(DisasContext *s, target_ulong cur_pc)
{
    gen_jmp_im(cur_pc);
    gen_helper_debug();
    s->is_jmp = 3;
}

static void gen_eob(DisasContext *s)
{
    if (s->tb->flags & HF_INHIBIT_IRQ_MASK) {
        gen_helper_reset_inhibit_irq();
    }
    if (s->singlestep_enabled) {
        gen_helper_debug();
    } else {
        tcg_gen_exit_tb(0);
    }
    s->is_jmp = 3;
}

static void gen_exception(DisasContext *s, int trapno, target_ulong cur_pc)
{
    gen_jmp_im(cur_pc);
    gen_helper_raise_exception(trapno);
    s->is_jmp = 3;
}

/* Conditions */

static const char *const cc[8] = {
    "nz",
    "z",
    "nc",
    "c",
    "po",
    "pe",
    "p",
    "m",
};

enum {
    COND_NZ = 0,
    COND_Z,
    COND_NC,
    COND_C,
    COND_PO,
    COND_PE,
    COND_P,
    COND_M,
};

static const int cc_flags[4] = {
    CC_Z,
    CC_C,
    CC_P,
    CC_S,
};

/* Arithmetic/logic operations */

static const char *const alu[8] = {
    "add a,",
    "adc a,",
    "sub ",
    "sbc a,",
    "and ",
    "xor ",
    "or ",
    "cp ",
};

typedef void (alu_helper_func)(void);

static alu_helper_func *const gen_alu[8] = {
    gen_helper_add_cc,
    gen_helper_adc_cc,
    gen_helper_sub_cc,
    gen_helper_sbc_cc,
    gen_helper_and_cc,
    gen_helper_xor_cc,
    gen_helper_or_cc,
    gen_helper_cp_cc,
};

/* Rotation/shift operations */

static const char *const rot[8] = {
    "rlc",
    "rrc",
    "rl",
    "rr",
    "sla",
    "sra",
    "sll",
    "srl",
};

typedef void (rot_helper_func)(void);

static rot_helper_func *const gen_rot_T0[8] = {
    gen_helper_rlc_T0_cc,
    gen_helper_rrc_T0_cc,
    gen_helper_rl_T0_cc,
    gen_helper_rr_T0_cc,
    gen_helper_sla_T0_cc,
    gen_helper_sra_T0_cc,
    gen_helper_sll_T0_cc,
    gen_helper_srl_T0_cc,
};

/* Block instructions */

static const char *const bli[4][4] = {
    { "ldi",  "cpi",  "ini",  "outi", },
    { "ldd",  "cpd",  "ind",  "outd", },
    { "ldir", "cpir", "inir", "otir", },
    { "lddr", "cpdr", "indr", "otdr", },
};

static const int imode[8] = {
    0, 0, 1, 2, 0, 0, 1, 2,
};

static inline void gen_goto_tb(DisasContext *s, int tb_num, target_ulong pc)
{
    gen_jmp_im(pc);
    gen_eob(s);
}

static inline void gen_cond_jump(int cc, int l1)
{
    gen_movb_v_F(cpu_T[0]);

    tcg_gen_andi_tl(cpu_T[0], cpu_T[0], cc_flags[cc >> 1]);

    tcg_gen_brcondi_tl((cc & 1) ? TCG_COND_NE : TCG_COND_EQ, cpu_T[0], 0, l1);
}

static inline void gen_jcc(DisasContext *s, int cc,
                           target_ulong val, target_ulong next_pc)
{
    TranslationBlock *tb;
    int l1;

    tb = s->tb;

    l1 = gen_new_label();

    gen_cond_jump(cc, l1);

    gen_goto_tb(s, 0, next_pc);

    gen_set_label(l1);
    gen_goto_tb(s, 1, val);

    s->is_jmp = 3;
}

static inline void gen_callcc(DisasContext *s, int cc,
                              target_ulong val, target_ulong next_pc)
{
    TranslationBlock *tb;
    int l1;

    tb = s->tb;

    l1 = gen_new_label();

    gen_cond_jump(cc, l1);

    gen_goto_tb(s, 0, next_pc);

    gen_set_label(l1);
    tcg_gen_movi_tl(cpu_T[0], next_pc);
    gen_pushw(cpu_T[0]);
    gen_goto_tb(s, 1, val);

    s->is_jmp = 3;
}

static inline void gen_retcc(DisasContext *s, int cc,
                             target_ulong next_pc)
{
    TranslationBlock *tb;
    int l1;

    tb = s->tb;

    l1 = gen_new_label();

    gen_cond_jump(cc, l1);

    gen_goto_tb(s, 0, next_pc);

    gen_set_label(l1);
    gen_popw(cpu_T[0]);
    gen_helper_jmp_T0();
    gen_eob(s);

    s->is_jmp = 3;
}

static inline void gen_ex(int regpair1, int regpair2)
{
    TCGv tmp1 = tcg_temp_new();
    TCGv tmp2 = tcg_temp_new();
    gen_movw_v_reg(tmp1, regpair1);
    gen_movw_v_reg(tmp2, regpair2);
    gen_movw_reg_v(regpair2, tmp1);
    gen_movw_reg_v(regpair1, tmp2);
    tcg_temp_free(tmp1);
    tcg_temp_free(tmp2);
}

/* TODO: condition code optimisation */

/* micro-ops that modify condition codes should end in _cc */

/* convert one instruction. s->is_jmp is set if the translation must
   be stopped. Return the next pc value */
static target_ulong disas_insn(DisasContext *s, target_ulong pc_start)
{
    int b, prefixes;
    int rex_w, rex_r;
    int m;

    s->pc = pc_start;
    prefixes = 0;
    s->override = -1;
    rex_w = -1;
    rex_r = 0;

    //printf("PC = %04x: ", s->pc);
next_byte:
    s->prefix = prefixes;

/* START */

    if (prefixes & PREFIX_DD) {
        m = MODE_DD;
    } else if (prefixes & PREFIX_FD) {
        m = MODE_FD;
    } else {
        m = MODE_NORMAL;
    }

    /* unprefixed opcodes */

    if ((prefixes & (PREFIX_CB | PREFIX_ED)) == 0) {
        b = ldub_code(s->pc);
        s->pc++;

        int x, y, z, p, q;
        int n, d;
        int r1, r2;

        x = (b >> 6) & 0x03;
        y = (b >> 3) & 0x07;
        z = b & 0x07;
        p = y >> 1;
        q = y & 0x01;

        switch (x) {
        case 0:
            switch (z) {

            case 0:
                switch (y) {
                case 0:
                    zprintf("nop\n");
                    break;
                case 1:
                    gen_ex(OR2_AF, OR2_AFX);
                    zprintf("ex af,af'\n");
                    break;
                case 2:
                    n = ldsb_code(s->pc);
                    s->pc++;
                    gen_helper_djnz(tcg_const_tl(s->pc + n), tcg_const_tl(s->pc));
                    gen_eob(s);
                    s->is_jmp = 3;
                    zprintf("djnz $%02x\n", n);
                    break;
                case 3:
                    n = ldsb_code(s->pc);
                    s->pc++;
                    gen_jmp_im(s->pc + n);
                    gen_eob(s);
                    s->is_jmp = 3;
                    zprintf("jr $%02x\n", n);
                    break;
                case 4:
                case 5:
                case 6:
                case 7:
                    n = ldsb_code(s->pc);
                    s->pc++;
                    zprintf("jr %s,$%04x\n", cc[y-4], (s->pc + n) & 0xffff);
                    gen_jcc(s, y-4, s->pc + n, s->pc);
                    break;
                }
                break;

            case 1:
                switch (q) {
                case 0:
                    n = lduw_code(s->pc);
                    s->pc += 2;
                    tcg_gen_movi_tl(cpu_T[0], n);
                    r1 = regpairmap(regpair[p], m);
                    gen_movw_reg_v(r1, cpu_T[0]);
                    zprintf("ld %s,$%04x\n", regpairnames[r1], n);
                    break;
                case 1:
                    r1 = regpairmap(regpair[p], m);
                    r2 = regpairmap(OR2_HL, m);
                    gen_movw_v_reg(cpu_T[0], r1);
                    gen_movw_v_reg(cpu_T[1], r2);
                    gen_helper_addw_T0_T1_cc();
                    gen_movw_reg_v(r2, cpu_T[0]);
                    zprintf("add %s,%s\n", regpairnames[r2], regpairnames[r1]);
                    break;
                }
                break;

            case 2:
                switch (q) {
                case 0:
                    switch (p) {
                    case 0:
                        gen_movb_v_A(cpu_T[0]);
                        gen_movw_v_BC(cpu_A0);
                        tcg_gen_qemu_st8(cpu_T[0], cpu_A0, MEM_INDEX);
                        zprintf("ld (bc),a\n");
                        break;
                    case 1:
                        gen_movb_v_A(cpu_T[0]);
                        gen_movw_v_DE(cpu_A0);
                        tcg_gen_qemu_st8(cpu_T[0], cpu_A0, MEM_INDEX);
                        zprintf("ld (de),a\n");
                        break;
                    case 2:
                        n = lduw_code(s->pc);
                        s->pc += 2;
                        r1 = regpairmap(OR2_HL, m);
                        gen_movw_v_reg(cpu_T[0], r1);
                        tcg_gen_movi_i32(cpu_A0, n);
                        tcg_gen_qemu_st16(cpu_T[0], cpu_A0, MEM_INDEX);
                        zprintf("ld ($%04x),%s\n", n, regpairnames[r1]);
                        break;
                    case 3:
                        n = lduw_code(s->pc);
                        s->pc += 2;
                        gen_movb_v_A(cpu_T[0]);
                        tcg_gen_movi_i32(cpu_A0, n);
                        tcg_gen_qemu_st8(cpu_T[0], cpu_A0, MEM_INDEX);
                        zprintf("ld ($%04x),a\n", n);
                        break;
                    }
                    break;
                case 1:
                    switch (p) {
                    case 0:
                        gen_movw_v_BC(cpu_A0);
                        tcg_gen_qemu_ld8u(cpu_T[0], cpu_A0, MEM_INDEX);
                        gen_movb_A_v(cpu_T[0]);
                        zprintf("ld a,(bc)\n");
                        break;
                    case 1:
                        gen_movw_v_DE(cpu_A0);
                        tcg_gen_qemu_ld8u(cpu_T[0], cpu_A0, MEM_INDEX);
                        gen_movb_A_v(cpu_T[0]);
                        zprintf("ld a,(de)\n");
                        break;
                    case 2:
                        n = lduw_code(s->pc);
                        s->pc += 2;
                        r1 = regpairmap(OR2_HL, m);
                        tcg_gen_movi_i32(cpu_A0, n);
                        tcg_gen_qemu_ld16u(cpu_T[0], cpu_A0, MEM_INDEX);
                        gen_movw_reg_v(r1, cpu_T[0]);
                        zprintf("ld %s,($%04x)\n", regpairnames[r1], n);
                        break;
                    case 3:
                        n = lduw_code(s->pc);
                        s->pc += 2;
                        tcg_gen_movi_i32(cpu_A0, n);
                        tcg_gen_qemu_ld8u(cpu_T[0], cpu_A0, MEM_INDEX);
                        gen_movb_A_v(cpu_T[0]);
                        zprintf("ld a,($%04x)\n", n);
                        break;
                    }
                    break;
                }
                break;

            case 3:
                switch (q) {
                case 0:
                    r1 = regpairmap(regpair[p], m);
                    gen_movw_v_reg(cpu_T[0], r1);
                    tcg_gen_addi_tl(cpu_T[0], cpu_T[0], 1);
                    gen_movw_reg_v(r1, cpu_T[0]);
                    zprintf("inc %s\n", regpairnames[r1]);
                    break;
                case 1:
                    r1 = regpairmap(regpair[p], m);
                    gen_movw_v_reg(cpu_T[0], r1);
                    tcg_gen_subi_tl(cpu_T[0], cpu_T[0], 1);
                    gen_movw_reg_v(r1, cpu_T[0]);
                    zprintf("dec %s\n", regpairnames[r1]);
                    break;
                }
                break;

            case 4:
                r1 = regmap(reg[y], m);
                if (is_indexed(r1)) {
                    d = ldsb_code(s->pc);
                    s->pc++;
                    gen_movb_v_idx(cpu_T[0], r1, d);
                } else {
                    gen_movb_v_reg(cpu_T[0], r1);
                }
                gen_helper_incb_T0_cc();
                if (is_indexed(r1)) {
                    gen_movb_idx_v(r1, cpu_T[0], d);
                } else {
                    gen_movb_reg_v(r1, cpu_T[0]);
                }
                if (is_indexed(r1)) {
                    zprintf("inc (%s%c$%02x)\n", idxnames[r1], shexb(d));
                } else {
                    zprintf("inc %s\n", regnames[r1]);
                }
                break;

            case 5:
                r1 = regmap(reg[y], m);
                if (is_indexed(r1)) {
                    d = ldsb_code(s->pc);
                    s->pc++;
                    gen_movb_v_idx(cpu_T[0], r1, d);
                } else {
                    gen_movb_v_reg(cpu_T[0], r1);
                }
                gen_helper_decb_T0_cc();
                if (is_indexed(r1)) {
                    gen_movb_idx_v(r1, cpu_T[0], d);
                } else {
                    gen_movb_reg_v(r1, cpu_T[0]);
                }
                if (is_indexed(r1)) {
                    zprintf("dec (%s%c$%02x)\n", idxnames[r1], shexb(d));
                } else {
                    zprintf("dec %s\n", regnames[r1]);
                }
                break;

            case 6:
                r1 = regmap(reg[y], m);
                if (is_indexed(r1)) {
                    d = ldsb_code(s->pc);
                    s->pc++;
                }
                n = ldub_code(s->pc);
                s->pc++;
                tcg_gen_movi_tl(cpu_T[0], n);
                if (is_indexed(r1)) {
                    gen_movb_idx_v(r1, cpu_T[0], d);
                } else {
                    gen_movb_reg_v(r1, cpu_T[0]);
                }
                if (is_indexed(r1)) {
                    zprintf("ld (%s%c$%02x),$%02x\n", idxnames[r1], shexb(d), n);
                } else {
                    zprintf("ld %s,$%02x\n", regnames[r1], n);
                }
                break;

            case 7:
                switch (y) {
                case 0:
                    gen_helper_rlca_cc();
                    zprintf("rlca\n");
                    break;
                case 1:
                    gen_helper_rrca_cc();
                    zprintf("rrca\n");
                    break;
                case 2:
                    gen_helper_rla_cc();
                    zprintf("rla\n");
                    break;
                case 3:
                    gen_helper_rra_cc();
                    zprintf("rra\n");
                    break;
                case 4:
                    gen_helper_daa_cc();
                    zprintf("daa\n");
                    break;
                case 5:
                    gen_helper_cpl_cc();
                    zprintf("cpl\n");
                    break;
                case 6:
                    gen_helper_scf_cc();
                    zprintf("scf\n");
                    break;
                case 7:
                    gen_helper_ccf_cc();
                    zprintf("ccf\n");
                    break;
                }
                break;
            }
            break;

        case 1:
            if (z == 6 && y == 6) {
                gen_jmp_im(s->pc);
                gen_helper_halt();
                zprintf("halt\n");
            } else {
                if (z == 6) {
                    r1 = regmap(reg[z], m);
                    r2 = regmap(reg[y], 0);
                } else if (y == 6) {
                    r1 = regmap(reg[z], 0);
                    r2 = regmap(reg[y], m);
                } else {
                    r1 = regmap(reg[z], m);
                    r2 = regmap(reg[y], m);
                }
                if (is_indexed(r1) || is_indexed(r2)) {
                    d = ldsb_code(s->pc);
                    s->pc++;
                }
                if (is_indexed(r1)) {
                    gen_movb_v_idx(cpu_T[0], r1, d);
                } else {
                    gen_movb_v_reg(cpu_T[0], r1);
                }
                if (is_indexed(r2)) {
                    gen_movb_idx_v(r2, cpu_T[0], d);
                } else {
                    gen_movb_reg_v(r2, cpu_T[0]);
                }
                if (is_indexed(r1)) {
                    zprintf("ld %s,(%s%c$%02x)\n", regnames[r2], idxnames[r1], shexb(d));
                } else if (is_indexed(r2)) {
                    zprintf("ld (%s%c$%02x),%s\n", idxnames[r2], shexb(d), regnames[r1]);
                } else {
                    zprintf("ld %s,%s\n", regnames[r2], regnames[r1]);
                }
            }
            break;

        case 2:
            r1 = regmap(reg[z], m);
            if (is_indexed(r1)) {
                d = ldsb_code(s->pc);
                s->pc++;
                gen_movb_v_idx(cpu_T[0], r1, d);
            } else {
                gen_movb_v_reg(cpu_T[0], r1);
            }
            gen_alu[y](); /* places output in A */
            if (is_indexed(r1)) {
                zprintf("%s(%s%c$%02x)\n", alu[y], idxnames[r1], shexb(d));
            } else {
                zprintf("%s%s\n", alu[y], regnames[r1]);
            }
            break;

        case 3:
            switch (z) {
            case 0:
                gen_retcc(s, y, s->pc);
                zprintf("ret %s\n", cc[y]);
                break;

            case 1:
                switch (q) {
                case 0:
                    r1 = regpairmap(regpair2[p], m);
                    gen_popw(cpu_T[0]);
                    gen_movw_reg_v(r1, cpu_T[0]);
                    zprintf("pop %s\n", regpairnames[r1]);
                    break;
                case 1:
                    switch (p) {
                    case 0:
                        gen_popw(cpu_T[0]);
                        gen_helper_jmp_T0();
                        zprintf("ret\n");
                        gen_eob(s);
                        s->is_jmp = 3;
//                      s->is_ei = 1;
                        break;
                    case 1:
                        gen_ex(OR2_BC, OR2_BCX);
                        gen_ex(OR2_DE, OR2_DEX);
                        gen_ex(OR2_HL, OR2_HLX);
                        zprintf("exx\n");
                        break;
                    case 2:
                        r1 = regpairmap(OR2_HL, m);
                        gen_movw_v_reg(cpu_T[0], r1);
                        gen_helper_jmp_T0();
                        zprintf("jp %s\n", regpairnames[r1]);
                        gen_eob(s);
                        s->is_jmp = 3;
                        break;
                    case 3:
                        r1 = regpairmap(OR2_HL, m);
                        gen_movw_v_reg(cpu_T[0], r1);
                        gen_movw_SP_v(cpu_T[0]);
                        zprintf("ld sp,%s\n", regpairnames[r1]);
                        break;
                    }
                    break;
                }
                break;

            case 2:
                n = lduw_code(s->pc);
                s->pc += 2;
                gen_jcc(s, y, n, s->pc);
                zprintf("jp %s,$%04x\n", cc[y], n);
                break;

            case 3:
                switch (y) {
                case 0:
                    n = lduw_code(s->pc);
                    s->pc += 2;
                    gen_jmp_im(n);
                    zprintf("jp $%04x\n", n);
                    gen_eob(s);
                    s->is_jmp = 3;
                    break;
                case 1:
                    zprintf("cb prefix\n");
                    prefixes |= PREFIX_CB;
                    goto next_byte;
                    break;
                case 2:
                    n = ldub_code(s->pc);
                    s->pc++;
                    gen_movb_v_A(cpu_T[0]);
                    if (use_icount) {
                        gen_io_start();
                    }
                    gen_helper_out_T0_im(tcg_const_tl(n));
                    if (use_icount) {
                        gen_io_end();
                        gen_jmp_im(s->pc);
                    }
                    zprintf("out ($%02x),a\n", n);
                    break;
                case 3:
                    n = ldub_code(s->pc);
                    s->pc++;
                    if (use_icount) {
                        gen_io_start();
                    }
                    gen_helper_in_T0_im(tcg_const_tl(n));
                    gen_movb_A_v(cpu_T[0]);
                    if (use_icount) {
                        gen_io_end();
                        gen_jmp_im(s->pc);
                    }
                    zprintf("in a,($%02x)\n", n);
                    break;
                case 4:
                    r1 = regpairmap(OR2_HL, m);
                    gen_popw(cpu_T[1]);
                    gen_movw_v_reg(cpu_T[0], r1);
                    gen_pushw(cpu_T[0]);
                    gen_movw_reg_v(r1, cpu_T[1]);
                    zprintf("ex (sp),%s\n", regpairnames[r1]);
                    break;
                case 5:
                    gen_ex(OR2_DE, OR2_HL);
                    zprintf("ex de,hl\n");
                    break;
                case 6:
                    gen_helper_di();
                    zprintf("di\n");
                    break;
                case 7:
                    gen_helper_ei();
                    zprintf("ei\n");
//                  gen_eob(s);
//                  s->is_ei = 1;
                    break;
                }
                break;

            case 4:
                n = lduw_code(s->pc);
                s->pc += 2;
                gen_callcc(s, y, n, s->pc);
                zprintf("call %s,$%04x\n", cc[y], n);
                break;

            case 5:
                switch (q) {
                case 0:
                    r1 = regpairmap(regpair2[p], m);
                    gen_movw_v_reg(cpu_T[0], r1);
                    gen_pushw(cpu_T[0]);
                    zprintf("push %s\n", regpairnames[r1]);
                    break;
                case 1:
                    switch (p) {
                    case 0:
                        n = lduw_code(s->pc);
                        s->pc += 2;
                        tcg_gen_movi_tl(cpu_T[0], s->pc);
                        gen_pushw(cpu_T[0]);
                        gen_jmp_im(n);
                        zprintf("call $%04x\n", n);
                        gen_eob(s);
                        s->is_jmp = 3;
                        break;
                    case 1:
                        zprintf("dd prefix\n");
                        prefixes |= PREFIX_DD;
                        goto next_byte;
                        break;
                    case 2:
                        zprintf("ed prefix\n");
                        prefixes |= PREFIX_ED;
                        goto next_byte;
                        break;
                    case 3:
                        zprintf("fd prefix\n");
                        prefixes |= PREFIX_FD;
                        goto next_byte;
                        break;
                    }
                    break;
                }
                break;

            case 6:
                n = ldub_code(s->pc);
                s->pc++;
                tcg_gen_movi_tl(cpu_T[0], n);
                gen_alu[y](); /* places output in A */
                zprintf("%s$%02x\n", alu[y], n);
                break;

            case 7:
                tcg_gen_movi_tl(cpu_T[0], s->pc);
                gen_pushw(cpu_T[0]);
                gen_jmp_im(y*8);
                zprintf("rst $%02x\n", y*8);
                gen_eob(s);
                s->is_jmp = 3;
                break;
            }
            break;
        }
    } else if (prefixes & PREFIX_CB) {
        /* cb mode: */

        int x, y, z, p, q;
        int d;
        int r1, r2;

        if (m != MODE_NORMAL) {
            d = ldsb_code(s->pc);
            s->pc++;
        }

        b = ldub_code(s->pc);
        s->pc++;

        x = (b >> 6) & 0x03;
        y = (b >> 3) & 0x07;
        z = b & 0x07;
        p = y >> 1;
        q = y & 0x01;

        if (m != MODE_NORMAL) {
            r1 = regmap(OR_HLmem, m);
            gen_movb_v_idx(cpu_T[0], r1, d);
            if (z != 6) {
                r2 = regmap(reg[z], 0);
            }
        } else {
            r1 = regmap(reg[z], m);
            gen_movb_v_reg(cpu_T[0], r1);
        }

        switch (x) {
        case 0:
            /* TODO: TST instead of SLL for R800 */
            gen_rot_T0[y]();
            if (m != MODE_NORMAL) {
                gen_movb_idx_v(r1, cpu_T[0], d);
                if (z != 6) {
                    gen_movb_reg_v(r2, cpu_T[0]);
                }
            } else {
                gen_movb_reg_v(r1, cpu_T[0]);
            }
            zprintf("%s %s\n", rot[y], regnames[r1]);
            break;
        case 1:
            gen_helper_bit_T0(tcg_const_tl(1 << y));
            zprintf("bit %i,%s\n", y, regnames[r1]);
            break;
        case 2:
            tcg_gen_andi_tl(cpu_T[0], cpu_T[0], ~(1 << y));
            if (m != MODE_NORMAL) {
                gen_movb_idx_v(r1, cpu_T[0], d);
                if (z != 6) {
                    gen_movb_reg_v(r2, cpu_T[0]);
                }
            } else {
                gen_movb_reg_v(r1, cpu_T[0]);
            }
            zprintf("res %i,%s\n", y, regnames[r1]);
            break;
        case 3:
            tcg_gen_ori_tl(cpu_T[0], cpu_T[0], 1 << y);
            if (m != MODE_NORMAL) {
                gen_movb_idx_v(r1, cpu_T[0], d);
                if (z != 6) {
                    gen_movb_reg_v(r2, cpu_T[0]);
                }
            } else {
                gen_movb_reg_v(r1, cpu_T[0]);
            }
            zprintf("set %i,%s\n", y, regnames[r1]);
            break;
        }

    } else if (prefixes & PREFIX_ED) {
        /* ed mode: */

        b = ldub_code(s->pc);
        s->pc++;

        int x, y, z, p, q;
        int n;
        int r1, r2;

        x = (b >> 6) & 0x03;
        y = (b >> 3) & 0x07;
        z = b & 0x07;
        p = y >> 1;
        q = y & 0x01;

        switch (x) {
        case 0:
            zprintf("nop\n");
            break;
        case 3:
            if (s->model == Z80_CPU_R800) {
                switch (z) {
                case 1:
                    /* does mulub work with r1 == h, l, (hl) or a? */
                    r1 = regmap(reg[y], m);
                    gen_movb_v_reg(cpu_T[0], r1);
                    gen_helper_mulub_cc();
                    zprintf("mulub a,%s\n", regnames[r1]);
                    break;
                case 3:
                    if (q == 0) {
                        /* does muluw work with r1 == de or hl? */
                        /* what is the effect of DD/FD prefixes here? */
                        r1 = regpairmap(regpair[p], m);
                        gen_movw_v_reg(cpu_T[0], r1);
                        gen_helper_muluw_cc();
                        zprintf("muluw hl,%s\n", regpairnames[r1]);
                    } else {
                        zprintf("nop\n");
                    }
                    break;
                default:
                    zprintf("nop\n");
                    break;
                }
            } else {
                zprintf("nop\n");
            }
            break;

        case 1:
            switch (z) {
            case 0:
                if (use_icount) {
                    gen_io_start();
                }
                gen_helper_in_T0_bc_cc();
                if (y != 6) {
                    r1 = regmap(reg[y], m);
                    gen_movb_reg_v(r1, cpu_T[0]);
                    zprintf("in %s,(c)\n", regnames[r1]);
                } else {
                    zprintf("in (c)\n");
                }
                if (use_icount) {
                    gen_io_end();
                    gen_jmp_im(s->pc);
                }
                break;
            case 1:
                if (y != 6) {
                    r1 = regmap(reg[y], m);
                    gen_movb_v_reg(cpu_T[0], r1);
                    zprintf("out (c),%s\n", regnames[r1]);
                } else {
                    tcg_gen_movi_tl(cpu_T[0], 0);
                    zprintf("out (c),0\n");
                }
                if (use_icount) {
                    gen_io_start();
                }
                gen_helper_out_T0_bc();
                if (use_icount) {
                    gen_io_end();
                    gen_jmp_im(s->pc);
                }
                break;
            case 2:
                r1 = regpairmap(OR2_HL, m);
                r2 = regpairmap(regpair[p], m);
                gen_movw_v_reg(cpu_T[0], r1);
                gen_movw_v_reg(cpu_T[1], r2);
                if (q == 0) {
                    zprintf("sbc %s,%s\n", regpairnames[r1], regpairnames[r2]);
                    gen_helper_sbcw_T0_T1_cc();
                } else {
                    zprintf("adc %s,%s\n", regpairnames[r1], regpairnames[r2]);
                    gen_helper_adcw_T0_T1_cc();
                }
                gen_movw_reg_v(r1, cpu_T[0]);
                break;
            case 3:
                n = lduw_code(s->pc);
                s->pc += 2;
                r1 = regpairmap(regpair[p], m);
                if (q == 0) {
                    gen_movw_v_reg(cpu_T[0], r1);
                    tcg_gen_movi_i32(cpu_A0, n);
                    tcg_gen_qemu_st16(cpu_T[0], cpu_A0, MEM_INDEX);
                    zprintf("ld ($%02x),%s\n", n, regpairnames[r1]);
                } else {
                    tcg_gen_movi_i32(cpu_A0, n);
                    tcg_gen_qemu_ld16u(cpu_T[0], cpu_A0, MEM_INDEX);
                    gen_movw_reg_v(r1, cpu_T[0]);
                    zprintf("ld %s,($%02x)\n", regpairnames[r1], n);
                }
                break;
            case 4:
                zprintf("neg\n");
                gen_helper_neg_cc();
                break;
            case 5:
                /* FIXME */
                gen_popw(cpu_T[0]);
                gen_helper_jmp_T0();
                gen_helper_ri();
                if (q == 0) {
                    zprintf("retn\n");
                } else {
                    zprintf("reti\n");
                }
                gen_eob(s);
                s->is_jmp = 3;
//              s->is_ei = 1;
                break;
            case 6:
                gen_helper_imode(tcg_const_tl(imode[y]));
                zprintf("im im[%i]\n", imode[y]);
//              gen_eob(s);
//              s->is_ei = 1;
                break;
            case 7:
                switch (y) {
                case 0:
                    gen_helper_ld_I_A();
                    zprintf("ld i,a\n");
                    break;
                case 1:
                    gen_helper_ld_R_A();
                    zprintf("ld r,a\n");
                    break;
                case 2:
                    gen_helper_ld_A_I();
                    zprintf("ld a,i\n");
                    break;
                case 3:
                    gen_helper_ld_A_R();
                    zprintf("ld a,r\n");
                    break;
                case 4:
                    gen_movb_v_HLmem(cpu_T[0]);
                    gen_helper_rrd_cc();
                    gen_movb_HLmem_v(cpu_T[0]);
                    zprintf("rrd\n");
                    break;
                case 5:
                    gen_movb_v_HLmem(cpu_T[0]);
                    gen_helper_rld_cc();
                    gen_movb_HLmem_v(cpu_T[0]);
                    zprintf("rld\n");
                    break;
                case 6:
                case 7:
                    zprintf("nop2\n");
                    /* nop */
                    break;
                }
                break;
            }
            break;

        case 2:
            /* FIXME */
            if (y >= 4) {
                switch (z) {
                case 0: /* ldi/ldd/ldir/lddr */
                    gen_movw_v_HL(cpu_A0);
                    tcg_gen_qemu_ld8u(cpu_T[0], cpu_A0, MEM_INDEX);
                    gen_movw_v_DE(cpu_A0);
                    tcg_gen_qemu_st8(cpu_T[0], cpu_A0, MEM_INDEX);

                    if (!(y & 1)) {
                        gen_helper_bli_ld_inc_cc();
                    } else {
                        gen_helper_bli_ld_dec_cc();
                    }
                    if ((y & 2)) {
                        gen_helper_bli_ld_rep(tcg_const_tl(s->pc));
                        gen_eob(s);
                        s->is_jmp = 3;
                    }
                    break;

                case 1: /* cpi/cpd/cpir/cpdr */
                    gen_movw_v_HL(cpu_A0);
                    tcg_gen_qemu_ld8u(cpu_T[0], cpu_A0, MEM_INDEX);
                    gen_helper_bli_cp_cc();

                    if (!(y & 1)) {
                        gen_helper_bli_cp_inc_cc();
                    } else {
                        gen_helper_bli_cp_dec_cc();
                    }
                    if ((y & 2)) {
                        gen_helper_bli_cp_rep(tcg_const_tl(s->pc));
                        gen_eob(s);
                        s->is_jmp = 3;
                    }
                    break;

                case 2: /* ini/ind/inir/indr */
                    if (use_icount) {
                        gen_io_start();
                    }
                    gen_helper_in_T0_bc_cc();
                    if (use_icount) {
                        gen_io_end();
                    }
                    gen_movw_v_HL(cpu_A0);
                    tcg_gen_qemu_st8(cpu_T[0], cpu_A0, MEM_INDEX);
                    if (!(y & 1)) {
                        gen_helper_bli_io_T0_inc(0);
                    } else {
                        gen_helper_bli_io_T0_dec(0);
                    }
                    if ((y & 2)) {
                        gen_helper_bli_io_rep(tcg_const_tl(s->pc));
                        gen_eob(s);
                        s->is_jmp = 3;
                    } else if (use_icount) {
                        gen_jmp_im(s->pc);
                    }
                    break;

                case 3: /* outi/outd/otir/otdr */
                    gen_movw_v_HL(cpu_A0);
                    tcg_gen_qemu_ld8u(cpu_T[0], cpu_A0, MEM_INDEX);
                    if (use_icount) {
                        gen_io_start();
                    }
                    gen_helper_out_T0_bc();
                    if (use_icount) {
                        gen_io_end();
                    }
                    if (!(y & 1)) {
                        gen_helper_bli_io_T0_inc(1);
                    } else {
                        gen_helper_bli_io_T0_dec(1);
                    }
                    if ((y & 2)) {
                        gen_helper_bli_io_rep(tcg_const_tl(s->pc));
                        gen_eob(s);
                        s->is_jmp = 3;
                    } else if (use_icount) {
                        gen_jmp_im(s->pc);
                    }
                    break;
                }

                zprintf("%s\n", bli[y-4][z]);
                break;
            }
        }
    }

    prefixes = 0;

    /* now check op code */
//    switch (b) {
//    default:
//        goto illegal_op;
//    }
    /* lock generation */
    return s->pc;
 illegal_op:
    /* XXX: ensure that no lock was generated */
    gen_exception(s, EXCP06_ILLOP, pc_start - s->cs_base);
    return s->pc;
}

#define CC_SZHPNC (CC_S | CC_Z | CC_H | CC_P | CC_N | CC_C)
#define CC_SZHPN (CC_S | CC_Z | CC_H | CC_P | CC_N)

void z80_translate_init(void)
{
    cpu_env = tcg_global_reg_new_ptr(TCG_AREG0, "env");

#if TARGET_LONG_BITS > HOST_LONG_BITS
    cpu_T[0] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, t0), "T0");
    cpu_T[1] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, t1), "T1");
#else
    cpu_T[0] = tcg_global_reg_new_i32(TCG_AREG1, "T0");
    cpu_T[1] = tcg_global_reg_new_i32(TCG_AREG2, "T1");
#endif
    cpu_A0 = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, a0), "A0");

    /* register helpers */
#define GEN_HELPER 2
#include "helper.h"
}

/* generate intermediate code in gen_opc_buf and gen_opparam_buf for
   basic block 'tb'. If search_pc is TRUE, also generate PC
   information for each intermediate instruction. */
static inline int gen_intermediate_code_internal(CPUState *env,
                                                 TranslationBlock *tb,
                                                 int search_pc)
{
    DisasContext dc1, *dc = &dc1;
    target_ulong pc_ptr;
    uint16_t *gen_opc_end;
    CPUBreakpoint *bp;
    int flags, j, lj, cflags;
    target_ulong pc_start;
    target_ulong cs_base;
    int num_insns;
    int max_insns;

    /* generate intermediate code */
    pc_start = tb->pc;
    cs_base = tb->cs_base;
    flags = tb->flags;
    cflags = tb->cflags;

    dc->singlestep_enabled = env->singlestep_enabled;
    dc->cs_base = cs_base;
    dc->tb = tb;
    dc->flags = flags;
    dc->jmp_opt = !(env->singlestep_enabled ||
                    (flags & HF_INHIBIT_IRQ_MASK)
#ifndef CONFIG_SOFTMMU
                    || (flags & HF_SOFTMMU_MASK)
#endif
                    );

    gen_opc_ptr = gen_opc_buf;
    gen_opc_end = gen_opc_buf + OPC_MAX_SIZE;
    gen_opparam_ptr = gen_opparam_buf;

    dc->is_jmp = DISAS_NEXT;
    pc_ptr = pc_start;
    lj = -1;
    dc->model = env->model;

    num_insns = 0;
    max_insns = tb->cflags & CF_COUNT_MASK;
    if (max_insns == 0) {
        max_insns = CF_COUNT_MASK;
    }

    gen_icount_start();
    for (;;) {
        if (unlikely(!TAILQ_EMPTY(&env->breakpoints))) {
            TAILQ_FOREACH(bp, &env->breakpoints, entry) {
                if (bp->pc == pc_ptr) {
                    gen_debug(dc, pc_ptr - dc->cs_base);
                    break;
                }
            }
        }
        if (search_pc) {
            j = gen_opc_ptr - gen_opc_buf;
            if (lj < j) {
                lj++;
                while (lj < j) {
                    gen_opc_instr_start[lj++] = 0;
                }
            }
            gen_opc_pc[lj] = pc_ptr;
            gen_opc_instr_start[lj] = 1;
            gen_opc_icount[lj] = num_insns;
        }
        if (num_insns + 1 == max_insns && (tb->cflags & CF_LAST_IO)) {
            gen_io_start();
        }

        pc_ptr = disas_insn(dc, pc_ptr);
        num_insns++;
        /* stop translation if indicated */
        if (dc->is_jmp) {
            break;
        }
        /* if single step mode, we generate only one instruction and
           generate an exception */
        /* if irq were inhibited with HF_INHIBIT_IRQ_MASK, we clear
           the flag and abort the translation to give the irqs a
           change to be happen */
        if (dc->singlestep_enabled ||
            (flags & HF_INHIBIT_IRQ_MASK)) {
            gen_jmp_im(pc_ptr - dc->cs_base);
            gen_eob(dc);
            break;
        }
        /* if too long translation, stop generation too */
        if (gen_opc_ptr >= gen_opc_end ||
            (pc_ptr - pc_start) >= (TARGET_PAGE_SIZE - 32) ||
            num_insns >= max_insns) {
            gen_jmp_im(pc_ptr - dc->cs_base);
            gen_eob(dc);
            break;
        }
        if (singlestep) {
            gen_jmp_im(pc_ptr - dc->cs_base);
            gen_eob(dc);
            break;
        }
    }
    if (tb->cflags & CF_LAST_IO) {
        gen_io_end();
    }
    gen_icount_end(tb, num_insns);
    *gen_opc_ptr = INDEX_op_end;
    /* we don't forget to fill the last values */
    if (search_pc) {
        j = gen_opc_ptr - gen_opc_buf;
        lj++;
        while (lj <= j) {
            gen_opc_instr_start[lj++] = 0;
        }
    }

#ifdef DEBUG_DISAS
    log_cpu_state_mask(CPU_LOG_TB_CPU, env, 0);
    if (qemu_loglevel_mask(CPU_LOG_TB_IN_ASM)) {
        qemu_log("----------------\n");
        qemu_log("IN: %s\n", lookup_symbol(pc_start));
        log_target_disas(pc_start, pc_ptr - pc_start, 0);
        qemu_log("\n");
    }
#endif

    if (!search_pc) {
        tb->size = pc_ptr - pc_start;
        tb->icount = num_insns;
    }
    return 0;
}

void gen_intermediate_code(CPUState *env, TranslationBlock *tb)
{
    gen_intermediate_code_internal(env, tb, 0);
}

void gen_intermediate_code_pc(CPUState *env, TranslationBlock *tb)
{
    gen_intermediate_code_internal(env, tb, 1);
}

void gen_pc_load(CPUState *env, TranslationBlock *tb,
                 unsigned long searched_pc, int pc_pos, void *puc)
{
    env->pc = gen_opc_pc[pc_pos];
}
