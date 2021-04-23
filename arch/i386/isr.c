/*************************************************************************
 * isr.c -- This file is part of OS/0.                                   *
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
#include <i386/timer.h>
#include <sys/ata.h>
#include <sys/io.h>
#include <stdlib.h>

void
exc0_handler (void)
{
  panic ("CPU Exception: Divide-by-zero Fault");
}

void
exc1_handler (void)
{
  panic ("CPU Exception: Debug Trap");
}

void
exc2_handler (void)
{
  panic ("CPU Exception: Non-maskable Interrupt");
}

void
exc3_handler (void)
{
  panic ("CPU Exception: Breakpoint Trap");
}

void
exc4_handler (void)
{
  panic ("CPU Exception: Overflow Trap");
}

void
exc5_handler (void)
{
  panic ("CPU Exception: Bound Range Exceeded");
}

void
exc6_handler (void)
{
  panic ("CPU Exception: Invalid Opcode");
}

void
exc7_handler (void)
{
  panic ("CPU Exception: Device Not Available");
}

void
exc8_handler (void)
{
  panic ("CPU Exception: Double Fault");
}

void
exc10_handler (void)
{
  panic ("CPU Exception: Invalid TSS");
}

void
exc11_handler (void)
{
  panic ("CPU Exception: Segment Not Present");
}

void
exc12_handler (void)
{
  panic ("CPU Exception: Stack-Segment Fault");
}

void
exc13_handler (void)
{
  panic ("CPU Exception: General Protection Fault");
}

void
exc14_handler (void)
{
  uint32_t addr;
  __asm__ volatile ("mov %%cr2, %0" : "=r" (addr));
  panic ("CPU Exception: Page Fault\nFault address: 0x%lx", addr);
}

void
exc16_handler (void)
{
  panic ("CPU Exception: x87 Floating-Point Exception");
}

void
exc17_handler (void)
{
  panic ("CPU Exception: Alignment Check");
}

void
exc18_handler (void)
{
  panic ("CPU Exception: Machine Check");
}

void
exc19_handler (void)
{
  panic ("CPU Exception: SIMD Floating-Point Exception");
}

void
exc20_handler (void)
{
  panic ("CPU Exception: Virtualization Exception");
}

void
exc30_handler (void)
{
  panic ("CPU Exception: Security Exception");
}

void
irq0_handler (void)
{
  timer_tick ();
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq1_handler (void)
{
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq2_handler (void)
{
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq3_handler (void)
{
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq4_handler (void)
{
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq5_handler (void)
{
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq6_handler (void)
{
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq7_handler (void)
{
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq9_handler (void)
{
  outb (PIC_EOI, PIC_SLAVE_COMMAND);
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq10_handler (void)
{
  outb (PIC_EOI, PIC_SLAVE_COMMAND);
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq11_handler (void)
{
  outb (PIC_EOI, PIC_SLAVE_COMMAND);
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq12_handler (void)
{
  outb (PIC_EOI, PIC_SLAVE_COMMAND);
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq13_handler (void)
{
  outb (PIC_EOI, PIC_SLAVE_COMMAND);
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq14_handler (void)
{
  ide_irq = 1;
  outb (PIC_EOI, PIC_SLAVE_COMMAND);
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq15_handler (void)
{
  ide_irq = 1;
  outb (PIC_EOI, PIC_SLAVE_COMMAND);
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}
