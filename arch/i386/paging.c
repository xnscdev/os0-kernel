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

uint32_t page_dir[PAGE_DIR_SIZE];
uint32_t page_table[PAGE_TBL_SIZE][PAGE_DIR_SIZE];

void
paging_init (void)
{
  uint32_t addr;
  int i;

  /* Fill page directory */
  for (i = 0; i < PAGE_DIR_SIZE; i++)
    page_dir[i] = ((uint32_t) page_table[i] - RELOC_VADDR) | PAGE_FLAG_WRITE
      | PAGE_FLAG_PRESENT;

  /* Map low memory + kernel to RELOC_VADDR */
  for (i = 0, addr = 0; addr < RELOC_LEN; i++, addr += PAGE_SIZE)
    map_page (addr + RELOC_PADDR, addr + RELOC_VADDR, PAGE_FLAG_WRITE);

  paging_loaddir ((uint32_t) page_dir - RELOC_VADDR);
}

void *
get_paddr (void *vaddr)
{
  uint32_t pdi = (uint32_t) vaddr >> 22;
  uint32_t pti = (uint32_t) vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  uint32_t *table = (uint32_t *) ((page_dir[pdi] & 0xfffff000) + RELOC_VADDR);
  return (void *) ((table[pti] & 0xfffff000) + ((uint32_t) vaddr & 0xfff));
}

void
map_page (uint32_t paddr, uint32_t vaddr, uint32_t flags)
{
  uint32_t pdi = vaddr >> 22;
  uint32_t pti = vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  uint32_t *table = (uint32_t *) ((page_dir[pdi] & 0xfffff000) + RELOC_VADDR);
  table[pti] = paddr | PAGE_FLAG_PRESENT | (flags & 0xfff);
}
