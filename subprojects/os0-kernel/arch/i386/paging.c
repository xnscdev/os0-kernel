/*************************************************************************
 * paging.c -- This file is part of OS/0.                                *
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
#include <sys/memory.h>
#include <vm/paging.h>

u32 page_dir[PAGE_DIR_SIZE];
u32 page_table[PAGE_TBL_SIZE][PAGE_DIR_SIZE];

void
paging_init (void)
{
  u32 addr;
  int i;

  /* Fill page directory */
  for (i = 0; i < PAGE_DIR_SIZE; i++)
    page_dir[i] = (u32) page_table[i] | PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT;

  /* Map low memory + kernel to RELOC_VADDR */
  for (i = 0, addr = 0; addr < RELOC_LEN; i++, addr += PAGE_SIZE)
    map_page ((void *) (addr + RELOC_PADDR), (void *) (addr + RELOC_VADDR),
	      PAGE_FLAG_WRITE);

  /* paging_loaddir ((u32) page_dir); */
  /* paging_enable (); */
}

void *
get_paddr (void *vaddr)
{
  u32 pdi = (u32) vaddr >> 22;
  u32 pti = (u32) vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  u32 *table = (u32 *) (page_dir[pdi] & 0xfffff000);
  return (void *) ((table[pti] & 0xfffff000) + ((u32) vaddr & 0xfff));
}

void
map_page (u32 paddr, u32 vaddr, u32 flags)
{
  u32 pdi = vaddr >> 22;
  u32 pti = vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  u32 *table = (u32 *) (page_dir[pdi] & 0xfffff000);
  table[pti] = paddr | PAGE_FLAG_PRESENT | (flags & 0xfff);
}
