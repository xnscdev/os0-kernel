/*************************************************************************
 * heap.c -- This file is part of OS/0.                                  *
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
#include <vm/heap.h>
#include <vm/paging.h>

MemHeap kernel_heap;

static int
heap_cmp (const void *a, const void *b)
{
  return ((MemHeader *) a)->mh_size < ((MemHeader *) b)->mh_size;
}

int
heap_new (MemHeap *heap, void *vaddr, u32 indexsize, u32 heapsize,
	  u8 supervisor, u8 readonly)
{
  void *indexaddr;
  u32 addr;

  /* Page-align indexsize and heapsize */
  if (indexsize & (PAGE_SIZE - 1))
    {
      indexsize &= 0xfffff000;
      indexsize += PAGE_SIZE;
    }
  if (heapsize & (PAGE_SIZE - 1))
    {
      heapsize &= 0xfffff000;
      heapsize += PAGE_SIZE;
    }

  indexaddr = mem_alloc (sizeof (void *) * indexsize, 0);
  if (indexaddr == NULL)
    return -1;
  /* Identity map the index array */
  for (addr = 0; addr < sizeof (void *) * indexsize; addr += PAGE_SIZE)
    map_page (addr + (u32) indexaddr, addr + (u32) indexaddr, PAGE_FLAG_WRITE);
  if (sorted_array_place (&heap->mh_index, indexaddr, indexsize, heap_cmp) != 0)
    return -1;

  heap->mh_addr = (u32) vaddr;
  heap->mh_size = heapsize;
  heap->mh_supvsr = supervisor;
  heap->mh_rdonly = readonly;
  return 0;
}

void
heap_init (void)
{
  if (heap_new (&kernel_heap, (void *) KERNEL_HEAP_ADDR, KERNEL_HEAP_INDEX,
		KERNEL_HEAP_SIZE, 1, 0) != 0)
    panic ("Failed to create kernel heap");
}
