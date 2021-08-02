/*************************************************************************
 * pic.h -- This file is part of OS/0.                                   *
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

#ifndef _I386_PIC_H
#define _I386_PIC_H

#ifndef _ASM
#include <i386/gdt.h>
#endif

#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA    0x21
#define PIC_SLAVE_COMMAND  0xa0
#define PIC_SLAVE_DATA     0xa1

#define PIC_EOI 0x20

#define IDT_GATE_TASK 0x5
#define IDT_GATE_INT  0xe
#define IDT_GATE_TRAP 0xf

#define IDT_SIZE 256

#ifndef _ASM

typedef struct
{
  uint16_t ie_basel;
  uint16_t ie_sel;
  unsigned char ie_reserved;
  unsigned char ie_flags;
  uint16_t ie_baseh;
} __attribute__ ((packed)) IDTEntry;

__BEGIN_DECLS

static inline void
idt_load (DTPtr *addr)
{
  __asm__ volatile ("lidt (%0)" :: "r" (addr));
}

__END_DECLS

#endif

#endif
