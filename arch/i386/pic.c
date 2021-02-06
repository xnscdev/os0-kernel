/*************************************************************************
 * pic.c -- This file is part of OS/0.                                   *
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
#include <i386/pic.h>
#include <sys/io.h>

static IDTEntry idt_entries[IDT_SIZE];
static DTPtr idt;

#define EXC(x) void exc ## x (void);
#define IRQ(x) void irq ## x (void);
#include "irq.inc"
#undef EXC
#undef IRQ

void
idt_init (void)
{
#define EXC(x) u32 exc ## x ## _addr;
#define IRQ(x) u32 irq ## x ## _addr;
#include "irq.inc"
#undef EXC
#undef IRQ

  /* Remap PIC */
  outb (0x11, PIC_MASTER_COMMAND);
  outb (0x11, PIC_SLAVE_COMMAND);
  outb (0x20, PIC_MASTER_DATA);
  outb (0x28, PIC_SLAVE_DATA);
  outb (4, PIC_MASTER_DATA);
  outb (2, PIC_SLAVE_DATA);
  outb (1, PIC_MASTER_DATA);
  outb (1, PIC_SLAVE_DATA);
  outb (0, PIC_MASTER_DATA);
  outb (0, PIC_SLAVE_DATA);

  idt.dp_limit = sizeof (IDTEntry) * IDT_SIZE - 1;
  idt.dp_base = (u32) &idt_entries;

#define EXC(x) exc ## x ## _addr = (u32) exc ## x;			\
  idt_entries[x].ie_basel = exc ## x ## _addr & 0xffff;			\
  idt_entries[x].ie_sel = 0x08;						\
  idt_entries[x].ie_reserved = 0;					\
  idt_entries[x].ie_flags = 0x8e;					\
  idt_entries[x].ie_baseh = (exc ## x ## _addr & 0xffff0000) >> 16;
#define IRQ(x) irq ## x ## _addr = (u32) irq ## x;			\
  idt_entries[x + 32].ie_basel = irq ## x ## _addr & 0xffff;		\
  idt_entries[x + 32].ie_sel = 0x08;					\
  idt_entries[x + 32].ie_reserved = 0;					\
  idt_entries[x + 32].ie_flags = 0x8e;					\
  idt_entries[x + 32].ie_baseh = (irq ## x ## _addr & 0xffff0000) >> 16;
#include "irq.inc"
#undef EXC
#undef IRQ

  idt_load ((u32) &idt);
}
