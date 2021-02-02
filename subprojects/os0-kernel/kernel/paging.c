/*************************************************************************
 * paging.c -- This file is part of OS/0.                                *
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

#include <libk/types.h>
#include <sys/memory.h>
#include <vm/paging.h>

extern void *_kernel_end;

static u32 page_dir[PAGE_DIR_SIZE] __attribute__ ((aligned (MEM_PAGESIZE)));
static u32 page_table0[PAGE_TBL_SIZE] __attribute__ ((aligned (MEM_PAGESIZE)));

void
paging_init (void)
{
  int i;
  page_dir[0] = (u32) page_table0 | 3;
  for (i = 1; i < PAGE_DIR_SIZE; i++)
    page_dir[i] = 2;
  /* Identity map the first 4 MiB */
  for (i = 0; i < PAGE_TBL_SIZE; i++)
    page_table0[i] = (i * MEM_PAGESIZE) | 3;
  paging_loaddir ((u32) page_dir);
  paging_enable ();
}
