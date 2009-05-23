/*
 * Z80 micro operations
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

#define ASM_SOFTMMU
#include "exec.h"

void OPPROTO op_movl_pc_im(void)
{
    PC = (uint16_t)PARAM1;
}

void OPPROTO op_debug(void)
{
    env->exception_index = EXCP_DEBUG;
    cpu_loop_exit();
}

void OPPROTO op_raise_exception(void)
{
    int exception_index;
    exception_index = PARAM1;
    raise_exception(exception_index);
}

void OPPROTO op_set_inhibit_irq(void)
{
    env->hflags |= HF_INHIBIT_IRQ_MASK;
}

void OPPROTO op_reset_inhibit_irq(void)
{
    env->hflags &= ~HF_INHIBIT_IRQ_MASK;
}

/************ Z80 MICRO-OPS ***********/

/* Loads/stores */

void OPPROTO op_mov_T0_im(void)
{
    T0 = (uint16_t)PARAM1;
}

void OPPROTO op_mov_T1_im(void)
{
    T0 = (uint16_t)PARAM1;
}

void OPPROTO op_mov_A0_im(void)
{
    A0 = (uint16_t)PARAM1;
}

void OPPROTO op_movb_T0_IXmem(void)
{
    A0 = (uint16_t)(IX + PARAM1);
    T0 = ldub_kernel(A0);
}

void OPPROTO op_movb_IXmem_T0(void)
{
    A0 = (uint16_t)(IX + PARAM1);
    stb_kernel(A0, T0);
}

void OPPROTO op_movb_T0_IYmem(void)
{
    A0 = (uint16_t)(IY + PARAM1);
    T0 = ldub_kernel(A0);
}

void OPPROTO op_movb_IYmem_T0(void)
{
    A0 = (uint16_t)(IY + PARAM1);
    stb_kernel(A0, T0);
}

/* Stack operations */

void OPPROTO op_pushw_T0(void)
{
    SP = (uint16_t)(SP - 2);
    A0 = SP;
    /* high byte pushed first: i.e. little endian */
    stw_kernel(A0, T0);
}

void OPPROTO op_popw_T0(void)
{
    A0 = SP;
    /* low byte popped first: i.e. little endian */
    T0 = lduw_kernel(A0);
    SP = (uint16_t)(SP + 2);
}

void OPPROTO op_popw_T1(void)
{
    A0 = SP;
    /* low byte popped first: i.e. little endian */
    T1 = lduw_kernel(A0);
    SP = (uint16_t)(SP + 2);
}

/* Misc */

void OPPROTO op_in_T0_im(void)
{
    helper_in_debug((A << 8) | PARAM1);
    T0 = cpu_inb(env, (A << 8) | PARAM1);
}

void OPPROTO op_in_T0_bc_cc(void)
{
    int sf, zf, pf;

    helper_in_debug(BC);
    T0 = cpu_inb(env, BC);

    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    pf = parity_table[(uint8_t)T0];
    F = (F & CC_C) | sf | zf | pf;
}

void OPPROTO op_out_T0_im(void)
{
    cpu_outb(env, (A << 8) | PARAM1, T0);
}

void OPPROTO op_out_T0_bc(void)
{
    cpu_outb(env, BC, T0);
}

void OPPROTO op_bit_T0(void)
{
    int sf, zf, pf;

    sf = (T0 & PARAM1 & 0x80) ? CC_S : 0;
    zf = (T0 & PARAM1) ? 0 : CC_Z;
    pf = (T0 & PARAM1) ? 0 : CC_P;
    F = (F & CC_C) | sf | zf | CC_H | pf;
}

void OPPROTO op_res_T0(void)
{
    T0 &= (uint8_t)PARAM1;
}

void OPPROTO op_set_T0(void)
{
    T0 |= (uint8_t)PARAM1;
}

void OPPROTO op_jmp_T0(void)
{
    PC = T0;
}

void OPPROTO op_djnz(void)
{
    BC = (uint16_t)(BC - 0x0100);
    if (BC & 0xff00)
        PC = PARAM1;
    else
        PC = PARAM2;
    FORCE_RET();
}

/* Conditional jumps */

void OPPROTO op_jp_nz(void)
{
    if (!(F & CC_Z))
        GOTO_LABEL_PARAM(1);
    FORCE_RET();
}

void OPPROTO op_jp_z(void)
{
    if (F & CC_Z)
        GOTO_LABEL_PARAM(1);
    FORCE_RET();
}

void OPPROTO op_jp_nc(void)
{
    if (!(F & CC_C))
        GOTO_LABEL_PARAM(1);
    FORCE_RET();
}

void OPPROTO op_jp_c(void)
{
    if (F & CC_C)
        GOTO_LABEL_PARAM(1);
    FORCE_RET();
}

void OPPROTO op_jp_po(void)
{
    if (!(F & CC_P))
        GOTO_LABEL_PARAM(1);
    FORCE_RET();
}

void OPPROTO op_jp_pe(void)
{
    if (F & CC_P)
        GOTO_LABEL_PARAM(1);
    FORCE_RET();
}

void OPPROTO op_jp_p(void)
{
    if (!(F & CC_S))
        GOTO_LABEL_PARAM(1);
    FORCE_RET();
}

void OPPROTO op_jp_m(void)
{
    if (F & CC_S)
        GOTO_LABEL_PARAM(1);
    FORCE_RET();
}

/* Arithmetic/logic operations */

#define signed_overflow_add(op1, op2, res, size) \
    (!!((~(op1 ^ op2) & (op1 ^ res)) >> (size - 1)))

#define signed_overflow_sub(op1, op2, res, size) \
    (!!(((op1 ^ op2) & (op1 ^ res)) >> (size - 1)))

void OPPROTO op_add_cc(void)
{
    int sf, zf, hf, pf, cf;
    int tmp = A;
    int carry;

    A = (uint8_t)(A + T0);
    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    carry = (tmp & T0) | ((tmp | T0) & ~A);
    hf = (carry & 0x08) ? CC_H : 0;
    pf = signed_overflow_add(tmp, T0, A, 8) ? CC_P : 0;
    cf = (carry & 0x80) ? CC_C : 0;

    F = sf | zf | hf | pf | cf;

    FORCE_RET();
}

void OPPROTO op_adc_cc(void)
{
    int sf, zf, hf, pf, cf;
    int tmp = A;
    int carry;

    A = (uint8_t)(A + T0 + !!(F & CC_C));
    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    carry = (tmp & T0) | ((tmp | T0) & ~A);
    hf = (carry & 0x08) ? CC_H : 0;
    pf = signed_overflow_add(tmp, T0, A, 8) ? CC_P : 0;
    cf = (carry & 0x80) ? CC_C : 0;

    F = sf | zf | hf | pf | cf;

    FORCE_RET();
}

void OPPROTO op_sub_cc(void)
{
    int sf, zf, hf, pf, cf;
    int tmp = A;
    int carry;

    A = (uint8_t)(A - T0);
    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    carry = (~tmp & T0) | (~(tmp ^ T0) & A);
    hf = (carry & 0x08) ? CC_H : 0;
    pf = signed_overflow_sub(tmp, T0, A, 8) ? CC_P : 0;
    cf = (carry & 0x80) ? CC_C : 0;

    F = sf | zf | hf | pf | CC_N | cf;

    FORCE_RET();
}

void OPPROTO op_sbc_cc(void)
{
    int sf, zf, hf, pf, cf;
    int tmp = A;
    int carry;

    A = (uint8_t)(A - T0 - !!(F & CC_C));
    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    carry = (~tmp & T0) | (~(tmp ^ T0) & A);
    hf = (carry & 0x08) ? CC_H : 0;
    pf = signed_overflow_sub(tmp, T0, A, 8) ? CC_P : 0;
    cf = (carry & 0x80) ? CC_C : 0;

    F = sf | zf | hf | pf | CC_N | cf;

    FORCE_RET();
}

void OPPROTO op_and_cc(void)
{
    int sf, zf, pf;
    A = (uint8_t)(A & T0);

    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    pf = parity_table[(uint8_t)A];
    F = sf | zf | CC_H | pf;

    FORCE_RET();
}

void OPPROTO op_xor_cc(void)
{
    int sf, zf, pf;
    A = (uint8_t)(A ^ T0);

    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    pf = parity_table[(uint8_t)A];
    F = sf | zf | pf;

    FORCE_RET();
}

void OPPROTO op_or_cc(void)
{
    int sf, zf, pf;
    A = (uint8_t)(A | T0);

    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    pf = parity_table[(uint8_t)A];
    F = sf | zf | pf;

    FORCE_RET();
}

void OPPROTO op_cp_cc(void)
{
    int sf, zf, hf, pf, cf;
    int res, carry;

    res = (uint8_t)(A - T0);
    sf = (res & 0x80) ? CC_S : 0;
    zf = res ? 0 : CC_Z;
    carry = (~A & T0) | (~(A ^ T0) & res);
    hf = (carry & 0x08) ? CC_H : 0;
    pf = signed_overflow_sub(A, T0, res, 8) ? CC_P : 0;
    cf = (carry & 0x80) ? CC_C : 0;

    F = sf | zf | hf | pf | CC_N | cf;

    FORCE_RET();
//  CC_DST = (uint8_t)(A - T0);
}

/* Rotation/shift operations */

void OPPROTO op_rlc_T0_cc(void)
{
    int sf, zf, pf, cf;
    int tmp;

    tmp = T0;
    T0 = (uint8_t)((T0 << 1) | !!(T0 & 0x80));
    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    pf = parity_table[T0];
    cf = (tmp & 0x80) ? CC_C : 0;
    F = sf | zf | pf | cf;

    FORCE_RET();
}

void OPPROTO op_rrc_T0_cc(void)
{
    int sf, zf, pf, cf;
    int tmp;

    tmp = T0;
    T0 = (uint8_t)((T0 >> 1) | ((tmp & 0x01) ? 0x80 : 0));
    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    pf = parity_table[T0];
    cf = (tmp & 0x01) ? CC_C : 0;
    F = sf | zf | pf | cf;

    FORCE_RET();
}

void OPPROTO op_rl_T0_cc(void)
{
    int sf, zf, pf, cf;
    int tmp;

    tmp = T0;
    T0 = (uint8_t)((T0 << 1) | !!(F & CC_C));
    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    pf = parity_table[T0];
    cf = (tmp & 0x80) ? CC_C : 0;
    F = sf | zf | pf | cf;

    FORCE_RET();
}

void OPPROTO op_rr_T0_cc(void)
{
    int sf, zf, pf, cf;
    int tmp;

    tmp = T0;
    T0 = (uint8_t)((T0 >> 1) | ((F & CC_C) ? 0x80 : 0));
    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    pf = parity_table[T0];
    cf = (tmp & 0x01) ? CC_C : 0;
    F = sf | zf | pf | cf;

    FORCE_RET();
}

void OPPROTO op_sla_T0_cc(void)
{
    int sf, zf, pf, cf;
    int tmp;

    tmp = T0;
    T0 = (uint8_t)(T0 << 1);
    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    pf = parity_table[T0];
    cf = (tmp & 0x80) ? CC_C : 0;
    F = sf | zf | pf | cf;

    FORCE_RET();
}

void OPPROTO op_sra_T0_cc(void)
{
    int sf, zf, pf, cf;
    int tmp;

    tmp = T0;
    T0 = (uint8_t)((T0 >> 1) | (T0 & 0x80));
    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    pf = parity_table[T0];
    cf = (tmp & 0x01) ? CC_C : 0;
    F = sf | zf | pf | cf;

    FORCE_RET();
}

/* Z80-specific: R800 has tst instruction */
void OPPROTO op_sll_T0_cc(void)
{
    int sf, zf, pf, cf;
    int tmp;

    tmp = T0;
    T0 = (uint8_t)((T0 << 1) | 1); /* Yes -- bit 0 is *set* */
    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    pf = parity_table[T0];
    cf = (tmp & 0x80) ? CC_C : 0;
    F = sf | zf | pf | cf;

    FORCE_RET();
}

void OPPROTO op_srl_T0_cc(void)
{
    int sf, zf, pf, cf;
    int tmp;

    tmp = T0;
    T0 = (uint8_t)(T0 >> 1);
    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    pf = parity_table[T0];
    cf = (tmp & 0x01) ? CC_C : 0;
    F = sf | zf | pf | cf;

    FORCE_RET();
}

void OPPROTO op_rld_cc(void)
{
    int sf, zf, pf;
    int tmp = A & 0x0f;
    A = (A & 0xf0) | ((T0 >> 4) & 0x0f);
    T0 = ((T0 << 4) & 0xf0) | tmp;

    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    pf = parity_table[A];

    F = (F & CC_C) | sf | zf | pf;
}

void OPPROTO op_rrd_cc(void)
{
    int sf, zf, pf;
    int tmp = A & 0x0f;
    A = (A & 0xf0) | (T0 & 0x0f);
    T0 = (T0 >> 4) | (tmp << 4);

    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    pf = parity_table[A];

    F = (F & CC_C) | sf | zf | pf;
}

/* Block instructions */

void OPPROTO op_bli_ld_inc_cc(void)
{
    int pf;

    BC = (uint16_t)(BC - 1);
    DE = (uint16_t)(DE + 1);
    HL = (uint16_t)(HL + 1);

    pf = BC ? CC_P : 0;
    F = (F & (CC_S | CC_Z | CC_C)) | pf;

    FORCE_RET();
}

void OPPROTO op_bli_ld_dec_cc(void)
{
    int pf;

    BC = (uint16_t)(BC - 1);
    DE = (uint16_t)(DE - 1);
    HL = (uint16_t)(HL - 1);

    pf = BC ? CC_P : 0;
    F = (F & (CC_S | CC_Z | CC_C)) | pf;

    FORCE_RET();
}

void OPPROTO op_bli_ld_rep(void)
{
    if (BC)
        PC = PARAM1 - 2;
    else
        PC = PARAM1;
    FORCE_RET();
}

void OPPROTO op_bli_cp_cc(void)
{
    int sf, zf, hf, pf;
    int res, carry;

    res = (uint8_t)(A - T0);
    sf = (res & 0x80) ? CC_S : 0;
    zf = res ? 0 : CC_Z;
    carry = (~A & T0) | (~(A ^ T0) & res);
    hf = (carry & 0x08) ? CC_H : 0;
    pf = BC ? CC_P : 0;

    F = (F & CC_C) | sf | zf | hf | pf | CC_N;

    FORCE_RET();
}

void OPPROTO op_bli_cp_inc_cc(void)
{
    int pf;

    BC = (uint16_t)(BC - 1);
    HL = (uint16_t)(HL + 1);

    pf = BC ? CC_P : 0;
    F = (F & ~CC_P) | pf;

    FORCE_RET();
}

void OPPROTO op_bli_cp_dec_cc(void)
{
    int pf;

    BC = (uint16_t)(BC - 1);
    HL = (uint16_t)(HL - 1);

    pf = BC ? CC_P : 0;
    F = (F & ~CC_P) | pf;

    FORCE_RET();
}

void OPPROTO op_bli_cp_rep(void)
{
    if (BC && T0 != A)
        PC = PARAM1 - 2;
    else
        PC = PARAM1;
    FORCE_RET();
}

void OPPROTO op_bli_io_inc(void)
{
    HL = (uint16_t)(HL + 1);
    BC = (uint16_t)BC - 0x0100;
}

void OPPROTO op_bli_io_dec(void)
{
    HL = (uint16_t)(HL - 1);
    BC = (uint16_t)BC - 0x0100;
}

void OPPROTO op_bli_io_rep(void)
{
    if (BC & 0xff00)
        PC = PARAM1 - 2;
    else
        PC = PARAM1;
    FORCE_RET();
}

/* misc */

void OPPROTO op_rlca_cc(void)
{
    int cf;
    int tmp;

    tmp = A;
    A = (uint8_t)((A << 1) | !!(tmp & 0x80));
    cf = (tmp & 0x80) ? CC_C : 0;
    F = (F & (CC_S | CC_Z | CC_P)) | cf;

    FORCE_RET();
}

void OPPROTO op_rrca_cc(void)
{
    int cf;
    int tmp;

    tmp = A;
    A = (A >> 1) | ((tmp & 0x01) ? 0x80 : 0);
    cf = (tmp & 0x01) ? CC_C : 0;
    F = (F & (CC_S | CC_Z | CC_P)) | cf;

    FORCE_RET();
}

void OPPROTO op_rla_cc(void)
{
    int cf;
    int tmp;

    tmp = A;
    A = (uint8_t)((A << 1) | !!(F & CC_C));
    cf = (tmp & 0x80) ? CC_C : 0;
    F = (F & (CC_S | CC_Z | CC_P)) | cf;

    FORCE_RET();
}

void OPPROTO op_rra_cc(void)
{
    int cf;
    int tmp;

    tmp = A;
    A = (A >> 1) | ((F & CC_C) ? 0x80 : 0);
    cf = (tmp & 0x01) ? CC_C : 0;
    F = (F & (CC_S | CC_Z | CC_P)) | cf;

    FORCE_RET();
}

/* TODO */
void OPPROTO op_daa_cc(void)
{
    int sf, zf, hf, pf, cf;
    int cor = 0;
    int tmp = A;

    if (A > 0x99 || (F & CC_C)) {
        cor |= 0x60;
        cf = CC_C;
    } else {
        cf = 0;
    }

    if ((A & 0x0f) > 0x09 || (F & CC_H)) {
        cor |= 0x06;
    }

    if (!(F & CC_N)) {
        A = (uint8_t)(A + cor);
    } else {
        A = (uint8_t)(A - cor);
    }

    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    hf = ((tmp ^ A) & 0x10) ? CC_H : 0;
    pf = parity_table[(uint8_t)A];

    F = (F & CC_N) | sf | zf | hf | pf | cf;
}

void OPPROTO op_cpl_cc(void)
{
    A = (uint8_t)~A;
    F |= CC_H | CC_N;
}

void OPPROTO op_scf_cc(void)
{
    F = (F & (CC_S | CC_Z | CC_P)) | CC_C;
}

void OPPROTO op_ccf_cc(void)
{
    int hf, cf;

    hf = (F & CC_C) ? CC_H : 0;
    cf = (F & CC_C) ^ CC_C;
    F = (F & (CC_S | CC_Z | CC_P)) | hf | cf;

    FORCE_RET();
}

/* misc */

void OPPROTO op_neg_cc(void)
{
    int sf, zf, hf, pf, cf;
    int tmp = A;
    int carry;

    A = (uint8_t)-A;
    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    carry = (tmp & T0) | ((tmp | T0) & ~A);
    hf = (carry & 0x08) ? CC_H : 0;
    pf = signed_overflow_sub(tmp, T0, A, 8) ? CC_P : 0;
    cf = (carry & 0x80) ? CC_C : 0;

    F = sf | zf | hf | pf | CC_N | cf;

    FORCE_RET();
}

/* word operations -- HL only? */

void OPPROTO op_incw_T0(void)
{
    T0++;
    T0 = (uint16_t)T0;
}

void OPPROTO op_decw_T0(void)
{
    T0--;
    T0 = (uint16_t)T0;
}

void OPPROTO op_sbcw_T0_T1_cc(void)
{
    int sf, zf, hf, pf, cf;
    int tmp = T0;
    int carry;

    T0 = (uint16_t)(T0 - T1 - !!(F & CC_C));
    sf = (T0 & 0x8000) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    carry = (~tmp & T1) | (~(tmp ^ T1) & T0);
    hf = (carry & 0x0800) ? CC_H : 0;
    pf = signed_overflow_sub(tmp, T1, T0, 16) ? CC_P : 0;
    cf = (carry & 0x8000) ? CC_C : 0;

    F = sf | zf | hf | pf | CC_N | cf;

    FORCE_RET();
}

void OPPROTO op_addw_T0_T1_cc(void)
{
    int hf, cf;
    int tmp = T0;
    int carry;

    T0 = (uint16_t)(T0 + T1);
    carry = (tmp & T1) | ((tmp | T1) & ~T0);
    hf = (carry & 0x0800) ? CC_H : 0;
    cf = (carry & 0x8000) ? CC_C : 0;

    F = (F & (CC_S | CC_Z | CC_P)) | hf | cf;

    FORCE_RET();
}

void OPPROTO op_adcw_T0_T1_cc(void)
{
    int sf, zf, hf, pf, cf;
    int tmp = T0;
    int carry;

    T0 = (uint16_t)(T0 + T1 + !!(F & CC_C));
    sf = (T0 & 0x8000) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;
    carry = (tmp & T1) | ((tmp | T1) & ~T0);
    hf = (carry & 0x0800) ? CC_H : 0;
    pf = signed_overflow_add(tmp, T1, T0, 8) ? CC_P : 0;
    cf = (carry & 0x8000) ? CC_C : 0;

    F = sf | zf | hf | pf | cf;

    FORCE_RET();
}

/* misc */

void OPPROTO op_incb_T0_cc(void)
{
    int sf, zf, hf, pf;
    int tmp;
    int carry;

    tmp = T0;
    T0 = (uint8_t)(T0 + 1);
    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;

    carry = (tmp & 1) | ((tmp | 1) & ~T0);
    hf = (carry & 0x08) ? CC_H : 0;
    pf = signed_overflow_add(tmp, 1, T0, 8) ? CC_P : 0;

    F = (F & CC_C) | sf | zf | hf | pf;

    FORCE_RET();
}

void OPPROTO op_decb_T0_cc(void)
{
    int sf, zf, hf, pf;
    int tmp;
    int carry;

    tmp = T0;
    T0 = (uint8_t)(T0 - 1);
    sf = (T0 & 0x80) ? CC_S : 0;
    zf = T0 ? 0 : CC_Z;

    carry = (~tmp & 1) | (~(tmp ^ 1) & T0);
    hf = (carry & 0x08) ? CC_H : 0;
    pf = signed_overflow_sub(tmp, 1, T0, 8) ? CC_P : 0;

    F = (F & CC_C) | sf | zf | hf | CC_N | pf;
    /* TODO: check CC_N is set */

    FORCE_RET();
}

/* value on data bus is 0xff for speccy */
/* IM0 = execute data on bus (rst $38 on speccy) */
/* IM1 = execute rst $38 (ROM uses this)*/
/* IM2 = indirect jump -- address is held at (I << 8) | DATA */

/* when an interrupt occurs, iff1 and iff2 are reset, disabling interrupts */
/* when an NMI occurs, iff1 is reset. iff2 is left unchanged */

void OPPROTO op_imode(void)
{
    env->imode = PARAM1;
}

/* enable interrupts */
void OPPROTO op_ei(void)
{
    env->iff1 = 1;
    env->iff2 = 1;
}

/* disable interrupts */
void OPPROTO op_di(void)
{
    env->iff1 = 0;
    env->iff2 = 0;
}

/* reenable interrupts if enabled */
void OPPROTO op_ri(void)
{
    env->iff1 = env->iff2;
}

void OPPROTO op_ld_R_A(void)
{
    R = A;
}

void OPPROTO op_ld_I_A(void)
{
    I = A;
}

void OPPROTO op_ld_A_R(void)
{
    int sf, zf, pf;

    A = R;
    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    pf = env->iff2 ? CC_P : 0;

    F = (F & CC_C) | sf | zf | pf;
}

void OPPROTO op_ld_A_I(void)
{
    int sf, zf, pf;

    A = I;
    sf = (A & 0x80) ? CC_S : 0;
    zf = A ? 0 : CC_Z;
    pf = env->iff2 ? CC_P : 0;

    F = (F & CC_C) | sf | zf | pf;
}

void OPPROTO op_mulub_cc(void)
{
    /* TODO: flags */

    HL = A * T0;
}

void OPPROTO op_muluw_cc(void)
{
    /* TODO: flags */
    uint32_t tmp;

    tmp = HL * T0;
    DE = tmp >> 16;
    HL = tmp & 0xff;
}

void OPPROTO op_dump_registers(void)
{
    helper_dump_registers(PARAM1);
}

/*********** END OF Z80 OPS ***********/

void OPPROTO op_set_cc_op(void)
{
    CC_OP = PARAM1;
}

static int compute_all_eflags(void)
{
    return CC_SRC;
}

static int compute_c_eflags(void)
{
    return CC_SRC & CC_C;
}

CCTable cc_table[CC_OP_NB] = {
    [CC_OP_DYNAMIC] = { /* should never happen */ },

    [CC_OP_EFLAGS] = { compute_all_eflags, compute_c_eflags },
};

/* threading support */
void OPPROTO op_lock(void)
{
    cpu_lock();
}

void OPPROTO op_unlock(void)
{
    cpu_unlock();
}
