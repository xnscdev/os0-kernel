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
#include <vm/heap.h>

uint32_t kernel_page_dir[PAGE_DIR_SIZE];
uint32_t kernel_page_table[PAGE_TBL_SIZE][PAGE_DIR_SIZE];
uint32_t *curr_page_dir;

void
paging_init (void)
{
  uint32_t addr;
  int i;

  /* Use kernel page directory */
  curr_page_dir = kernel_page_dir;

  /* Fill page directory */
  for (i = 0; i < PAGE_DIR_SIZE; i++)
    kernel_page_dir[i] = ((uint32_t) kernel_page_table[i] - RELOC_VADDR)
      | PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT | PAGE_FLAG_USER;

  /* Map low memory + kernel to RELOC_VADDR */
  for (i = 0, addr = 0; addr < RELOC_LEN; i++, addr += PAGE_SIZE)
    map_page (addr + RELOC_PADDR, addr + RELOC_VADDR, PAGE_FLAG_WRITE);

  paging_loaddir ((uint32_t) kernel_page_dir - RELOC_VADDR);
}

void *
get_paddr (void *vaddr)
{
  uint32_t pdi = (uint32_t) vaddr >> 22;
  uint32_t pti = (uint32_t) vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  uint32_t *table =
    (uint32_t *) ((curr_page_dir[pdi] & 0xfffff000) + RELOC_VADDR);
  return (void *) ((table[pti] & 0xfffff000) + ((uint32_t) vaddr & 0xfff));
}

void
map_page (uint32_t paddr, uint32_t vaddr, uint32_t flags)
{
  uint32_t pdi = vaddr >> 22;
  uint32_t pti = vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  uint32_t *table =
    (uint32_t *) ((curr_page_dir[pdi] & 0xfffff000) + RELOC_VADDR);
  table[pti] = paddr | PAGE_FLAG_PRESENT | (flags & 0xfff);
}

uint32_t *
page_table_clone (uint32_t *orig)
{
  int i;
  uint32_t *table = kvalloc (PAGE_TBL_SIZE << 2);
  if (unlikely (table == NULL))
    return NULL;
  memset (table, 0, PAGE_TBL_SIZE << 2);
  for (i = 0; i < PAGE_TBL_SIZE; i++)
    {
      void *buffer;
      if (orig[i] == 0)
	continue;
      buffer = mem_alloc (PAGE_SIZE, 0);
      if (unlikely (buffer == NULL))
	{
	  kfree (table);
	  return NULL;
	}
      map_page ((uint32_t) buffer, PAGE_COPY_VADDR, PAGE_FLAG_WRITE);
#ifdef INVLPG_SUPPORT
      vm_page_inval ((void *) PAGE_COPY_VADDR);
#else
      vm_tlb_reset ();
#endif
      memcpy ((void *) PAGE_COPY_VADDR, (const void *) (orig[i] & 0xfffff000),
	      PAGE_SIZE);
#ifdef INVLPG_SUPPORT
      vm_page_inval ((void *) PAGE_COPY_VADDR);
#else
      vm_tlb_reset ();
#endif
      table[i] = (uint32_t) buffer | (orig[i] & 0xfff);
    }
  return table;
}

uint32_t *
page_dir_clone (uint32_t *orig)
{
  int i;
  uint32_t *dir = kvalloc (PAGE_DIR_SIZE << 2);
  if (unlikely (dir == NULL))
    return NULL;
  memset (dir, 0, PAGE_DIR_SIZE << 2);
  for (i = 0; i < PAGE_DIR_SIZE; i++)
    {
      if (orig[i] == 0)
	continue;
      if (curr_page_dir[i] == orig[i])
        dir[i] = orig[i];
      else
	dir[i] = (uint32_t) page_table_clone ((uint32_t *) orig[i])
	  | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_USER;
    }
  return dir;
}
