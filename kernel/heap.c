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

int
heap_new (MemHeap *heap, void *vaddr, u32 indexsize, u32 heapsize,
	  u8 supervisor, u8 readonly)
{
  MemHeader *header;
  MemFooter *footer;
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

  /* TODO Add a single unallocated memory block to the heap */
  header = vaddr;
  header->mh_magic = MEM_MAGIC;
  header->mh_size = heapsize - sizeof (MemHeader) - sizeof (MemFooter);
  header->mh_alloc = 0;
  sorted_array_insert (&heap->mh_index, header);

  footer = (MemFooter *) ((u32) header + header->mh_size);
  footer->mf_cigam = MEM_CIGAM;
  footer->mf_header = (u32) header;

  heap->mh_addr = (u32) vaddr;
  heap->mh_size = heapsize;
  heap->mh_supvsr = supervisor;
  heap->mh_rdonly = readonly;
  return 0;
}

void *
heap_alloc (MemHeap *heap, u32 size, u8 aligned)
{
  u32 i;
  assert (heap != NULL);

  for (i = 0; i < heap->mh_index.sa_size; i++)
    {
      MemHeader *header = sorted_array_lookup (&heap->mh_index, i);
      /* Bad magic number or already allocated? */
      if (header->mh_magic != MEM_MAGIC || header->mh_alloc)
	continue;

      /* Check if there is enough extra space to insert a new free block
	 to fill the extra space */
      if (header->mh_size > size + sizeof (MemHeader) + sizeof (MemFooter))
	{
	  MemFooter *footer =
	    (MemFooter *) ((u32) header + sizeof (MemHeader) + size);
	  MemHeader *new_header;
	  MemFooter *new_footer;
	  u32 new_size =
	    header->mh_size - size - sizeof (MemHeader) - sizeof (MemFooter);

	  /* Change the size and set the header to allocated */
	  header->mh_size = size;
	  header->mh_alloc = 1;

	  /* Create a new footer for the allocated header */
	  footer->mf_cigam = MEM_CIGAM;
	  footer->mf_header = (u32) header;

	  /* Create a new block of memory for the remaining part of
	     the larger block; splitting a block of memory looks like this:

	     H............F
	     H....FH......F

	     A new header needs to be created after the footer of the
	     allocated block, then the previous footer needs to point
	     to the new header */

	  /* Create a new header for the unused memory portion */
	  new_header = (MemHeader *) ((u32) footer + sizeof (MemFooter));
	  new_header->mh_magic = MEM_MAGIC;
	  new_header->mh_size = new_size;
	  new_header->mh_alloc = 0;
	  sorted_array_insert (&heap->mh_index, new_header);

	  /* Create the footer for the new header */
	  new_footer = (MemFooter *) ((u32) new_header + sizeof (MemHeader) +
				      new_header->mh_size);
	  new_footer->mf_cigam = MEM_CIGAM;
	  new_footer->mf_header = (u32) new_header;

	  return (void *) ((u32) header + sizeof (MemHeader));
	}
      else if (header->mh_size >= size)
	{
	  /* Just allocate the entire block including the extra memory */
	  header->mh_alloc = 1;
	  return (void *) ((u32) header + sizeof (MemHeader));
	}
    }
  return NULL;
}

void
heap_free (MemHeap *heap, void *ptr)
{
  assert (heap != NULL);
  if (ptr == NULL)
    return;
}

void *
kmalloc (size_t size)
{
  return heap_alloc (kernel_heap, size, 0);
}

void *
kvalloc (size_t size)
{
  return heap_alloc (kernel_heap, size, 1);
}

void *
kzalloc (size_t size)
{
  void *ptr = kmalloc (size);
  if (ptr == NULL)
    return NULL;
  memset (ptr, 0, size);
  return ptr;
}

void
kfree (void *ptr)
{
  heap_free (kernel_heap, ptr);
}

void
heap_init (void)
{
  kernel_heap = &_kernel_heap;
  if (heap_new (kernel_heap, (void *) KERNEL_HEAP_ADDR, KERNEL_HEAP_INDEX,
		KERNEL_HEAP_SIZE, 1, 0) != 0)
    panic ("Failed to create kernel heap");
}
