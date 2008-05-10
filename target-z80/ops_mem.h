void OPPROTO glue(glue(op_ldub, MEMSUFFIX), _T0_A0)(void)
{
    T0 = glue(ldub, MEMSUFFIX)(A0);
}

void OPPROTO glue(glue(op_ldsb, MEMSUFFIX), _T0_A0)(void)
{
    T0 = glue(ldsb, MEMSUFFIX)(A0);
}

void OPPROTO glue(glue(op_lduw, MEMSUFFIX), _T0_A0)(void)
{
    T0 = glue(lduw, MEMSUFFIX)(A0);
}

void OPPROTO glue(glue(op_ldsw, MEMSUFFIX), _T0_A0)(void)
{
    T0 = glue(ldsw, MEMSUFFIX)(A0);
}

void OPPROTO glue(glue(op_ldub, MEMSUFFIX), _T1_A0)(void)
{
    T1 = glue(ldub, MEMSUFFIX)(A0);
}

void OPPROTO glue(glue(op_ldsb, MEMSUFFIX), _T1_A0)(void)
{
    T1 = glue(ldsb, MEMSUFFIX)(A0);
}

void OPPROTO glue(glue(op_lduw, MEMSUFFIX), _T1_A0)(void)
{
    T1 = glue(lduw, MEMSUFFIX)(A0);
}

void OPPROTO glue(glue(op_ldsw, MEMSUFFIX), _T1_A0)(void)
{
    T1 = glue(ldsw, MEMSUFFIX)(A0);
}

void OPPROTO glue(glue(op_stb, MEMSUFFIX), _T0_A0)(void)
{
    glue(stb, MEMSUFFIX)(A0, T0);
    FORCE_RET();
}

void OPPROTO glue(glue(op_stw, MEMSUFFIX), _T0_A0)(void)
{
    glue(stw, MEMSUFFIX)(A0, T0);
    FORCE_RET();
}

void OPPROTO glue(glue(op_stw, MEMSUFFIX), _T1_A0)(void)
{
    glue(stw, MEMSUFFIX)(A0, T1);
    FORCE_RET();
}

#undef MEMSUFFIX
