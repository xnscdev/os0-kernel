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

static int
heap_search_hole (MemHeap *heap, u32 minsize, u8 aligned)
{
  u32 i = 0;
  while (i < heap->mh_index.sa_size)
    {
      MemHeader *header =
	(MemHeader *) sorted_array_lookup (&heap->mh_index, i);
      if (aligned)
	{
	  u32 addr = (u32) header;
	  int offset = 0;
	  if ((addr + sizeof (MemHeader)) & 0xfffff000)
	    offset = PAGE_SIZE - (addr + sizeof (MemHeader)) % PAGE_SIZE;
	  if (header->mh_size - offset >= minsize)
	    break;
	}
      else if (header->mh_size >= minsize)
	break;
      i++;
    }
  return i == heap->mh_index.sa_size ? -1 : i;
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

void *
heap_alloc (MemHeap *heap, u32 size, u8 aligned)
{
  u32 realsize = size + sizeof (MemHeader) + sizeof (MemFooter);
  MemHeader *header;
  MemHeader *block_header;
  MemFooter *block_footer;
  u32 addr;
  u32 holesize;
  int i = heap_search_hole (heap, size, aligned);

  if (i == -1)
    return NULL;

  header = (MemHeader *) sorted_array_lookup (&heap->mh_index, i);
  addr = (u32) header;
  holesize = header->mh_size;
  if (holesize - realsize < sizeof (MemHeader) + sizeof (MemFooter))
    {
      size += holesize - realsize;
      realsize = holesize;
    }
  if (aligned && addr & 0xfffff000)
    {
      u32 newaddr =
	addr + PAGE_SIZE - (addr & (PAGE_SIZE - 1)) - sizeof (MemHeader);
      MemFooter *footer = (MemFooter *) (newaddr - sizeof (MemFooter));

      header->mh_magic = MEM_MAGIC;
      header->mh_size = PAGE_SIZE - (addr & (PAGE_SIZE - 1)) -
	sizeof (MemHeader);
      header->mh_alloc = 0;

      footer->mf_cigam = MEM_CIGAM;
      footer->mf_header = (u32) header;

      addr = newaddr;
      holesize -= header->mh_size;
    }
  else
    sorted_array_remove (&heap->mh_index, i);

  block_header = (MemHeader *) addr;
  block_header->mh_magic = MEM_MAGIC;
  block_header->mh_size = realsize;
  block_header->mh_alloc = 1;

  block_footer = (MemFooter *) (addr + size + sizeof (MemHeader));
  block_footer->mf_cigam = MEM_CIGAM;
  block_footer->mf_header = (u32) block_header;

  if (holesize - realsize > 0)
    {
      MemHeader *hole_header =
	(MemHeader *) (addr + size + sizeof (MemHeader) + sizeof (MemFooter));
      MemFooter *hole_footer =
	(MemFooter *) ((u32) hole_header + holesize - realsize -
		       sizeof (MemFooter));

      hole_header->mh_magic = MEM_MAGIC;
      hole_header->mh_size = holesize - realsize;
      hole_header->mh_alloc = 0;

      if ((u32) hole_footer < heap->mh_addr + heap->mh_size)
	{
	  hole_footer->mf_cigam = MEM_CIGAM;
	  hole_footer->mf_header = (u32) hole_header;
	}

      sorted_array_insert (&heap->mh_index, hole_header);
    }
  return (void *) ((u32) block_header + sizeof (MemHeader));
}

void
heap_free (MemHeap *heap, void *ptr)
{
  if (ptr == NULL)
    return;
}

void
heap_init (void)
{
  if (heap_new (&kernel_heap, (void *) KERNEL_HEAP_ADDR, KERNEL_HEAP_INDEX,
		KERNEL_HEAP_SIZE, 1, 0) != 0)
    panic ("Failed to create kernel heap");
}
