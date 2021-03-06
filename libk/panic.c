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

void print_registers (void);
void halt (void) __attribute__ ((noreturn));

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
