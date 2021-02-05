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

#include <libk/types.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

extern u32 page_dir[PAGE_DIR_SIZE] __attribute__ ((aligned (PAGE_SIZE)));
extern u32 page_table[PAGE_TBL_SIZE][PAGE_DIR_SIZE]
  __attribute__ ((aligned (PAGE_SIZE)));

void paging_loaddir (u32 addr);
void paging_enable (void);

void *get_paddr (void *vaddr);
void map_page (void *paddr, void *vaddr, u32 flags);

__END_DECLS

#endif
