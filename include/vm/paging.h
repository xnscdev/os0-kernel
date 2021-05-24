/*************************************************************************
 * paging.h -- This file is part of OS/0.                                *
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

#ifndef _VM_PAGING_H
#define _VM_PAGING_H

#ifdef ARCH_I386
#include <i386/paging.h>
#endif

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

extern uint32_t kernel_page_dir[PAGE_DIR_SIZE]
  __attribute__ ((aligned (PAGE_SIZE)));
extern uint32_t kernel_page_table[2][PAGE_TBL_SIZE]
  __attribute__ ((aligned (PAGE_SIZE)));
extern uint32_t kernel_stack_table[PAGE_TBL_SIZE]
  __attribute__ ((aligned (PAGE_SIZE)));
extern uint32_t kernel_vmap[PAGE_DIR_SIZE]
  __attribute__ ((aligned (PAGE_SIZE)));
extern uint32_t kheap_page_table[65][PAGE_TBL_SIZE]
  __attribute__ ((aligned (PAGE_SIZE)));
extern uint32_t page_stack_table[PAGE_TBL_SIZE]
  __attribute__ ((aligned (PAGE_SIZE)));
extern uint32_t *curr_page_dir;

void paging_loaddir (uint32_t addr);
void paging_enable (void);

uint32_t get_paddr (uint32_t *dir, void *vaddr);
void map_page (uint32_t *dir, uint32_t paddr, uint32_t vaddr, uint32_t flags);
void unmap_page (uint32_t *dir, uint32_t vaddr);

__END_DECLS

#endif
