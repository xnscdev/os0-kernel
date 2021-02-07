/*************************************************************************
 * panic.c -- This file is part of OS/0.                                 *
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

#include <libk/libk.h>

void halt (void) __attribute__ ((noreturn));

static void
print_registers (void)
{
  u32 eax;
  u32 ecx;
  u32 edx;
  u32 ebx;
  u32 esi;
  u32 edi;
  __asm__ volatile ("mov %%eax, %0" : "=r" (eax));
  __asm__ volatile ("mov %%ecx, %0" : "=r" (ecx));
  __asm__ volatile ("mov %%edx, %0" : "=r" (edx));
  __asm__ volatile ("mov %%ebx, %0" : "=r" (ebx));
  __asm__ volatile ("mov %%esi, %0" : "=r" (esi));
  __asm__ volatile ("mov %%edi, %0" : "=r" (edi));
  printk ("\nEAX 0x%x  ECX 0x%x  EDX 0x%x\nEBX 0x%x  ESI 0x%x  EDI 0x%x",
	  eax, ecx, edx, ebx, esi, edi);
}

void
panic (const char *__restrict fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  printk ("\n============[ KERNEL PANIC ]============\n");
  vprintk (fmt, args);
  print_registers ();
  printk ("\n==========[ END KERNEL PANIC ]==========\n");
  va_end (args);
  halt ();
}
