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
#include <sys/io.h>

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
irq8_handler (void)
{
  outb (PIC_EOI, PIC_SLAVE_COMMAND);
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
  outb (PIC_EOI, PIC_SLAVE_COMMAND);
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}

void
irq15_handler (void)
{
  outb (PIC_EOI, PIC_SLAVE_COMMAND);
  outb (PIC_EOI, PIC_MASTER_COMMAND);
}
