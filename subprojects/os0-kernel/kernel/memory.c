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

#undef  MEM_MAX_BLOCK_ORDER
#define MEM_MAX_BLOCK_ORDER 3

extern void *_kernel_end;

static u8 o3[1];
static u8 o2[2];
static u8 o1[4];
static u8 o0[8];
static u8 *block_list[] = {o0, o1, o2, o3};

static void
mem_print_bits (u8 n, u32 spaces)
{
  int i;
  for (i = 0; i < CHAR_BIT; i++)
    {
      u32 j;
      printk (n & (1 << (CHAR_BIT - i - 1)) ? "x" : ".");
      for (j = 0; j < spaces; j++)
	printk (" ");
    }
}

static void
mem_dump (void)
{
  int i;
  printk ("x - Used\n. - Free\n0 ");
  for (i = 0; i < 8; i++)
    mem_print_bits (o0[i], 0);
  printk ("\n1 ");
  for (i = 0; i < 4; i++)
    mem_print_bits (o1[i], 1);
  printk ("\n2 ");
  for (i = 0; i < 2; i++)
    mem_print_bits (o2[i], 3);
  printk ("\n3 ");
  for (i = 0; i < 1; i++)
    mem_print_bits (o3[i], 7);
  printk ("\n");
}

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
  u32 addr = (u32) &_kernel_end;
  mem -= (addr - 0x100000) / 1024; /* Skip kernel code */
  printk ("Detected %dK of available upper memory\n", mem);
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
  printk ("Minimum order for requested size: %d\n", order);

  if (mem_search_free (order, &index, &bit) != 0)
    return NULL; /* No blocks that can fit size bytes are available */
  printk ("Found available block: %d\n", index * CHAR_BIT + bit);

  addr = (1 << (order + 12)) * (index * CHAR_BIT + bit);
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
	  block_list[i][ti] |= (1 << (CHAR_BIT - tb - 1));
	}
      offset <<= 1;
      index = offset / CHAR_BIT;
      bit = offset - index * CHAR_BIT;
    }

  mem_dump ();
  return (void *) ((u32) &_kernel_end + addr);
}
