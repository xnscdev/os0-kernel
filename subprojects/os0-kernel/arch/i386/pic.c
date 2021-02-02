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

static GDTEntry gdt_entries[GDT_SIZE];
static IDTEntry idt_entries[IDT_SIZE];
static DTPtr gdt;
static DTPtr idt;

#define IRQ(x) void irq ## x (void);
#include "irq.inc"
#undef IRQ

static void
gdt_init (void)
{
  gdt.dp_limit = sizeof (GDTEntry) * GDT_SIZE - 1;
  gdt.dp_base = (u32) &gdt_entries;

  gdt_set_gate (0, 0, 0, 0, 0); /* Null segment */
  gdt_set_gate (1, 0, 0xffffffff, 0x9a, 0xcf); /* Code segment */
  gdt_set_gate (2, 0, 0xffffffff, 0x92, 0xcf); /* Data segment */
  gdt_set_gate (3, 0, 0xffffffff, 0xfa, 0xcf); /* User mode code segment */
  gdt_set_gate (4, 0, 0xffffffff, 0xf2, 0xcf); /* User mode data segment */

  /* load_gdt ((u32) &gdt); */
}

static void
idt_init (void)
{
#define IRQ(x) u32 irq ## x ## _addr;
#include "irq.inc"
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

#define IRQ(x) irq ## x ## _addr = (u32) irq ## x;			\
  idt_entries[x + 32].ie_basel = irq ## x ## _addr & 0xffff;		\
  idt_entries[x + 32].ie_sel = 0x08;					\
  idt_entries[x + 32].ie_reserved = 0;					\
  idt_entries[x + 32].ie_flags = 0x8e;					\
  idt_entries[x + 32].ie_baseh = (irq ## x ## _addr & 0xffff0000) >> 16;
#include "irq.inc"
#undef IRQ

  /* load_idt ((u32) &idt); */
}

void
gdt_set_gate (u32 n, u32 base, u32 limit, u8 access, u8 granularity)
{
  gdt_entries[n].ge_basel = base & 0xffff;
  gdt_entries[n].ge_basem = (base >> 16) & 0xff;
  gdt_entries[n].ge_baseh = (base >> 24) & 0xff;
  gdt_entries[n].ge_liml = limit & 0xffff;
  gdt_entries[n].ge_gran = ((limit >> 16) & 0x0f) | (granularity & 0xf0);
  gdt_entries[n].ge_access = access;
}

void
pic_init (void)
{
  gdt_init ();
  idt_init ();
}
