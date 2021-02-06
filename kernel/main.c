/*************************************************************************
 * main.c -- This file is part of OS/0.                                  *
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

#include <kconfig.h>

#include <libk/libk.h>
#include <sys/multiboot.h>
#include <sys/timer.h>
#include <video/vga.h>
#include <vm/heap.h>
#include <vm/paging.h>

static void
splash (void)
{
  printk ("Welcome to OS/0 " VERSION "\nCopyright (C) XNSC 2021\n\n");
}

void
kmain (MultibootInfo *info)
{
  timer_set_freq (1000);
  vga_init ();
  splash ();
  assert (info->mi_flags & MULTIBOOT_FLAG_MEMORY);
  mem_init (info->mi_memhigh);
  heap_init ();
}
