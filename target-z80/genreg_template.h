/*
 * Z80 translation (templates for various register related operations)
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

/* Loads */

static inline void glue(gen_movb_v_,REGHIGH)(TCGv v)
{
    tcg_gen_ld8u_tl(v, cpu_env,
                    offsetof(CPUState, regs[glue(R_,REGPAIR)]) +
                                            BYTE_OFFSET(cpu_env->regs[], 1));
}

static inline void glue(gen_movb_v_,REGLOW)(TCGv v)
{
    tcg_gen_ld8u_tl(v, cpu_env,
                    offsetof(CPUState, regs[glue(R_,REGPAIR)]) +
                                            BYTE_OFFSET(cpu_env->regs[], 0));
}

static inline void glue(gen_movw_v_,REGPAIR)(TCGv v)
{
    tcg_gen_ld16u_tl(v, cpu_env,
                     offsetof(CPUState, regs[glue(R_,REGPAIR)]) +
                                             WORD_OFFSET(cpu_env->regs[], 0));
}

/* Stores */

static inline void glue(glue(gen_movb_,REGHIGH),_v)(TCGv v)
{
    tcg_gen_st8_tl(v, cpu_env,
                   offsetof(CPUState, regs[glue(R_,REGPAIR)]) +
                                           BYTE_OFFSET(cpu_env->regs[], 1));
}

static inline void glue(glue(gen_movb_,REGLOW),_v)(TCGv v)
{
    tcg_gen_st8_tl(v, cpu_env,
                   offsetof(CPUState, regs[glue(R_,REGPAIR)]) +
                                           BYTE_OFFSET(cpu_env->regs[], 0));
}

static inline void glue(glue(gen_movw_,REGPAIR),_v)(TCGv v)
{
    tcg_gen_st16_tl(v, cpu_env,
                    offsetof(CPUState, regs[glue(R_,REGPAIR)]) +
                                            WORD_OFFSET(cpu_env->regs[], 0));
}
