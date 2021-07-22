/*************************************************************************
 * bitmap.c -- This file is part of OS/0.                                *
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

/* Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <bits/mount.h>
#include <fs/ext2.h>
#include <libk/libk.h>
#include <vm/heap.h>

static int
ext2_reserve_super_bgd (VFSSuperblock *sb, unsigned int group, Ext2Bitmap *bmap)
{
  Ext2Filesystem *fs = sb->sb_private;
  block_t super;
  block_t old_desc;
  block_t new_desc;
  block_t used;
  int old_desc_blocks;
  int nblocks;
  ext2_super_bgd_loc (sb, group, &super, &old_desc, &new_desc, &used);

  if (fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_META_BG)
    old_desc_blocks = fs->f_super.s_first_meta_bg;
  else
    old_desc_blocks = fs->f_desc_blocks + fs->f_super.s_reserved_gdt_blocks;

  if (super != 0 || group == 0)
    ext2_mark_bitmap (bmap, super);
  if (group == 0 && sb->sb_blksize == 1024 && EXT2_CLUSTER_RATIO (fs) > 1)
    ext2_mark_bitmap (bmap, 0);

  if (old_desc != 0)
    {
      nblocks = old_desc_blocks;
      if (old_desc + nblocks >= ext2_blocks_count (&fs->f_super))
	nblocks = ext2_blocks_count (&fs->f_super) - old_desc;
      ext2_mark_block_bitmap_range (bmap, old_desc, nblocks);
    }
  if (new_desc != 0)
    ext2_mark_bitmap (bmap, new_desc);

  nblocks = ext2_group_blocks_count (fs, group);
  nblocks -= fs->f_inode_blocks_per_group + used + 2;
  return nblocks;
}

static int
ext2_mark_uninit_bg_group_blocks (VFSSuperblock *sb)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Bitmap *bmap = fs->f_block_bitmap;
  block_t block;
  unsigned int i;
  for (i = 0; i < fs->f_group_desc_count; i++)
    {
      if (!ext2_bg_test_flags (sb, i, EXT2_BG_BLOCK_UNINIT))
	continue;
      ext2_reserve_super_bgd (sb, i, bmap);
      block = ext2_inode_table_loc (sb, i);
      if (block != 0)
	ext2_mark_block_bitmap_range (bmap, block,
				      fs->f_inode_blocks_per_group);
      block = ext2_block_bitmap_loc (sb, i);
      if (block != 0)
	ext2_mark_bitmap (bmap, block);
      block = ext2_inode_bitmap_loc (sb, i);
      if (block != 0)
	ext2_mark_bitmap (bmap, block);
    }
  return 0;
}

static int
ext2_make_bitmap_32 (VFSSuperblock *sb, int magic, uint32_t start, uint32_t end,
		     uint32_t real_end, char *initmap, Ext2Bitmap **result)
{
  Ext2Bitmap32 *bmap;
  size_t size;

  bmap = kmalloc (sizeof (Ext2Bitmap32));
  if (unlikely (bmap == NULL))
    return -ENOMEM;

  bmap->b_magic = magic;
  bmap->b_start = start;
  bmap->b_end = end;
  bmap->b_real_end = real_end;

  size = (size_t) ((bmap->b_real_end - bmap->b_start) / 8 + 1);
  size = (size + 7) & ~3;

  bmap->b_bitmap = kmalloc (size);
  if (unlikely (bmap->b_bitmap == NULL))
    {
      kfree (bmap);
      return -ENOMEM;
    }

  if (initmap != NULL)
    memcpy (bmap->b_bitmap, initmap, size);
  else
    memset (bmap->b_bitmap, 0, size);
  *result = (Ext2Bitmap *) bmap;
  return 0;
}

/* TODO Support making rbtree bitmaps */

static int
ext2_make_bitmap_64 (VFSSuperblock *sb, int magic, Ext2BitmapType type,
		     uint64_t start, uint64_t end, uint64_t real_end,
		     Ext2Bitmap **result)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Bitmap64 *bmap;
  Ext2BitmapOps *ops;
  int ret;

  if (type == EXT2_BMAP64_BITARRAY)
    ops = &ext2_bitarray_ops;
  else
    return -EINVAL;

  bmap = kmalloc (sizeof (Ext2Bitmap64));
  if (unlikely (bmap == NULL))
    return -ENOMEM;

  bmap->b_magic = magic;
  bmap->b_start = start;
  bmap->b_end = end;
  bmap->b_real_end = real_end;
  bmap->b_ops = ops;
  if (magic == EXT2_BMAP_MAGIC_BLOCK64)
    bmap->b_cluster_bits = fs->f_cluster_ratio_bits;
  else
    bmap->b_cluster_bits = 0;

  ret = bmap->b_ops->b_new_bmap (sb, bmap);
  if (ret != 0)
    {
      kfree (bmap);
      return ret;
    }
  *result = (Ext2Bitmap *) bmap;
  return 0;
}

static int
ext2_allocate_block_bitmap (VFSSuperblock *sb, Ext2Bitmap **result)
{
  Ext2Filesystem *fs = sb->sb_private;
  uint64_t start = EXT2_B2C (fs, fs->f_super.s_first_data_block);
  uint64_t end = EXT2_B2C (fs, ext2_blocks_count (&fs->f_super) - 1);
  uint64_t real_end = (uint64_t) fs->f_super.s_clusters_per_group *
    (uint64_t) fs->f_group_desc_count - 1 + start;
  if (fs->f_flags & EXT2_FLAG_64BIT)
    return ext2_make_bitmap_64 (sb, EXT2_BMAP_MAGIC_BLOCK64,
				EXT2_BMAP64_BITARRAY, start, end, real_end,
				result);
  if (end > 0xffffffff || real_end > 0xffffffff)
    return -EINVAL;
  return ext2_make_bitmap_32 (sb, EXT2_BMAP_MAGIC_BLOCK, start, end, real_end,
			      NULL, result);
}

static int
ext2_allocate_inode_bitmap (VFSSuperblock *sb, Ext2Bitmap **result)
{
  Ext2Filesystem *fs = sb->sb_private;
  uint64_t start = 1;
  uint64_t end = fs->f_super.s_inodes_count;
  uint64_t real_end =
    (uint64_t) fs->f_super.s_inodes_per_group * fs->f_group_desc_count;
  if (fs->f_flags & EXT2_FLAG_64BIT)
    return ext2_make_bitmap_64 (sb, EXT2_BMAP_MAGIC_BLOCK64,
				EXT2_BMAP64_BITARRAY, start, end, real_end,
				result);
  if (end > 0xffffffff || real_end > 0xffffffff)
    return -EINVAL;
  return ext2_make_bitmap_32 (sb, EXT2_BMAP_MAGIC_BLOCK, start, end, real_end,
			      NULL, result);
}

static void
ext2_free_bitmap (Ext2Bitmap *bmap)
{
  Ext2Bitmap64 *b = (Ext2Bitmap64 *) bmap;
  if (b == NULL)
    return;
  if (EXT2_BITMAP_IS_32 (b))
    {
      Ext2Bitmap32 *b32 = (Ext2Bitmap32 *) bmap;
      if (!EXT2_BMAP_MAGIC_VALID (b32->b_magic))
	return;
      b32->b_magic = 0;
      if (b32->b_bitmap != NULL)
	{
	  kfree (b32->b_bitmap);
	  b32->b_bitmap = NULL;
	}
      kfree (b32);
      return;
    }
  if (!EXT2_BITMAP_IS_64 (b))
    return;
  b->b_ops->b_free_bmap (b);
  b->b_magic = 0;
  kfree (b);
}

static int
ext2_prepare_read_bitmap (VFSSuperblock *sb, int flags)
{
  Ext2Filesystem *fs = sb->sb_private;
  int block_nbytes = fs->f_super.s_clusters_per_group / 8;
  int inode_nbytes = fs->f_super.s_inodes_per_group / 8;
  int ret;
  if (unlikely (block_nbytes > sb->sb_blksize || inode_nbytes > sb->sb_blksize))
    return -EINVAL;

  if (flags & EXT2_BITMAP_BLOCK)
    {
      if (fs->f_block_bitmap != NULL)
	ext2_free_bitmap (fs->f_block_bitmap);
      ret = ext2_allocate_block_bitmap (sb, &fs->f_block_bitmap);
      if (ret != 0)
	{
	  ext2_free_bitmap (fs->f_block_bitmap);
	  fs->f_block_bitmap = NULL;
	  return ret;
	}
    }

  if (flags & EXT2_BITMAP_INODE)
    {
      if (fs->f_inode_bitmap != NULL)
	ext2_free_bitmap (fs->f_inode_bitmap);
      ret = ext2_allocate_inode_bitmap (sb, &fs->f_inode_bitmap);
      if (ret != 0)
	{
	  ext2_free_bitmap (fs->f_inode_bitmap);
	  fs->f_inode_bitmap = NULL;
	  return ret;
	}
    }
  return 0;
}

static int
ext2_read_bitmap_start (VFSSuperblock *sb, int flags, unsigned int start,
			unsigned int end)
{
  Ext2Filesystem *fs = sb->sb_private;
  char *block_bitmap = NULL;
  char *inode_bitmap = NULL;
  int block_nbytes = fs->f_super.s_clusters_per_group / 8;
  int inode_nbytes = fs->f_super.s_inodes_per_group / 8;
  int csum_flag = ext2_has_group_desc_checksum (&fs->f_super);
  unsigned int count;
  unsigned int i;
  block_t block;
  block_t blkitr = EXT2_B2C (fs, fs->f_super.s_first_data_block);
  ino64_t inoitr = 1;
  int ret;

  if (flags & EXT2_BITMAP_BLOCK)
    {
      block_bitmap = kmalloc (sb->sb_blksize);
      if (unlikely (block_bitmap == NULL))
	goto end;
    }
  else
    block_nbytes = 0;

  if (flags & EXT2_BITMAP_INODE)
    {
      inode_bitmap = kmalloc (sb->sb_blksize);
      if (unlikely (inode_bitmap == NULL))
	goto end;
    }
  else
    inode_nbytes = 0;

  blkitr += (block_t) start * (block_nbytes << 3);
  inoitr += (block_t) start * (inode_nbytes << 3);
  for (i = start; i <= end; i++)
    {
      if (block_bitmap != NULL)
	{
	  block = ext2_block_bitmap_loc (sb, i);
	  if ((csum_flag && ext2_bg_test_flags (sb, i, EXT2_BG_BLOCK_UNINIT)
	       && ext2_group_desc_checksum_valid (sb, i))
	      || block >= ext2_blocks_count (&fs->f_super))
	    block = 0;
	  if (block != 0)
	    {
	      ret = ext2_read_blocks (block_bitmap, sb, block, 1);
	      if (ret != 0)
		goto end;
	      if (!ext2_block_bitmap_checksum_valid (sb, i, block_bitmap,
						     block_nbytes))
		{
		  ret = -EINVAL;
		  goto end;
		}
	    }
	  else
	    memset (block_bitmap, 0, block_nbytes);

	  count = block_nbytes << 3;
	  ret = ext2_set_bitmap_range (fs->f_block_bitmap, blkitr, count,
				       block_bitmap);
	  if (ret != 0)
	    goto end;
	  blkitr += block_nbytes << 3;
	}

      if (inode_bitmap != NULL)
	{
	  block = ext2_inode_bitmap_loc (sb, i);
	  if ((csum_flag && ext2_bg_test_flags (sb, i, EXT2_BG_INODE_UNINIT)
	       && ext2_group_desc_checksum_valid (sb, i))
	      || block >= ext2_blocks_count (&fs->f_super))
	    block = 0;
	  if (block != 0)
	    {
	      ret = ext2_read_blocks (inode_bitmap, sb, block, 1);
	      if (ret != 0)
		goto end;
	      if (!ext2_inode_bitmap_checksum_valid (sb, i, inode_bitmap,
						     inode_nbytes))
		{
		  ret = -EINVAL;
		  goto end;
		}
	    }
	  else
	    memset (inode_bitmap, 0, inode_nbytes);

	  count = inode_nbytes << 3;
	  ret = ext2_set_bitmap_range (fs->f_inode_bitmap, inoitr, count,
				       inode_bitmap);
	  if (ret != 0)
	    goto end;
	  inoitr += inode_nbytes << 3;
	}
    }

 end:
  if (block_bitmap != NULL)
    kfree (block_bitmap);
  if (inode_bitmap != NULL)
    kfree (inode_bitmap);
  return ret;
}

static int
ext2_read_bitmap_end (VFSSuperblock *sb, int flags)
{
  if (flags & EXT2_BITMAP_BLOCK)
    {
      int ret = ext2_mark_uninit_bg_group_blocks (sb);
      if (ret != 0)
	return ret;
    }
  return 0;
}

static void
ext2_clean_bitmap (VFSSuperblock *sb, int flags)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (flags & EXT2_BITMAP_BLOCK)
    {
      ext2_free_bitmap (fs->f_block_bitmap);
      fs->f_block_bitmap = NULL;
    }
  if (flags & EXT2_BITMAP_INODE)
    {
      ext2_free_bitmap (fs->f_inode_bitmap);
      fs->f_inode_bitmap = NULL;
    }
}

int
ext2_read_bitmap (VFSSuperblock *sb, int flags, unsigned int start,
		  unsigned int end)
{
  int ret = ext2_prepare_read_bitmap (sb, flags);
  if (ret != 0)
    return ret;
  ret = ext2_read_bitmap_start (sb, flags, start, end);
  if (ret == 0)
    ret = ext2_read_bitmap_end (sb, flags);
  if (ret != 0)
    ext2_clean_bitmap (sb, flags);
  return ret;
}

int
ext2_read_bitmaps (VFSSuperblock *sb)
{
  Ext2Filesystem *fs = sb->sb_private;
  int flags = 0;
  if (fs->f_inode_bitmap == NULL)
    flags |= EXT2_BITMAP_INODE;
  if (fs->f_block_bitmap == NULL)
    flags |= EXT2_BITMAP_BLOCK;
  if (flags == 0)
    return 0;
  return ext2_read_bitmap (sb, flags, 0, fs->f_group_desc_count - 1);
}

int
ext2_write_bitmaps (VFSSuperblock *sb)
{
  Ext2Filesystem *fs = sb->sb_private;
  int do_inode =
    fs->f_inode_bitmap != NULL && (fs->f_flags & EXT2_FLAG_IB_DIRTY);
  int do_block =
    fs->f_block_bitmap != NULL && (fs->f_flags & EXT2_FLAG_BB_DIRTY);
  unsigned int i;
  unsigned int j;
  int block_nbytes;
  int inode_nbytes;
  unsigned int nbits;
  char *blockbuf = NULL;
  char *inodebuf = NULL;
  int csum_flag;
  block_t block;
  block_t blkitr = EXT2_B2C (fs, fs->f_super.s_first_data_block);
  ino64_t inoitr = 1;
  int ret;
  if (sb->sb_mntflags & MNT_RDONLY)
    return -EROFS;
  if (!do_block && !do_inode)
    return 0;
  csum_flag = ext2_has_group_desc_checksum (&fs->f_super);

  block_nbytes = 0;
  inode_nbytes = 0;
  if (do_block)
    {
      block_nbytes = fs->f_super.s_clusters_per_group / 8;
      blockbuf = kmalloc (sb->sb_blksize);
      if (unlikely (blockbuf == NULL))
	goto err;
      memset (blockbuf, 0xff, sb->sb_blksize);
    }
  if (do_inode)
    {
      inode_nbytes = (size_t) ((fs->f_super.s_inodes_per_group + 7) / 8);
      inodebuf = kmalloc (sb->sb_blksize);
      if (unlikely (inodebuf == NULL))
	goto err;
      memset (inodebuf, 0xff, sb->sb_blksize);
    }

  for (i = 0; i < fs->f_group_desc_count; i++)
    {
      if (!do_block)
	goto skip_block;
      if (csum_flag && ext2_bg_test_flags (sb, i, EXT2_BG_BLOCK_UNINIT))
	goto skip_curr_block;

      ret = ext2_get_bitmap_range (fs->f_block_bitmap, blkitr,
				   block_nbytes << 3, blockbuf);
      if (ret != 0)
	goto err;
      if (i == fs->f_group_desc_count - 1)
	{
	  nbits = EXT2_NUM_B2C (fs, (ext2_blocks_count (&fs->f_super) -
				     (block_t) fs->f_super.s_first_data_block)
				% (block_t) fs->f_super.s_blocks_per_group);
	  if (nbits != 0)
	    {
	      for (j = nbits; j < sb->sb_blksize * 8; j++)
		fast_set_bit (blockbuf, j);
	    }
	}

      ext2_block_bitmap_checksum_update (sb, i, blockbuf, block_nbytes);
      ext2_group_desc_checksum_update (sb, i);
      fs->f_flags |= EXT2_FLAG_DIRTY;

      block = ext2_block_bitmap_loc (sb, i);
      if (block != 0)
	{
	  ret = ext2_write_blocks (blockbuf, sb, block, 1);
	  if (ret != 0)
	    goto err;
	}

    skip_curr_block:
      blkitr += block_nbytes << 3;

    skip_block:
      if (!do_inode)
	continue;
      if (csum_flag && ext2_bg_test_flags (sb, i, EXT2_BG_INODE_UNINIT))
	goto skip_curr_inode;

      ret = ext2_get_bitmap_range (fs->f_inode_bitmap, inoitr,
				   inode_nbytes << 3, inodebuf);
      if (ret != 0)
	goto err;

      ext2_inode_bitmap_checksum_update (sb, i, inodebuf, inode_nbytes);
      ext2_group_desc_checksum_update (sb, i);
      fs->f_flags |= EXT2_FLAG_DIRTY;

      block = ext2_inode_bitmap_loc (sb, i);
      if (block != 0)
	{
	  ret = ext2_write_blocks (inodebuf, sb, block, 1);
	  if (ret != 0)
	    goto err;
	}

    skip_curr_inode:
      inoitr += inode_nbytes << 3;
    }

  if (do_block)
    {
      fs->f_flags &= ~EXT2_FLAG_BB_DIRTY;
      kfree (blockbuf);
    }
  if (do_inode)
    {
      fs->f_flags &= ~EXT2_FLAG_IB_DIRTY;
      kfree (inodebuf);
    }
  return 0;

 err:
  if (inodebuf != NULL)
    kfree (inodebuf);
  if (blockbuf != NULL)
    kfree (blockbuf);
  return ret;
}

void
ext2_mark_bitmap (Ext2Bitmap *bmap, uint64_t arg)
{
  Ext2Bitmap64 *b = (Ext2Bitmap64 *) bmap;
  if (b == NULL)
    return;
  if (EXT2_BITMAP_IS_32 (b))
    {
      Ext2Bitmap32 *b32 = (Ext2Bitmap32 *) bmap;
      if (arg & ~0xffffffffULL)
	return;
      if (arg < b32->b_start || arg > b32->b_end)
	return;
      fast_set_bit (b32->b_bitmap, arg - b32->b_start);
    }
  else
    {
      arg >>= b->b_cluster_bits;
      if (arg < b->b_start || arg > b->b_end)
	return;
      b->b_ops->b_mark_bmap (b, arg);
    }
}

void
ext2_unmark_bitmap (Ext2Bitmap *bmap, uint64_t arg)
{
  Ext2Bitmap64 *b = (Ext2Bitmap64 *) bmap;
  if (b == NULL)
    return;
  if (EXT2_BITMAP_IS_32 (b))
    {
      Ext2Bitmap32 *b32 = (Ext2Bitmap32 *) bmap;
      if (arg & ~0xffffffffULL)
	return;
      if (arg < b32->b_start || arg > b32->b_end)
	return;
      fast_clear_bit (b32->b_bitmap, arg - b32->b_start);
    }
  else
    {
      arg >>= b->b_cluster_bits;
      if (arg < b->b_start || arg > b->b_end)
	return;
      b->b_ops->b_unmark_bmap (b, arg);
    }
}

int
ext2_test_bitmap (Ext2Bitmap *bmap, uint64_t arg)
{
  Ext2Bitmap64 *b = (Ext2Bitmap64 *) bmap;
  if (b == NULL)
    return 0;
  if (EXT2_BITMAP_IS_32 (b))
    {
      Ext2Bitmap32 *b32 = (Ext2Bitmap32 *) bmap;
      if (arg & ~0xffffffffULL)
	return 0;
      if (arg < b32->b_start || arg > b32->b_end)
	return 0;
      return fast_test_bit (b32->b_bitmap, arg - b32->b_start);
    }
  else
    {
      arg >>= b->b_cluster_bits;
      if (arg < b->b_start || arg > b->b_end)
	return 0;
      return b->b_ops->b_test_bmap (b, arg);
    }
}

void
ext2_mark_block_bitmap_range (Ext2Bitmap *bmap, block_t block, blkcnt64_t num)
{
  Ext2Bitmap64 *b = (Ext2Bitmap64 *) bmap;
  block_t end = block + num;
  if (b == NULL)
    return;
  if (EXT2_BITMAP_IS_32 (b))
    {
      Ext2Bitmap32 *b32 = (Ext2Bitmap32 *) bmap;
      int i;
      if ((block & ~0xffffffffULL) || ((block + num - 1) & ~0xffffffffULL))
	return;
      if (block < b32->b_start || block > b32->b_end
	  || block + num - 1 > b32->b_end)
	return;
      for (i = 0; i < num; i++)
	fast_set_bit (b32->b_bitmap, block + i - b32->b_start);
    }
  if (!EXT2_BITMAP_IS_64 (b))
    return;
  block >>= b->b_cluster_bits;
  end += (1 << b->b_cluster_bits) - 1;
  end >>= b->b_cluster_bits;
  num = end - block;
  if (block < b->b_start || block > b->b_end || block + num - 1 > b->b_end)
    return;
  b->b_ops->b_mark_bmap_extent (b, block, num);
}

int
ext2_set_bitmap_range (Ext2Bitmap *bmap, uint64_t start, unsigned int num,
		       void *data)
{
  Ext2Bitmap64 *b = (Ext2Bitmap64 *) bmap;
  if (b == NULL)
    return -EINVAL;
  if (EXT2_BITMAP_IS_32 (b))
    {
      Ext2Bitmap32 *b32 = (Ext2Bitmap32 *) bmap;
      if ((start + num - 1) & ~0xffffffffULL)
	return -EINVAL;
      if (start < b32->b_start || start + num - 1 > b32->b_real_end)
	return -EINVAL;
      memcpy (b32->b_bitmap + ((start - b32->b_start) >> 3), data,
	      (num + 7) >> 3);
      return 0;
    }
  if (!EXT2_BITMAP_IS_64 (b))
    return -EINVAL;
  b->b_ops->b_set_bmap_range (b, start, num, data);
  return 0;
}

int
ext2_get_bitmap_range (Ext2Bitmap *bmap, uint64_t start, unsigned int num,
		       void *data)
{
  Ext2Bitmap64 *b = (Ext2Bitmap64 *) bmap;
  if (b == NULL)
    return -EINVAL;
  if (EXT2_BITMAP_IS_32 (b))
    {
      Ext2Bitmap32 *b32 = (Ext2Bitmap32 *) bmap;
      if ((start + num - 1) & ~0xffffffffULL)
	return -EINVAL;
      if (start < b32->b_start || start + num - 1 > b32->b_real_end)
	return -EINVAL;
      memcpy (data, b32->b_bitmap + ((start - b32->b_start) >> 3),
	      (num + 7) >> 3);
      return 0;
    }
  if (!EXT2_BITMAP_IS_64 (b))
    return -EINVAL;
  b->b_ops->b_get_bmap_range (b, start, num, data);
  return 0;
}

int
ext2_find_first_zero_bitmap (Ext2Bitmap *bmap, block_t start, block_t end,
			     block_t *result)
{
  Ext2Bitmap64 *b = (Ext2Bitmap64 *) bmap;
  uint64_t cstart;
  uint64_t cend;
  uint64_t cout;
  int ret;

  if (bmap == NULL)
    return -EINVAL;
  if (EXT2_BITMAP_IS_32 (bmap))
    {
      Ext2Bitmap32 *b32 = (Ext2Bitmap32 *) bmap;
      uint32_t block;
      if ((start & ~0xffffffffULL) || (end & ~0xffffffffULL))
	return -EINVAL;
      if (start < b32->b_start || end > b32->b_end || start > end)
	return -EINVAL;
      while (start <= end)
	{
	  block = fast_test_bit (b32->b_bitmap, start - b32->b_start);
	  if (block == 0)
	    {
	      *result = start;
	      return 0;
	    }
	  start++;
	}
      return -ENOENT;
    }
  if (!EXT2_BITMAP_IS_64 (bmap))
    return -EINVAL;

  cstart = start >> b->b_cluster_bits;
  cend = start >> b->b_cluster_bits;
  if (cstart < b->b_start || cend > b->b_end || start > end)
    return -EINVAL;
  if (b->b_ops->b_find_first_zero != NULL)
    {
      ret = b->b_ops->b_find_first_zero (b, cstart, cend, &cout);
      if (ret != 0)
	return ret;
    found:
      cout <<= b->b_cluster_bits;
      *result = cout >= start ? cout : start;
      return 0;
    }

  for (cout = cstart; cout <= cend; cout++)
    {
      if (!b->b_ops->b_test_bmap (b, cout))
	goto found;
    }
  return -ENOENT;
}
