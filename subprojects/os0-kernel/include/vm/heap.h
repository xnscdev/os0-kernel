/*************************************************************************
 * heap.h -- This file is part of OS/0.                                  *
 * Copyright (C) 2020 XNSC                                               *
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

#include <sys/memory.h>

#define MEM_PAGEALIGN (1 << 0)

typedef struct
{
  u32 mh_magic;
  u32 mh_size;
  u8 mh_alloc;
  u8 mh_reserved[7];
} __attribute__ ((packed)) MemHeader;

typedef struct
{
  u32 mf_cigam;
  u32 mf_header;
} __attribute__ ((packed)) MemFooter;

typedef struct
{
  SortedArray mh_index;
  u32 mh_saddr;
  u32 mh_eaddr;
  u32 mh_maddr;
  u16 mh_supvsr;
  u16 mh_rdonly;
} __attribute__ ((packed)) MemHeap;

#endif
