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
#include <limits.h>

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

void
free_page (uint32_t addr)
{
  stack_push (page_stack, (void *) (addr & 0xfffff000));
}
