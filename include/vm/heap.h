/*************************************************************************
 * heap.h -- This file is part of OS/0.                                  *
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

#ifndef _VM_HEAP_H
#define _VM_HEAP_H

#include <libk/array.h>
#include <sys/memory.h>

#define MEM_PAGEALIGN (1 << 0)

#define KERNEL_HEAP_ADDR  0xd0000000
#define KERNEL_HEAP_INDEX 0x8000
#define KERNEL_HEAP_SIZE  0x10000000

typedef struct
{
  uint32_t mh_magic;
  uint32_t mh_size;
  unsigned char mh_alloc;
  unsigned char mh_reserved[7];
} MemHeader;

typedef struct
{
  uint32_t mf_cigam;
  uint32_t mf_header;
} MemFooter;

typedef struct
{
  SortedArray mh_index;
  uint32_t mh_addr;
  uint32_t mh_size;
  uint16_t mh_supvsr;
  uint16_t mh_rdonly;
} MemHeap;

__BEGIN_DECLS

extern MemHeap *kernel_heap;

int heap_new (MemHeap *heap, void *vaddr, uint32_t indexsize, uint32_t heapsize,
	      unsigned char supervisor, unsigned char readonly);
void *heap_alloc (MemHeap *heap, uint32_t size, unsigned char aligned);
void heap_free (MemHeap *heap, void *ptr);

void *kmalloc (size_t size);
void *kvalloc (size_t size);
void *kzalloc (size_t size);
void kfree (void *ptr);

void heap_init (void);

__END_DECLS

#endif
