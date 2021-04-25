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

static char page_empty [PAGE_SIZE];

extern uint32_t _kernel_heap_start;
extern uint32_t _kernel_heap_end;

uint32_t kernel_page_dir[PAGE_DIR_SIZE];
uint32_t kernel_page_table[PAGE_DIR_SIZE][PAGE_TBL_SIZE];
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
  for (i = 0, addr = 0; addr <= RELOC_LEN; i++, addr += PAGE_SIZE)
    {
      uint32_t vaddr = addr + RELOC_VADDR;
      uint32_t pdi = vaddr >> 22;
      uint32_t pti = vaddr >> 12 & (PAGE_DIR_SIZE - 1);
      uint32_t *table =
	(uint32_t *) ((kernel_page_dir[pdi] & 0xfffff000) + RELOC_VADDR);
      table[pti] = addr | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
    }

  paging_loaddir ((uint32_t) kernel_page_dir - RELOC_VADDR);

  /* Map last page table to page directory virtual addresses */
  kernel_page_dir[PAGE_DIR_SIZE - 1] =
    (uint32_t) kernel_page_table[PAGE_DIR_SIZE - 1];
  for (i = 0; i < PAGE_TBL_SIZE; i++)
    kernel_page_table[PAGE_DIR_SIZE - 1][i] = (uint32_t) kernel_page_table[i];
}

uint32_t
get_paddr (uint32_t *dir, void *vaddr)
{
  uint32_t pdi = (uint32_t) vaddr >> 22;
  uint32_t pti = (uint32_t) vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  uint32_t *table;
  if (!(dir[pdi] & PAGE_FLAG_PRESENT))
    return 0;
  table = (uint32_t *) ((uint32_t *) dir[PAGE_DIR_SIZE - 1])[pdi];
  return (table[pti] & 0xfffff000) + ((uint32_t) vaddr & 0xfff);
}

void
map_page (uint32_t *dir, uint32_t paddr, uint32_t vaddr, uint32_t flags)
{
  uint32_t pdi = vaddr >> 22;
  uint32_t pti = vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  uint32_t *table;
  if (dir[pdi] & PAGE_FLAG_PRESENT)
    table = (uint32_t *) ((uint32_t *) dir[PAGE_DIR_SIZE - 1])[pdi];
  else
    {
      table = kvalloc (PAGE_TBL_SIZE << 2);
      if (unlikely (table == NULL))
        panic ("Failed to allocate page table");
      memset (table, 0, PAGE_TBL_SIZE << 2);
      dir[pdi] = get_paddr (curr_page_dir, table) | PAGE_FLAG_WRITE
	| PAGE_FLAG_USER | PAGE_FLAG_PRESENT;
      ((uint32_t *) dir[PAGE_DIR_SIZE - 1])[pdi] = (uint32_t) table;
    }
  table[pti] = paddr | PAGE_FLAG_PRESENT | (flags & 0xfff);
}

void
unmap_page (uint32_t *dir, uint32_t vaddr)
{
  uint32_t pdi = vaddr >> 22;
  uint32_t pti = vaddr >> 12 & (PAGE_DIR_SIZE - 1);
  if (dir[pdi] & PAGE_FLAG_PRESENT)
    {
      uint32_t *table = (uint32_t *) dir[PAGE_DIR_SIZE - 1];
      ((uint32_t *) table[pdi])[pti] = 0;
    }
}

uint32_t *
page_table_clone (uint32_t index, uint32_t *orig)
{
  int i;
  uint32_t *table = kvalloc (PAGE_TBL_SIZE << 2);
  if (unlikely (table == NULL))
    return NULL;
  for (i = 0; i < PAGE_TBL_SIZE; i++)
    {
      uint32_t vaddr = (index << 22) | (i << 12);
      void *buffer;
      if (!(orig[i] & PAGE_FLAG_PRESENT))
	{
	  table[i] = 0;
	  continue;
	}

      /* If cloning kernel code or heap, link instead of copy */
      if (vaddr < TASK_LOCAL_BOUND)
	{
	  table[i] = orig[i];
	  continue;
	}

      /* Allocate and copy page contents */
      buffer = mem_alloc (PAGE_SIZE, 0);
      if (unlikely (buffer == NULL))
	{
	  kfree (table);
	  table = NULL;
	  goto end;
	}
      map_page (curr_page_dir, (uint32_t) buffer, PAGE_COPY_VADDR,
		PAGE_FLAG_WRITE);
#ifdef INVLPG_SUPPORT
      vm_page_inval (PAGE_COPY_VADDR);
#else
      vm_tlb_reset ();
#endif
      memcpy ((void *) PAGE_COPY_VADDR, (const void *) vaddr, PAGE_SIZE);
      table[i] = (uint32_t) buffer | (orig[i] & 0xfff);
    }

 end:
  unmap_page (curr_page_dir, PAGE_COPY_VADDR);
#ifdef INVLPG_SUPPORT
  vm_page_inval (PAGE_COPY_VADDR);
#else
  vm_tlb_reset ();
#endif
  return table;
}

uint32_t *
page_dir_clone (uint32_t *orig)
{
  int i;
  uint32_t *dir = kvalloc (PAGE_DIR_SIZE << 2);
  uint32_t *vmap;
  if (unlikely (dir == NULL))
    return NULL;
  memset (dir, 0, PAGE_DIR_SIZE << 2);
  vmap = kvalloc (PAGE_TBL_SIZE << 2);
  if (unlikely (vmap == NULL))
    {
      kfree (dir);
      return NULL;
    }
  dir[PAGE_DIR_SIZE - 1] = (uint32_t) vmap;
  for (i = 0; i < PAGE_DIR_SIZE - 1; i++)
    {
      uint32_t *vtable;
      if (!(orig[i] & PAGE_FLAG_PRESENT))
	{
	  vmap[i] = 0;
	  continue;
	}
      vtable = (uint32_t *) orig[PAGE_DIR_SIZE - 1];
      if (memcmp ((const void *) vtable[i], page_empty, PAGE_SIZE) == 0)
	{
	  /* Table has no present entries, no need to copy */
	  dir[i] = 0;
	  vmap[i] = 0;
	}
      else
	{
	  vmap[i] = (uint32_t) page_table_clone (i, (uint32_t *) vtable[i]);
	  if (vmap[i] == 0)
	    {
	      page_dir_free (dir);
	      return NULL;
	    }
	  dir[i] = get_paddr (curr_page_dir, (void *) vmap[i])
	    | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_USER;
	}
    }
  return dir;
}

void
page_dir_free (uint32_t *dir)
{
  uint32_t *vmap = (uint32_t *) dir[PAGE_DIR_SIZE - 1];
  int i;
  for (i = 0; i < PAGE_DIR_SIZE - 1; i++)
    {
      /* If page table is on kernel heap, assume it's allocated and free it */
      if (vmap[i] >= kernel_heap->mh_addr
	  && vmap[i] < kernel_heap->mh_addr + kernel_heap->mh_size)
	kfree ((uint32_t *) vmap[i]);
      dir[i] = 0;
      vmap[i] = 0;
    }
  kfree (dir);
}
