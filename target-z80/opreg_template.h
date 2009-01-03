/*
 * Z80 micro operations (templates for various register related operations)
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

void OPPROTO glue(op_movw_T0,REGNAME)(void)
{
    T0 = REG;
}

#ifdef REGHIGH
void OPPROTO glue(op_movb_T0,REGHIGH)(void)
{
    T0 = (REG >> 8) & 0xff;
}
#endif

#ifdef REGLOW
void OPPROTO glue(op_movb_T0,REGLOW)(void)
{
    T0 = REG & 0xff;
}
#endif

void OPPROTO glue(op_movw_T1,REGNAME)(void)
{
    T1 = REG;
}

void OPPROTO glue(op_movw_A0,REGNAME)(void)
{
    A0 = REG;
}

void OPPROTO glue(glue(op_movw,REGNAME),_T0)(void)
{
    REG = (uint16_t)T0;
}

#ifdef REGHIGH
void OPPROTO glue(glue(op_movb,REGHIGH),_T0)(void)
{
    REG = (REG & 0xff) | ((T0 & 0xff) << 8);
}
#endif

#ifdef REGLOW
void OPPROTO glue(glue(op_movb,REGLOW),_T0)(void)
{
    REG = (REG & 0xff00) | (T0 & 0xff);
}
#endif

void OPPROTO glue(glue(op_movw,REGNAME),_T1)(void)
{
    REG = (uint16_t)T1;
}
