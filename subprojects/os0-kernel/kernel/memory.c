/*************************************************************************
 * memory.c -- This file is part of OS/0.                                *
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

extern void *_kernel_end;

void
memory_init (u32 mem)
{
  u32 addr = (u32) &_kernel_end;
  mem -= (addr - 0x100000) / 1024; /* Skip kernel code */
  printk ("Detected %dK of available upper memory\n", mem);
  while (mem > 0)
    {
      struct MemoryHeader *header;
      u16 order = 0;
      u32 size;
      while (1 << (order + 3) <= mem)
	order++;
      size = 1 << (order + 2);
      mem -= size;
      header = (struct MemoryHeader *) (addr - sizeof (struct MemoryHeader));
      header->mh_magic = MEMORY_MAGIC;
      header->mh_order = order;
      header->mh_alloc = 0;
      header->mh_flags = 0;
      header->mh_reserved = 0;
      addr += size << 10;
    }
}
