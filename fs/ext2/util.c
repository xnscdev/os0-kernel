/*************************************************************************
 * util.c -- This file is part of OS/0.                                  *
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

#include <bits/mount.h>
#include <fs/ext2.h>
#include <libk/libk.h>
#include <sys/ata.h>
#include <sys/process.h>
#include <vm/heap.h>

#define EXT2_INDIR1_THRESH (blksize + EXT2_STORED_INODES)
#define EXT2_INDIR1_BLOCK0 (block - EXT2_STORED_INODES)

#define EXT2_INDIR2_THRESH (blksize * blksize + EXT2_INDIR1_THRESH)
#define EXT2_INDIR2_BLOCK0 (block - EXT2_INDIR1_THRESH)
#define EXT2_INDIR2_BLOCK1 (bptr1[realblock / blksize])
#define EXT2_INDIR2_BLOCK2 (bptr2[realblock % blksize])

#define EXT2_INDIR3_THRESH (blksize * blksize * blksize + EXT2_INDIR2_THRESH)
#define EXT2_INDIR3_BLOCK0 (block - EXT2_INDIR2_THRESH)
#define EXT2_INDIR3_BLOCK1 (bptr1[realblock / (blksize * blksize)])
#define EXT2_INDIR3_BLOCK2 (bptr2[realblock % (blksize * blksize) / blksize])
#define EXT2_INDIR3_BLOCK3 (bptr3[realblock % blksize])

static block_t
ext2_try_alloc_block (VFSSuperblock *sb, int index)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Superblock *esb = &fs->f_super;
  SpecDevice *dev = sb->sb_dev;
  Ext2GroupDesc *bgdt = fs->f_group_desc;
  unsigned char *busage;
  int ret;
  int i;
  if (bgdt[index].bg_free_blocks_count == 0)
    return -ENOSPC; /* No free block */
  busage = kmalloc (esb->s_blocks_per_group >> 3);
  if (unlikely (busage == NULL))
    return -ENOMEM;
  ret = dev->sd_read (dev, busage, esb->s_blocks_per_group >> 3,
		      bgdt[index].bg_block_bitmap * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (busage);
      return ret;
    }
  for (i = 0; i < esb->s_blocks_per_group; i++)
    {
      uint32_t byte = i >> 3;
      uint32_t offset = i % 8;
      if (busage[byte] & 1 << offset)
	continue;

      /* Mark the block as allocated */
      busage[byte] |= 1 << offset;
      ret = dev->sd_write (dev, busage, esb->s_blocks_per_group >> 3,
			   bgdt[index].bg_block_bitmap * sb->sb_blksize);
      kfree (busage);
      if (ret != 0)
	return ret;

      /* Subtract from free blocks in superblock and BGDT */
      esb->s_free_blocks_count--;
      bgdt[index].bg_free_blocks_count--;
      ext2_update (sb);

      return (block_t) esb->s_blocks_per_group * index + i;
    }
  kfree (busage);
  return -ENOSPC;
}

static ino64_t
ext2_try_create_inode (VFSSuperblock *sb, int index)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Superblock *esb = &fs->f_super;
  SpecDevice *dev = sb->sb_dev;
  Ext2GroupDesc *bgdt = fs->f_group_desc;
  unsigned char *iusage;
  int ret;
  int i;
  if (bgdt[index].bg_free_inodes_count == 0)
    return 0; /* No free inodes */
  iusage = kmalloc (esb->s_inodes_per_group >> 3);
  if (unlikely (iusage == NULL))
    return 0;
  ret = dev->sd_read (dev, iusage, esb->s_inodes_per_group >> 3,
		      bgdt[index].bg_inode_bitmap * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (iusage);
      return ret;
    }
  for (i = 0; i < esb->s_inodes_per_group; i++)
    {
      uint32_t byte = i >> 3;
      uint32_t offset = i % 8;
      if (iusage[byte] & 1 << offset)
	continue;

      /* Mark the inode as allocated */
      iusage[byte] |= 1 << offset;
      ret = dev->sd_write (dev, iusage, esb->s_inodes_per_group >> 3,
			   bgdt[index].bg_inode_bitmap * sb->sb_blksize);
      kfree (iusage);
      if (ret != 0)
	return ret;

      /* Subtract from free inodes in superblock and BGDT */
      esb->s_free_inodes_count--;
      bgdt[index].bg_free_inodes_count--;
      ext2_update (sb);

      return (ino64_t) esb->s_inodes_per_group * index + i + 1;
    }
  kfree (iusage);
  return 0;
}

static int
ext2_unalloc_block (VFSSuperblock *sb, block_t block)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Superblock *esb = &fs->f_super;
  SpecDevice *dev = sb->sb_dev;
  Ext2GroupDesc *bgdt = fs->f_group_desc;
  unsigned char *busage;
  uint32_t blkgrp = block / esb->s_blocks_per_group;
  uint32_t rem = block % esb->s_blocks_per_group;
  uint32_t byte = rem >> 3;
  uint32_t offset = rem % 8;
  int ret;
  busage = kmalloc (esb->s_blocks_per_group >> 3);
  if (unlikely (busage == NULL))
    return 0;
  ret = dev->sd_read (dev, busage, esb->s_blocks_per_group >> 3,
		      bgdt[blkgrp].bg_block_bitmap * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (busage);
      return ret;
    }

  /* Mark the block as unallocated */
  busage[byte] &= ~(1 << offset);
  ret = dev->sd_write (dev, busage, esb->s_blocks_per_group >> 3,
		       bgdt[blkgrp].bg_block_bitmap * sb->sb_blksize);
  kfree (busage);
  if (ret != 0)
    return ret;

  /* Add to free blocks in superblock and BGDT */
  esb->s_free_blocks_count++;
  bgdt[blkgrp].bg_free_blocks_count++;
  ext2_update (sb);
  return 0;
}

static int
ext2_clear_inode (VFSSuperblock *sb, ino64_t inode)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Superblock *esb = &fs->f_super;
  SpecDevice *dev = sb->sb_dev;
  Ext2GroupDesc *bgdt = fs->f_group_desc;
  unsigned char *iusage;
  uint32_t blkgrp = inode / esb->s_inodes_per_group;
  uint32_t rem = inode % esb->s_inodes_per_group;
  uint32_t byte = rem >> 3;
  uint32_t offset = rem % 8;
  int ret;
  iusage = kmalloc (esb->s_inodes_per_group >> 3);
  if (unlikely (iusage == NULL))
    return 0;
  ret = dev->sd_read (dev, iusage, esb->s_inodes_per_group >> 3,
		      bgdt[blkgrp].bg_inode_bitmap * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (iusage);
      return ret;
    }

  /* Mark the inode as unallocated */
  iusage[byte] &= ~(1 << offset);
  ret = dev->sd_write (dev, iusage, esb->s_inodes_per_group >> 3,
		       bgdt[blkgrp].bg_inode_bitmap * sb->sb_blksize);
  kfree (iusage);
  if (ret != 0)
    return ret;

  /* Add to free inodes in superblock and BGDT */
  esb->s_free_inodes_count++;
  bgdt[blkgrp].bg_free_inodes_count++;
  ext2_update (sb);
  return 0;
}

static int
ext2_try_unalloc_pointer (VFSInode *inode, block_t block, uint32_t *data)
{
  int i;
  for (i = 0; i < inode->vi_sb->sb_blksize >> 2; i++)
    {
      if (data[i] != 0)
	return 1;
    }
  return ext2_unalloc_block (inode->vi_sb, block);
}

static int
ext2_bg_super_test_root (unsigned int group, unsigned int x)
{
  while (1)
    {
      if (group < x)
	return 0;
      if (group == x)
	return 1;
      if (group % x)
	return 0;
      group /= x;
    }
}

static void
ext2_clear_block_uninit (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (group >= fs->f_group_desc_count
      || !ext2_has_group_desc_checksum (&fs->f_super)
      || !ext2_bg_test_flags (sb, group, EXT2_BG_BLOCK_UNINIT))
    return;
  ext2_bg_clear_flags (sb, group, EXT2_BG_BLOCK_UNINIT);
  ext2_group_desc_checksum_update (sb, group);
  fs->f_flags |= EXT2_FLAG_CHANGED | EXT2_FLAG_DIRTY | EXT2_FLAG_BB_DIRTY;
}

static void
ext2_check_inode_uninit (VFSSuperblock *sb, Ext2Bitmap *map, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  ino64_t ino;
  ino64_t i;
  if (group >= fs->f_group_desc_count
      || !ext2_has_group_desc_checksum (&fs->f_super)
      || !ext2_bg_test_flags (sb, group, EXT2_BG_INODE_UNINIT))
    return;
  ino = group * fs->f_super.s_inodes_per_group + 1;
  for (i = 0; i < fs->f_super.s_inodes_per_group; i++, ino++)
    ext2_unmark_bitmap (map, ino);
  ext2_bg_clear_flags (sb, group, EXT2_BG_INODE_UNINIT | EXT2_BG_BLOCK_UNINIT);
  ext2_group_desc_checksum_update (sb, group);
  fs->f_flags |= EXT2_FLAG_CHANGED | EXT2_FLAG_DIRTY | EXT2_FLAG_IB_DIRTY;
}

static int
ext2_file_zero_remainder (Ext2File *file, off64_t offset)
{
  blksize_t blksize = file->f_sb->sb_blksize;
  off64_t off = offset % blksize;
  char *b = NULL;
  block_t block;
  int retflags;
  int ret;
  if (off == 0)
    return 0;

  ret = ext2_sync_file_buffer_pos (file);
  if (ret != 0)
    return ret;

  ret = ext2_bmap (file->f_sb, file->f_ino, NULL, NULL, 0, offset / blksize,
		   &retflags, &block);
  if (ret != 0)
    return ret;
  if (block == 0 || (retflags & BMAP_RET_UNINIT))
    return 0;

  b = kmalloc (blksize);
  if (unlikely (b == NULL))
    return -ENOMEM;
  ret = ext2_read_blocks (b, file->f_sb, block, 1);
  if (ret != 0)
    {
      kfree (b);
      return ret;
    }
  memset (b + off, 0, blksize - off);
  ret = ext2_write_blocks (b, file->f_sb, block, 1);
  kfree (b);
  return ret;
}

static int
ext2_check_zero_block (char *buffer, blksize_t blksize)
{
  blksize_t left = blksize;
  char *ptr = buffer;
  while (left > 0)
    {
      if (*ptr++ != 0)
	return 0;
      left--;
    }
  return 1;
}

static int
ext2_dealloc_indirect_block (VFSSuperblock *sb, Ext2Inode *inode,
			     char *blockbuf, uint32_t *p, int level,
			     block_t start, block_t count, int max)
{
  Ext2Filesystem *fs = sb->sb_private;
  block_t offset;
  block_t inc;
  uint32_t b;
  int freed = 0;
  int i;
  int ret;

  inc = 1ULL << ((EXT2_BLOCK_SIZE_BITS (fs->f_super) - 2) * level);
  for (i = 0, offset = 0; i < max; i++, p++, offset += inc)
    {
      if (offset >= start + count)
	break;
      if (*p == 0 || offset + inc <= start)
	continue;
      b = *p;
      if (level > 0)
	{
	  uint32_t s;
	  ret = ext2_read_blocks (blockbuf, sb, b, 1);
	  if (ret != 0)
	    return ret;
	  s = start > offset ? start - offset : 0;
	  ret = ext2_dealloc_indirect_block (sb, inode,
					     blockbuf + sb->sb_blksize,
					     (uint32_t *) blockbuf, level - 1,
					     s, count - offset,
					     sb->sb_blksize >> 2);
	  if (ret != 0)
	    return ret;
	  ret = ext2_write_blocks (blockbuf, sb, b, 1);
	  if (ret != 0)
	    return ret;
	  if (!ext2_check_zero_block (blockbuf, sb->sb_blksize))
	    continue;
	}
      ext2_block_alloc_stats (sb, b, -1);
      *p = 0;
      freed++;
    }
  return ext2_iblk_sub_blocks (sb, inode, freed);
}

static int
ext2_dealloc_indirect (VFSSuperblock *sb, Ext2Inode *inode, char *blockbuf,
		       block_t start, block_t end)
{
  char *buffer = NULL;
  int num = EXT2_NDIR_BLOCKS;
  uint32_t *bp = inode->i_block;
  uint32_t addr_per_block;
  block_t max = EXT2_NDIR_BLOCKS;
  block_t count;
  int level;
  int ret = 0;
  if (start > 0xffffffff)
    return 0;
  if (end >= 0xffffffff || end - start + 1 >= 0xffffffff)
    count = ~start;
  else
    count = end - start + 1;

  if (blockbuf == NULL)
    {
      buffer = kmalloc (sb->sb_blksize * 3);
      if (unlikely (buffer == NULL))
	return -ENOMEM;
      blockbuf = buffer;
    }
  addr_per_block = sb->sb_blksize >> 2;

  for (level = 0; level < 4; level++, max *= addr_per_block)
    {
      if (start < max)
	{
	  ret = ext2_dealloc_indirect_block (sb, inode, blockbuf, bp, level,
					     start, count, num);
	  if (ret != 0)
	    goto end;
	  if (count > max)
	    count -= max - start;
	  else
	    break;
	  start = 0;
	}
      else
	start -= max;
      bp += num;
      if (level == 0)
	{
	  num = 1;
	  max = 1;
	}
    }

 end:
  if (buffer != NULL)
    kfree (buffer);
  return ret;
}

static int
ext2_block_iterate_ind (uint32_t *ind_block, uint32_t ref_block, int ref_offset,
			Ext2BlockContext *ctx)
{
  Ext2Filesystem *fs = ctx->b_sb->sb_private;
  int changed = 0;
  int flags;
  int limit;
  int offset;
  uint32_t *blockno;
  block_t block;
  int ret = 0;
  int i;

  limit = ctx->b_sb->sb_blksize;
  if (!(ctx->b_flags & BLOCK_FLAG_DEPTH_TRAVERSE)
      && !(ctx->b_flags & BLOCK_FLAG_DATA_ONLY))
    {
      block = *ind_block;
      ret = ctx->b_func (ctx->b_sb, &block, BLOCK_COUNT_IND, ref_block,
			 ref_offset, ctx->b_private);
      *ind_block = (uint32_t) block;
    }
  if ((ctx->b_flags & BLOCK_FLAG_READ_ONLY) && (ret & BLOCK_CHANGED))
    {
      ctx->b_err = -EROFS;
      return ret | BLOCK_ABORT | BLOCK_ERROR;
    }

  if (*ind_block == 0 || (ret & BLOCK_ABORT))
    {
      ctx->b_blkcnt += limit;
      return ret;
    }
  if (*ind_block >= ext2_blocks_count (&fs->f_super)
      || *ind_block < fs->f_super.s_first_data_block)
    {
      ctx->b_err = -EINVAL;
      return ret | BLOCK_ERROR;
    }
  ctx->b_err = ext2_read_blocks (ctx->b_ind_buf, ctx->b_sb, *ind_block, 1);
  if (ctx->b_err != 0)
    return ret | BLOCK_ERROR;

  blockno = (uint32_t *) ctx->b_ind_buf;
  offset = 0;
  if (ctx->b_flags & BLOCK_FLAG_APPEND)
    {
      for (i = 0; i < limit; i++, ctx->b_blkcnt++, blockno++)
	{
	  block = *blockno;
	  flags = ctx->b_func (ctx->b_sb, &block, ctx->b_blkcnt, *ind_block,
			       offset, ctx->b_private);
	  *blockno = (uint32_t) block;
	  changed |= flags;
	  if (flags & BLOCK_ABORT)
	    {
	      ret |= BLOCK_ABORT;
	      break;
	    }
	  offset += 4;
	}
    }
  else
    {
      for (i = 0; i < limit; i++, ctx->b_blkcnt++, blockno++)
	{
	  if (*blockno == 0)
	    goto skip_sparse;
	  block = *blockno;
	  flags = ctx->b_func (ctx->b_sb, &block, ctx->b_blkcnt, *ind_block,
			       offset, ctx->b_private);
	  *blockno = (uint32_t) block;
	  changed |= flags;
	  if (flags & BLOCK_ABORT)
	    {
	      ret |= BLOCK_ABORT;
	      break;
	    }

	skip_sparse:
	  offset += 4;
	}
    }

  if ((ctx->b_flags & BLOCK_FLAG_READ_ONLY) && (changed & BLOCK_CHANGED))
    {
      ctx->b_err = -EROFS;
      return changed | BLOCK_ABORT | BLOCK_ERROR;
    }
  if (changed & BLOCK_CHANGED)
    {
      ctx->b_err = ext2_write_blocks (ctx->b_ind_buf, ctx->b_sb, *ind_block, 1);
      if (ctx->b_err != 0)
	ret |= BLOCK_ERROR | BLOCK_ABORT;
    }
  if ((ctx->b_flags & BLOCK_FLAG_DEPTH_TRAVERSE)
      && !(ctx->b_flags & BLOCK_FLAG_DATA_ONLY) && !(ret & BLOCK_ABORT))
    {
      block = *ind_block;
      ret |= ctx->b_func (ctx->b_sb, &block, BLOCK_COUNT_IND, ref_block,
			  ref_offset, ctx->b_private);
      *ind_block = (uint32_t) block;
    }
  if ((ctx->b_flags & BLOCK_FLAG_READ_ONLY) && (ret & BLOCK_CHANGED))
    {
      ctx->b_err = -EROFS;
      return ret | BLOCK_ABORT | BLOCK_ERROR;
    }
  return ret;
}

static int
ext2_block_iterate_dind (uint32_t *dind_block, uint32_t ref_block,
			 int ref_offset, Ext2BlockContext *ctx)
{
  Ext2Filesystem *fs = ctx->b_sb->sb_private;
  int changed = 0;
  int flags;
  int limit;
  int offset;
  uint32_t *blockno;
  block_t block;
  int ret = 0;
  int i;

  limit = ctx->b_sb->sb_blksize;
  if (!(ctx->b_flags & BLOCK_FLAG_DEPTH_TRAVERSE)
      && !(ctx->b_flags & BLOCK_FLAG_DATA_ONLY))
    {
      block = *dind_block;
      ret = ctx->b_func (ctx->b_sb, &block, BLOCK_COUNT_DIND, ref_block,
			 ref_offset, ctx->b_private);
      *dind_block = (uint32_t) block;
    }
  if ((ctx->b_flags & BLOCK_FLAG_READ_ONLY) && (ret & BLOCK_CHANGED))
    {
      ctx->b_err = -EROFS;
      return ret | BLOCK_ABORT | BLOCK_ERROR;
    }

  if (*dind_block == 0 || (ret & BLOCK_ABORT))
    {
      ctx->b_blkcnt += limit * limit;
      return ret;
    }
  if (*dind_block >= ext2_blocks_count (&fs->f_super)
      || *dind_block < fs->f_super.s_first_data_block)
    {
      ctx->b_err = -EINVAL;
      return ret | BLOCK_ERROR;
    }
  ctx->b_err = ext2_read_blocks (ctx->b_dind_buf, ctx->b_sb, *dind_block, 1);
  if (ctx->b_err != 0)
    return ret | BLOCK_ERROR;

  blockno = (uint32_t *) ctx->b_dind_buf;
  offset = 0;
  if (ctx->b_flags & BLOCK_FLAG_APPEND)
    {
      for (i = 0; i < limit; i++, ctx->b_blkcnt++, blockno++)
	{
	  flags = ext2_block_iterate_ind (blockno, *dind_block, offset, ctx);
	  changed |= flags;
	  if (flags & (BLOCK_ABORT | BLOCK_ERROR))
	    {
	      ret |= flags & (BLOCK_ABORT | BLOCK_ERROR);
	      break;
	    }
	  offset += 4;
	}
    }
  else
    {
      for (i = 0; i < limit; i++, ctx->b_blkcnt++, blockno++)
	{
	  if (*blockno == 0)
	    {
	      ctx->b_blkcnt += limit;
	      continue;
	    }
	  flags = ext2_block_iterate_ind (blockno, *dind_block, offset, ctx);
	  changed |= flags;
	  if (flags & (BLOCK_ABORT | BLOCK_ERROR))
	    {
	      ret |= flags & (BLOCK_ABORT | BLOCK_ERROR);
	      break;
	    }
	  offset += 4;
	}
    }

  if ((ctx->b_flags & BLOCK_FLAG_READ_ONLY) && (changed & BLOCK_CHANGED))
    {
      ctx->b_err = -EROFS;
      return changed | BLOCK_ABORT | BLOCK_ERROR;
    }
  if (changed & BLOCK_CHANGED)
    {
      ctx->b_err =
	ext2_write_blocks (ctx->b_dind_buf, ctx->b_sb, *dind_block, 1);
      if (ctx->b_err != 0)
	ret |= BLOCK_ERROR | BLOCK_ABORT;
    }
  if ((ctx->b_flags & BLOCK_FLAG_DEPTH_TRAVERSE)
      && !(ctx->b_flags & BLOCK_FLAG_DATA_ONLY) && !(ret & BLOCK_ABORT))
    {
      block = *dind_block;
      ret |= ctx->b_func (ctx->b_sb, &block, BLOCK_COUNT_DIND, ref_block,
			  ref_offset, ctx->b_private);
      *dind_block = (uint32_t) block;
    }
  if ((ctx->b_flags & BLOCK_FLAG_READ_ONLY) && (ret & BLOCK_CHANGED))
    {
      ctx->b_err = -EROFS;
      return ret | BLOCK_ABORT | BLOCK_ERROR;
    }
  return ret;
}

static int
ext2_block_iterate_tind (uint32_t *tind_block, uint32_t ref_block,
			 int ref_offset, Ext2BlockContext *ctx)
{
  Ext2Filesystem *fs = ctx->b_sb->sb_private;
  int changed = 0;
  int flags;
  int limit;
  int offset;
  uint32_t *blockno;
  block_t block;
  int ret = 0;
  int i;

  limit = ctx->b_sb->sb_blksize;
  if (!(ctx->b_flags & BLOCK_FLAG_DEPTH_TRAVERSE)
      && !(ctx->b_flags & BLOCK_FLAG_DATA_ONLY))
    {
      block = *tind_block;
      ret = ctx->b_func (ctx->b_sb, &block, BLOCK_COUNT_TIND, ref_block,
			 ref_offset, ctx->b_private);
      *tind_block = (uint32_t) block;
    }
  if ((ctx->b_flags & BLOCK_FLAG_READ_ONLY) && (ret & BLOCK_CHANGED))
    {
      ctx->b_err = -EROFS;
      return ret | BLOCK_ABORT | BLOCK_ERROR;
    }

  if (*tind_block == 0 || (ret & BLOCK_ABORT))
    {
      ctx->b_blkcnt += (blkcnt64_t) limit * limit * limit;
      return ret;
    }
  if (*tind_block >= ext2_blocks_count (&fs->f_super)
      || *tind_block < fs->f_super.s_first_data_block)
    {
      ctx->b_err = -EINVAL;
      return ret | BLOCK_ERROR;
    }
  ctx->b_err = ext2_read_blocks (ctx->b_tind_buf, ctx->b_sb, *tind_block, 1);
  if (ctx->b_err != 0)
    return ret | BLOCK_ERROR;

  blockno = (uint32_t *) ctx->b_tind_buf;
  offset = 0;
  if (ctx->b_flags & BLOCK_FLAG_APPEND)
    {
      for (i = 0; i < limit; i++, ctx->b_blkcnt++, blockno++)
	{
	  flags = ext2_block_iterate_dind (blockno, *tind_block, offset, ctx);
	  changed |= flags;
	  if (flags & (BLOCK_ABORT | BLOCK_ERROR))
	    {
	      ret |= flags & (BLOCK_ABORT | BLOCK_ERROR);
	      break;
	    }
	  offset += 4;
	}
    }
  else
    {
      for (i = 0; i < limit; i++, ctx->b_blkcnt++, blockno++)
	{
	  if (*blockno == 0)
	    {
	      ctx->b_blkcnt += limit;
	      continue;
	    }
	  flags = ext2_block_iterate_dind (blockno, *tind_block, offset, ctx);
	  changed |= flags;
	  if (flags & (BLOCK_ABORT | BLOCK_ERROR))
	    {
	      ret |= flags & (BLOCK_ABORT | BLOCK_ERROR);
	      break;
	    }
	  offset += 4;
	}
    }

  if ((ctx->b_flags & BLOCK_FLAG_READ_ONLY) && (changed & BLOCK_CHANGED))
    {
      ctx->b_err = -EROFS;
      return changed | BLOCK_ABORT | BLOCK_ERROR;
    }
  if (changed & BLOCK_CHANGED)
    {
      ctx->b_err =
	ext2_write_blocks (ctx->b_tind_buf, ctx->b_sb, *tind_block, 1);
      if (ctx->b_err != 0)
	ret |= BLOCK_ERROR | BLOCK_ABORT;
    }
  if ((ctx->b_flags & BLOCK_FLAG_DEPTH_TRAVERSE)
      && !(ctx->b_flags & BLOCK_FLAG_DATA_ONLY) && !(ret & BLOCK_ABORT))
    {
      block = *tind_block;
      ret |= ctx->b_func (ctx->b_sb, &block, BLOCK_COUNT_TIND, ref_block,
			  ref_offset, ctx->b_private);
      *tind_block = (uint32_t) block;
    }
  if ((ctx->b_flags & BLOCK_FLAG_READ_ONLY) && (ret & BLOCK_CHANGED))
    {
      ctx->b_err = -EROFS;
      return ret | BLOCK_ABORT | BLOCK_ERROR;
    }
  return ret;
}

static int
ext2_read_dir_block (VFSSuperblock *sb, block_t block, void *buffer, int flags,
		     VFSInode *inode)
{
  int corrupt = 0;
  int ret = ext2_read_blocks (buffer, sb, block, 1);
  if (ret != 0)
    return ret;
  if (!ext2_dir_block_checksum_valid (sb, inode, (Ext2DirEntry *) buffer))
    corrupt = 1;
  return ret != 0 && corrupt ? -EUCLEAN : ret;
}

static int
ext2_dirent_valid (VFSSuperblock *sb, char *buffer, unsigned int offset,
		   unsigned int final_offset)
{
  Ext2DirEntry *dirent;
  unsigned int rec_len;
  while (offset < final_offset && offset <= sb->sb_blksize - 12)
    {
      dirent = (Ext2DirEntry *) (buffer + offset);
      if (ext2_get_rec_len (sb, dirent, &rec_len) != 0)
	return 0;
      offset += rec_len;
      if (rec_len < 8 || rec_len % 4 == 0
	  || (dirent->d_name_len & 0xff) + 8 > rec_len)
	return 0;
    }
  return offset == final_offset;
}

static int
ext2_process_dir_block (VFSSuperblock *sb, block_t *blockno, blkcnt64_t blkcnt,
			block_t ref_block, int ref_offset, void *private)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2DirContext *ctx = private;
  Ext2DirEntry *dirent;
  unsigned int next_real_entry = 0;
  unsigned int offset = 0;
  unsigned int bufsize;
  unsigned int size;
  unsigned int rec_len;
  int changed = 0;
  int csum_size = 0;
  int do_abort = 0;
  int entry;
  int inline_data;
  int ret = 0;

  entry = blkcnt == 0 ? DIRENT_DOT_FILE : DIRENT_OTHER_FILE;
  inline_data = ctx->d_flags & DIRENT_FLAG_INLINE ? 1 : 0;
  if (!inline_data)
    {
      ctx->d_err =
	ext2_read_dir_block (sb, *blockno, ctx->d_buffer, 0, ctx->d_dir);
      if (ctx->d_err != 0)
	return BLOCK_ABORT;
      bufsize = sb->sb_blksize;
    }
  else
    bufsize = ctx->d_bufsize;

  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM)
    csum_size = sizeof (Ext2DirEntryTail);

  while (offset < bufsize - 8)
    {
      dirent = (Ext2DirEntry *) (ctx->d_buffer + offset);
      if (ext2_get_rec_len (sb, dirent, &rec_len))
	return BLOCK_ABORT;
      if (offset + rec_len > bufsize || rec_len < 8 || rec_len % 4 != 0
	  || (dirent->d_name_len & 0xff) + 8 > rec_len)
	{
	  ctx->d_err = -EUCLEAN;
	  return BLOCK_ABORT;
	}
      if (dirent->d_inode == 0)
	{
	  if (!inline_data && offset == bufsize - csum_size
	      && dirent->d_rec_len == csum_size &&
	      dirent->d_name_len == EXT2_DIR_NAME_LEN_CHECKSUM)
	    {
	      if (!(ctx->d_flags & DIRENT_FLAG_CHECKSUM))
		goto next;
	      entry = DIRENT_CHECKSUM;
	    }
	  else if (!(ctx->d_flags & DIRENT_FLAG_EMPTY))
	    goto next;
	}

      ret = ctx->d_func (ctx->d_dir, next_real_entry > offset ?
			 DIRENT_DELETED_FILE : entry, dirent, offset, bufsize,
			 ctx->d_buffer, ctx->d_private);
      if (entry < DIRENT_OTHER_FILE)
	entry++;

      if (ret & DIRENT_CHANGED)
	{
	  if (ext2_get_rec_len (sb, dirent, &rec_len) != 0)
	    return BLOCK_ABORT;
	  changed++;
	}
      if (ret & DIRENT_ABORT)
	{
	  do_abort++;
	  break;
	}

    next:
      if (next_real_entry == offset)
	next_real_entry += rec_len;
      if (ctx->d_flags & DIRENT_FLAG_REMOVED)
	{
	  size = ((dirent->d_name_len & 0xff) + 11) & ~3;
	  if (rec_len != size)
	    {
	      unsigned int final_offset = offset + rec_len;
	      offset += size;
	      while (offset < final_offset
		     && !ext2_dirent_valid (sb, ctx->d_buffer, offset,
					    final_offset))
		offset += 4;
	      continue;
	    }
	}
      offset += rec_len;
    }

  if (changed)
    {
      if (!inline_data)
	{
	  ctx->d_err = ext2_write_dir_block (sb, *blockno, ctx->d_buffer, 0,
					     ctx->d_dir);
	  if (ctx->d_err != 0)
	    return BLOCK_ABORT;
	}
      else
	ret = BLOCK_INLINE_CHANGED;
    }
  return ret | (do_abort ? BLOCK_ABORT : 0);
}

static int
ext2_process_lookup (VFSInode *dir, int entry, Ext2DirEntry *dirent, int offset,
		     int blocksize, char *buffer, void *priv)
{
  Ext2Lookup *l = priv;
  if (l->namelen != (dirent->d_name_len & 0xff))
    return 0;
  if (strncmp (l->name, dirent->d_name, dirent->d_name_len & 0xff) != 0)
    return 0;
  *l->inode = dirent->d_inode;
  l->found = 1;
  return DIRENT_ABORT;
}

static int
ext2_process_dir_expand (VFSSuperblock *sb, block_t *blockno, blkcnt64_t blkcnt,
			 block_t ref_block, int ref_offset, void *private)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2DirExpand *e = private;
  block_t newblock;
  char *block;
  int ret;
  if (*blockno != 0)
    {
      if (blkcnt >= 0)
	e->de_goal = *blockno;
      return 0;
    }

  if (blkcnt != 0 && EXT2_B2C (fs, e->de_goal) == EXT2_B2C (fs, e->de_goal + 1))
    newblock = e->de_goal + 1;
  else
    {
      e->de_goal &= ~EXT2_CLUSTER_MASK (fs);
      ret = ext2_new_block (sb, e->de_goal, NULL, &newblock, NULL);
      if (ret != 0)
	{
	  e->de_err = ret;
	  return BLOCK_ABORT;
	}
      e->de_newblocks++;
      ext2_block_alloc_stats (sb, newblock, 1);
    }

  if (blkcnt > 0)
    {
      ret = ext2_new_dir_block (sb, 0, 0, &block);
      if (ret != 0)
	{
	  e->de_err = ret;
	  return BLOCK_ABORT;
	}
      e->de_done = 1;
      ret = ext2_write_dir_block (sb, newblock, block, 0, e->de_dir);
      kfree (block);
    }
  else
    ret = ext2_zero_blocks (sb, newblock, 1, NULL, NULL);
  if (blkcnt >= 0)
    e->de_goal = newblock;
  if (ret != 0)
    {
      e->de_err = ret;
      return BLOCK_ABORT;
    }
  *blockno = newblock;
  return e->de_done ? BLOCK_CHANGED | BLOCK_ABORT : BLOCK_CHANGED;
}

static int
ext2_get_dirent_tail (VFSSuperblock *sb, Ext2DirEntry *dirent,
		      Ext2DirEntryTail **tail)
{
  Ext2DirEntry *d = dirent;
  Ext2DirEntryTail *t;
  void *top = EXT2_DIRENT_TAIL (dirent, sb->sb_blksize);

  while ((void *) d < top)
    {
      if (d->d_rec_len < 8 || (d->d_rec_len & 3))
	return -EUCLEAN;
      d = (Ext2DirEntry *) ((char *) d + d->d_rec_len);
    }
  if ((char *) d > (char *) dirent + sb->sb_blksize)
    return -EUCLEAN;
  if (d != top)
    return -ENOSPC;

  t = (Ext2DirEntryTail *) d;
  if (t->det_reserved_zero1 != 0 || t->det_rec_len != sizeof (Ext2DirEntryTail)
      || t->det_reserved_name_len != EXT2_DIR_NAME_LEN_CHECKSUM)
    return -ENOSPC;
  if (tail != NULL)
    *tail = t;
  return 0;
}

static int
ext2_get_dx_count_limit (VFSSuperblock *sb, Ext2DirEntry *dirent,
			 Ext2DXCountLimit **cl, int *offset)
{
  Ext2DirEntry *d;
  Ext2DXRootInfo *root;
  Ext2DXCountLimit *c;
  int count_offset;
  int max_entries;
  unsigned int rec_len = dirent->d_rec_len;

  if (rec_len == sb->sb_blksize && dirent->d_name_len == 0)
    count_offset = 8;
  else if (rec_len == 12)
    {
      d = (Ext2DirEntry *) ((char *) dirent + 12);
      rec_len = d->d_rec_len;
      if (rec_len != sb->sb_blksize - 12)
	return -EINVAL;
      root = (Ext2DXRootInfo *) ((char *) d + 12);
      if (root->i_reserved_zero != 0
	  || root->i_info_length != sizeof (Ext2DXRootInfo))
	return -EINVAL;
      count_offset = 32;
    }
  else
    return -EINVAL;

  c = (Ext2DXCountLimit *) ((char *) dirent + count_offset);
  max_entries = (sb->sb_blksize - count_offset) / sizeof (Ext2DXEntry);
  if (c->cl_limit > max_entries || c->cl_count > max_entries)
    return -ENOSPC;

  if (offset != NULL)
    *offset = count_offset;
  if (cl != NULL)
    *cl = c;
  return 0;
}

int
ext2_bg_has_super (Ext2Filesystem *fs, unsigned int group)
{
  if (group == 0)
    return 1;
  if (fs->f_super.s_feature_ro_compat & EXT2_FT_RO_COMPAT_SPARSE_SUPER)
    return group == fs->f_super.s_backup_bgs[0]
      || group == fs->f_super.s_backup_bgs[1];
  if (group <= 1)
    return 1;
  if (!(group & 1))
    return 0;
  if (ext2_bg_super_test_root (group, 3)
      || ext2_bg_super_test_root (group, 5)
      || ext2_bg_super_test_root (group, 7))
    return 1;
  return 0;
}

int
ext2_bg_test_flags (VFSSuperblock *sb, unsigned int group, uint16_t flags)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return gdp->bg_flags & flags;
}

void
ext2_bg_clear_flags (VFSSuperblock *sb, unsigned int group, uint16_t flags)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  gdp->bg_flags &= ~flags;
}

void
ext2_update_super_revision (Ext2Superblock *s)
{
  if (s->s_rev_level > EXT2_OLD_REV)
    return;
  s->s_rev_level = EXT2_DYNAMIC_REV;
  s->s_first_ino = EXT2_OLD_FIRST_INODE;
  s->s_inode_size = EXT2_OLD_INODE_SIZE;
}

Ext2GroupDesc *
ext2_group_desc (VFSSuperblock *sb, Ext2GroupDesc *gdp, unsigned int group)
{
  static char *buffer;
  static size_t bufsize;
  Ext2Filesystem *fs = sb->sb_private;
  block_t block;
  int desc_size = EXT2_DESC_SIZE (fs->f_super);
  int desc_per_block = EXT2_DESC_PER_BLOCK (fs->f_super);
  int ret;

  if (group > fs->f_group_desc_count)
    return NULL;
  if (gdp != NULL)
    return (Ext2GroupDesc *) ((char *) gdp + group * desc_size);

  if (bufsize < sb->sb_blksize)
    {
      kfree (buffer);
      buffer = NULL;
    }
  if (buffer == NULL)
    {
      buffer = kmalloc (sb->sb_blksize);
      if (unlikely (buffer == NULL))
	return NULL;
      bufsize = sb->sb_blksize;
    }
  block = ext2_descriptor_block (sb, fs->f_super.s_first_data_block,
				 group / desc_per_block);
  ret = ext2_read_blocks (buffer, sb, block, 1);
  if (ret != 0)
    return NULL;
  return (Ext2GroupDesc *) (buffer + group % desc_per_block * desc_size);
}

Ext4GroupDesc *
ext4_group_desc (VFSSuperblock *sb, Ext2GroupDesc *gdp, unsigned int group)
{
  return (Ext4GroupDesc *) ext2_group_desc (sb, gdp, group);
}

void
ext2_super_bgd_loc (VFSSuperblock *sb, unsigned int group, block_t *super,
		    block_t *old_desc, block_t *new_desc, blkcnt64_t *used)
{
  Ext2Filesystem *fs = sb->sb_private;
  block_t group_block;
  block_t super_block = 0;
  block_t old_desc_block = 0;
  block_t new_desc_block = 0;
  unsigned int meta_bg;
  size_t meta_bg_size;
  blkcnt64_t nblocks = 0;
  block_t old_desc_blocks;
  int has_super;

  group_block = ext2_group_first_block (fs, group);
  if (group_block == 0 && sb->sb_blksize == 1024)
    group_block = 1;

  if (fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_META_BG)
    old_desc_blocks = fs->f_super.s_first_meta_bg;
  else
    old_desc_blocks = fs->f_desc_blocks + fs->f_super.s_reserved_gdt_blocks;

  has_super = ext2_bg_has_super (fs, group);
  if (has_super)
    {
      super_block = group_block;
      nblocks++;
    }
  meta_bg_size = EXT2_DESC_PER_BLOCK (fs->f_super);
  meta_bg = group / meta_bg_size;

  if (!(fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_META_BG)
      || meta_bg < fs->f_super.s_first_meta_bg)
    {
      if (has_super)
	{
	  old_desc_block = group_block + 1;
	  nblocks += old_desc_blocks;
	}
    }
  else
    {
      if (group % meta_bg_size == 0 || group % meta_bg_size == 1
	  || group % meta_bg_size == meta_bg_size - 1)
	{
	  new_desc_block = group_block + (has_super ? 1 : 0);
	  nblocks++;
	}
    }

  if (super != NULL)
    *super = super_block;
  if (old_desc != NULL)
    *old_desc = old_desc_block;
  if (new_desc != NULL)
    *new_desc = new_desc_block;
  if (used != NULL)
    *used = nblocks;
}

block_t
ext2_block_bitmap_loc (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return (block_t) gdp->bg_block_bitmap |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (block_t) gdp->bg_block_bitmap_hi << 32 : 0);
}

block_t
ext2_inode_bitmap_loc (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return (block_t) gdp->bg_inode_bitmap |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (block_t) gdp->bg_inode_bitmap_hi << 32 : 0);
}

block_t
ext2_inode_table_loc (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return (block_t) gdp->bg_inode_table |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (block_t) gdp->bg_inode_table_hi << 32 : 0);
}

block_t
ext2_descriptor_block (VFSSuperblock *sb, block_t group_block, unsigned int i)
{
  int has_super = 0;
  int group_zero_adjust = 0;
  int bg;
  block_t block;
  Ext2Filesystem *fs = sb->sb_private;

  if (i == 0 && sb->sb_blksize == 1024 && EXT2_CLUSTER_RATIO (fs) > 1)
    group_zero_adjust = 1;

  if (!(fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_META_BG)
      || i < fs->f_super.s_first_meta_bg)
    return group_block + group_zero_adjust + i + 1;

  bg = EXT2_DESC_PER_BLOCK (fs->f_super) * i;
  if (ext2_bg_has_super (fs, bg))
    has_super = 1;
  block = ext2_group_first_block (fs, bg);

  if (group_block != fs->f_super.s_first_data_block
      && block + has_super + fs->f_super.s_blocks_per_group <
      ext2_blocks_count (&fs->f_super))
    {
      block += fs->f_super.s_blocks_per_group;
      if (ext2_bg_has_super (fs, bg + 1))
	has_super = 1;
      else
	has_super = 0;
    }
  return block + has_super + group_zero_adjust;
}

uint32_t
ext2_bg_free_blocks_count (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return gdp->bg_free_blocks_count |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (uint32_t) gdp->bg_free_blocks_count_hi << 16 : 0);
}

void
ext2_bg_free_blocks_count_set (VFSSuperblock *sb, unsigned int group,
			       uint32_t blocks)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  gdp->bg_free_blocks_count = blocks;
  if (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT)
    gdp->bg_free_blocks_count_hi = blocks >> 16;
}

uint32_t
ext2_bg_free_inodes_count (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return gdp->bg_free_inodes_count |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (uint32_t) gdp->bg_free_inodes_count_hi << 16 : 0);
}

void
ext2_bg_free_inodes_count_set (VFSSuperblock *sb, unsigned int group,
			       uint32_t inodes)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  gdp->bg_free_inodes_count = inodes;
  if (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT)
    gdp->bg_free_inodes_count_hi = inodes >> 16;
}

uint32_t
ext2_bg_used_dirs_count (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return gdp->bg_used_dirs_count |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (uint32_t) gdp->bg_used_dirs_count_hi << 16 : 0);
}

void
ext2_bg_used_dirs_count_set (VFSSuperblock *sb, unsigned int group,
			     uint32_t dirs)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  gdp->bg_used_dirs_count = dirs;
  if (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT)
    gdp->bg_used_dirs_count_hi = dirs >> 16;
}

uint32_t
ext2_bg_itable_unused (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return gdp->bg_itable_unused |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (uint32_t) gdp->bg_itable_unused_hi << 16 : 0);
}

void
ext2_bg_itable_unused_set (VFSSuperblock *sb, unsigned int group,
			   uint32_t unused)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  gdp->bg_itable_unused = unused;
  if (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT)
    gdp->bg_itable_unused_hi = unused >> 16;
}

void
ext2_cluster_alloc (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
		    Ext3ExtentHandle *handle, block_t block, block_t *physblock)
{
  Ext2Filesystem *fs = sb->sb_private;
  block_t pblock = 0;
  block_t base;
  int i;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_BIGALLOC))
    return;

  base = block & ~EXT2_CLUSTER_MASK (fs);
  for (i = 0; i < EXT2_CLUSTER_RATIO (fs); i++)
    {
      if (base + i == block)
	continue;
      ext3_extent_bmap (sb, ino, inode, handle, 0, 0, base + i, 0, 0, &pblock);
      if (pblock != 0)
	break;
    }
  if (pblock != 0)
    *physblock = pblock - i + block - base;
}

int
ext2_map_cluster_block (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
			block_t block, block_t *physblock)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext3ExtentHandle *handle;
  int ret;
  *physblock = 0;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_BIGALLOC)
      || !(inode->i_flags & EXT4_EXTENTS_FL))
    return 0;

  ret = ext3_extent_open (sb, ino, inode, &handle);
  if (ret != 0)
    return ret;

  ext2_cluster_alloc (sb, ino, inode, handle, block, physblock);
  ext3_extent_free (handle);
  return 0;
}

void
ext2_inode_alloc_stats (VFSSuperblock *sb, ino64_t ino, int inuse, int isdir)
{
  Ext2Filesystem *fs = sb->sb_private;
  unsigned int group = ext2_group_of_inode (fs, ino);
  if (ino > fs->f_super.s_inodes_count)
    return;
  if (inuse > 0)
    ext2_mark_bitmap (fs->f_inode_bitmap, ino);
  else
    ext2_unmark_bitmap (fs->f_inode_bitmap, ino);
  ext2_bg_free_inodes_count_set (sb, group,
				 ext2_bg_free_inodes_count (sb, group) - inuse);
  if (isdir)
    ext2_bg_used_dirs_count_set (sb, group,
				 ext2_bg_used_dirs_count (sb, group) - inuse);
  ext2_bg_clear_flags (sb, group, EXT2_BG_INODE_UNINIT);
  if (ext2_has_group_desc_checksum (&fs->f_super))
    {
      ino64_t first_unused_inode =
	fs->f_super.s_inodes_per_group - ext2_bg_itable_unused (sb, group) +
	group * fs->f_super.s_inodes_per_group + 1;
      if (ino >= first_unused_inode)
	ext2_bg_itable_unused_set (sb, group, group *
				   fs->f_super.s_inodes_per_group +
				   fs->f_super.s_inodes_per_group - ino);
      ext2_group_desc_checksum_update (sb, group);
    }
  fs->f_super.s_free_inodes_count -= inuse;
  fs->f_flags |= EXT2_FLAG_CHANGED | EXT2_FLAG_DIRTY | EXT2_FLAG_IB_DIRTY;
}

void
ext2_block_alloc_stats (VFSSuperblock *sb, block_t block, int inuse)
{
  Ext2Filesystem *fs = sb->sb_private;
  unsigned int group = ext2_group_of_block (fs, block);
  if (block >= ext2_blocks_count (&fs->f_super))
    return;
  if (inuse > 0)
    ext2_mark_bitmap (fs->f_block_bitmap, block);
  else
    ext2_unmark_bitmap (fs->f_block_bitmap, block);
  ext2_bg_free_blocks_count_set (sb, group,
				 ext2_bg_free_blocks_count (sb, group) - inuse);
  ext2_bg_clear_flags (sb, group, EXT2_BG_BLOCK_UNINIT);
  ext2_group_desc_checksum_update (sb, group);
  ext2_free_blocks_count_add (&fs->f_super,
			      -inuse * (blkcnt64_t) EXT2_CLUSTER_RATIO (fs));
  fs->f_flags |= EXT2_FLAG_CHANGED | EXT2_FLAG_DIRTY | EXT2_FLAG_BB_DIRTY;
}

int
ext2_write_backup_superblock (VFSSuperblock *sb, unsigned int group,
			      block_t group_block, Ext2Superblock *s)
{
  unsigned int sgroup = group;
  if (sgroup > 65535)
    sgroup = 65535;
  s->s_block_group_nr = sgroup;
  ext2_superblock_checksum_update (sb->sb_private, s);
  return sb->sb_dev->sd_write (sb->sb_dev, s, sizeof (Ext2Superblock),
			       group_block * sb->sb_blksize);
}

int
ext2_write_primary_superblock (VFSSuperblock *sb, Ext2Superblock *s)
{
  return sb->sb_dev->sd_write (sb->sb_dev, s, sizeof (Ext2Superblock), 1024);
}

int
ext2_flush (VFSSuperblock *sb, int flags)
{
  Ext2Filesystem *fs = sb->sb_private;
  unsigned int state;
  unsigned int i;
  Ext2Superblock *super_shadow = NULL;
  Ext2GroupDesc *group_shadow = NULL;
  char *group_ptr;
  block_t old_desc_blocks;
  int ret;
  if (fs->f_super.s_magic != EXT2_MAGIC)
    return -EINVAL;
  if (!(fs->f_super.s_feature_incompat & EXT3_FT_INCOMPAT_JOURNAL_DEV)
      && fs->f_group_desc == NULL)
    return -EINVAL;

  state = fs->f_super.s_state;
  fs->f_super.s_wtime = fs->f_now != 0 ? fs->f_now : time (NULL);
  fs->f_super.s_block_group_nr = 0;
  fs->f_super.s_state &= ~EXT2_STATE_VALID;
  fs->f_super.s_feature_incompat &= ~EXT3_FT_INCOMPAT_RECOVER;

  ret = ext2_write_bitmaps (sb);
  if (ret != 0)
    return ret;

  super_shadow = &fs->f_super;
  group_shadow = fs->f_group_desc;

  if (fs->f_super.s_feature_incompat & EXT3_FT_INCOMPAT_JOURNAL_DEV)
    goto write_super;

  group_ptr = (char *) group_shadow;
  if (fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_META_BG)
    {
      old_desc_blocks = fs->f_super.s_first_meta_bg;
      if (old_desc_blocks > fs->f_desc_blocks)
	old_desc_blocks = fs->f_desc_blocks;
    }
  else
    old_desc_blocks = fs->f_desc_blocks;

  for (i = 0; i < fs->f_group_desc_count; i++)
    {
      block_t super_block;
      block_t old_desc_block;
      block_t new_desc_block;
      ext2_super_bgd_loc (sb, i, &super_block, &old_desc_block, &new_desc_block,
			  NULL);
      if (i > 0 && super_block != 0)
	{
	  ret = ext2_write_backup_superblock (sb, i, super_block, super_shadow);
	  if (ret != 0)
	    return ret;
	}
      if (old_desc_block != 0)
	{
	  ret = ext2_write_blocks (group_ptr, sb, old_desc_block,
				   old_desc_blocks);
	  if (ret != 0)
	    return ret;
	}
      if (new_desc_block != 0)
	{
	  int meta_bg = i / EXT2_DESC_PER_BLOCK (fs->f_super);
	  ret = ext2_write_blocks (group_ptr + meta_bg * sb->sb_blksize, sb,
				   new_desc_block, 1);
	  if (ret != 0)
	    return ret;
	}
    }

 write_super:
  fs->f_super.s_block_group_nr = 0;
  fs->f_super.s_state = state;
  ext2_superblock_checksum_update (fs, super_shadow);
  ret = ext2_write_primary_superblock (sb, super_shadow);
  if (ret != 0)
    return ret;
  fs->f_flags &= ~EXT2_FLAG_DIRTY;
  return 0;
}

int
ext2_open_file (VFSSuperblock *sb, ino64_t inode, Ext2File *file)
{
  int ret = ext2_read_inode (sb, inode, &file->f_inode);
  if (ret != 0)
    return ret;
  file->f_ino = inode;
  file->f_sb = sb;
  file->f_flags = 0;
  file->f_buffer = kmalloc (sb->sb_blksize * 3);
  if (unlikely (file->f_buffer == NULL))
    return -ENOMEM;
  return 0;
}

int
ext2_file_block_offset_too_big (VFSSuperblock *sb, Ext2Inode *inode,
				block_t offset)
{
  block_t addr_per_block;
  block_t max_map_block;

  if (offset >= (1ULL << 32) - 1)
    return 1;
  if (inode->i_flags & EXT4_EXTENTS_FL)
    return 0;

  addr_per_block = sb->sb_blksize >> 2;
  max_map_block = addr_per_block;
  max_map_block += addr_per_block * addr_per_block;
  max_map_block += addr_per_block * addr_per_block * addr_per_block;
  max_map_block += EXT2_NDIR_BLOCKS;
  return offset >= max_map_block;
}

int
ext2_file_set_size (Ext2File *file, off64_t size)
{
  Ext2Filesystem *fs = file->f_sb->sb_private;
  blksize_t blksize = file->f_sb->sb_blksize;
  block_t old_truncate;
  block_t truncate_block;
  off64_t old_size;
  int ret;
  if (size > 0 && ext2_file_block_offset_too_big (file->f_sb, &file->f_inode,
						  (size - 1) / blksize))
    return -EFBIG;

  truncate_block = (size + blksize - 1) >> EXT2_BLOCK_SIZE_BITS (fs->f_super);
  old_size = EXT2_I_SIZE (file->f_inode);
  old_truncate = (old_size + blksize - 1) >> EXT2_BLOCK_SIZE_BITS (fs->f_super);

  ret = ext2_inode_set_size (file->f_sb, &file->f_inode, size);
  if (ret != 0)
    return ret;

  if (file->f_ino != 0)
    {
      ret = ext2_update_inode (file->f_sb, file->f_ino, &file->f_inode,
			       sizeof (Ext2Inode));
      if (ret != 0)
	return ret;
    }

  ret = ext2_file_zero_remainder (file, size);
  if (ret != 0)
    return ret;

  if (truncate_block >= old_truncate)
    return 0;
  return ext2_dealloc_blocks (file->f_sb, file->f_ino, &file->f_inode, 0,
			      truncate_block, ~0ULL);
}

int
ext2_read_inode (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode)
{
  Ext2Filesystem *fs = sb->sb_private;
  int len = EXT2_INODE_SIZE (fs->f_super);
  size_t bufsize = sizeof (Ext2Inode);
  Ext2LargeInode *iptr;
  char *ptr;
  block_t blockno;
  unsigned int group;
  unsigned long block;
  unsigned long offset;
  unsigned int i;
  int cache_slot;
  int csum_valid;
  int clen;
  int ret;

  if (unlikely (ino == 0 || ino > fs->f_super.s_inodes_count))
    return -EINVAL;

  /* Try lookup in inode cache */
  if (fs->f_icache == NULL)
    {
      ret = ext2_create_inode_cache (sb, 4);
      if (ret != 0)
	return ret;
    }
  for (i = 0; i < fs->f_icache->ic_cache_size; i++)
    {
      if (fs->f_icache->ic_cache[i].e_ino == ino)
	{
	  memcpy (inode, fs->f_icache->ic_cache[i].e_inode,
		  bufsize > len ? len : bufsize);
	  return 0;
	}
    }

  group = (ino - 1) / fs->f_super.s_inodes_per_group;
  if (group > fs->f_group_desc_count)
    return -EINVAL;
  offset = (ino - 1) % fs->f_super.s_inodes_per_group *
    EXT2_INODE_SIZE (fs->f_super);
  block = offset >> EXT2_BLOCK_SIZE_BITS (fs->f_super);
  blockno = ext2_inode_table_loc (sb, group);
  if (blockno == 0 || blockno < fs->f_super.s_first_data_block
      || blockno + fs->f_inode_blocks_per_group - 1 >=
      ext2_blocks_count (&fs->f_super))
    return -EINVAL;
  blockno += block;
  offset &= EXT2_BLOCK_SIZE (fs->f_super) - 1;

  cache_slot = (fs->f_icache->ic_cache_last + 1) % fs->f_icache->ic_cache_size;
  iptr = (Ext2LargeInode *) fs->f_icache->ic_cache[cache_slot].e_inode;
  ptr = (char *) iptr;
  while (len != 0)
    {
      clen = len;
      if (offset + len > sb->sb_blksize)
	clen = sb->sb_blksize - offset;
      if (blockno != fs->f_icache->ic_block)
	{
	  ret = ext2_read_blocks (fs->f_icache->ic_buffer, sb, blockno, 1);
	  if (ret != 0)
	    return ret;
	  fs->f_icache->ic_block = blockno;
	}
      memcpy (ptr, fs->f_icache->ic_buffer + (unsigned int) offset, clen);
      offset = 0;
      len -= clen;
      ptr += clen;
      blockno++;
    }
  len = EXT2_INODE_SIZE (fs->f_super);

  csum_valid = ext2_inode_checksum_valid (fs, ino, iptr);
  if (csum_valid)
    {
      fs->f_icache->ic_cache_last = cache_slot;
      fs->f_icache->ic_cache[cache_slot].e_ino = ino;
    }
  memcpy (inode, iptr, bufsize > len ? len : bufsize);
  return csum_valid ? 0 : -EINVAL;
}

int
ext2_update_inode (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
		   size_t bufsize)
{
  Ext2Filesystem *fs = sb->sb_private;
  int len = EXT2_INODE_SIZE (fs->f_super);
  Ext2LargeInode *winode;
  block_t blockno;
  unsigned long block;
  unsigned long offset;
  unsigned int group;
  char *ptr;
  unsigned int i;
  int clen;
  int ret;
  if (sb->sb_mntflags & MS_RDONLY)
    return -EROFS;

  winode = kmalloc (len);
  if (unlikely (winode == NULL))
    return -ENOMEM;
  if (bufsize < len)
    {
      ret = ext2_read_inode (sb, ino, (Ext2Inode *) winode);
      if (ret != 0)
	{
	  kfree (winode);
	  return ret;
	}
    }

  /* Update inode cache if necessary */
  if (fs->f_icache != NULL)
    {
      for (i = 0; i < fs->f_icache->ic_cache_size; i++)
	{
	  if (fs->f_icache->ic_cache[i].e_ino == ino)
	    {
	      memcpy (fs->f_icache->ic_cache[i].e_inode, inode,
		      bufsize > len ? len : bufsize);
	      break;
	    }
	}
    }
  else
    {
      ret = ext2_create_inode_cache (sb, 4);
      if (ret != 0)
	{
	  kfree (winode);
	  return ret;
	}
    }
  memcpy (winode, inode, bufsize > len ? len : bufsize);

  /* Update inode checksum */
  ext2_inode_checksum_update (fs, ino, winode);

  group = (ino - 1) / fs->f_super.s_inodes_per_group;
  offset = (ino - 1) % fs->f_super.s_inodes_per_group *
    EXT2_INODE_SIZE (fs->f_super);
  block = offset >> EXT2_BLOCK_SIZE_BITS (fs->f_super);
  blockno = ext2_inode_table_loc (sb, group);
  if (blockno == 0 || blockno < fs->f_super.s_first_data_block
      || blockno + fs->f_inode_blocks_per_group - 1 >=
      ext2_blocks_count (&fs->f_super))
    {
      kfree (winode);
      return -EINVAL;
    }
  blockno += block;
  offset &= EXT2_BLOCK_SIZE (fs->f_super) - 1;

  ptr = (char *) winode;
  while (len)
    {
      clen = len;
      if (offset + len > sb->sb_blksize)
	clen = sb->sb_blksize - offset;
      if (fs->f_icache->ic_block != blockno)
	{
	  ret = ext2_read_blocks (fs->f_icache->ic_buffer, sb, blockno, 1);
	  if (ret != 0)
	    {
	      kfree (winode);
	      return ret;
	    }
	  fs->f_icache->ic_block = blockno;
	}
      memcpy (fs->f_icache->ic_buffer + (unsigned int) offset, ptr, clen);
      ret = ext2_write_blocks (fs->f_icache->ic_buffer, sb, blockno, 1);
      if (ret != 0)
	{
	  kfree (winode);
	  return ret;
	}
      offset = 0;
      ptr += clen;
      len -= clen;
      blockno++;
    }
  fs->f_flags |= EXT2_FLAG_CHANGED;
  kfree (winode);
  return 0;
}

void
ext2_update_vfs_inode (VFSInode *inode)
{
  Ext2Filesystem *fs = inode->vi_sb->sb_private;
  Ext2File *file = inode->vi_private;
  inode->vi_uid = file->f_inode.i_uid;
  inode->vi_gid = file->f_inode.i_gid;
  inode->vi_nlink = file->f_inode.i_links_count;
  inode->vi_size = file->f_inode.i_size;
  if (S_ISREG (file->f_inode.i_mode) && fs->f_super.s_rev_level > 0
      && (fs->f_super.s_feature_ro_compat & EXT2_FT_RO_COMPAT_LARGE_FILE))
    inode->vi_size |= (off64_t) file->f_inode.i_size_high << 32;
  inode->vi_atime.tv_sec = file->f_inode.i_atime;
  inode->vi_atime.tv_nsec = 0;
  inode->vi_mtime.tv_sec = file->f_inode.i_mtime;
  inode->vi_mtime.tv_nsec = 0;
  inode->vi_ctime.tv_sec = file->f_inode.i_ctime;
  inode->vi_ctime.tv_nsec = 0;
  inode->vi_sectors = file->f_inode.i_blocks;
  inode->vi_blocks =
    file->f_inode.i_blocks * ATA_SECTSIZE / inode->vi_sb->sb_blksize;

  /* Set mode and device numbers if applicable */
  inode->vi_mode = file->f_inode.i_mode & S_IFMT;
  if (S_ISBLK (inode->vi_mode) || S_ISCHR (inode->vi_mode))
    inode->vi_rdev = *((dev_t *) file->f_inode.i_block);
  inode->vi_mode |= file->f_inode.i_mode & 07777;
}

int
ext2_inode_set_size (VFSSuperblock *sb, Ext2Inode *inode, off64_t size)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (size < 0)
    return -EINVAL;
  if (ext2_needs_large_file (size))
    {
      int dirty_sb = 0;
      if (S_ISREG (inode->i_mode))
	{
	  if (!(fs->f_super.s_feature_ro_compat & EXT2_FT_RO_COMPAT_LARGE_FILE))
	    {
	      fs->f_super.s_feature_ro_compat |= EXT2_FT_RO_COMPAT_LARGE_FILE;
	      dirty_sb = 1;
	    }
	}
      else if (S_ISDIR (inode->i_mode))
	{
	  if (!(fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_LARGEDIR))
	    {
	      fs->f_super.s_feature_incompat |= EXT4_FT_INCOMPAT_LARGEDIR;
	      dirty_sb = 1;
	    }
	}
      else
	return -EFBIG;

      if (dirty_sb)
	{
	  if (fs->f_super.s_rev_level == EXT2_OLD_REV)
	    ext2_update_super_revision (&fs->f_super);
	  fs->f_flags |= EXT2_FLAG_DIRTY | EXT2_FLAG_CHANGED;
	}
    }

  inode->i_size = size & 0xffffffff;
  inode->i_size_high = size >> 32;
  return 0;
}

block_t
ext2_find_inode_goal (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
		      block_t block)
{
  Ext2Filesystem *fs = sb->sb_private;
  unsigned int group;
  unsigned char log_flex;
  Ext3GenericExtent extent;
  Ext3ExtentHandle *handle = NULL;
  int ret;

  if (inode == NULL || ext2_is_inline_symlink (inode)
      || (inode->i_flags & EXT4_INLINE_DATA_FL))
    goto noblocks;
  if (inode->i_flags & EXT4_EXTENTS_FL)
    {
      ret = ext3_extent_open (sb, ino, inode, &handle);
      if (ret != 0)
	goto noblocks;
      ret = ext3_extent_goto (handle, 0, block);
      if (ret != 0)
	goto noblocks;
      ret = ext3_extent_get (handle, EXT2_EXTENT_CURRENT, &extent);
      if (ret != 0)
	goto noblocks;
      ext3_extent_free (handle);
      return extent.e_pblk + block - extent.e_lblk;
    }

  if (inode->i_block[0] != 0)
    return inode->i_block[0];

 noblocks:
  ext3_extent_free (handle);
  log_flex = fs->f_super.s_log_groups_per_flex;
  group = ext2_group_of_inode (fs, ino);
  if (log_flex != 0)
    group = group & ~((1 << log_flex) - 1);
  return ext2_group_first_block (fs, group);
}

int
ext2_create_inode_cache (VFSSuperblock *sb, unsigned int cache_size)
{
  Ext2Filesystem *fs = sb->sb_private;
  unsigned int i;
  if (fs->f_icache != NULL)
    return 0;

  fs->f_icache = kzalloc (sizeof (Ext2InodeCache));
  if (unlikely (fs->f_icache == NULL))
    return -ENOMEM;
  fs->f_icache->ic_buffer = kmalloc (sb->sb_blksize);
  if (unlikely (fs->f_icache->ic_buffer == NULL))
    goto err;

  fs->f_icache->ic_block = 0;
  fs->f_icache->ic_cache_last = -1;
  fs->f_icache->ic_cache_size = cache_size;
  fs->f_icache->ic_refcnt = 1;

  fs->f_icache->ic_cache = kmalloc (sizeof (Ext2InodeCacheEntry) * cache_size);
  if (unlikely (fs->f_icache->ic_cache == NULL))
    goto err;

  for (i = 0; i < cache_size; i++)
    {
      fs->f_icache->ic_cache[i].e_inode =
	kmalloc (EXT2_INODE_SIZE (fs->f_super));
      if (unlikely (fs->f_icache->ic_cache[i].e_inode == NULL))
	goto err;
    }
  ext2_flush_inode_cache (fs->f_icache);
  return 0;

 err:
  ext2_free_inode_cache (fs->f_icache);
  fs->f_icache = NULL;
  return -ENOMEM;
}

void
ext2_free_inode_cache (Ext2InodeCache *cache)
{
  unsigned int i;
  if (--cache->ic_refcnt > 0)
    return;
  if (cache->ic_buffer != NULL)
    kfree (cache->ic_buffer);
  for (i = 0; i < cache->ic_cache_size; i++)
    kfree (cache->ic_cache[i].e_inode);
  if (cache->ic_cache != NULL)
    kfree (cache->ic_cache);
  cache->ic_block = 0;
  kfree (cache);
}

int
ext2_flush_inode_cache (Ext2InodeCache *cache)
{
  unsigned int i;
  if (cache == NULL)
    return 0;
  for (i = 0; i < cache->ic_cache_size; i++)
    cache->ic_cache[i].e_ino = 0;
  cache->ic_block = 0;
  return 0;
}

int
ext2_file_flush (Ext2File *file)
{
  block_t ignore;
  int retflags;
  int ret;
  if (!(file->f_flags & (EXT2_FILE_BUFFER_VALID | EXT2_FILE_BUFFER_DIRTY)))
    return 0;
  if (file->f_physblock && (file->f_inode.i_flags & EXT4_EXTENTS_FL))
    {
      ret = ext2_bmap (file->f_sb, file->f_ino, &file->f_inode,
		       file->f_buffer + file->f_sb->sb_blksize, 0,
		       file->f_block, &retflags, &ignore);
      if (ret != 0)
	return ret;
      if (retflags & BMAP_RET_UNINIT)
	{
	  ret = ext2_bmap (file->f_sb, file->f_ino, &file->f_inode,
			   file->f_buffer + file->f_sb->sb_blksize, BMAP_SET,
			   file->f_block, 0, &file->f_physblock);
	  if (ret != 0)
	    return ret;
	}
    }

  if (file->f_physblock == 0)
    {
      ret = ext2_bmap (file->f_sb, file->f_ino, &file->f_inode,
		       file->f_buffer + file->f_sb->sb_blksize,
		       file->f_ino == 0 ? 0 : BMAP_ALLOC, file->f_block, 0,
		       &file->f_physblock);
      if (ret != 0)
	return ret;
    }

  ret = ext2_write_blocks (file->f_buffer, file->f_sb, file->f_physblock, 1);
  if (ret != 0)
    return ret;
  file->f_flags &= ~EXT2_FILE_BUFFER_DIRTY;
  return ret;
}

int
ext2_sync_file_buffer_pos (Ext2File *file)
{
  block_t block = file->f_pos / file->f_sb->sb_blksize;
  int ret;
  if (block != file->f_block)
    {
      ret = ext2_file_flush (file);
      if (ret != 0)
	return ret;
      file->f_flags &= ~EXT2_FILE_BUFFER_VALID;
    }
  file->f_block = block;
  return 0;
}

int
ext2_load_file_buffer (Ext2File *file, int nofill)
{
  int retflags;
  int ret;
  if (!(file->f_flags & EXT2_FILE_BUFFER_VALID))
    {
      ret = ext2_bmap (file->f_sb, file->f_ino, &file->f_inode,
		       file->f_buffer + file->f_sb->sb_blksize, 0,
		       file->f_block, &retflags, &file->f_physblock);
      if (ret != 0)
	return ret;
      if (!nofill)
	{
	  if (file->f_physblock && !(retflags & BMAP_RET_UNINIT))
	    {
	      ret = ext2_read_blocks (file->f_buffer, file->f_sb,
				      file->f_physblock, 1);
	      if (ret != 0)
		return ret;
	    }
	  else
	    memset (file->f_buffer, 0, file->f_sb->sb_blksize);
	}
      file->f_flags |= EXT2_FILE_BUFFER_VALID;
    }
  return 0;
}

int
ext2_iblk_add_blocks (VFSSuperblock *sb, Ext2Inode *inode, block_t nblocks)
{
  Ext2Filesystem *fs = sb->sb_private;
  blkcnt64_t b = inode->i_blocks;
  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
    b += (blkcnt64_t) inode->osd2.linux2.l_i_blocks_hi << 32;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
      || !(inode->i_flags & EXT4_HUGE_FILE_FL))
    nblocks *= sb->sb_blksize / 512;
  nblocks *= EXT2_CLUSTER_RATIO (fs);
  b += nblocks;
  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
    inode->osd2.linux2.l_i_blocks_hi = b >> 32;
  else if (b > 0xffffffff)
    return -EOVERFLOW;
  inode->i_blocks = b & 0xffffffff;
  return 0;
}

int
ext2_iblk_sub_blocks (VFSSuperblock *sb, Ext2Inode *inode, block_t nblocks)
{
  Ext2Filesystem *fs = sb->sb_private;
  blkcnt64_t b = inode->i_blocks;
  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
    b += (blkcnt64_t) inode->osd2.linux2.l_i_blocks_hi << 32;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
      || !(inode->i_flags & EXT4_HUGE_FILE_FL))
    nblocks *= sb->sb_blksize / 512;
  nblocks *= EXT2_CLUSTER_RATIO (fs);
  if (nblocks > b)
    return -EOVERFLOW;
  b -= nblocks;
  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
    inode->osd2.linux2.l_i_blocks_hi = b >> 32;
  inode->i_blocks = b & 0xffffffff;
  return 0;
}

int
ext2_iblk_set (VFSSuperblock *sb, Ext2Inode *inode, block_t nblocks)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
      || !(inode->i_flags & EXT4_HUGE_FILE_FL))
    nblocks *= sb->sb_blksize / 512;
  nblocks *= EXT2_CLUSTER_RATIO (fs);
  inode->i_blocks = nblocks & 0xffffffff;
  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
    inode->osd2.linux2.l_i_blocks_hi = nblocks >> 32;
  else if (nblocks >> 32)
    return -EOVERFLOW;
  return 0;
}

int
ext2_zero_blocks (VFSSuperblock *sb, block_t block, int num, block_t *result,
		  int *count)
{
  static void *buffer;
  static int stride_len;
  int c;
  int i;
  int ret;
  if (sb->sb_private == NULL)
    {
      if (buffer != NULL)
	{
	  kfree (buffer);
	  buffer = NULL;
	  stride_len = 0;
	}
      return 0;
    }
  if (num <= 0)
    return 0;

  if (num > stride_len && stride_len < EXT2_MAX_STRIDE_LENGTH (sb))
    {
      void *ptr;
      int new_stride = num;
      if (new_stride > EXT2_MAX_STRIDE_LENGTH (sb))
	new_stride = EXT2_MAX_STRIDE_LENGTH (sb);
      ptr = kmalloc (sb->sb_blksize * new_stride);
      if (unlikely (ptr == NULL))
	return -ENOMEM;
      kfree (buffer);
      buffer = ptr;
      stride_len = new_stride;
      memset (buffer, 0, sb->sb_blksize * stride_len);
    }

  i = 0;
  while (i < num)
    {
      if (block % stride_len)
	{
	  c = stride_len - block % stride_len;
	  if (c > num - i)
	    c = num - i;
	}
      else
	{
	  c = num - i;
	  if (c > stride_len)
	    c = stride_len;
	}
      ret = ext2_write_blocks (buffer, sb, block, c);
      if (ret != 0)
	{
	  if (count != NULL)
	    *count = c;
	  if (result != NULL)
	    *result = block;
	  return ret;
	}
      i += c;
      block += c;
    }
  return 0;
}

int
ext2_new_dir_block (VFSSuperblock *sb, ino64_t ino, ino64_t parent,
		    char **block)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2DirEntry *dir = NULL;
  char *buffer;
  int rec_len;
  int filetype = 0;
  int csum_size = 0;
  Ext2DirEntryTail *t;
  int ret;

  buffer = kzalloc (sb->sb_blksize);
  if (unlikely (buffer == NULL))
    return -ENOMEM;
  dir = (Ext2DirEntry *) buffer;

  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM)
    csum_size = sizeof (Ext2DirEntryTail);

  ret = ext2_set_rec_len (sb, sb->sb_blksize - csum_size, dir);
  if (ret != 0)
    {
      kfree (buffer);
      return ret;
    }

  if (ino != 0)
    {
      if (fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_FILETYPE)
	filetype = EXT2_DIRTYPE_DIR;

      /* Setup `.' */
      dir->d_inode = ino;
      dir->d_name_len = (filetype << 8) | 1;
      dir->d_name[0] = '.';
      rec_len = sb->sb_blksize - csum_size - ext2_dir_rec_len (1, 0);
      dir->d_rec_len = ext2_dir_rec_len (1, 0);

      /* Setup `..' */
      dir = (Ext2DirEntry *) (buffer + dir->d_rec_len);
      ret = ext2_set_rec_len (sb, rec_len, dir);
      if (ret != 0)
	{
	  kfree (buffer);
	  return ret;
	}
      dir->d_inode = parent;
      dir->d_name_len = (filetype << 8) | 2;
      dir->d_name[0] = '.';
      dir->d_name[1] = '.';
    }

  if (csum_size > 0)
    {
      t = EXT2_DIRENT_TAIL (buffer, sb->sb_blksize);
      ext2_init_dirent_tail (sb, t);
    }
  *block = buffer;
  return 0;
}

int
ext2_write_dir_block (VFSSuperblock *sb, block_t block, char *buffer, int flags,
		      VFSInode *inode)
{
  int ret = ext2_dir_block_checksum_update (sb, inode, (Ext2DirEntry *) buffer);
  if (ret != 0)
    return ret;
  return ext2_write_blocks (buffer, sb, block, 1);
}

int
ext2_new_block (VFSSuperblock *sb, block_t goal, Ext2Bitmap *map,
		block_t *result, Ext2BlockAllocContext *ctx)
{
  Ext2Filesystem *fs = sb->sb_private;
  block_t b = 0;
  int ret;
  if (map == NULL)
    map = fs->f_block_bitmap;
  if (map == NULL)
    return -EINVAL;

  if (goal == 0 || goal >= ext2_blocks_count (&fs->f_super))
    goal &= ~EXT2_CLUSTER_MASK (fs);

  ret = ext2_find_first_zero_bitmap (map, goal,
				     ext2_blocks_count (&fs->f_super) - 1, &b);
  if (ret == -ENOENT && goal != fs->f_super.s_first_data_block)
    ret = ext2_find_first_zero_bitmap (map, fs->f_super.s_first_data_block,
				       goal - 1, &b);
  if (ret == -ENOENT)
    return -ENOENT;
  if (ret != 0)
    return ret;
  ext2_clear_block_uninit (sb, ext2_group_of_block (fs, b));
  *result = b;
  return 0;
}

int
ext2_new_inode (VFSSuperblock *sb, ino64_t dir, Ext2Bitmap *map,
		ino64_t *result)
{
  Ext2Filesystem *fs = sb->sb_private;
  ino64_t start_inode = 0;
  ino64_t ino_in_group;
  ino64_t upto;
  ino64_t first_zero;
  ino64_t i;
  unsigned int group;
  int ret;
  if (map == NULL)
    map = fs->f_inode_bitmap;
  if (map == NULL)
    return -EINVAL;

  if (dir > 0)
    {
      group = (dir - 1) / fs->f_super.s_inodes_per_group;
      start_inode = group * fs->f_super.s_inodes_per_group + 1;
    }

  if (start_inode < EXT2_FIRST_INODE (fs->f_super))
    start_inode = EXT2_FIRST_INODE (fs->f_super);
  if (start_inode > fs->f_super.s_inodes_count)
    return -ENOSPC;

  i = start_inode;
  do
    {
      ino_in_group = (i - 1) % fs->f_super.s_inodes_per_group;
      group = (i - 1) / fs->f_super.s_inodes_per_group;

      ext2_check_inode_uninit (sb, map, group);
      upto = i + fs->f_super.s_inodes_per_group - ino_in_group;

      if (i < start_inode && upto >= start_inode)
	upto = start_inode - 1;
      if (upto > fs->f_super.s_inodes_count)
	upto = fs->f_super.s_inodes_count;

      ret = ext2_find_first_zero_bitmap (map, i, upto, &first_zero);
      if (ret == 0)
	{
	  i = first_zero;
	  break;
	}
      if (ret != -ENOENT)
	return -ENOSPC;

      i = upto + 1;
      if (i > fs->f_super.s_inodes_count)
	i = EXT2_FIRST_INODE (fs->f_super);
    }
  while (i != start_inode);

  if (ext2_test_bitmap (map, i))
    return -ENOSPC;
  *result = i;
  return 0;
}

int
ext2_write_new_inode (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Inode *buffer;
  Ext2LargeInode *large;
  int size = EXT2_INODE_SIZE (fs->f_super);
  time_t t = fs->f_now != 0 ? fs->f_now : time (NULL);
  int ret;
  if (inode->i_ctime == 0)
    inode->i_ctime = t;
  if (inode->i_mtime == 0)
    inode->i_mtime = t;
  if (inode->i_atime == 0)
    inode->i_atime = t;

  if (size == sizeof (Ext2Inode))
    return ext2_update_inode (sb, ino, inode, sizeof (Ext2Inode));

  buffer = kzalloc (size);
  if (unlikely (buffer == NULL))
    return -ENOMEM;
  memcpy (buffer, inode, sizeof (Ext2Inode));

  large = (Ext2LargeInode *) buffer;
  large->i_extra_isize = sizeof (Ext2LargeInode) - EXT2_OLD_INODE_SIZE;
  if (large->i_crtime == 0)
    large->i_crtime = t;

  ret = ext2_update_inode (sb, ino, buffer, size);
  kfree (buffer);
  return ret;
}

int
ext2_new_file (VFSInode *dir, const char *name, mode_t mode, VFSInode **result)
{
  Ext2Filesystem *fs = dir->vi_sb->sb_private;
  Process *proc = &process_table[task_getpid ()];
  VFSInode *inode;
  Ext2File *file;
  Ext2Inode *ei;
  ino64_t ino;
  int ret = ext2_read_bitmaps (dir->vi_sb);
  if (ret != 0)
    return ret;

  inode = vfs_alloc_inode (dir->vi_sb);
  if (unlikely (inode == NULL))
    return -ENOMEM;
  ret = ext2_new_inode (dir->vi_sb, dir->vi_ino, fs->f_inode_bitmap, &ino);
  if (ret != 0)
    {
      vfs_unref_inode (inode);
      return ret;
    }

  file = kzalloc (sizeof (Ext2File));
  if (unlikely (file == NULL))
    {
      vfs_unref_inode (inode);
      return -ENOMEM;
    }
  inode->vi_private = file;
  ret = ext2_open_file (dir->vi_sb, ino, file);
  if (ret != 0)
    {
      vfs_unref_inode (inode);
      return ret;
    }

  ei = &file->f_inode;
  ei->i_mode = mode;
  ei->i_uid = proc->p_euid;
  ei->i_gid = proc->p_egid;
  ei->i_links_count = 1;
  ext2_write_new_inode (dir->vi_sb, ino, ei);
  ext2_inode_alloc_stats (dir->vi_sb, ino, 1, S_ISDIR (ei->i_mode));
  ret = ext2_add_link (dir->vi_sb, dir, name, ino, ext2_dir_type (mode));

  inode->vi_ino = ino;
  inode->vi_mode = ei->i_mode;
  inode->vi_uid = ei->i_uid;
  inode->vi_gid = ei->i_gid;
  inode->vi_nlink = ei->i_links_count;

  if (result == NULL)
    vfs_unref_inode (inode);
  else
    *result = inode;
  return ret;
}

int
ext2_alloc_block (VFSSuperblock *sb, block_t goal, char *blockbuf,
		  block_t *result, Ext2BlockAllocContext *ctx)
{
  Ext2Filesystem *fs = sb->sb_private;
  block_t block;
  int ret;
  if (fs->f_block_bitmap == NULL)
    {
      ret =
	ext2_read_bitmap (sb, EXT2_BITMAP_BLOCK, 0, fs->f_group_desc_count - 1);
      if (ret != 0)
	return ret;
      ret = ext2_new_block (sb, goal, NULL, &block, ctx);
      if (ret != 0)
	return ret;
    }
  if (blockbuf != NULL)
    {
      memset (blockbuf, 0, sb->sb_blksize);
      ret = ext2_write_blocks (blockbuf, sb, block, 1);
    }
  else
    ret = ext2_zero_blocks (sb, block, 1, NULL, NULL);
  if (ret != 0)
    return ret;
  ext2_block_alloc_stats (sb, block, 1);
  *result = block;
  return ret;
}

int
ext2_dealloc_blocks (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
		     char *blockbuf, block_t start, block_t end)
{
  Ext2Inode inode_buf;
  int ret;
  if (start > end)
    return -EINVAL;
  if (inode == NULL)
    {
      ret = ext2_read_inode (sb, ino, &inode_buf);
      if (ret != 0)
	return ret;
      inode = &inode_buf;
    }

  if (inode->i_flags & EXT4_INLINE_DATA_FL)
    return -ENOTSUP;

  if (inode->i_flags & EXT4_EXTENTS_FL)
    ret = ext3_extent_dealloc_blocks (sb, ino, inode, start, end);
  else
    ret = ext2_dealloc_indirect (sb, inode, blockbuf, start, end);
  if (ret != 0)
    return ret;
  return ext2_update_inode (sb, ino, inode, sizeof (Ext2Inode));
}

int
ext2_lookup_inode (VFSSuperblock *sb, VFSInode *dir, const char *name,
		   int namelen, char *buffer, ino64_t *inode)
{
  Ext2Lookup l;
  int ret;
  l.name = name;
  l.namelen = namelen;
  l.inode = inode;
  l.found = 0;
  ret = ext2_dir_iterate (sb, dir, 0, buffer, ext2_process_lookup, &l);
  if (ret != 0)
    return ret;
  return l.found ? 0 : -ENOENT;
}

int
ext2_expand_dir (VFSInode *dir)
{
  Ext2File *file = dir->vi_private;
  Ext2DirExpand e;
  int ret;
  if (dir->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  e.de_done = 0;
  e.de_err = 0;
  e.de_goal = ext2_find_inode_goal (dir->vi_sb, dir->vi_ino, &file->f_inode, 0);
  e.de_newblocks = 0;
  e.de_dir = dir;
  ret = ext2_block_iterate (dir->vi_sb, dir, BLOCK_FLAG_APPEND, NULL,
			    ext2_process_dir_expand, &e);
  if (ret == -ENOTSUP)
    return -ENOTSUP;
  if (e.de_err != 0)
    return e.de_err;
  if (!e.de_done)
    return -EIO;
  ret =
    ext2_inode_set_size (dir->vi_sb, &file->f_inode,
			 EXT2_I_SIZE (file->f_inode) +dir->vi_sb->sb_blksize);
  if (ret != 0)
    return ret;
  ext2_iblk_add_blocks (dir->vi_sb, &file->f_inode, e.de_newblocks);
  return ext2_write_inode (dir);
}

int
ext2_extend_inode (VFSInode *inode, blkcnt64_t origblocks, blkcnt64_t newblocks)
{
  Ext2Superblock *esb = &((Ext2Filesystem *) inode->vi_sb->sb_private)->f_super;
  Ext2Inode *ei = inode->vi_private;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  blkcnt64_t i;
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  int ret;

  for (i = origblocks; i < MIN (newblocks, EXT2_STORED_INODES); i++)
    {
      block_t block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
      if (block < 0)
	return block;
      ei->i_block[i - 1] = block;
    }

  /* If the loop above was enough to allocate all needed blocks, finish */
  if (newblocks <= EXT2_STORED_INODES)
    {
      ext2_write_inode (inode);
      return 0;
    }

  /* Fill indirect block pointer */
  if (ei->i_block[12] == 0)
    {
      block_t block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
      if (block < 0)
	return block;
      ei->i_block[12] = block;
    }
  bptr1 = kmalloc (blksize);
  if (unlikely (bptr1 == NULL))
    goto err;
  ret = ext2_read_blocks (bptr1, inode->vi_sb, ei->i_block[12], 1);
  if (ret != 0)
    goto err;
  for (i = origblocks; i < MIN (newblocks, blksize + EXT2_STORED_INODES); i++)
    {
      block_t block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
      if (block < 0)
	{
	  ret = block;
	  goto err;
	}
      bptr1[i - EXT2_STORED_INODES] = block;
    }
  ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->i_block[12], 1);
  if (ret != 0)
    goto err;

  /* Fill doubly indirect block pointer */
  if (ei->i_block[13] == 0)
    {
      block_t block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
      if (block < 0)
	return block;
      ei->i_block[13] = block;
    }
  bptr2 = kmalloc (blksize);
  if (unlikely (bptr2 == NULL))
    goto err;
  ret = ext2_read_blocks (bptr2, inode->vi_sb, ei->i_block[13], 1);
  if (ret != 0)
    goto err;
  for (i = 0; i < blksize; i++)
    {
      block_t j;
      ret = ext2_read_blocks (bptr1, inode->vi_sb, bptr2[i], 1);
      if (ret != 0)
	goto err;
      for (j = 0; j < blksize; j++)
	{
	  block_t block;
	  if (origblocks >= i * blksize + j + EXT2_STORED_INODES)
	    continue;
	  if (newblocks < i * blksize + j + EXT2_STORED_INODES)
	    goto err; /* Done allocating, return */
	  block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					esb->s_inodes_per_group);
	  if (block < 0)
	    {
	      ret = block;
	      goto err;
	    }
	  bptr1[j] = block;
	}
      ret = ext2_write_blocks (bptr1, inode->vi_sb, bptr2[i], 1);
      if (ret != 0)
	goto err;
    }
  ret = ext2_write_blocks (bptr2, inode->vi_sb, ei->i_block[13], 1);
  if (ret != 0)
    goto err;

  /* Fill triply indirect block pointer */
  if (ei->i_block[14] == 0)
    {
      block_t block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
      if (block < 0)
	return block;
      ei->i_block[14] = block;
    }
  bptr3 = kmalloc (blksize);
  if (unlikely (bptr3 == NULL))
    goto err;
  ret = ext2_read_blocks (bptr3, inode->vi_sb, ei->i_block[14], 1);
  if (ret != 0)
    goto err;
  for (i = 0; i < blksize; i++)
    {
      block_t j;
      ret = ext2_read_blocks (bptr2, inode->vi_sb, bptr3[i], 1);
      if (ret != 0)
	goto err;
      for (j = 0; j < blksize; j++)
	{
	  block_t k;
	  ret = ext2_read_blocks (bptr1, inode->vi_sb, bptr2[j], 1);
	  if (ret != 0)
	    goto err;
	  for (k = 0; k < blksize; k++)
	    {
	      block_t block;
	      if (origblocks >=
		  i * blksize * blksize + j * blksize + k + EXT2_STORED_INODES)
		continue;
	      if (newblocks <
		  i * blksize * blksize + j * blksize + k + EXT2_STORED_INODES)
		goto err; /* Done allocating, return */
	      block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
	      if (block < 0)
		{
		  ret = block;
		  goto err;
		}
	      bptr1[k] = block;
	    }
	  ret = ext2_write_blocks (bptr1, inode->vi_sb, bptr2[j], 1);
	  if (ret != 0)
	    goto err;
	}
      ret = ext2_write_blocks (bptr2, inode->vi_sb, bptr3[i], 1);
      if (ret != 0)
	goto err;
    }
  ret = ext2_write_blocks (bptr3, inode->vi_sb, ei->i_block[14], 1);
  if (ret != 0)
    goto err;

  ext2_write_inode (inode);

 err:
  if (bptr1 != NULL)
    kfree (bptr1);
  if (bptr2 != NULL)
    kfree (bptr2);
  if (bptr3 != NULL)
    kfree (bptr3);
  return ret;
}

int
ext2_read_blocks (void *buffer, VFSSuperblock *sb, uint32_t block,
		  size_t nblocks)
{
  return sb->sb_dev->sd_read (sb->sb_dev, buffer, nblocks * sb->sb_blksize,
			      block * sb->sb_blksize);
}

int
ext2_write_blocks (const void *buffer, VFSSuperblock *sb, uint32_t block,
		   size_t nblocks)
{
  return sb->sb_dev->sd_write (sb->sb_dev, buffer, nblocks * sb->sb_blksize,
			       block * sb->sb_blksize);
}

int
ext2_data_blocks (Ext2Inode *inode, VFSSuperblock *sb, block_t block,
		  blkcnt64_t nblocks, block_t *result)
{
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  block_t realblock;
  blksize_t blksize = sb->sb_blksize;
  int ret = 0;
  int i = 0;

  /* First 12 data blocks stored directly in inode */
  while (block < EXT2_STORED_INODES)
    {
      if (i == nblocks)
	return 0;
      result[i++] = inode->i_block[block++];
    }

  /* Read indirect block pointer */
  bptr1 = kmalloc (blksize);
  if (block < EXT2_INDIR1_THRESH)
    {
      if (ext2_read_blocks (bptr1, sb, inode->i_block[12], 1) != 0)
	goto err;
      while (block < EXT2_INDIR1_THRESH)
	{
	  if (i == nblocks)
	    goto end;
	  result[i++] = bptr1[EXT2_INDIR1_BLOCK0];
	  block++;
	}
    }

  /* Read doubly indirect block pointer */
  bptr2 = kmalloc (blksize);
  if (block < EXT2_INDIR2_THRESH)
    {
      realblock = EXT2_INDIR2_BLOCK0;
      if (ext2_read_blocks (bptr1, sb, inode->i_block[13], 1) != 0)
	goto err;
      if (ext2_read_blocks (bptr2, sb, EXT2_INDIR2_BLOCK1, 1) != 0)
	goto err;
      while (block < EXT2_INDIR2_THRESH)
	{
	  if (i == nblocks)
	    goto end;
	  if (realblock % blksize == 0)
	    {
	      if (ext2_read_blocks (bptr2, sb, EXT2_INDIR2_BLOCK1, 1) != 0)
		goto err;
	    }
	  result[i++] = EXT2_INDIR2_BLOCK2;
	  block++;
	  realblock++;
	}
    }

  /* Read triply indirect block pointer */
  bptr3 = kmalloc (blksize);
  if (block >= EXT2_INDIR3_THRESH)
    goto large;
  realblock = EXT2_INDIR3_BLOCK0;
  if (ext2_read_blocks (bptr1, sb, inode->i_block[14], 1) != 0)
    goto err;
  if (ext2_read_blocks (bptr2, sb, EXT2_INDIR3_BLOCK1, 1) != 0)
    goto err;
  if (ext2_read_blocks (bptr3, sb, EXT2_INDIR3_BLOCK2, 1) != 0)
    goto err;
  while (block < EXT2_INDIR3_THRESH)
    {
      if (i == nblocks)
	goto end;
      if (realblock % (blksize * blksize) == 0)
	{
	  if (ext2_read_blocks (bptr2, sb, EXT2_INDIR3_BLOCK1, 1) != 0)
	    goto err;
	}
      if (realblock % blksize == 0)
	{
	  if (ext2_read_blocks (bptr3, sb, EXT2_INDIR3_BLOCK2, 1) != 0)
	    goto err;
	}
      result[i++] = EXT2_INDIR3_BLOCK3;
      block++;
      realblock++;
    }

 large: /* File offset too large */
  ret = -EINVAL;
  goto end;

 err:
  ret = -EIO;
 end:
  kfree (bptr1);
  kfree (bptr2);
  kfree (bptr3);
  return ret;
}

#define EXT2_TRY_UNALLOC(inode, param, data)				\
  do									\
    {									\
      ret = ext2_try_unalloc_pointer (inode, param, data);		\
      if (ret == 0)							\
        param = 0;							\
      else if (ret < 0)							\
	goto err;							\
    }									\
  while (0)

int
ext2_unalloc_data_blocks (VFSInode *inode, block_t start, blkcnt64_t nblocks)
{
  Ext2Inode *ei = inode->vi_private;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  blkcnt64_t i;
  int ret = 0;

  for (i = 0; i < nblocks; i++)
    {
      block_t block = start + i;
      block_t realblock;

      /* First 12 data blocks stored directly in inode */
      if (block < EXT2_STORED_INODES)
        {
	  ret = ext2_unalloc_block (inode->vi_sb, ei->i_block[block]);
	  if (ret != 0)
	    goto err;
	  continue;
	}

      /* Read indirect block pointer */
      if (bptr1 == NULL)
	bptr1 = kmalloc (blksize);
      if (block < blksize + EXT2_STORED_INODES)
	{
	  if (ext2_read_blocks (bptr1, inode->vi_sb, ei->i_block[12], 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  ret = ext2_unalloc_block (inode->vi_sb,
				    bptr1[block - EXT2_STORED_INODES]);
	  bptr1[block - EXT2_STORED_INODES] = 0;
	  if (ret != 0)
	    goto err;
	  ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->i_block[12], 1);
	  if (ret != 0)
	    goto err;
	  EXT2_TRY_UNALLOC (inode, ei->i_block[12], bptr1);
	  if (ret == 0)
	    ext2_write_inode (inode);
	  continue;
	}

      /* Read doubly indirect block pointer */
      if (bptr2 == NULL)
	bptr2 = kmalloc (blksize);
      if (block < blksize * blksize + blksize + EXT2_STORED_INODES)
	{
	  realblock = block - blksize - EXT2_STORED_INODES;
	  if (ext2_read_blocks (bptr1, inode->vi_sb, ei->i_block[13], 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  if (ext2_read_blocks (bptr2, inode->vi_sb,
				bptr1[realblock / blksize], 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  ret = ext2_unalloc_block (inode->vi_sb, bptr2[realblock % blksize]);
	  bptr2[realblock % blksize] = 0;
	  if (ret != 0)
	    goto err;
	  ret = ext2_write_blocks (bptr2, inode->vi_sb,
				   bptr1[realblock / blksize], 1);
	  if (ret != 0)
	    goto err;
	  EXT2_TRY_UNALLOC (inode, bptr1[realblock / blksize], bptr2);
	  if (ret == 0)
	    {
	      ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->i_block[13], 1);
	      if (ret != 0)
		goto err;
	    }
	  EXT2_TRY_UNALLOC (inode, ei->i_block[12], bptr1);
	  if (ret == 0)
	    ext2_write_inode (inode);
	  continue;
	}

      /* Read triply indirect block pointer */
      if (bptr3 == NULL)
	bptr3 = kmalloc (blksize);
      if (block < blksize * blksize * blksize + blksize * blksize + blksize +
	  EXT2_STORED_INODES)
	{
	  uint32_t blk;
	  realblock = block - blksize * blksize - blksize - EXT2_STORED_INODES;
	  if (ext2_read_blocks (bptr1, inode->vi_sb, ei->i_block[14], 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  if (ext2_read_blocks (bptr2, inode->vi_sb,
				bptr1[realblock / (blksize * blksize)], 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  blk = bptr2[realblock % (blksize * blksize) / blksize];
	  if (ext2_read_blocks (bptr3, inode->vi_sb, blk, 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  ret = ext2_unalloc_block (inode->vi_sb, bptr3[realblock % blksize]);
	  bptr3[realblock % blksize] = 0;
	  if (ret != 0)
	    goto err;
	  ret = ext2_write_blocks (bptr3, inode->vi_sb, blk, 1);
	  if (ret != 0)
	    goto err;
	  EXT2_TRY_UNALLOC (inode,
			    bptr2[realblock % (blksize * blksize) / blksize],
			    bptr3);
	  if (ret == 0)
	    {
	      ret =
		ext2_write_blocks (bptr2, inode->vi_sb,
				   bptr1[realblock / (blksize * blksize)], 1);
	      if (ret != 0)
		goto err;
	    }
	  EXT2_TRY_UNALLOC (inode, bptr1[realblock / (blksize * blksize)],
			    bptr2);
	  if (ret == 0)
	    {
	      ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->i_block[14], 1);
	      if (ret != 0)
		goto err;
	    }
	  EXT2_TRY_UNALLOC (inode, ei->i_block[14], bptr1);
	  if (ret == 0)
	    ext2_write_inode (inode);
	  continue;
	}

      /* Block offset too large */
      return -EINVAL;
    }

 err:
  if (bptr1 != NULL)
    kfree (bptr1);
  if (bptr2 != NULL)
    kfree (bptr2);
  if (bptr3 != NULL)
    kfree (bptr3);
  return ret;
}

#undef EXT2_TRY_UNALLOC

int
ext2_unref_inode (VFSSuperblock *sb, VFSInode *inode)
{
  Ext2Inode *ei = inode->vi_private;
  switch (ei->i_links_count)
    {
    case 0:
      return 0;
    case 1:
      ei->i_dtime = time (NULL);
      return ext2_clear_inode (sb, inode->vi_ino);
    default:
      ei->i_links_count--;
      ext2_write_inode (inode);
      return 0;
    }
}

block_t
ext2_old_alloc_block (VFSSuperblock *sb, int prefbg)
{
  int i;
  if (prefbg >= 0)
    {
      block_t block = ext2_try_alloc_block (sb, prefbg);
      if (block == -ENOMEM)
	return -ENOMEM;
      else if (block >= 0)
	return block;
    }
  for (i = 0; i < ext2_bgdt_size (sb->sb_private); i++)
    {
      block_t block;
      if (i == prefbg)
	continue; /* Already tried this block group */
      block = ext2_try_alloc_block (sb, i);
      if (block == -ENOMEM)
	return -ENOMEM;
      else if (block >= 0)
	return block;
    }
  return -ENOSPC;
}

ino64_t
ext2_create_inode (VFSSuperblock *sb, int prefbg)
{
  ino64_t inode;
  int i;
  if (prefbg >= 0)
    {
      inode = ext2_try_create_inode (sb, prefbg);
      if (inode > 0)
	return inode;
    }
  for (i = 0; i < ext2_bgdt_size (sb->sb_private); i++)
    {
      if (i == prefbg)
	continue; /* Already tried this block group */
      inode = ext2_try_create_inode (sb, i);
      if (inode > 0)
	return inode;
    }
  return 0;
}

int
ext2_get_rec_len (VFSSuperblock *sb, Ext2DirEntry *dirent,
		  unsigned int *rec_len)
{
  unsigned int len = dirent->d_rec_len;
  if (sb->sb_blksize < 65536)
    *rec_len = len;
  else if (len == 65535 || len == 0)
    *rec_len = sb->sb_blksize;
  else
    *rec_len = (len & 65532) | ((len & 3) << 16);
  return 0;
}

int
ext2_set_rec_len (VFSSuperblock *sb, unsigned int len, Ext2DirEntry *dirent)
{
  if (len > sb->sb_blksize || sb->sb_blksize > 262144 || len & 3)
    return -EINVAL;
  if (len < 65536)
    {
      dirent->d_rec_len = len;
      return 0;
    }
  if (len == sb->sb_blksize)
    {
      if (sb->sb_blksize == 65536)
	dirent->d_rec_len = 65535;
      else
	dirent->d_rec_len = 0;
    }
  else
    dirent->d_rec_len = (len & 65532) | ((len >> 16) & 3);
  return 0;
}

int
ext2_block_iterate (VFSSuperblock *sb, VFSInode *dir, int flags, char *blockbuf,
		    int (*func) (VFSSuperblock *, block_t *, blkcnt64_t,
				 block_t, int, void *), void *private)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2File *file = dir->vi_private;
  Ext2Inode *inode = &file->f_inode;
  Ext2BlockContext ctx;
  block_t block;
  int limit;
  int i;
  int r;
  int ret = 0;
  if (inode->i_flags & EXT4_INLINE_DATA_FL)
    return -ENOTSUP;

  if (flags & BLOCK_FLAG_NO_LARGE)
    {
      if (!S_ISDIR (inode->i_mode) && inode->i_size_high != 0)
	return -EFBIG;
    }

  limit = sb->sb_blksize >> 2;
  ctx.b_sb = sb;
  ctx.b_func = func;
  ctx.b_private = private;
  ctx.b_flags = flags;
  ctx.b_blkcnt = 0;
  if (blockbuf != NULL)
    ctx.b_ind_buf = blockbuf;
  else
    {
      ctx.b_ind_buf = kmalloc (sb->sb_blksize * 3);
      if (unlikely (ctx.b_ind_buf == NULL))
	return -ENOMEM;
    }
  ctx.b_dind_buf = ctx.b_ind_buf + sb->sb_blksize;
  ctx.b_tind_buf = ctx.b_dind_buf + sb->sb_blksize;

  if (fs->f_super.s_creator_os == EXT2_OS_HURD
      && !(flags & BLOCK_FLAG_DATA_ONLY))
    {
      if (inode->osd1.hurd1.h_i_translator != 0)
	{
	  block = inode->osd1.hurd1.h_i_translator;
	  ret |= ctx.b_func (sb, &block, BLOCK_COUNT_TRANSLATOR, 0, 0, private);
	  inode->osd1.hurd1.h_i_translator = (uint32_t) block;
	  if (ret & BLOCK_ABORT)
	    goto err;
	  if ((ctx.b_flags & BLOCK_FLAG_READ_ONLY) && (ret & BLOCK_CHANGED))
	    {
	      ctx.b_err = -EROFS;
	      ret |= BLOCK_ABORT | BLOCK_ERROR;
	      goto err;
	    }
	}
    }

  if (inode->i_flags & EXT4_EXTENTS_FL)
    {
      Ext3ExtentHandle *handle;
      Ext3GenericExtent extent;
      Ext3GenericExtent next;
      blkcnt64_t blkcnt = 0;
      block_t block;
      block_t new_block;
      int op = EXT2_EXTENT_ROOT;
      int uninit;
      unsigned int j;

      ctx.b_err = ext3_extent_open (sb, dir->vi_ino, inode, &handle);
      if (ctx.b_err != 0)
	goto err;

      while (1)
	{
	  if (op == EXT2_EXTENT_CURRENT)
	    ctx.b_err = 0;
	  else
	    ctx.b_err = ext3_extent_get (handle, op, &extent);
	  if (ctx.b_err != 0)
	    {
	      if (ctx.b_err != -ESRCH)
		break;
	      ctx.b_err = 0;
	      if (!(flags & BLOCK_FLAG_APPEND))
		break;

	    next_block_set:
	      block = 0;
	      r = ctx.b_func (sb, &block, blkcnt, 0, 0, private);
	      ret |= r;
	      if ((ctx.b_flags & BLOCK_FLAG_READ_ONLY) && (ret & BLOCK_CHANGED))
		{
		  ctx.b_err = -EROFS;
		  ret |= BLOCK_ABORT | BLOCK_ERROR;
		  goto err;
		}
	      if (r & BLOCK_CHANGED)
		{
		  ctx.b_err = ext3_extent_set_bmap (handle, blkcnt++, block, 0);
		  if (ctx.b_err != 0 || (ret & BLOCK_ABORT))
		    break;
		  if (block != 0)
		    goto next_block_set;
		}
	      break;
	    }

	  op = EXT2_EXTENT_NEXT;
	  block = extent.e_pblk;
	  if (!(extent.e_flags & EXT2_EXTENT_FLAGS_LEAF))
	    {
	      if (ctx.b_flags & BLOCK_FLAG_DATA_ONLY)
		continue;
	      if ((!(extent.e_flags & EXT2_EXTENT_FLAGS_SECOND_VISIT)
		   && !(ctx.b_flags & BLOCK_FLAG_DEPTH_TRAVERSE))
		  || ((extent.e_flags & EXT2_EXTENT_FLAGS_SECOND_VISIT)
		      && (ctx.b_flags & BLOCK_FLAG_DEPTH_TRAVERSE)))
		{
		  ret |= ctx.b_func (sb, &block, -1, 0, 0, private);
		  if (ret & BLOCK_CHANGED)
		    {
		      extent.e_pblk = block;
		      ctx.b_err = ext3_extent_replace (handle, 0, &extent);
		      if (ctx.b_err != 0)
			break;
		    }
		  if (ret & BLOCK_ABORT)
		    break;
		}
	      continue;
	    }

	  uninit = 0;
	  if (extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT)
	    uninit = EXT2_EXTENT_SET_BMAP_UNINIT;
	  ret = ext3_extent_get (handle, op, &next);

	  if (extent.e_lblk + extent.e_len <= (block_t) blkcnt)
	    continue;
	  if (extent.e_lblk > blkcnt)
	    blkcnt=  extent.e_lblk;
	  j = blkcnt - extent.e_lblk;
	  block += j;
	  for (blkcnt = extent.e_lblk, j = 0; j < extent.e_len;
	       block++, blkcnt++, j++)
	    {
	      new_block = block;
	      r = ctx.b_func (sb, &new_block, blkcnt, 0, 0, private);
	      ret |= r;
	      if ((ctx.b_flags & BLOCK_FLAG_READ_ONLY) && (ret & BLOCK_CHANGED))
		{
		  ctx.b_err = -EROFS;
		  ret |= BLOCK_ABORT | BLOCK_ERROR;
		  goto err;
		}
	      if (r & BLOCK_CHANGED)
		{
		  ctx.b_err = ext3_extent_set_bmap (handle, blkcnt, new_block,
						    uninit);
		  if (ctx.b_err != 0)
		    goto done;
		}
	      if (ret & BLOCK_ABORT)
		goto done;
		}
	  if (ret == 0)
	    {
	      extent = next;
	      op = EXT2_EXTENT_CURRENT;
	    }
	}

    done:
      ext3_extent_free (handle);
      ret |= BLOCK_ERROR;
      goto end;
    }

  for (i = 0; i < EXT2_NDIR_BLOCKS; i++, ctx.b_blkcnt++)
    {
      if (inode->i_block[i] != 0 || (flags & BLOCK_FLAG_APPEND))
	{
	  block = inode->i_block[i];
	  ret |= ctx.b_func (sb, &block, ctx.b_blkcnt, 0, i, private);
	  inode->i_block[i] = (uint32_t) block;
	  if (ret & BLOCK_ABORT)
	    goto err;
	}
    }
  if ((ctx.b_flags & BLOCK_FLAG_READ_ONLY) && (ret & BLOCK_CHANGED))
    {
      ctx.b_err = -EROFS;
      ret |= BLOCK_ABORT | BLOCK_ERROR;
      goto err;
    }

  if (inode->i_block[EXT2_IND_BLOCK] != 0 || (flags & BLOCK_FLAG_APPEND))
    {
      ret |= ext2_block_iterate_ind (&inode->i_block[EXT2_IND_BLOCK], 0,
				     EXT2_IND_BLOCK, &ctx);
      if (ret & BLOCK_ABORT)
	goto err;
    }
  else
    ctx.b_blkcnt += limit;

  if (inode->i_block[EXT2_DIND_BLOCK] != 0 || (flags & BLOCK_FLAG_APPEND))
    {
      ret |= ext2_block_iterate_dind (&inode->i_block[EXT2_DIND_BLOCK], 0,
				      EXT2_DIND_BLOCK, &ctx);
      if (ret & BLOCK_ABORT)
	goto err;
    }
  else
    ctx.b_blkcnt += limit * limit;

  if (inode->i_block[EXT2_TIND_BLOCK] != 0 || (flags & BLOCK_FLAG_APPEND))
    {
      ret |= ext2_block_iterate_tind (&inode->i_block[EXT2_TIND_BLOCK], 0,
				      EXT2_TIND_BLOCK, &ctx);
      if (ret & BLOCK_ABORT)
	goto err;
    }

 err:
  if (ret & BLOCK_CHANGED)
    {
      ret = ext2_update_inode (sb, dir->vi_ino, inode, sizeof (Ext2Inode));
      if (ret != 0)
	{
	  ret |= BLOCK_ERROR;
	  ctx.b_err = ret;
	}
    }

 end:
  if (blockbuf == NULL)
    kfree (ctx.b_ind_buf);
  return ret & BLOCK_ERROR ? ctx.b_err : 0;
}

int
ext2_dir_iterate (VFSSuperblock *sb, VFSInode *dir, int flags, char *blockbuf,
		  int (*func) (VFSInode *, int, Ext2DirEntry *, int, blksize_t,
			       char *, void *), void *private)
{
  Ext2DirContext ctx;
  int ret;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;

  ctx.d_dir = dir;
  ctx.d_flags = flags;
  if (blockbuf != NULL)
    ctx.d_buffer = blockbuf;
  else
    {
      ctx.d_buffer = kmalloc (sb->sb_blksize);
      if (unlikely (ctx.d_buffer == NULL))
	return -ENOMEM;
    }
  ctx.d_func = func;
  ctx.d_private = private;
  ctx.d_err = 0;
  ret = ext2_block_iterate (sb, dir, BLOCK_FLAG_READ_ONLY, 0,
			    ext2_process_dir_block, &ctx);

  if (blockbuf == NULL)
    kfree (ctx.d_buffer);
  if (ret == -ENOTSUP)
    return -ENOTSUP; /* TODO Support iterating over inline data */
  if (ret != 0)
    return ret;
  return ctx.d_err;
}

uint32_t
ext2_superblock_checksum (Ext2Superblock *s)
{
  int offset = offsetof (Ext2Superblock, s_checksum);
  return crc32 (0xffffffff, s, offset);
}

int
ext2_superblock_checksum_valid (Ext2Filesystem *fs)
{
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;
  return fs->f_super.s_checksum == ext2_superblock_checksum (&fs->f_super);
}

void
ext2_superblock_checksum_update (Ext2Filesystem *fs, Ext2Superblock *s)
{
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return;
  s->s_checksum = ext2_superblock_checksum (s);
}

uint16_t
ext2_bg_checksum (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return gdp->bg_checksum;
}

void
ext2_bg_checksum_update (VFSSuperblock *sb, unsigned int group,
			 uint16_t checksum)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  gdp->bg_checksum = checksum;
}

uint16_t
ext2_group_desc_checksum (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2GroupDesc *desc = ext2_group_desc (sb, fs->f_group_desc, group);
  size_t size = EXT2_DESC_SIZE (fs->f_super);
  size_t offset;
  uint16_t crc;
  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM)
    {
      uint16_t old_crc = desc->bg_checksum;
      uint32_t c32;
      desc->bg_checksum = 0;
      c32 = crc32 (fs->f_checksum_seed, &group, sizeof (unsigned int));
      c32 = crc32 (c32, desc, size);
      desc->bg_checksum = old_crc;
      crc = c32 & 0xffff;
    }
  else
    {
      offset = offsetof (Ext2GroupDesc, bg_checksum);
      crc = crc16 (0xffff, fs->f_super.s_uuid, 16);
      crc = crc16 (crc, &group, sizeof (unsigned int));
      crc = crc16 (crc, desc, offset);
      offset += sizeof (desc->bg_checksum);
      if (offset < size)
	crc = crc16 (crc, (char *) desc + offset, size - offset);
    }
  return crc;
}

int
ext2_group_desc_checksum_valid (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (ext2_has_group_desc_checksum (&fs->f_super)
      && ext2_bg_checksum (sb, group) != ext2_group_desc_checksum (sb, group))
    return 0;
  return 1;
}

void
ext2_group_desc_checksum_update (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (!ext2_has_group_desc_checksum (&fs->f_super))
    return;
  ext2_bg_checksum_update (sb, group, ext2_group_desc_checksum (sb, group));
}

uint32_t
ext2_inode_checksum (Ext2Filesystem *fs, ino64_t ino, Ext2LargeInode *inode,
		     int has_hi)
{
  uint32_t gen;
  size_t size = EXT2_INODE_SIZE (fs->f_super);
  uint16_t old_lo;
  uint16_t old_hi = 0;
  uint32_t crc;

  /* Set checksum fields to zero */
  old_lo = inode->i_checksum_lo;
  inode->i_checksum_lo = 0;
  if (has_hi)
    {
      old_hi = inode->i_checksum_hi;
      inode->i_checksum_hi = 0;
    }

  /* Calculate checksum */
  gen = inode->i_generation;
  crc = crc32 (fs->f_checksum_seed, &ino, sizeof (ino64_t));
  crc = crc32 (crc, &gen, 4);
  crc = crc32 (crc, inode, size);

  /* Restore inode checksum fields */
  inode->i_checksum_lo = old_lo;
  if (has_hi)
    inode->i_checksum_hi = old_hi;
  return crc;
}

int
ext2_inode_checksum_valid (Ext2Filesystem *fs, ino64_t ino,
			   Ext2LargeInode *inode)
{
  uint32_t crc;
  uint32_t provided;
  unsigned int i;
  unsigned int has_hi;
  char *ptr;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;

  has_hi = EXT2_INODE_SIZE (fs->f_super) > EXT2_OLD_INODE_SIZE
    && inode->i_extra_isize >= EXT4_INODE_CSUM_HI_EXTRA_END;
  provided = inode->i_checksum_lo;
  crc = ext2_inode_checksum (fs, ino, inode, has_hi);

  if (has_hi)
    provided |= inode->i_checksum_hi << 16;
  else
    crc &= 0xffff;
  if (provided == crc)
    return 1;

  /* Check if inode is all zeros */
  for (ptr = (char *) inode, i = 0; i < sizeof (Ext2Inode); ptr++, i++)
    {
      if (*ptr != '\0')
	return 0;
    }
  return 1;
}

void
ext2_inode_checksum_update (Ext2Filesystem *fs, ino64_t ino,
			    Ext2LargeInode *inode)
{
  uint32_t crc;
  int has_hi;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return;

  has_hi = EXT2_INODE_SIZE (fs->f_super) > EXT2_OLD_INODE_SIZE
    && inode->i_extra_isize >= EXT4_INODE_CSUM_HI_EXTRA_END;
  crc = ext2_inode_checksum (fs, ino, inode, has_hi);

  inode->i_checksum_lo = crc & 0xffff;
  if (has_hi)
    inode->i_checksum_hi = crc >> 16;
}

int
ext3_extent_block_checksum (VFSSuperblock *sb, ino64_t ino,
			    Ext3ExtentHeader *eh, uint32_t *crc)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Inode inode;
  uint32_t gen;
  size_t size =
    EXT2_EXTENT_TAIL_OFFSET (eh) + offsetof (Ext3ExtentTail, et_checksum);
  int ret = ext2_read_inode (sb, ino, &inode);
  if (ret != 0)
    return ret;
  gen = inode.i_generation;
  *crc = crc32 (fs->f_checksum_seed, &ino, sizeof (ino64_t));
  *crc = crc32 (*crc, &gen, 4);
  *crc = crc32 (*crc, eh, size);
  return 0;
}

int
ext3_extent_block_checksum_valid (VFSSuperblock *sb, ino64_t ino,
				  Ext3ExtentHeader *eh)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext3ExtentTail *t = EXT2_EXTENT_TAIL (eh);
  uint32_t provided;
  uint32_t crc;
  int ret;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;

  provided = t->et_checksum;
  ret = ext3_extent_block_checksum (sb, ino, eh, &crc);
  if (ret != 0)
    return 0;
  return provided == crc;
}

int
ext3_extent_block_checksum_update (VFSSuperblock *sb, ino64_t ino,
				   Ext3ExtentHeader *eh)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext3ExtentTail *t = EXT2_EXTENT_TAIL (eh);
  uint32_t crc;
  int ret;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;

  ret = ext3_extent_block_checksum (sb, ino, eh, &crc);
  if (ret != 0)
    return ret;
  t->et_checksum = crc;
  return 0;
}

uint32_t
ext2_block_bitmap_checksum (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t checksum = gdp->bg_block_bitmap_csum_lo;
  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_BLOCK_BITMAP_CSUM_HI_END)
    checksum |= (uint32_t) gdp->bg_block_bitmap_csum_hi << 16;
  return checksum;
}

int
ext2_block_bitmap_checksum_valid (VFSSuperblock *sb, unsigned int group,
				  char *bitmap, int size)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t provided;
  uint32_t crc;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;

  provided = gdp->bg_block_bitmap_csum_lo;
  crc = crc32 (fs->f_checksum_seed, bitmap, size);

  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_BLOCK_BITMAP_CSUM_HI_END)
    provided |= (uint32_t) gdp->bg_block_bitmap_csum_hi << 16;
  else
    crc &= 0xffff;
  return provided == crc;
}

void
ext2_block_bitmap_checksum_update (VFSSuperblock *sb, unsigned int group,
				   char *bitmap, int size)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t crc;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return;

  crc = crc32 (fs->f_checksum_seed, bitmap, size);
  gdp->bg_block_bitmap_csum_lo = crc & 0xffff;
  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_BLOCK_BITMAP_CSUM_HI_END)
    gdp->bg_block_bitmap_csum_hi = crc >> 16;
}

uint32_t
ext2_inode_bitmap_checksum (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t checksum = gdp->bg_inode_bitmap_csum_lo;
  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_INODE_BITMAP_CSUM_HI_END)
    checksum |= (uint32_t) gdp->bg_inode_bitmap_csum_hi << 16;
  return checksum;
}

int
ext2_inode_bitmap_checksum_valid (VFSSuperblock *sb, unsigned int group,
				  char *bitmap, int size)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t provided;
  uint32_t crc;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;

  provided = gdp->bg_inode_bitmap_csum_lo;
  crc = crc32 (fs->f_checksum_seed, bitmap, size);

  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_INODE_BITMAP_CSUM_HI_END)
    provided |= (uint32_t) gdp->bg_inode_bitmap_csum_hi << 16;
  else
    crc &= 0xffff;
  return provided == crc;
}

void
ext2_inode_bitmap_checksum_update (VFSSuperblock *sb, unsigned int group,
				   char *bitmap, int size)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t crc;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return;

  crc = crc32 (fs->f_checksum_seed, bitmap, size);
  gdp->bg_inode_bitmap_csum_lo = crc & 0xffff;
  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_INODE_BITMAP_CSUM_HI_END)
    gdp->bg_inode_bitmap_csum_hi = crc >> 16;
}

void
ext2_init_dirent_tail (VFSSuperblock *sb, Ext2DirEntryTail *t)
{
  memset (t, 0, sizeof (Ext2DirEntryTail));
  ext2_set_rec_len (sb, sizeof (Ext2DirEntryTail), (Ext2DirEntry *) t);
  t->det_reserved_name_len = EXT2_DIR_NAME_LEN_CHECKSUM;
}

uint32_t
ext2_dirent_checksum (VFSSuperblock *sb, VFSInode *dir, Ext2DirEntry *dirent,
		      size_t size)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2File *file = dir->vi_private;
  uint32_t gen = file->f_inode.i_generation;
  uint32_t crc = crc32 (fs->f_checksum_seed, &file->f_ino, sizeof (ino64_t));
  crc = crc32 (crc, &gen, 4);
  crc = crc32 (crc, dirent, size);
  return crc;
}

int
ext2_dirent_checksum_valid (VFSSuperblock *sb, VFSInode *dir,
			    Ext2DirEntry *dirent)
{
  Ext2DirEntryTail *t;
  int ret = ext2_get_dirent_tail (sb, dirent, &t);
  if (ret != 0)
    return 1;
  return t->det_checksum ==
    ext2_dirent_checksum (sb, dir, dirent, (char *) t - (char *) dirent);
}

int
ext2_dirent_checksum_update (VFSSuperblock *sb, VFSInode *dir,
			     Ext2DirEntry *dirent)
{
  Ext2DirEntryTail *t;
  int ret = ext2_get_dirent_tail (sb, dirent, &t);
  if (ret != 0)
    return ret;
  t->det_checksum =
    ext2_dirent_checksum (sb, dir, dirent, (char *) t - (char *) dirent);
  return 0;
}

int
ext2_dx_checksum (VFSSuperblock *sb, VFSInode *dir, Ext2DirEntry *dirent,
		  uint32_t *crc, Ext2DXTail **tail)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2File *file = dir->vi_private;
  Ext2DXTail *t;
  Ext2DXCountLimit *c;
  uint32_t dummy_checksum = 0;
  size_t size;
  uint32_t gen;
  int count_offset;
  int limit;
  int count;
  int ret = ext2_get_dx_count_limit (sb, dirent, &c, &count_offset);
  if (ret != 0)
    return ret;
  limit = c->cl_limit;
  count = c->cl_count;
  if (count_offset + limit * sizeof (Ext2DXEntry) > sb->sb_blksize -
      sizeof (Ext2DXTail))
    return -ENOSPC;

  t = (Ext2DXTail *) ((Ext2DXEntry *) c + limit);
  size = count_offset + count * sizeof (Ext2DXEntry);
  gen = file->f_inode.i_generation;

  *crc = crc32 (fs->f_checksum_seed, &file->f_ino, sizeof (ino64_t));
  *crc = crc32 (*crc, &gen, 4);
  *crc = crc32 (*crc, dirent, size);
  *crc = crc32 (*crc, t, 4);
  *crc = crc32 (*crc, &dummy_checksum, 4);

  if (tail != NULL)
    *tail = t;
  return 0;
}

int
ext2_dx_checksum_valid (VFSSuperblock *sb, VFSInode *dir, Ext2DirEntry *dirent)
{
  Ext2DXTail *t;
  uint32_t crc;
  int ret = ext2_dx_checksum (sb, dir, dirent, &crc, &t);
  if (ret != 0)
    return 0;
  return t->dt_checksum == crc;
}

int
ext2_dx_checksum_update (VFSSuperblock *sb, VFSInode *dir, Ext2DirEntry *dirent)
{
  Ext2DXTail *t;
  uint32_t crc;
  int ret = ext2_dx_checksum (sb, dir, dirent, &crc, &t);
  if (ret != 0)
    return ret;
  t->dt_checksum = crc;
  return 0;
}

int
ext2_dir_block_checksum_valid (VFSSuperblock *sb, VFSInode *dir,
			       Ext2DirEntry *dirent)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;
  if (ext2_get_dirent_tail (sb, dirent, NULL) == 0)
    return ext2_dirent_checksum_valid (sb, dir, dirent);
  if (ext2_get_dx_count_limit (sb, dirent, NULL, NULL) == 0)
    return ext2_dx_checksum_valid (sb, dir, dirent);
  return 0;
}

int
ext2_dir_block_checksum_update (VFSSuperblock *sb, VFSInode *dir,
				Ext2DirEntry *dirent)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 0;
  if (ext2_get_dirent_tail (sb, dirent, NULL) == 0)
    return ext2_dirent_checksum_update (sb, dir, dirent);
  if (ext2_get_dx_count_limit (sb, dirent, NULL, NULL) == 0)
    return ext2_dx_checksum_update (sb, dir, dirent);
  return -ENOSPC;
}
