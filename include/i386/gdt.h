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

#include <sys/cdefs.h>
#include <stdint.h>

#define GDT_SIZE 5

typedef struct
{
  uint16_t ge_liml;
  uint16_t ge_basel;
  unsigned char ge_basem;
  unsigned char ge_access;
  unsigned char ge_gran;
  unsigned char ge_baseh;
} __attribute__ ((packed)) GDTEntry;

typedef struct
{
  uint16_t dp_limit;
  uint32_t dp_base;
} __attribute__ ((packed)) DTPtr;

__BEGIN_DECLS

void gdt_load (uint32_t addr);

void gdt_set_gate (uint32_t n, uint32_t base, uint32_t limit,
		   unsigned char access, unsigned char granularity);

__END_DECLS

#endif
