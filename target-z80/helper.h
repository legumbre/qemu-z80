#include "def-helper.h"

DEF_HELPER_0(debug, void)
DEF_HELPER_1(raise_exception, void, i32)
DEF_HELPER_0(set_inhibit_irq, void)
DEF_HELPER_0(reset_inhibit_irq, void)

DEF_HELPER_1(movl_pc_im, void, i32)

DEF_HELPER_0(halt, void)

/* In / Out */
DEF_HELPER_1(in_T0_im, void, i32)
DEF_HELPER_0(in_T0_bc_cc, void)
DEF_HELPER_1(out_T0_im, void, i32)
DEF_HELPER_0(out_T0_bc, void)

/* Misc */
DEF_HELPER_1(bit_T0, void, i32)
DEF_HELPER_0(jmp_T0, void)
DEF_HELPER_2(djnz, void, i32, i32)

/* 8-bit arithmetic */
DEF_HELPER_0(add_cc, void)
DEF_HELPER_0(adc_cc, void)
DEF_HELPER_0(sub_cc, void)
DEF_HELPER_0(sbc_cc, void)
DEF_HELPER_0(and_cc, void)
DEF_HELPER_0(xor_cc, void)
DEF_HELPER_0(or_cc, void)
DEF_HELPER_0(cp_cc, void)

/* Rotation/shifts */
DEF_HELPER_0(rlc_T0_cc, void)
DEF_HELPER_0(rrc_T0_cc, void)
DEF_HELPER_0(rl_T0_cc, void)
DEF_HELPER_0(rr_T0_cc, void)
DEF_HELPER_0(sla_T0_cc, void)
DEF_HELPER_0(sra_T0_cc, void)
DEF_HELPER_0(sll_T0_cc, void)
DEF_HELPER_0(srl_T0_cc, void)
DEF_HELPER_0(rld_cc, void)
DEF_HELPER_0(rrd_cc, void)

/* Block instructions */
DEF_HELPER_0(bli_ld_inc_cc, void)
DEF_HELPER_0(bli_ld_dec_cc, void)
DEF_HELPER_1(bli_ld_rep, void, i32)
DEF_HELPER_0(bli_cp_cc, void)
DEF_HELPER_0(bli_cp_inc_cc, void)
DEF_HELPER_0(bli_cp_dec_cc, void)
DEF_HELPER_1(bli_cp_rep, void, i32)
DEF_HELPER_1(bli_io_T0_inc, void, i32)
DEF_HELPER_1(bli_io_T0_dec, void, i32)
DEF_HELPER_1(bli_io_rep, void, i32)

/* Misc */
DEF_HELPER_0(rlca_cc, void)
DEF_HELPER_0(rrca_cc, void)
DEF_HELPER_0(rla_cc, void)
DEF_HELPER_0(rra_cc, void)
DEF_HELPER_0(daa_cc, void)
DEF_HELPER_0(cpl_cc, void)
DEF_HELPER_0(scf_cc, void)
DEF_HELPER_0(ccf_cc, void)
DEF_HELPER_0(neg_cc, void)

/* 16-bit arithmetic */
DEF_HELPER_0(sbcw_T0_T1_cc, void)
DEF_HELPER_0(addw_T0_T1_cc, void)
DEF_HELPER_0(adcw_T0_T1_cc, void)
DEF_HELPER_0(incb_T0_cc, void)
DEF_HELPER_0(decb_T0_cc, void)

/* Interrupt handling / IR registers */
DEF_HELPER_1(imode, void, i32)
DEF_HELPER_0(ei, void)
DEF_HELPER_0(di, void)
DEF_HELPER_0(ri, void)
DEF_HELPER_0(ld_R_A, void)
DEF_HELPER_0(ld_I_A, void)
DEF_HELPER_0(ld_A_R, void)
DEF_HELPER_0(ld_A_I, void)

/* R800 */
DEF_HELPER_0(mulub_cc, void)
DEF_HELPER_0(muluw_cc, void)

#include "def-helper.h"
