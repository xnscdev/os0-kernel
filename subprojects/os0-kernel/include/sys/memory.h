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

#include <sys/cdefs.h>
#include <stddef.h>

#define MEM_MAGIC 0xefbeadde
#define MEM_CIGAM 0xdeadbeef

#define MEM_MAX_BLOCK_ORDER 16 /* 256 MiB blocks */
#define MEM_PAGESIZE 0x1000

__BEGIN_DECLS

void mem_init (u32 mem);

void *mem_alloc (size_t size, u32 flags);
void mem_free (void *ptr, size_t size);

__END_DECLS

#endif
