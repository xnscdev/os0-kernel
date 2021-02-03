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
#include <libk/libk.h>
#include <sys/memory.h>
#include <vm/paging.h>

static u32 page_dir[PAGE_DIR_SIZE] __attribute__ ((aligned (PAGE_SIZE)));
static u32 page_table[PAGE_TBL_SIZE][PAGE_DIR_SIZE]
  __attribute__ ((aligned (PAGE_SIZE)));

void
paging_init (void)
{
  int i;
  int j;

  /* Fill page directory */
  for (i = 0; i < PAGE_DIR_SIZE; i++)
    page_dir[i] = (u32) page_table[i] | PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT;

  /* Identity mapping */
  for (i = 0; i < 2; i++)
    {
      for (j = 0; j < PAGE_TBL_SIZE; j++)
	page_table[i][j] = ((i * PAGE_DIR_SIZE + j) * PAGE_SIZE)
	  | PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT;
    }

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
  u32 *table = (u32 *) (page_dir[pdi] & 0xfffff000);
  table[pti] = (u32) paddr | PAGE_FLAG_PRESENT | (flags & 0xfff);
}
