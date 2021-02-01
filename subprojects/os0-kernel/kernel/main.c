/*************************************************************************
 * main.c -- This file is part of OS/0.                                  *
 * Copyright (C) 2020 XNSC                                               *
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
#include <sys/memory.h>
#include <sys/multiboot.h>
#include <video/vga.h>

extern void *_kernel_end;

static void
pass (void)
{
  u32 addr = (u32) &_kernel_end;
  struct MemoryHeader *header = (struct MemoryHeader *) (addr - 0x10);
  while (header->mh_magic == MEMORY_MAGIC)
    {
      printk ("Block at 0x%x: order %d, allocated %d\n", addr, header->mh_order,
	      header->mh_alloc);
      addr += 1 << (header->mh_order + 12);
      header = (struct MemoryHeader *) (addr - 0x10);
    }
}

void
main (struct MultibootInfo *info)
{
  vga_init ();
  assert (info->mi_flags & MULTIBOOT_FLAG_MEMORY);
  memory_init (info->mi_memhigh);
  kmalloc (0x1000, 0);
  pass ();
}
