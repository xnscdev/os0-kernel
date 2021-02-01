/*************************************************************************
 * memory.h -- This file is part of OS/0.                                *
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

#ifndef _SYS_MEMORY_H
#define _SYS_MEMORY_H

#include <libk/types.h>
#include <sys/cdefs.h>
#include <stddef.h>

#define MEM_MAGIC 0xefbeadde
#define MEM_CIGAM 0xdeadbeef

struct MemHeader
{
  u32 mh_magic;
  u32 mh_size;
  u8 mh_alloc;
  u8 mh_reserved[7];
} __attribute__ ((packed));

struct MemFooter
{
  u32 mh_cigam;
  u32 mh_header;
} __attribute__ ((packed));

__BEGIN_DECLS

void mem_init (u32 mem);

__END_DECLS

#endif