/*************************************************************************
 * memory.c -- This file is part of OS/0.                                *
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

#include <libk/libk.h>
#include <sys/memory.h>

extern void *_kernel_end;

static u8 o5[1];
static u8 o4[1];
static u8 o3[1];
static u8 o2[2];
static u8 o1[4];
static u8 o0[8];
static u8 *block_list[] = {o0, o1, o2, o3, o4, o5};

static int
mem_get_bit (u32 order, int index, int bit)
{
  return block_list[order][index] & (1 << (bit - 1));
}

static void
mem_set_bit (u32 order, int index, int bit)
{
  block_list[order][index] &= (1 << (bit - 1));
}

static int
mem_search_free (u32 order, int *index, int *bit)
{
  size_t i;
  for (i = 0; i < sizeof block_list[order]; i++)
    {
      u8 num = block_list[order][i];
      int nbit = fls (num);
      if (nbit == 0)
	continue;
      *index = i;
      *bit = nbit;
      return 0;
    }
  return -1;
}

void
mem_init (u32 mem)
{
  u32 addr = (u32) &_kernel_end;
  mem -= (addr - 0x100000) / 1024; /* Skip kernel code */
  printk ("Detected %dK of available upper memory\n", mem);
}

void *
kmalloc (size_t size, u32 flags)
{
  u8 order = 0;
  int index;
  int bit;

  while (1 << (order + 2) < size)
    order++;

  if (mem_search_free (order, &index, &bit) != 0)
    return NULL; /* No blocks that can fit size bytes are available */
  return NULL;
}
