/*************************************************************************
 * memory.h -- This file is part of OS/0.                                *
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

#ifndef _SYS_MEMORY_H
#define _SYS_MEMORY_H

#include <sys/syslimits.h>
#ifndef _ASM
#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>
#endif

#define MEM_MAGIC 0xefbeadde
#define MEM_CIGAM 0xdeadbeef

#define KERNEL_PADDR 0x00100000
#define KERNEL_VADDR 0xc0100000
#ifndef _ASM
#define KERNEL_LEN   ((uint32_t) &_kernel_end - KERNEL_VADDR)
#endif

#define RELOC_PADDR 0x00000000
#define RELOC_VADDR 0xc0000000
#ifndef _ASM
#define RELOC_LEN   ((uint32_t) &_kernel_end - RELOC_VADDR)
#endif

#define KHEAP_DATA_PADDR 0x00808000
#define KHEAP_DATA_VADDR 0xd0000000
#define KHEAP_DATA_LEN   0x10000000

#define KHEAP_INDEX_PADDR 0x00800000
#define KHEAP_INDEX_VADDR 0xe0000000
#define KHEAP_INDEX_NELEM 0x8000
#define KHEAP_INDEX_LEN   (KHEAP_INDEX_NELEM << 2)

#define PAGE_STACK_PADDR 0x10808000
#define PAGE_STACK_VADDR 0xe0400000
#define PAGE_STACK_NELEM 0x100000
#ifndef _ASM
#define PAGE_STACK_LEN   (PAGE_STACK_NELEM * sizeof (void *))
#endif

#define EXEC_DATA_PADDR 0x10908000
#define EXEC_DATA_VADDR 0xff410000
#define EXEC_DATA_LEN   ARG_MAX

#define MEM_ALLOC_START 0x1c000000

#define MIN_MEMORY 524288

#define TASK_LOCAL_BOUND 0xf0000000

#ifndef _ASM

__BEGIN_DECLS

extern void *_kernel_start;
extern void *_kernel_end;

void mem_init (uint32_t mem);

uint32_t alloc_page (void);
void free_page (uint32_t addr);

__END_DECLS

#endif

#endif
