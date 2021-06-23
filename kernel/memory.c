/*************************************************************************
 * memory.c -- This file is part of OS/0.                                *
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
#include <sys/memory.h>
#include <sys/process.h>
#include <limits.h>
#include <vm/paging.h>

static Stack *page_stack;
static uint32_t mem_curraddr;
static uint32_t mem_maxaddr; /* Physical address of max memory location */

void
mem_init (uint32_t mem)
{
  printk ("Detected %luK of available upper memory\n", mem);
  if (mem < 524288)
    panic ("Too little memory available, at least 512K is required");
  mem_maxaddr = mem * 1024 + KERNEL_PADDR;
  page_stack = stack_place ((void *) PAGE_STACK_VADDR, PAGE_STACK_NELEM);
  if (unlikely (page_stack == NULL))
    panic ("Failed to setup page allocation stack");
  mem_curraddr = MEM_ALLOC_START;
}

uint32_t
alloc_page (void)
{
  uint32_t addr = (uint32_t) stack_pop (page_stack);
  if (addr == 0)
    {
      addr = mem_curraddr;
      mem_curraddr += PAGE_SIZE;
    }
  return addr;
}

void *
alloc_map_page (int flags)
{
  Process *proc = &process_table[task_getpid ()];
  uint32_t paddr = alloc_page ();
  uint32_t vaddr;
  if (unlikely (paddr == 0))
    return NULL;
  while (get_paddr (curr_page_dir, (void *) proc->p_nvaddr) != 0)
    proc->p_nvaddr += PAGE_SIZE;
  vaddr = proc->p_nvaddr;
  proc->p_nvaddr += PAGE_SIZE;
  map_page (curr_page_dir, paddr, vaddr, flags);
#ifdef INVLPG_SUPPORT
  vm_page_inval (paddr);
#else
  vm_tlb_reset ();
#endif
  return (void *) vaddr;
}

void
free_page (uint32_t addr)
{
  stack_push (page_stack, (void *) (addr & 0xfffff000));
}

void
free_unmap_page (void *addr)
{
  uint32_t paddr = get_paddr (curr_page_dir, addr);
  if (unlikely (paddr == 0))
    return;
  free_page (paddr);
}
