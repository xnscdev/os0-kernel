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

#include <i386/paging.h>
#include <libk/types.h>
#include <sys/memory.h>
#include <vm/paging.h>

extern void *_kernel_end;

static u32 page_dir[PAGE_DIR_SIZE] __attribute__ ((aligned (MEM_PAGESIZE)));
static u32 page_table0[PAGE_TBL_SIZE] __attribute__ ((aligned (MEM_PAGESIZE)));

void
paging_init (void)
{
  int i;
  page_dir[0] = (u32) page_table0 | PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT;
  for (i = 1; i < PAGE_DIR_SIZE; i++)
    page_dir[i] = 2;
  /* Identity map the first 4 MiB */
  for (i = 0; i < PAGE_TBL_SIZE; i++)
    page_table0[i] = (i * MEM_PAGESIZE) | PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT;
  paging_loaddir ((u32) page_dir);
  paging_enable ();
}

void *
get_paddr (void *vaddr)
{
  u32 pdi = (u32) vaddr >> 22;
  u32 pti = (u32) vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  u32 *table;
  if (!(page_dir[pdi] & PAGE_FLAG_PRESENT))
    return NULL;
  table = (u32 *) (page_dir[pdi] & 0xfffff000);
  return (void *) ((table[pti] & 0xfffff000) + ((u32) vaddr & 0xfff));
}

void
map_page (void *paddr, void *vaddr, u32 flags)
{
  u32 pdi = (u32) vaddr >> 22;
  u32 pti = (u32) vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  u32 *table;
  if (!(page_dir[pdi] & PAGE_FLAG_PRESENT))
    {
      u32 addr = (u32) mem_alloc (sizeof (u32) * PAGE_TBL_SIZE, 0);
      if (addr == 0)
	return; /* TODO Cause a kernel panic */
      /* Should the page be writeable? */
      page_dir[pdi] = addr | PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT;
    }
  table = (u32 *) (page_dir[pdi] & 0xfffff000);
  /* TODO Check if table entry is present */
  table[pti] = (u32) paddr | PAGE_FLAG_PRESENT | (flags & 0xfff);
}
