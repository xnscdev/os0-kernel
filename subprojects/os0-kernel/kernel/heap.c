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

#ifdef ARCH_I386
#include <i386/paging.h>
#endif

#include <libk/libk.h>
#include <vm/heap.h>
#include <vm/paging.h>

static MemHeap kernel_heap;

static int
heap_cmp (const void *a, const void *b)
{
  return ((const MemHeader *) a)->mh_size < ((const MemHeader *) b)->mh_size;
}

int
heap_new (MemHeap *heap, void *vaddr, u32 indexsize, u32 heapsize,
	  u8 supervisor, u8 readonly)
{
  void *indexbuf;
  void *heapbuf;
  u32 flags = 0;
  u32 i;

  if (heap == NULL || vaddr == NULL || indexsize == 0 || heapsize == 0)
    return -1;

  /* Page align the index and heap sizes */
  if (indexsize & (MEM_PAGESIZE - 1))
    {
      indexsize &= 0xfffff000;
      indexsize += MEM_PAGESIZE;
    }
  if (heapsize & (MEM_PAGESIZE - 1))
    {
      heapsize &= 0xfffff000;
      heapsize += MEM_PAGESIZE;
    }

  indexbuf = mem_alloc (indexsize, 0);
  if (indexbuf == NULL)
    return -1;

  if (sorted_array_place (&heap->mh_index, indexbuf, indexsize, heap_cmp) != 0)
    {
      mem_free (indexbuf, indexsize);
      return -1;
    }

  heapbuf = mem_alloc (heapsize, 0);
  if (heapbuf == NULL)
    {
      mem_free (indexbuf, indexsize);
      return -1;
    }

#ifdef ARCH_I386
  if (!supervisor)
    flags |= PAGE_FLAG_USER;
  if (!readonly)
    flags |= PAGE_FLAG_WRITE;
  for (i = 0; i < indexsize / MEM_PAGESIZE; i++)
    {
      void *pvaddr = (void *) ((u32) vaddr + i * MEM_PAGESIZE);
      map_page ((void *) ((u32) heapbuf + i * MEM_PAGESIZE), pvaddr, flags);
#ifdef INVLPG_SUPPORT
      vm_page_inval (pvaddr);
#endif
    }
#ifndef INVLPG_SUPPORT
  vm_tlb_reset ();
#endif
#endif

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
