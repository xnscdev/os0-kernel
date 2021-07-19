/*************************************************************************
 * bitarray.c -- This file is part of OS/0.                              *
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

/* Copyright (C) 2008 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <fs/ext2.h>
#include <libk/libk.h>
#include <vm/heap.h>

typedef struct
{
  char *p_bitarray;
} Ext2BitArrayPrivate;

static int
ext2_bitarray_alloc_private (Ext2Bitmap64 *bmap)
{
  Ext2BitArrayPrivate *priv = kmalloc (sizeof (Ext2BitArrayPrivate));
  size_t size = (size_t) ((bmap->b_real_end - bmap->b_start) / 8 + 1);
  if (unlikely (priv == NULL))
    return -ENOMEM;
  priv->p_bitarray = kmalloc (size);
  if (unlikely (priv->p_bitarray == NULL))
    {
      kfree (priv);
      return -ENOMEM;
    }
  bmap->b_private = priv;
  return 0;
}

static int
ext2_bitarray_new_bmap (VFSSuperblock *sb, Ext2Bitmap64 *bmap)
{
  Ext2BitArrayPrivate *priv;
  size_t size;
  int ret = ext2_bitarray_alloc_private (bmap);
  if (ret != 0)
    return ret;
  priv = bmap->b_private;
  size = (size_t) ((bmap->b_real_end - bmap->b_start) / 8 + 1);
  memset (priv->p_bitarray, 0, size);
  return 0;
}

static void
ext2_bitarray_free_bmap (Ext2Bitmap64 *bmap)
{
  Ext2BitArrayPrivate *priv = bmap->b_private;
  if (priv->p_bitarray != NULL)
    kfree (priv->p_bitarray);
  kfree (bmap);
}

static void
ext2_bitarray_mark_bmap (Ext2Bitmap64 *bmap, uint64_t arg)
{
  Ext2BitArrayPrivate *priv = bmap->b_private;
  fast_set_bit (priv->p_bitarray, arg - bmap->b_start);
}

static void
ext2_bitarray_unmark_bmap (Ext2Bitmap64 *bmap, uint64_t arg)
{
  Ext2BitArrayPrivate *priv = bmap->b_private;
  fast_clear_bit (priv->p_bitarray, arg - bmap->b_start);
}

static int
ext2_bitarray_test_bmap (Ext2Bitmap64 *bmap, uint64_t arg)
{
  Ext2BitArrayPrivate *priv = bmap->b_private;
  return fast_test_bit (priv->p_bitarray, arg - bmap->b_start);
}

static void
ext2_bitarray_mark_bmap_extent (Ext2Bitmap64 *bmap, uint64_t arg,
				unsigned int num)
{
  Ext2BitArrayPrivate *priv = bmap->b_private;
  unsigned int i;
  for (i = 0; i < num; i++)
    fast_set_bit (priv->p_bitarray, arg + i - bmap->b_start);
}

static void
ext2_bitarray_unmark_bmap_extent (Ext2Bitmap64 *bmap, uint64_t arg,
				  unsigned int num)
{
  Ext2BitArrayPrivate *priv = bmap->b_private;
  unsigned int i;
  for (i = 0; i < num; i++)
    fast_clear_bit (priv->p_bitarray, arg + i - bmap->b_start);
}

static void
ext2_bitarray_set_bmap_range (Ext2Bitmap64 *bmap, uint64_t start, size_t num,
			      void *data)
{
  Ext2BitArrayPrivate *priv = bmap->b_private;
  memcpy (priv->p_bitarray + (start >> 3), data, (num + 7) >> 3);
}

static void
ext2_bitarray_get_bmap_range (Ext2Bitmap64 *bmap, uint64_t start, size_t num,
			      void *data)
{
  Ext2BitArrayPrivate *priv = bmap->b_private;
  memcpy (data, priv->p_bitarray + (start >> 3), (num + 7) >> 3);
}

static int
ext2_bitarray_find_first_zero (Ext2Bitmap64 *bmap, uint64_t start, uint64_t end,
			       uint64_t *result)
{
  Ext2BitArrayPrivate *priv = bmap->b_private;
  const unsigned char *pos;
  unsigned long bitpos = start - bmap->b_start;
  unsigned long count = end - start + 1;
  unsigned long max_loop;
  unsigned long i;
  int found = 0;

  while ((bitpos & 7) != 0 && count > 0)
    {
      if (!fast_test_bit (priv->p_bitarray, bitpos))
	{
	  *result = bitpos + bmap->b_start;
	  return 0;
	}
      bitpos++;
      count--;
    }
  if (count == 0)
    return -ENOENT;

  pos = (unsigned char *) priv->p_bitarray + (bitpos >> 3);
  while (count >= 8 && ((uintptr_t) pos & 7))
    {
      if (*pos != 0xff)
	{
	  found = 1;
	  break;
	}
      pos++;
      count -= 8;
      bitpos += 8;
    }
  if (!found)
    {
      max_loop = count >> 6;
      i = max_loop;
      while (i != 0)
	{
	  if (*((uint64_t *) pos) != (uint64_t) -1)
	    break;
	  pos += 8;
	  i--;
	}
      count -= 64 * (max_loop - i);
      bitpos += 64 * (max_loop - i);

      max_loop = count >> 3;
      i = max_loop;
      while (i != 0)
	{
	  if (*pos != 0xff)
	    {
	      found = 1;
	      break;
	    }
	  pos++;
	  i--;
	}
      count -= 8 * (max_loop - i);
      bitpos += 8 * (max_loop + i);
    }

  while (count-- > 0)
    {
      if (!fast_test_bit (priv->p_bitarray, bitpos))
	{
	  *result = bitpos + bmap->b_start;
	  return 0;
	}
      bitpos++;
    }
  return -ENOENT;
}

static int
ext2_bitarray_find_first_set (Ext2Bitmap64 *bmap, uint64_t start, uint64_t end,
			      uint64_t *result)
{
  Ext2BitArrayPrivate *priv = bmap->b_private;
  const unsigned char *pos;
  unsigned long bitpos = start - bmap->b_start;
  unsigned long count = end - start + 1;
  unsigned long max_loop;
  unsigned long i;
  int found = 0;

  while ((bitpos & 7) != 0 && count > 0)
    {
      if (fast_test_bit (priv->p_bitarray, bitpos))
	{
	  *result = bitpos + bmap->b_start;
	  return 0;
	}
      bitpos++;
      count--;
    }
  if (count == 0)
    return -ENOENT;

  pos = (unsigned char *) priv->p_bitarray + (bitpos >> 3);
  while (count >= 8 && ((uintptr_t) pos & 7))
    {
      if (*pos != 0)
	{
	  found = 1;
	  break;
	}
      pos++;
      count -= 8;
      bitpos += 8;
    }
  if (!found)
    {
      max_loop = count >> 6;
      i = max_loop;
      while (i != 0)
	{
	  if (*((uint64_t *) pos) != 0)
	    break;
	  pos += 8;
	  i--;
	}
      count -= 64 * (max_loop - i);
      bitpos += 64 * (max_loop - i);

      max_loop = count >> 3;
      i = max_loop;
      while (i != 0)
	{
	  if (*pos != 0)
	    {
	      found = 1;
	      break;
	    }
	  pos++;
	  i--;
	}
      count -= 8 * (max_loop - i);
      bitpos += 8 * (max_loop + i);
    }

  while (count-- > 0)
    {
      if (fast_test_bit (priv->p_bitarray, bitpos))
	{
	  *result = bitpos + bmap->b_start;
	  return 0;
	}
      bitpos++;
    }
  return -ENOENT;
}

Ext2BitmapOps ext2_bitarray_ops = {
  .b_type = EXT2_BMAP64_BITARRAY,
  .b_new_bmap = ext2_bitarray_new_bmap,
  .b_free_bmap = ext2_bitarray_free_bmap,
  .b_mark_bmap = ext2_bitarray_mark_bmap,
  .b_unmark_bmap = ext2_bitarray_unmark_bmap,
  .b_test_bmap = ext2_bitarray_test_bmap,
  .b_mark_bmap_extent = ext2_bitarray_mark_bmap_extent,
  .b_unmark_bmap_extent = ext2_bitarray_unmark_bmap_extent,
  .b_set_bmap_range = ext2_bitarray_set_bmap_range,
  .b_get_bmap_range = ext2_bitarray_get_bmap_range,
  .b_find_first_zero = ext2_bitarray_find_first_zero,
  .b_find_first_set = ext2_bitarray_find_first_set
};
