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
#include <limits.h>

extern void *_kernel_end;

static u32 mem_maxaddr;

static u8 o16[0x1];
static u8 o15[0x2];
static u8 o14[0x4];
static u8 o13[0x8];
static u8 o12[0x10];
static u8 o11[0x20];
static u8 o10[0x40];
static u8 o9[0x80];
static u8 o8[0x100];
static u8 o7[0x200];
static u8 o6[0x400];
static u8 o5[0x800];
static u8 o4[0x1000];
static u8 o3[0x2000];
static u8 o2[0x4000];
static u8 o1[0x8000];
static u8 o0[0x10000];
static u8 *block_list[] =
  {o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14, o15, o16};

static int
mem_search_free (u32 order, int *index, int *bit)
{
  size_t i;
  for (i = 0; i < 1 << (MEM_MAX_BLOCK_ORDER - order); i++)
    {
      u8 num = ~block_list[order][i];
      int nbit = fls (num);
      if (nbit == 0)
	continue;
      *index = i;
      *bit = CHAR_BIT - nbit;
      return 0;
    }
  return -1;
}

void
mem_init (u32 mem)
{
  mem_maxaddr = mem * 1024 + 0x100000;
  printk ("Detected %dK of available upper memory\n",
	  mem - ((u32) &_kernel_end - 0x100000) / 1024);
}

void *
kmalloc (size_t size, u32 flags)
{
  u8 order = 0;
  u32 addr;
  int index;
  int bit;
  int save_index;
  int save_bit;
  int i;
  int j;

  while (1 << (order + 12) < size)
    order++;
  if (order > MEM_MAX_BLOCK_ORDER)
    return NULL; /* Requested too much memory */

  if (mem_search_free (order, &index, &bit) != 0)
    return NULL; /* No blocks that can fit size bytes are available */

  addr = (u32) &_kernel_end + (1 << (order + 12)) * (index * CHAR_BIT + bit);
  if (addr > mem_maxaddr)
    return NULL; /* System does not have enough memory */
  save_index = index;
  save_bit = bit;

  /* Mark block as used in bitmap and all corresponding indices in larger
     order bitmaps */
  for (i = order; i <= MEM_MAX_BLOCK_ORDER; i++)
    {
      int offset;
      block_list[i][index] |= (1 << (CHAR_BIT - bit - 1));
      offset = index * CHAR_BIT + bit;
      offset >>= 1;
      index = offset / CHAR_BIT;
      bit = offset - index * CHAR_BIT;
    }

  index = save_index;
  bit = save_bit;
  for (i = order - 1, j = 2; i >= 0; i--, j <<= 1)
    {
      int offset = index * CHAR_BIT + bit;
      int k;
      for (k = 0; k < j; k++)
	{
	  int to = offset + k;
	  int ti = to / CHAR_BIT;
	  int tb = to - ti * CHAR_BIT;
	  block_list[i][ti] |= 1 << (CHAR_BIT - tb - 1);
	}
      offset <<= 1;
      index = offset / CHAR_BIT;
      bit = offset - index * CHAR_BIT;
    }

  return (void *) addr;
}
