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

static MemHeap _kernel_heap;

MemHeap *kernel_heap;

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
  void *heapaddr;
  u32 addr;
  u32 flags = 0;
  if (!supervisor)
    flags |= PAGE_FLAG_USER;
  if (!readonly)
    flags |= PAGE_FLAG_WRITE;

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

  heapaddr = mem_alloc (heapsize, 0);
  if (heapaddr == NULL)
    {
      mem_free (indexaddr, sizeof (void *) * indexsize);
      return -1;
    }
  /* Map heap pages from the desired virtual address */
  for (addr = 0; addr < heapsize; addr += PAGE_SIZE)
    map_page (addr + (u32) heapaddr, addr + (u32) vaddr, flags);

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
  MemHeader *header;
  MemFooter *footer;
  MemHeader *hcheck;
  MemFooter *fcheck;
  int add_hole = 1;

  if (ptr == NULL)
    return;

  header = (MemHeader *) ((u32) ptr - sizeof (MemHeader));
  footer = (MemFooter *) ((u32) header + header->mh_size - sizeof (MemFooter));

  assert (header->mh_magic == MEM_MAGIC);
  assert (footer->mf_cigam == MEM_CIGAM);

  header->mh_alloc = 0;

  fcheck = (MemFooter *) ((u32) header - sizeof (MemFooter));
  if (fcheck->mf_cigam == MEM_CIGAM
      && !((MemHeader *) fcheck->mf_header)->mh_alloc)
    {
      u32 temp = header->mh_size;
      header = (MemHeader *) fcheck->mf_header;
      footer->mf_header = (u32) header;
      header->mh_size += temp;
      add_hole = 0;
    }

  hcheck = (MemHeader *) ((u32) footer + sizeof (MemFooter));
  if (hcheck->mh_magic == MEM_MAGIC && !hcheck->mh_alloc)
    {
      u32 i = 0;
      header->mh_size += hcheck->mh_size;
      fcheck = (MemFooter *) ((u32) hcheck + hcheck->mh_size -
			      sizeof (MemFooter));
      footer = fcheck;
      while (i < heap->mh_index.sa_size
	     && sorted_array_lookup (&heap->mh_index, i) != hcheck)
	i++;
      assert (i < heap->mh_index.sa_size);
      sorted_array_remove (&heap->mh_index, i);
    }

  if (add_hole)
    sorted_array_insert (&heap->mh_index, header);
}

void
heap_init (void)
{
  kernel_heap = &_kernel_heap;
  if (heap_new (kernel_heap, (void *) KERNEL_HEAP_ADDR, KERNEL_HEAP_INDEX,
		KERNEL_HEAP_SIZE, 1, 0) != 0)
    panic ("Failed to create kernel heap");
}
