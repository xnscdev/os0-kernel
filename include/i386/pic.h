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

#include <libk/types.h>
#include <sys/cdefs.h>

#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA    0x21
#define PIC_SLAVE_COMMAND  0xa0
#define PIC_SLAVE_DATA     0xa1

#define PIC_EOI 0x20

#define IDT_SIZE 256

typedef struct
{
  u16 ie_basel;
  u16 ie_sel;
  u8 ie_reserved;
  u8 ie_flags;
  u16 ie_baseh;
} __attribute__ ((packed)) IDTEntry;

__BEGIN_DECLS

void idt_load (u32 addr);

__END_DECLS

#endif
