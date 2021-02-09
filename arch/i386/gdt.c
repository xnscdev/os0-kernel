/*************************************************************************
 * gdt.c -- This file is part of OS/0.                                   *
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

#include <i386/gdt.h>
#include <sys/io.h>

static GDTEntry gdt_entries[GDT_SIZE];
static DTPtr gdt;

void
gdt_set_gate (uint32_t n, uint32_t base, uint32_t limit, unsigned char access, unsigned char granularity)
{
  gdt_entries[n].ge_basel = base & 0xffff;
  gdt_entries[n].ge_basem = (base >> 16) & 0xff;
  gdt_entries[n].ge_baseh = (base >> 24) & 0xff;
  gdt_entries[n].ge_liml = limit & 0xffff;
  gdt_entries[n].ge_gran = ((limit >> 16) & 0x0f) | (granularity & 0xf0);
  gdt_entries[n].ge_access = access;
}

void
gdt_init (void)
{
  gdt.dp_limit = sizeof (GDTEntry) * GDT_SIZE - 1;
  gdt.dp_base = (uint32_t) &gdt_entries;

  gdt_set_gate (0, 0, 0, 0, 0); /* Null segment */
  gdt_set_gate (1, 0, 0xffffffff, 0x9a, 0xcf); /* Code segment */
  gdt_set_gate (2, 0, 0xffffffff, 0x92, 0xcf); /* Data segment */
  gdt_set_gate (3, 0, 0xffffffff, 0xfa, 0xcf); /* User mode code segment */
  gdt_set_gate (4, 0, 0xffffffff, 0xf2, 0xcf); /* User mode data segment */

  gdt_load ((uint32_t) &gdt);
}
