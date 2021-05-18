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

#include <i386/paging.h>
#include <i386/pic.h>
#include <i386/timer.h>
#include <sys/ata.h>
#include <sys/io.h>
#include <stdlib.h>

void
exc0_handler (uint32_t eip)
{
  panic ("CPU Exception: Divide-by-zero Fault\nException address: 0x%lx", eip);
}

void
exc1_handler (uint32_t eip)
{
  panic ("CPU Exception: Debug Trap\nException address: 0x%lx", eip);
}

void
exc2_handler (uint32_t eip)
{
  panic ("CPU Exception: Non-maskable Interrupt\nException address: 0x%lx",
	 eip);
}

void
exc3_handler (uint32_t eip)
{
  panic ("CPU Exception: Breakpoint Trap\nException address: 0x%lx", eip);
}

void
exc4_handler (uint32_t eip)
{
  panic ("CPU Exception: Overflow Trap\nException address: 0x%lx", eip);
}

void
exc5_handler (uint32_t eip)
{
  panic ("CPU Exception: Bound Range Exceeded\nException address: 0x%lx", eip);
}

void
exc6_handler (uint32_t eip)
{
  panic ("CPU Exception: Invalid Opcode\nException address: 0x%lx", eip);
}

void
exc7_handler (uint32_t eip)
{
  panic ("CPU Exception: Device Not Available\nException address: 0x%lx", eip);
}

void
exc8_handler (uint32_t err, uint32_t eip)
{
  panic ("CPU Exception: Double Fault\nException address: 0x%lx", eip);
}

void
exc10_handler (uint32_t err, uint32_t eip)
{
  panic ("CPU Exception: Invalid TSS\nSegment selector: 0x%lx\n"
	 "Exception address: 0x%lx", err, eip);
}

void
exc11_handler (uint32_t err, uint32_t eip)
{
  panic ("CPU Exception: Segment Not Present\nSegment selector: 0x%lx\n"
	 "Exception address: 0x%lx", err, eip);
}

void
exc12_handler (uint32_t err, uint32_t eip)
{
  panic ("CPU Exception: Stack-Segment Fault\nSegment selector: 0x%lx\n"
	 "Exception address: 0x%lx", err, eip);
}

void
exc13_handler (uint32_t err, uint32_t eip)
{
  panic ("CPU Exception: General Protection Fault\nSegment selector: 0x%lx\n"
	 "Exception address: 0x%lx", err, eip);
}

void
exc14_handler (uint32_t err, uint32_t eip)
{
  uint32_t addr;
  __asm__ volatile ("mov %%cr2, %0" : "=r" (addr));
  panic ("CPU Exception: Page Fault\nAttributes: %s, %s, %s%s%s\n"
	 "Fault address: 0x%lx\nException address: 0x%lx",
	 err & PF_FLAG_PROT ? "protection violation" : "non-present",
	 err & PF_FLAG_WRITE ? "write access" : "read access",
	 err & PF_FLAG_USER ? "user mode" : "kernel mode",
	 err & PF_FLAG_RESERVED ? ", reserved entries" : "",
	 err & PF_FLAG_INSTFETCH ? ", instruction fetch" : "", addr, eip);
}

void
exc16_handler (uint32_t eip)
{
  panic ("CPU Exception: x87 Floating-Point Exception\n"
	 "Exception address: 0x%lx", eip);
}

void
exc17_handler (uint32_t err, uint32_t eip)
{
  panic ("CPU Exception: Alignment Check\nException address: 0x%lx", eip);
}

void
exc18_handler (uint32_t eip)
{
  panic ("CPU Exception: Machine Check\nException address: 0x%lx", eip);
}

void
exc19_handler (uint32_t eip)
{
  panic ("CPU Exception: SIMD Floating-Point Exception\n"
	 "Exception address: 0x%lx", eip);
}

void
exc20_handler (uint32_t eip)
{
  panic ("CPU Exception: Virtualization Exception\nException address: 0x%lx",
	 eip);
}

void
exc30_handler (uint32_t err, uint32_t eip)
{
  panic ("CPU Exception: Security Exception\nException address: 0x%lx", eip);
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
