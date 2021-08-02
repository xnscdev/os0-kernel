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

#include <i386/pic.h>
#include <sys/io.h>

static IDTEntry idt_entries[IDT_SIZE];
static DTPtr idt;

#define EXC(x) void exc ## x (void);
#define IRQ(x) void irq ## x (void);
#include "irq.inc"
#undef EXC
#undef IRQ

void syscall (void);
void task_set_fini_funcs (void);

static void
idt_set_gate (uint32_t n, uint32_t addr, uint32_t sel, uint8_t dpl,
	      uint32_t type)
{
  idt_entries[n].ie_basel = addr & 0xffff;
  idt_entries[n].ie_sel = sel;
  idt_entries[n].ie_reserved = 0;
  idt_entries[n].ie_flags = 0x80 | ((dpl & 3) << 5) | type;
  idt_entries[n].ie_baseh = (addr & 0xffff0000) >> 16;
}

void
idt_init (void)
{
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
  idt.dp_base = (uint32_t) &idt_entries;

#define EXC(x) idt_set_gate (x, (uint32_t) exc ## x, 0x08, 3, IDT_GATE_TRAP);
#define IRQ(x) idt_set_gate (x + 32, (uint32_t) irq ## x, 0x08, 3, \
			     IDT_GATE_INT);
#include "irq.inc"
#undef EXC
#undef IRQ
  idt_set_gate (0x80, (uint32_t) syscall, 0x08, 3, IDT_GATE_TRAP);
  idt_set_gate (0x81, (uint32_t) task_set_fini_funcs, 0x08, 3, IDT_GATE_TRAP);

  idt_load (&idt);
}
