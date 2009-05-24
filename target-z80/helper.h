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
#else /* !GEN_HELPER */
#define DEF_HELPER_0_0 DEF_HELPER
#define DEF_HELPER_0_1 DEF_HELPER
#define HELPER(x) glue(helper_,x)
#endif

DEF_HELPER_0_0(debug, void, (void))
DEF_HELPER_0_1(raise_exception, void, (int))
DEF_HELPER_0_0(set_inhibit_irq, void, (void))
DEF_HELPER_0_0(reset_inhibit_irq, void, (void))

DEF_HELPER_0_0(halt, void, (void))
DEF_HELPER_0_1(in_T0_im, void, (uint32_t))
DEF_HELPER_0_0(in_T0_bc_cc, void, (void))
DEF_HELPER_0_1(out_T0_im, void, (uint32_t))
DEF_HELPER_0_0(out_T0_bc, void, (void))

#undef DEF_HELPER
#undef DEF_HELPER_0_0
#undef DEF_HELPER_0_1
#undef GEN_HELPER
