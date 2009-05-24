#define DEF_HELPER(name, ret, args) ret glue(helper_,name) args;

#ifdef GEN_HELPER
#define DEF_HELPER_0_0(name, ret, args) \
DEF_HELPER(name, ret, args) \
static inline void gen_helper_##name(void) \
{ \
    tcg_gen_helper_0_0(helper_##name); \
}
#define DEF_HELPER_0_1(name, ret, args) \
DEF_HELPER(name, ret, args) \
static inline void gen_helper_##name(TCGv arg1) \
{ \
    tcg_gen_helper_0_1(helper_##name, arg1); \
}
#define DEF_HELPER_0_2(name, ret, args) \
DEF_HELPER(name, ret, args) \
static inline void gen_helper_##name(TCGv arg1, TCGv arg2) \
{ \
    tcg_gen_helper_0_2(helper_##name, arg1, arg2); \
}
#else /* !GEN_HELPER */
#define DEF_HELPER_0_0 DEF_HELPER
#define DEF_HELPER_0_1 DEF_HELPER
#define DEF_HELPER_0_2 DEF_HELPER
#define HELPER(x) glue(helper_,x)
#endif

DEF_HELPER_0_0(debug, void, (void))
DEF_HELPER_0_1(raise_exception, void, (int))
DEF_HELPER_0_0(set_inhibit_irq, void, (void))
DEF_HELPER_0_0(reset_inhibit_irq, void, (void))

DEF_HELPER_0_1(movl_pc_im, void, (uint32_t))
DEF_HELPER_0_0(halt, void, (void))
DEF_HELPER_0_1(in_T0_im, void, (uint32_t))
DEF_HELPER_0_0(in_T0_bc_cc, void, (void))
DEF_HELPER_0_1(out_T0_im, void, (uint32_t))
DEF_HELPER_0_0(out_T0_bc, void, (void))

DEF_HELPER_0_1(bit_T0, void, (uint32_t))
DEF_HELPER_0_0(jmp_T0, void, (void))
DEF_HELPER_0_2(djnz, void, (uint32_t, uint32_t))
DEF_HELPER_0_0(add_cc, void, (void))
DEF_HELPER_0_0(adc_cc, void, (void))
DEF_HELPER_0_0(sub_cc, void, (void))
DEF_HELPER_0_0(sbc_cc, void, (void))
DEF_HELPER_0_0(and_cc, void, (void))
DEF_HELPER_0_0(xor_cc, void, (void))
DEF_HELPER_0_0(or_cc, void, (void))
DEF_HELPER_0_0(cp_cc, void, (void))
DEF_HELPER_0_0(rlc_T0_cc, void, (void))
DEF_HELPER_0_0(rrc_T0_cc, void, (void))
DEF_HELPER_0_0(rl_T0_cc, void, (void))
DEF_HELPER_0_0(rr_T0_cc, void, (void))
DEF_HELPER_0_0(sla_T0_cc, void, (void))
DEF_HELPER_0_0(sra_T0_cc, void, (void))
DEF_HELPER_0_0(sll_T0_cc, void, (void))
DEF_HELPER_0_0(srl_T0_cc, void, (void))
DEF_HELPER_0_0(rld_cc, void, (void))
DEF_HELPER_0_0(rrd_cc, void, (void))
DEF_HELPER_0_0(bli_ld_inc_cc, void, (void))
DEF_HELPER_0_0(bli_ld_dec_cc, void, (void))
DEF_HELPER_0_1(bli_ld_rep, void, (uint32_t))
DEF_HELPER_0_0(bli_cp_cc, void, (void))
DEF_HELPER_0_0(bli_cp_inc_cc, void, (void))
DEF_HELPER_0_0(bli_cp_dec_cc, void, (void))
DEF_HELPER_0_1(bli_cp_rep, void, (uint32_t))
DEF_HELPER_0_0(bli_io_inc, void, (void))
DEF_HELPER_0_0(bli_io_dec, void, (void))
DEF_HELPER_0_1(bli_io_rep, void, (uint32_t))
DEF_HELPER_0_0(rlca_cc, void, (void))
DEF_HELPER_0_0(rrca_cc, void, (void))
DEF_HELPER_0_0(rla_cc, void, (void))
DEF_HELPER_0_0(rra_cc, void, (void))
DEF_HELPER_0_0(daa_cc, void, (void))
DEF_HELPER_0_0(cpl_cc, void, (void))
DEF_HELPER_0_0(scf_cc, void, (void))
DEF_HELPER_0_0(ccf_cc, void, (void))
DEF_HELPER_0_0(neg_cc, void, (void))
DEF_HELPER_0_0(sbcw_T0_T1_cc, void, (void))
DEF_HELPER_0_0(addw_T0_T1_cc, void, (void))
DEF_HELPER_0_0(adcw_T0_T1_cc, void, (void))
DEF_HELPER_0_0(incb_T0_cc, void, (void))
DEF_HELPER_0_0(decb_T0_cc, void, (void))
DEF_HELPER_0_1(imode, void, (int))
DEF_HELPER_0_0(ei, void, (void))
DEF_HELPER_0_0(di, void, (void))
DEF_HELPER_0_0(ri, void, (void))
DEF_HELPER_0_0(ld_R_A, void, (void))
DEF_HELPER_0_0(ld_I_A, void, (void))
DEF_HELPER_0_0(ld_A_R, void, (void))
DEF_HELPER_0_0(ld_A_I, void, (void))
DEF_HELPER_0_0(mulub_cc, void, (void))
DEF_HELPER_0_0(muluw_cc, void, (void))

#undef DEF_HELPER
#undef DEF_HELPER_0_0
#undef DEF_HELPER_0_1
#undef DEF_HELPER_0_2
#undef GEN_HELPER
