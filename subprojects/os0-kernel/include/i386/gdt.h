/*************************************************************************
 * gdt.h -- This file is part of OS/0.                                   *
 * Copyright (C) 2021 XNSC                                               *
 *                                                                       *
 * OS/0 is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * OS/0 is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with OS/0. If not, see <https://www.gnu.org/licenses/>.         *
 *************************************************************************/

#ifndef _I386_GDT_H
#define _I386_GDT_H

#include <libk/types.h>
#include <sys/cdefs.h>

#define GDT_SIZE 5

typedef struct
{
  u16 ge_liml;
  u16 ge_basel;
  u8 ge_basem;
  u8 ge_access;
  u8 ge_gran;
  u8 ge_baseh;
} __attribute__ ((packed)) GDTEntry;

typedef struct
{
  u16 dp_limit;
  u32 dp_base;
} __attribute__ ((packed)) DTPtr;

__BEGIN_DECLS

void gdt_load (u32 addr);

void gdt_set_gate (u32 n, u32 base, u32 limit, u8 access, u8 granularity);

__END_DECLS

#endif
