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

#define KERNEL_HEAP_ADDR  0xc0000000
#define KERNEL_HEAP_INDEX 0x8000
#define KERNEL_HEAP_SIZE  0x10000000

typedef struct
{
  u32 mh_magic;
  u32 mh_size;
  u8 mh_alloc;
  u8 mh_reserved[7];
} MemHeader;

typedef struct
{
  u32 mf_cigam;
  u32 mf_header;
} MemFooter;

typedef struct
{
  SortedArray mh_index;
  u32 mh_addr;
  u32 mh_size;
  u16 mh_supvsr;
  u16 mh_rdonly;
} MemHeap;

__BEGIN_DECLS

int heap_new (MemHeap *heap, void *vaddr, u32 indexsize, u32 heapsize,
	      u8 supervisor, u8 readonly);
void heap_init (void);

__END_DECLS

#endif
