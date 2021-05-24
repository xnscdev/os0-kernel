/*************************************************************************
 * tss.h -- This file is part of OS/0.                                   *
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

#ifndef _I386_TSS_H
#define _I386_TSS_H

#include <sys/cdefs.h>
#include <stdint.h>

typedef struct
{
  uint32_t tss_prev;
  uint32_t tss_esp0;
  uint32_t tss_ss0;
  uint32_t tss_esp1;
  uint32_t tss_ss1;
  uint32_t tss_esp2;
  uint32_t tss_ss2;
  uint32_t tss_cr3;
  uint32_t tss_eip;
  uint32_t tss_eflags;
  uint32_t tss_eax;
  uint32_t tss_ecx;
  uint32_t tss_edx;
  uint32_t tss_ebx;
  uint32_t tss_esp;
  uint32_t tss_ebp;
  uint32_t tss_esi;
  uint32_t tss_edi;
  uint32_t tss_es;
  uint32_t tss_cs;
  uint32_t tss_ss;
  uint32_t tss_ds;
  uint32_t tss_fs;
  uint32_t tss_gs;
  uint32_t tss_ldt;
  uint16_t tss_trap;
  uint16_t tss_iomap_base;
} TaskState;

__BEGIN_DECLS

void tss_write (uint32_t n, uint16_t ss0, uint32_t esp0);
void tss_load (void);

__END_DECLS

#endif
