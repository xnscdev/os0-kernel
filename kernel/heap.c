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

void *
heap_alloc (MemHeap *heap, uint32_t size, unsigned char aligned)
{
  uint32_t i;
  assert (heap != NULL);

  for (i = 0; i < heap->mh_index.sa_size; i++)
    {
      MemHeader *header = sorted_array_lookup (&heap->mh_index, i);
      /* Bad magic number or already allocated? */
      if (header->mh_magic != MEM_MAGIC || header->mh_alloc)
	continue;

      if (aligned && ((uint32_t) header + sizeof (MemHeader)) & (PAGE_SIZE - 1))
	{
	  /* Page-aligned start of new header */
	  uint32_t addr = (uint32_t) header + sizeof (MemHeader);
	  uint32_t new_size;
	  addr &= 0xfffff000;
	  addr += PAGE_SIZE;

	  /* Give up if the page-aligned address is no longer in the block */
	  if (addr >= (uint32_t) header + sizeof (MemHeader) + header->mh_size)
	    continue;
	  new_size = (uint32_t) header + sizeof (MemHeader) +
	    header->mh_size - addr;
	  if (new_size < size)
	    continue; /* Not enough space for the requested size anymore */

	  /* Check if there is enough extra space to insert a free block
	     behind the page-aligned address */
	  if (addr - (uint32_t) header > sizeof (MemHeader) * 2 +
	      sizeof (MemFooter))
	    {
	      MemFooter *footer =
		(MemFooter *) ((uint32_t) header + sizeof (MemHeader) +
			       header->mh_size);
	      MemHeader *page_header =
		(MemHeader *) (addr - sizeof (MemHeader));
	      MemFooter *prev_footer =
		(MemFooter *) ((uint32_t) page_header - sizeof (MemFooter));

	      /* Change the footer to point to the new header */
	      assert (footer->mf_cigam == MEM_CIGAM);
	      footer->mf_header = (uint32_t) page_header;

	      /* Create a new header behind the page-aligned address */
	      page_header->mh_magic = MEM_MAGIC;
	      page_header->mh_size = header->mh_size - addr +
		(uint32_t) header + sizeof (MemHeader);
	      page_header->mh_alloc = 1;

	      /* Shrink the original block and add a new footer */
	      prev_footer->mf_cigam = MEM_CIGAM;
	      prev_footer->mf_header = (uint32_t) header;

	      /* Update the original block size */
	      header->mh_size = (uint32_t) prev_footer - (uint32_t) header -
		sizeof (MemHeader);

	      sorted_array_insert (&heap->mh_index, page_header);
	      header = page_header; /* For allocation setup */
	    }
	  else
	    continue; /* TODO Create a new header and add a gap in the index */
	}

      /* Check if there is enough extra space to insert a new free block
	 to fill the extra space */
      if (header->mh_size > size + sizeof (MemHeader) + sizeof (MemFooter))
	{
	  MemFooter *footer =
	    (MemFooter *) ((uint32_t) header + sizeof (MemHeader) + size);
	  MemHeader *new_header;
	  MemFooter *new_footer;
	  uint32_t new_size =
	    header->mh_size - size - sizeof (MemHeader) - sizeof (MemFooter);

	  /* Change the size and set the header to allocated */
	  header->mh_size = size;
	  header->mh_alloc = 1;

	  /* Create a new footer for the allocated header */
	  footer->mf_cigam = MEM_CIGAM;
	  footer->mf_header = (uint32_t) header;

	  /* Create a new header for the unused memory portion */
	  new_header = (MemHeader *) ((uint32_t) footer + sizeof (MemFooter));
	  new_header->mh_magic = MEM_MAGIC;
	  new_header->mh_size = new_size;
	  new_header->mh_alloc = 0;
	  sorted_array_insert (&heap->mh_index, new_header);

	  /* Create the footer for the new header */
	  new_footer = (MemFooter *) ((uint32_t) new_header +
				      sizeof (MemHeader) + new_header->mh_size);
	  new_footer->mf_cigam = MEM_CIGAM;
	  new_footer->mf_header = (uint32_t) new_header;

	  return (void *) ((uint32_t) header + sizeof (MemHeader));
	}
      else if (header->mh_size >= size)
	{
	  /* Just allocate the entire block including the extra memory */
	  header->mh_alloc = 1;
	  return (void *) ((uint32_t) header + sizeof (MemHeader));
	}
    }
  return NULL;
}

void *
heap_realloc (MemHeap *heap, void *ptr, uint32_t size)
{
  MemHeader *header;
  MemFooter *footer;
  MemHeader *next_header;
  MemFooter *next_footer;
  void *new;
  size_t extra;
  size_t save;
  uint32_t i;
  assert (heap != NULL);

  /* Reallocating a NULL pointer is the same as allocating a new one */
  if (ptr == NULL)
    return heap_alloc (heap, size, 0);

  header = (MemHeader *) ((uint32_t) ptr - sizeof (MemHeader));
  assert (header->mh_magic == MEM_MAGIC);
  if (!header->mh_alloc)
    return NULL; /* Reallocating a freed pointer */
  for (i = 0; i < heap->mh_index.sa_size; i++)
    {
      if (sorted_array_lookup (&heap->mh_index, i) == header)
        goto found;
    }
  printk ("heap: aborting realloc not in heap index\n");
  return NULL;

 found:
  footer = (MemFooter *) ((uint32_t) header + sizeof (MemHeader) +
			  header->mh_size);
  assert (footer->mf_cigam == MEM_CIGAM);

  next_header = (MemHeader *) ((uint32_t) footer + sizeof (MemFooter));
  if ((uint32_t) next_header >= heap->mh_addr + heap->mh_size
      || next_header->mh_magic != MEM_MAGIC)
    return NULL;

  /* If the next block is free and large enough, take memory from it */
  save = header->mh_size;
  extra = size - save;
  if (!next_header->mh_alloc)
    {
      size_t i;
      for (i = 0; i < heap->mh_index.sa_size; i++)
	{
	  if (sorted_array_lookup (&heap->mh_index, i) == next_header)
	    goto next;
	}
      return NULL; /* Next header not in heap index */

    next:
      if (next_header->mh_size > extra + sizeof (MemHeader) +
	  sizeof (MemFooter))
	{
	  sorted_array_remove (&heap->mh_index, i);
	  header->mh_size = size;

	  /* Create new footer */
	  footer = (MemFooter *) ((uint32_t) header + sizeof (MemHeader) +
				  size);
	  footer->mf_cigam = MEM_CIGAM;
	  footer->mf_header = (uint32_t) header;

	  /* Create new header for next block */
	  next_header = (MemHeader *) ((uint32_t) footer + sizeof (MemFooter));
	  next_header->mh_magic = MEM_MAGIC;
	  next_header->mh_alloc = 0;
	  next_header->mh_size = save - extra - sizeof (MemHeader);

	  /* Create new footer for next block */
	  next_footer =
	    (MemFooter *) ((uint32_t) next_header + sizeof (MemHeader) +
			   next_header->mh_size);
	  next_footer->mf_cigam = MEM_CIGAM;
	  next_footer->mf_header = (uint32_t) next_header;
	  return ptr;
	}
      else if (next_header->mh_size >= extra)
	{
	  sorted_array_remove (&heap->mh_index, i);

	  /* Point footer of next block to current header */
	  footer = (MemFooter *) ((uint32_t) next_header + sizeof (MemHeader) +
				  next_header->mh_size);
	  assert (footer->mf_cigam == MEM_CIGAM);
	  footer->mf_header = (uint32_t) header;

	  /* Set new size of block */
	  header->mh_size += save + sizeof (MemHeader) + sizeof (MemFooter);
	  return ptr;
	}
    }

  /* Find a new free block large enough */
  new = heap_alloc (heap, size, 0);
  if (new == NULL)
    return NULL;
  memcpy (new, ptr, header->mh_size);
  heap_free (heap, ptr);
  return new;
}

void
heap_free (MemHeap *heap, void *ptr)
{
  MemHeader *header;
  MemFooter *footer;
  MemHeader *prev_header;
  uint32_t i;
  assert (heap != NULL);

  if (ptr == NULL)
    return;

  header = (MemHeader *) ((uint32_t) ptr - sizeof (MemHeader));
  assert (header->mh_magic == MEM_MAGIC);
  for (i = 0; i < heap->mh_index.sa_size; i++)
    {
      if (sorted_array_lookup (&heap->mh_index, i) == header)
        goto found;
    }
  printk ("heap: aborting free not in heap index\n");
  return;

 found:
  header->mh_alloc = 0;

  /* Left unify with previous block, if possible */
  footer = (MemFooter *) ((uint32_t) header - sizeof (MemFooter));
  if ((uint32_t) footer >= heap->mh_addr)
    {
      prev_header = (MemHeader *) footer->mf_header;
      if (footer->mf_cigam == MEM_CIGAM && prev_header->mh_magic == MEM_MAGIC
	  && !prev_header->mh_alloc)
	{
	  /* Merge the two blocks into one */
	  prev_header->mh_size += header->mh_size + sizeof (MemHeader) +
	    sizeof (MemFooter);

	  /* Invalidate header magic number and remove from index */
	  header->mh_magic = 0;
	  sorted_array_remove (&heap->mh_index, i);

	  header = prev_header; /* For the right unification test */
	}
    }

  /* Right unify with next block, if possible */
  prev_header = (MemHeader *) ((uint32_t) header + sizeof (MemHeader) +
			       sizeof (MemFooter) + header->mh_size);
  if ((uint32_t) prev_header < heap->mh_addr + heap->mh_size
      && prev_header->mh_magic == MEM_MAGIC && !prev_header->mh_alloc)
    {
      for (i = 0; i < heap->mh_index.sa_size; i++)
	{
	  if (sorted_array_lookup (&heap->mh_index, i) == prev_header)
	    goto skip;
	}
      return;

    skip:
      footer = (MemFooter *) ((uint32_t) prev_header + sizeof (MemHeader) +
			      prev_header->mh_size);
      if (footer->mf_cigam == MEM_CIGAM)
	{
	  /* Point to the new header */
	  footer->mf_header = (uint32_t) header;

	  /* Update the new block size */
	  header->mh_size += sizeof (MemHeader) + sizeof (MemFooter) +
	    prev_header->mh_size;

	  /* Delete the next header */
	  sorted_array_remove (&heap->mh_index, i);
	}
    }
}

void *
kmalloc (size_t size)
{
  return heap_alloc (&kernel_heap, size, 0);
}

void *
kvalloc (size_t size)
{
  return heap_alloc (&kernel_heap, size, 1);
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

void *
krealloc (void *ptr, size_t size)
{
  return heap_realloc (&kernel_heap, ptr, size);
}

void
kfree (void *ptr)
{
  heap_free (&kernel_heap, ptr);
}

void
heap_init (void)
{
  MemHeader *header = (MemHeader *) KHEAP_DATA_VADDR;
  MemFooter *footer;

  /* Setup heap index array */
  if (sorted_array_place (&kernel_heap.mh_index, (void *) KHEAP_INDEX_VADDR,
			  KHEAP_INDEX_NELEM, heap_cmp) != 0)
    panic ("Failed to create kernel heap");

  /* Add a single unallocated memory block to the heap */
  header->mh_magic = MEM_MAGIC;
  header->mh_size = KHEAP_DATA_LEN - sizeof (MemHeader) - sizeof (MemFooter);
  header->mh_alloc = 0;
  sorted_array_insert (&kernel_heap.mh_index, header);

  footer = (MemFooter *) ((uint32_t) header + sizeof (MemHeader) +
			  header->mh_size);
  footer->mf_cigam = MEM_CIGAM;
  footer->mf_header = (uint32_t) header;

  kernel_heap.mh_addr = KHEAP_DATA_VADDR;
  kernel_heap.mh_size = KHEAP_DATA_LEN;
  kernel_heap.mh_pdata = KHEAP_DATA_PADDR;
  kernel_heap.mh_pindex = KHEAP_INDEX_PADDR;
}
