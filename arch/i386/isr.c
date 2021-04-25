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
exc0_handler (uint32_t eip)
{
  panic ("CPU Exception: Divide-by-zero Fault\nEIP 0x%lx", eip);
}

void
exc1_handler (uint32_t eip)
{
  panic ("CPU Exception: Debug Trap\nEIP 0x%lx", eip);
}

void
exc2_handler (uint32_t eip)
{
  panic ("CPU Exception: Non-maskable Interrupt\nEIP 0x%lx", eip);
}

void
exc3_handler (uint32_t eip)
{
  panic ("CPU Exception: Breakpoint Trap\nEIP 0x%lx", eip);
}

void
exc4_handler (uint32_t eip)
{
  panic ("CPU Exception: Overflow Trap\nEIP 0x%lx", eip);
}

void
exc5_handler (uint32_t eip)
{
  panic ("CPU Exception: Bound Range Exceeded\nEIP 0x%lx", eip);
}

void
exc6_handler (uint32_t eip)
{
  panic ("CPU Exception: Invalid Opcode\nEIP 0x%lx", eip);
}

void
exc7_handler (uint32_t eip)
{
  panic ("CPU Exception: Device Not Available\nEIP 0x%lx", eip);
}

void
exc8_handler (uint32_t eip)
{
  panic ("CPU Exception: Double Fault\nEIP 0x%lx", eip);
}

void
exc10_handler (uint32_t eip)
{
  panic ("CPU Exception: Invalid TSS\nEIP 0x%lx", eip);
}

void
exc11_handler (uint32_t eip)
{
  panic ("CPU Exception: Segment Not Present\nEIP 0x%lx", eip);
}

void
exc12_handler (uint32_t eip)
{
  panic ("CPU Exception: Stack-Segment Fault\nEIP 0x%lx", eip);
}

void
exc13_handler (uint32_t eip)
{
  panic ("CPU Exception: General Protection Fault\nEIP 0x%lx", eip);
}

void
exc14_handler (uint32_t eip)
{
  uint32_t addr;
  __asm__ volatile ("mov %%cr2, %0" : "=r" (addr));
  panic ("CPU Exception: Page Fault\nCR2 0x%lx\nEIP 0x%lx", addr, eip);
}

void
exc16_handler (uint32_t eip)
{
  panic ("CPU Exception: x87 Floating-Point Exception\nEIP 0x%lx", eip);
}

void
exc17_handler (uint32_t eip)
{
  panic ("CPU Exception: Alignment Check\nEIP 0x%lx", eip);
}

void
exc18_handler (uint32_t eip)
{
  panic ("CPU Exception: Machine Check\nEIP 0x%lx", eip);
}

void
exc19_handler (uint32_t eip)
{
  panic ("CPU Exception: SIMD Floating-Point Exception\nEIP 0x%lx", eip);
}

void
exc20_handler (uint32_t eip)
{
  panic ("CPU Exception: Virtualization Exception\nEIP 0x%lx", eip);
}

void
exc30_handler (uint32_t eip)
{
  panic ("CPU Exception: Security Exception\nEIP 0x%lx", eip);
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
