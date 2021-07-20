/*************************************************************************
 * bmap.c -- This file is part of OS/0.                                  *
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

/* Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include <fs/ext2.h>
#include <libk/libk.h>
#include <sys/process.h>
#include <sys/sysmacros.h>
#include <vm/heap.h>

static int ext3_extent_bmap (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
			     Ext3ExtentHandle *handle, char *blockbuf,
			     int flags, block_t block, int *retflags,
			     int *blocks_alloc, block_t *physblock);

static int
block_ind_bmap (VFSSuperblock *sb, int flags, uint32_t ind, char *blockbuf,
		int *blocks_alloc, block_t blockno, uint32_t *result)
{
  uint32_t b;
  int ret;
  if (ind == 0)
    {
      if (flags & BMAP_SET)
	return -EINVAL;
      *result = 0;
      return 0;
    }

  ret = ext2_read_blocks (blockbuf, sb, ind, 1);
  if (ret != 0)
    return ret;

  if (flags & BMAP_SET)
    {
      b = *result;
      ((uint32_t *) blockbuf)[blockno] = b;
      return ext2_write_blocks (blockbuf, sb, ind, 1);
    }

  b = ((uint32_t *) blockbuf)[blockno];
  if (b == 0 && (flags & BMAP_ALLOC))
    {
      block_t b64;
      b = blockno != 0 ? ((uint32_t *) blockbuf)[blockno - 1] : ind;
      b64 = b;
      ret = ext2_alloc_block (sb, b64, blockbuf + sb->sb_blksize, &b64, NULL);
      b = b64;
      if (ret != 0)
	return ret;
      ((uint32_t *) blockbuf)[blockno] = b;

      ret = ext2_write_blocks (blockbuf, sb, ind, 1);
      if (ret != 0)
	return ret;
      blocks_alloc++;
    }

  *result = b;
  return 0;
}

static int
block_dind_bmap (VFSSuperblock *sb, int flags, uint32_t dind, char *blockbuf,
		 int *block_alloc, block_t blockno, uint32_t *result)
{
  uint32_t addr_per_block = sb->sb_blksize >> 2;
  uint32_t b;
  int ret = block_ind_bmap (sb, flags & ~BMAP_SET, dind, blockbuf, block_alloc,
			    blockno / addr_per_block, &b);
  if (ret != 0)
    return ret;
  return block_ind_bmap (sb, flags, b, blockbuf, block_alloc,
			 blockno % addr_per_block, result);
}

static int
block_tind_bmap (VFSSuperblock *sb, int flags, uint32_t tind, char *blockbuf,
		 int *block_alloc, block_t blockno, uint32_t *result)
{
  uint32_t addr_per_block = sb->sb_blksize >> 2;
  uint32_t b;
  int ret = block_dind_bmap (sb, flags & ~BMAP_SET, tind, blockbuf,
			     block_alloc, blockno / addr_per_block, &b);
  if (ret != 0)
    return ret;
  return block_ind_bmap (sb, flags, b, blockbuf, block_alloc,
			 blockno % addr_per_block, result);
}

static void
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
    *physblock = pblock -  + block - base;
}

static int
ext3_extent_bmap (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
		  Ext3ExtentHandle *handle, char *blockbuf, int flags,
		  block_t block, int *retflags, int *blocks_alloc,
		  block_t *physblock)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2BlockAllocContext alloc_ctx;
  Ext3GenericExtent extent;
  unsigned int offset;
  block_t b = 0;
  int alloc = 0;
  int ret = 0;
  int set_flags = flags & BMAP_UNINIT ? EXT2_EXTENT_SET_BMAP_UNINIT : 0;
  if (flags & BMAP_SET)
    return ext3_extent_set_bmap (handle, block, *physblock, set_flags);

  ret = ext3_extent_goto (handle, 0, block);
  if (ret != 0)
    {
      if (ret == -ENOENT)
	{
	  extent.e_lblk = block;
	  goto found;
	}
      return ret;
    }
  ret = ext3_extent_get (handle, EXT2_EXTENT_CURRENT, &extent);
  if (ret != 0)
    return ret;
  offset = block - extent.e_lblk;
  if (block >= extent.e_lblk && offset <= extent.e_len)
    {
      *physblock = extent.e_pblk + offset;
      if (retflags != NULL && (extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT))
	*retflags |= BMAP_RET_UNINIT;
    }

 found:
  if (*physblock == 0 && (flags & BMAP_ALLOC))
    {
      ext2_cluster_alloc (sb, ino, inode, handle, block, &b);
      if (b != 0)
	goto set_extent;
      ret = ext3_extent_bmap (sb, ino, inode, handle, blockbuf, 0, block - 1, 0,
			      blocks_alloc, &b);
      if (ret != 0)
	b = ext2_find_inode_goal (sb, ino, inode, block);
      alloc_ctx.bc_ino = ino;
      alloc_ctx.bc_inode = inode;
      alloc_ctx.bc_block = extent.e_lblk;
      alloc_ctx.bc_flags = BLOCK_ALLOC_DATA;
      ret = ext2_alloc_block (sb, b, blockbuf, &b, &alloc_ctx);
      if (ret != 0)
	return ret;
      b &= ~EXT2_CLUSTER_MASK (fs);
      b += EXT2_CLUSTER_MASK (fs) & block;
      alloc++;

    set_extent:
      ret = ext3_extent_set_bmap (handle, block, b, set_flags);
      if (ret != 0)
	{
	  ext2_block_alloc_stats (sb, b, -1);
	  return ret;
	}
      ret = ext2_read_inode (sb, ino, inode);
      if (ret != 0)
	return ret;
      *blocks_alloc += alloc;
      *physblock = b;
    }
  return 0;
}

static int
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
ext2_bmap (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode, char *blockbuf,
	   int flags, block_t block, int *retflags, block_t *physblock)
{
  Ext2Inode inode_buf;
  Ext3ExtentHandle *handle = NULL;
  block_t addr_per_block;
  uint32_t b;
  uint32_t b32;
  block_t b64;
  char *buffer = NULL;
  int blocks_alloc = 0;
  int inode_dirty = 0;
  int ret = 0;
  Ext2BlockAllocContext alloc_ctx = {ino, inode, 0, BLOCK_ALLOC_DATA};

  if (!(flags & BMAP_SET))
    *physblock = 0;
  if (retflags != NULL)
    *retflags = 0;

  if (inode == NULL)
    {
      ret = ext2_read_inode (sb, ino, &inode_buf);
      if (ret != 0)
	return ret;
      inode = &inode_buf;
    }
  addr_per_block = (block_t) sb->sb_blksize >> 2;

  if (ext2_file_block_offset_too_big (sb, inode, block))
    return -EFBIG;

  if (inode->i_flags & EXT4_INLINE_DATA_FL)
    return -EINVAL;
  if (blockbuf == NULL)
    {
      buffer = kmalloc (sb->sb_blksize * 2);
      if (unlikely (buffer == NULL))
	return -ENOMEM;
      blockbuf = buffer;
    }

  if (inode->i_flags & EXT4_EXTENTS_FL)
    {
      ret = ext3_extent_open (sb, ino, inode, &handle);
      if (ret != 0)
	goto end;
      ret = ext3_extent_bmap (sb, ino, inode, handle, blockbuf, flags, block,
			      retflags, &blocks_alloc, physblock);
      goto end;
    }

  if (block < EXT2_NDIR_BLOCKS)
    {
      if (flags & BMAP_SET)
	{
	  b = *physblock;
	  inode->i_block[block] = b;
	  inode_dirty++;
	  goto end;
	}

      *physblock = inode->i_block[block];
      b = block == 0 ? ext2_find_inode_goal (sb, ino, inode, block) :
	inode->i_block[block - 1];
      if (*physblock == 0 && (flags & BMAP_ALLOC))
	{
	  b64 = b;
	  ret = ext2_alloc_block (sb, b64, blockbuf, &b64, &alloc_ctx);
	  b = b64;
	  if (ret != 0)
	    goto end;
	  inode->i_block[block] = b;
	  blocks_alloc++;
	  *physblock = b;
	}
      goto end;
    }

  /* Indirect block */
  block -= EXT2_NDIR_BLOCKS;
  b32 = *physblock;
  if (block < addr_per_block)
    {
      b = inode->i_block[EXT2_IND_BLOCK];
      if (b == 0)
	{
	  if (!(flags & BMAP_ALLOC))
	    {
	      if (flags & BMAP_SET)
		ret = -EINVAL;
	      goto end;
	    }
	  b = inode->i_block[EXT2_IND_BLOCK - 1];
	  b64 = b;
	  ret = ext2_alloc_block (sb, b64, blockbuf, &b64, &alloc_ctx);
	  b = b64;
	  if (ret != 0)
	    goto end;
	  inode->i_block[EXT2_IND_BLOCK] = b;
	  blocks_alloc++;
	}
      ret = block_ind_bmap (sb, flags, b, blockbuf, &blocks_alloc, block, &b32);
      if (ret == 0)
	*physblock = b32;
      goto end;
    }

  /* Doubly indirect block */
  block -= addr_per_block;
  if (block < addr_per_block * addr_per_block)
    {
      b = inode->i_block[EXT2_DIND_BLOCK];
      if (b == 0)
	{
	  if (!(flags & BMAP_ALLOC))
	    {
	      if (flags & BMAP_SET)
		ret = -EINVAL;
	      goto end;
	    }
	  b = inode->i_block[EXT2_IND_BLOCK];
	  b64 = b;
	  ret = ext2_alloc_block (sb, b64, blockbuf, &b64, &alloc_ctx);
	  b = b64;
	  if (ret != 0)
	    goto end;
	  inode->i_block[EXT2_DIND_BLOCK] = b;
	  blocks_alloc++;
	}
      ret =
	block_dind_bmap (sb, flags, b, blockbuf, &blocks_alloc, block, &b32);
      if (ret == 0)
	*physblock = b32;
      goto end;
    }

  /* Triply indirect block */
  block -= addr_per_block * addr_per_block;
  b = inode->i_block[EXT2_TIND_BLOCK];
  if (b == 0)
    {
      if (!(flags & BMAP_ALLOC))
	{
	  if (flags & BMAP_SET)
	    ret = -EINVAL;
	  goto end;
	}
      b = inode->i_block[EXT2_DIND_BLOCK];
      b64 = b;
      ret = ext2_alloc_block (sb, b64, blockbuf, &b64, &alloc_ctx);
      b = b64;
      if (ret != 0)
	goto end;
      inode->i_block[EXT2_TIND_BLOCK] = b;
      blocks_alloc++;
    }
  ret = block_tind_bmap (sb, flags, b, blockbuf, &blocks_alloc, block, &b32);
  if (ret == 0)
    *physblock = b32;

 end:
  if (*physblock != 0 && ret == 0 && (flags & BMAP_ZERO))
    ret = ext2_zero_blocks (sb, *physblock, 1, NULL, NULL);
  if (buffer != NULL)
    kfree (buffer);
  if (handle != NULL)
    ext3_extent_free (handle);
  if (ret == 0 && (blocks_alloc || inode_dirty))
    {
      ext2_iblk_add_blocks (sb, inode, blocks_alloc);
      ret = ext2_update_inode (sb, ino, inode);
    }
  return ret;
}
