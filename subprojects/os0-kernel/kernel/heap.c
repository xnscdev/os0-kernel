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

#include <vm/heap.h>

static int
heap_cmp (const void *a, const void *b)
{
  return ((const MemHeader *) a)->mh_size < ((const MemHeader *) b)->mh_size;
}

MemHeap *
heap_new (u32 indexsize, u32 heapsize, u32 maxsize, u8 supervisor, u8 readonly)
{
  MemHeap *heap = mem_alloc (sizeof (MemHeap) + indexsize + maxsize, 0);
  MemHeader *hole;
  u32 start;

  if (heap == NULL)
    return NULL;

  if (sorted_array_place (&heap->mh_index, heap_get_index (heap), indexsize,
			  heap_cmp) != 0)
    {
      mem_free (heap, sizeof (MemHeap) + indexsize + maxsize);
      return NULL;
    }

  start = (u32) heap + sizeof (void *) * indexsize;
  if (start & 0xfffff000)
    {
      start &= 0xfffff000;
      start += MEM_PAGESIZE;
    }

  heap->mh_saddr = start;
  heap->mh_eaddr = start + heapsize;
  heap->mh_maddr = start + maxsize;
  heap->mh_supvsr = supervisor;
  heap->mh_rdonly = readonly;

  hole = (MemHeader *) start;
  hole->mh_magic = MEM_MAGIC;
  hole->mh_size = heapsize;
  hole->mh_alloc = 0;
  sorted_array_insert (&heap->mh_index, hole);
  return heap;
}

void *
heap_get_index (MemHeap *heap)
{
  return (SortedArray *) ((u32) heap + sizeof (MemHeap));
}
