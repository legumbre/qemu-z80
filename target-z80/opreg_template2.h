/*
 * Z80 micro operations (templates for various register related operations)
 *
 *  Copyright (c) 2007 Stuart Brady
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

void OPPROTO glue(op_movw_T0,REGPAIRNAME)(void)
{
    T0 = (REGHIGH << 8) | REGLOW;
}

void OPPROTO glue(op_movb_T0,REGHIGHNAME)(void)
{
    T0 = REGHIGH;
}

void OPPROTO glue(op_movb_T0,REGLOWNAME)(void)
{
    T0 = REGLOW;
}

void OPPROTO glue(op_movw_T1,REGPAIRNAME)(void)
{
    T1 = (REGHIGH << 8) | REGLOW;
}

void OPPROTO glue(glue(op_movw,REGPAIRNAME),_T0)(void)
{
    REGHIGH = (uint8_t)(T0 >> 8);
    REGLOW = (uint8_t)T0;
}

void OPPROTO glue(glue(op_movb,REGHIGHNAME),_T0)(void)
{
    REGHIGH = (uint8_t)T0;
}

void OPPROTO glue(glue(op_movb,REGLOWNAME),_T0)(void)
{
    REGLOW = (uint8_t)T0;
}

void OPPROTO glue(glue(op_movw,REGPAIRNAME),_T1)(void)
{
    REGHIGH = (uint16_t)(T1 >> 8);
    REGLOW = (uint16_t)T1;
}
