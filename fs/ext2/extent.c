/*************************************************************************
 * extent.c -- This file is part of OS/0.                                *
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

/* Copyright (C) 2007 Theodore Ts'o.
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
ext3_extent_update_path (Ext3ExtentHandle *handle)
{
  Ext3ExtentIndex *index;
  Ext3ExtentHeader *eh;
  block_t block;
  int ret;
  if (handle->eh_level == 0)
    ret = ext2_update_inode (handle->eh_sb, handle->eh_ino, handle->eh_inode);
  else
    {
      index = handle->eh_path[handle->eh_level - 1].p_curr;
      block = index->ei_leaf + ((uint64_t) index->ei_leaf_hi << 32);
      eh = (Ext3ExtentHeader *) handle->eh_path[handle->eh_level].p_buffer;
      ret =
	ext3_extent_block_checksum_update (handle->eh_sb, handle->eh_ino, eh);
      if (ret != 0)
	return ret;
      ret = ext2_write_blocks (handle->eh_path[handle->eh_level].p_buffer,
			       handle->eh_sb, block, 1);
    }
  return ret;
}

static int
ext3_extent_splitting_eof (Ext3ExtentHandle *handle,
			   Ext3GenericExtentPath *path)
{
  Ext3GenericExtentPath *p = path;
  if (handle->eh_level == 0)
    return 0;
  do
    {
      if (p->p_left != 0)
	return 0;
      p--;
    }
  while (p >= handle->eh_path);
  return 1;
}

static int
ext3_extent_dealloc_range (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
			   block_t lfree_start, block_t free_start,
			   uint32_t free_count, int *freed)
{
  Ext2Filesystem *fs = sb->sb_private;
  block_t block;
  uint32_t cluster_freed;
  int freed_now = 0;
  int ret;

  if (EXT2_CLUSTER_RATIO (fs) == 1)
    {
      *freed += free_count;
      while (free_count-- > 0)
	ext2_block_alloc_stats (sb, free_start++, -1);
      return 0;
    }

  if (free_start & EXT2_CLUSTER_MASK (fs))
    {
      ret = ext2_map_cluster_block (sb, ino, inode, lfree_start, &block);
      if (ret != 0)
	goto end;
      if (block == 0)
	{
	  ext2_block_alloc_stats (sb, free_start, -1);
	  freed_now++;
	}
    }

  while (free_count > 0 && free_count >= EXT2_CLUSTER_RATIO (fs))
    {
      ext2_block_alloc_stats (sb, free_start, -1);
      freed_now++;
      cluster_freed = EXT2_CLUSTER_RATIO (fs);
      free_count -= cluster_freed;
      free_start += cluster_freed;
      lfree_start += cluster_freed;
    }

  if (free_count > 0)
    {
      ret = ext2_map_cluster_block (sb, ino, inode, lfree_start, &block);
      if (ret != 0)
	goto end;
      if (block == 0)
	{
	  ext2_block_alloc_stats (sb, free_start, -1);
	  freed_now++;
	}
    }

 end:
  *freed += freed_now;
  return ret;
}

int
ext3_extent_open (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
		  Ext3ExtentHandle **handle)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext3ExtentHandle *h;
  Ext3ExtentHeader *eh;
  int ret;
  int i;

  if (inode == NULL)
    {
      if (ino == 0 || ino > fs->f_super.s_inodes_count)
	return -EINVAL;
    }

  h = kzalloc (sizeof (Ext3ExtentHandle));
  if (unlikely (h == NULL))
    return -ENOMEM;
  h->eh_ino = ino;
  h->eh_sb = sb;

  if (inode != NULL)
    h->eh_inode = inode;
  else
    {
      h->eh_inode = &h->eh_inode_buf;
      ret = ext2_read_inode (sb, ino, h->eh_inode);
      if (ret != 0)
	goto err;
    }

  eh = (Ext3ExtentHeader *) h->eh_inode->i_block;
  for (i = 0; i < EXT2_N_BLOCKS; i++)
    {
      if (h->eh_inode->i_block[i] != 0)
	break;
    }
  if (i >= EXT2_N_BLOCKS)
    {
      eh->eh_magic = EXT3_EXTENT_MAGIC;
      eh->eh_depth = 0;
      eh->eh_entries = 0;
      i = (sizeof (h->eh_inode->i_block) - sizeof (Ext3ExtentHeader)) /
	sizeof (Ext3Extent);
      eh->eh_max = i;
      h->eh_inode->i_flags |= EXT4_EXTENTS_FL;
    }
  if (!(h->eh_inode->i_flags & EXT4_EXTENTS_FL))
    {
      ret = -EINVAL;
      goto err;
    }

  ret = ext3_extent_header_valid (eh, sizeof (h->eh_inode->i_block));
  if (ret != 0)
    goto err;

  h->eh_max_depth = eh->eh_depth;
  h->eh_type = eh->eh_magic;
  h->eh_max_paths = h->eh_max_depth + 1;

  h->eh_path = kmalloc (h->eh_max_paths * sizeof (Ext3GenericExtentPath));
  if (unlikely (h->eh_path == NULL))
    {
      ret = -ENOMEM;
      goto err;
    }
  h->eh_path[0].p_buffer = (char *) h->eh_inode->i_block;
  h->eh_path[0].p_left = eh->eh_entries;
  h->eh_path[0].p_entries = eh->eh_entries;
  h->eh_path[0].p_max_entries = eh->eh_max;
  h->eh_path[0].p_curr = 0;
  h->eh_path[0].p_end_block = (EXT2_I_SIZE (*h->eh_inode) + sb->sb_blksize - 1)
    >> EXT2_BLOCK_SIZE_BITS (fs->f_super);
  h->eh_path[0].p_visit_num = 1;
  h->eh_level = 0;
  *handle = h;
  return 0;

 err:
  ext3_extent_free (h);
  return ret;
}

int
ext3_extent_header_valid (Ext3ExtentHeader *eh, size_t size)
{
  int hmax;
  size_t entry_size;
  if (eh->eh_magic != EXT3_EXTENT_MAGIC)
    return -EINVAL;
  if (eh->eh_entries > eh->eh_max)
    return -EINVAL;

  if (eh->eh_depth == 0)
    entry_size = sizeof (Ext3Extent);
  else
    entry_size = sizeof (Ext3ExtentIndex);
  hmax = (size - sizeof (Ext3ExtentHeader)) / entry_size;
  if (eh->eh_max > hmax || eh->eh_max < hmax - 2)
    return -EINVAL;
  return 0;
}

int
ext3_extent_goto (Ext3ExtentHandle *handle, int leaflvl, block_t block)
{
  return -ENOTSUP;
}

int
ext3_extent_get (Ext3ExtentHandle *handle, int flags, Ext3GenericExtent *extent)
{
  Ext3GenericExtentPath *path;
  Ext3GenericExtentPath *newpath;
  Ext3ExtentHeader *eh;
  Ext3ExtentIndex *index = NULL;
  Ext3Extent *ex;
  block_t block;
  block_t endblock;
  int orig_op;
  int op;
  int fail_csum = 0;
  int ret;

  if (handle->eh_path == NULL)
    return -ENOENT;
  orig_op = op = flags & EXT2_EXTENT_MOVE_MASK;

 retry:
  path = handle->eh_path + handle->eh_level;
  if (orig_op == EXT2_EXTENT_NEXT || orig_op == EXT2_EXTENT_NEXT_LEAF)
    {
      if (handle->eh_level < handle->eh_max_depth)
	{
	  if (path->p_visit_num == 0)
	    {
	      path->p_visit_num++;
	      op = EXT2_EXTENT_DOWN;
	    }
	  else if (path->p_left > 0)
	    op = EXT2_EXTENT_NEXT_SIB;
	  else if (handle->eh_level > 0)
	    op = EXT2_EXTENT_UP;
	  else
	    return -ESRCH;
	}
      else
	{
	  if (path->p_left > 0)
	    op = EXT2_EXTENT_NEXT_SIB;
	  else if (handle->eh_level > 0)
	    op = EXT2_EXTENT_UP;
	  else
	    return -ESRCH;
	}
    }

  if (orig_op == EXT2_EXTENT_PREV || orig_op == EXT2_EXTENT_PREV_LEAF)
    {
      if (handle->eh_level < handle->eh_max_depth)
	{
	  if (path->p_visit_num > 0)
	    op = EXT2_EXTENT_DOWN_LAST;
	  else if (path->p_left < path->p_entries - 1)
	    op = EXT2_EXTENT_PREV_SIB;
	  else if (handle->eh_level > 0)
	    op = EXT2_EXTENT_UP;
	  else
	    return -ESRCH;
	}
      else
	{
	  if (path->p_left < path->p_entries - 1)
	    op = EXT2_EXTENT_PREV_SIB;
	  else if (handle->eh_level > 0)
	    op = EXT2_EXTENT_UP;
	  else
	    return -ESRCH;
	}
    }

  if (orig_op == EXT2_EXTENT_LAST_LEAF)
    {
      if (handle->eh_level < handle->eh_max_depth && path->p_left == 0)
	op = EXT2_EXTENT_DOWN;
      else
	op = EXT2_EXTENT_LAST_SIB;
    }

  switch (op)
    {
    case EXT2_EXTENT_CURRENT:
      index = path->p_curr;
      break;
    case EXT2_EXTENT_ROOT:
      handle->eh_level = 0;
      path = handle->eh_path + handle->eh_level;
    case EXT2_EXTENT_FIRST_SIB:
      path->p_left = path->p_entries;
      path->p_curr = NULL;
    case EXT2_EXTENT_NEXT_SIB:
      if (path->p_left <= 0)
	return -ESRCH;
      if (path->p_curr != NULL)
	{
	  index = path->p_curr;
	  index++;
	}
      else
	{
	  eh = (Ext3ExtentHeader *) path->p_buffer;
	  index = EXT2_FIRST_INDEX (eh);
	}
      path->p_left--;
      path->p_curr = index;
      path->p_visit_num = 0;
      break;
    case EXT2_EXTENT_PREV_SIB:
      if (path->p_curr == NULL || path->p_left + 1 >= path->p_entries)
	return -ESRCH;
      index = path->p_curr;
      index--;
      path->p_curr = index;
      path->p_left++;
      if (handle->eh_level < handle->eh_max_depth)
	path->p_visit_num = 1;
      break;
    case EXT2_EXTENT_LAST_SIB:
      eh = (Ext3ExtentHeader *) path->p_buffer;
      path->p_curr = EXT2_LAST_EXTENT (eh);
      index = path->p_curr;
      path->p_left = 0;
      path->p_visit_num = 0;
      break;
    case EXT2_EXTENT_UP:
      if (handle->eh_level <= 0)
	return -EINVAL;
      handle->eh_level--;
      path--;
      index = path->p_curr;
      if (orig_op == EXT2_EXTENT_PREV || orig_op == EXT2_EXTENT_PREV_LEAF)
	path->p_visit_num = 0;
      break;
    case EXT2_EXTENT_DOWN:
    case EXT2_EXTENT_DOWN_LAST:
      if (path->p_curr == NULL || handle->eh_level >= handle->eh_max_depth)
	return -EINVAL;
      index = path->p_curr;
      newpath = path + 1;
      if (newpath->p_buffer == NULL)
	{
	  newpath->p_buffer = kmalloc (handle->eh_sb->sb_blksize);
	  if (unlikely (newpath->p_buffer == NULL))
	    return -ENOMEM;
	}
      block = index->ei_leaf + ((uint64_t) index->ei_leaf_hi << 32);
      ret = ext2_read_blocks (newpath->p_buffer, handle->eh_sb, block, 1);
      if (ret != 0)
	return ret;
      handle->eh_level++;
      eh = (Ext3ExtentHeader *) newpath->p_buffer;
      ret = ext3_extent_header_valid (eh, handle->eh_sb->sb_blksize);
      if (ret != 0)
	{
	  handle->eh_level--;
	  return ret;
	}

      if (!ext3_extent_block_checksum_valid (handle->eh_sb, handle->eh_ino, eh))
	fail_csum = 1;
      newpath->p_entries = eh->eh_entries;
      newpath->p_left = eh->eh_entries;
      newpath->p_max_entries = eh->eh_max;
      if (path->p_left > 0)
	{
	  index++;
	  newpath->p_end_block = index->ei_block;
	}
      else
	newpath->p_end_block = path->p_end_block;
      path = newpath;

      if (op == EXT2_EXTENT_DOWN)
	{
	  index = EXT2_FIRST_INDEX (eh);
	  path->p_curr = index;
	  path->p_left = path->p_entries - 1;
	  path->p_visit_num = 0;
	}
      else
	{
	  index = EXT2_LAST_INDEX (eh);
	  path->p_curr = index;
	  path->p_left = 0;
	  if (handle->eh_level < handle->eh_max_depth)
	    path->p_visit_num = 1;
	}
      break;
    default:
      return -EINVAL;
    }
  if (index == NULL)
    return -ENOENT;
  extent->e_flags = 0;

  if (handle->eh_level == handle->eh_max_depth)
    {
      ex = (Ext3Extent *) index;
      extent->e_pblk = ex->ee_start + ((uint64_t) ex->ee_start_hi << 32);
      extent->e_lblk = ex->ee_block;
      extent->e_len = ex->ee_len;
      extent->e_flags |= EXT2_EXTENT_FLAGS_LEAF;
      if (extent->e_len > EXT2_INIT_MAX_LEN)
	{
	  extent->e_len -= EXT2_INIT_MAX_LEN;
	  extent->e_flags |= EXT2_EXTENT_FLAGS_UNINIT;
	}
    }
  else
    {
      extent->e_pblk = index->ei_leaf + ((uint64_t) index->ei_leaf_hi << 32);
      extent->e_lblk = index->ei_block;
      if (path->p_left > 0)
	{
	  index++;
	  endblock = index->ei_block;
	}
      else
	endblock = path->p_end_block;
      extent->e_len = endblock - extent->e_lblk;
    }
  if (path->p_visit_num != 0)
    extent->e_flags |= EXT2_EXTENT_FLAGS_SECOND_VISIT;

  if ((orig_op == EXT2_EXTENT_NEXT_LEAF || orig_op == EXT2_EXTENT_PREV_LEAF)
      && handle->eh_level != handle->eh_max_depth)
    goto retry;
  if (orig_op == EXT2_EXTENT_LAST_LEAF
      && (handle->eh_level != handle->eh_max_depth || path->p_left != 0))
    goto retry;
  return fail_csum ? -EINVAL : 0;
}

int
ext3_extent_get_info (Ext3ExtentHandle *handle, Ext3ExtentInfo *info)
{
  Ext3GenericExtentPath *path;
  memset (info, 0, sizeof (Ext3ExtentInfo));
  path = handle->eh_path + handle->eh_level;
  if (path != NULL)
    {
      if (path->p_curr != NULL)
	info->i_curr_entry = ((char *) path->p_curr - path->p_buffer) /
	  sizeof (Ext3ExtentIndex);
      else
	info->i_curr_entry = 0;
      info->i_num_entries = path->p_entries;
      info->i_max_entries = path->p_max_entries;
      info->i_bytes_avail =
	(path->p_max_entries - path->p_entries) * sizeof (Ext3Extent);
    }
  info->i_curr_level = handle->eh_level;
  info->i_max_depth = handle->eh_max_depth;
  info->i_max_lblk = EXT2_MAX_EXTENT_LBLK;
  info->i_max_pblk = EXT2_MAX_EXTENT_PBLK;
  info->i_max_len = EXT2_INIT_MAX_LEN;
  info->i_max_uninit_len = EXT2_UNINIT_MAX_LEN;
  return 0;
}

int
ext3_extent_node_split (Ext3ExtentHandle *handle, int canexpand)
{
  Ext2Filesystem *fs = handle->eh_sb->sb_private;
  block_t new_node_block;
  block_t new_node_start;
  block_t orig_block;
  block_t goal_block = 0;
  char *blockbuf = NULL;
  Ext3GenericExtent extent;
  Ext3GenericExtentPath *path;
  Ext3GenericExtentPath *new_path = NULL;
  Ext3ExtentHeader *eh;
  Ext3ExtentHeader *new_eh;
  Ext3ExtentInfo info;
  int orig_height;
  int new_root = 0;
  int to_copy;
  int no_balance;
  int ret;
  if (handle->eh_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  if (handle->eh_path == NULL)
    return -ENOENT;

  ret = ext3_extent_get (handle, EXT2_EXTENT_CURRENT, &extent);
  if (ret != 0)
    goto end;
  ret = ext3_extent_get_info (handle, &info);
  if (ret != 0)
    goto end;

  orig_height = info.i_max_depth - info.i_curr_level;
  orig_block = extent.e_lblk;

  path = handle->eh_path + handle->eh_level;
  eh = (Ext3ExtentHeader *) path->p_buffer;
  if (handle->eh_level == handle->eh_max_depth)
    {
      Ext3Extent *ex = EXT2_FIRST_EXTENT (eh);
      goal_block = ex->ee_start + ((block_t) ex->ee_start_hi << 32);
    }
  else
    {
      Ext3ExtentIndex *index = EXT2_FIRST_INDEX (eh);
      goal_block = index->ei_leaf + ((block_t) index->ei_leaf_hi << 32);
    }
  goal_block -= EXT2_CLUSTER_RATIO (fs);
  goal_block &= ~EXT2_CLUSTER_MASK (fs);

  if (handle->eh_level && handle->eh_path[handle->eh_level - 1].p_entries >=
      handle->eh_path[handle->eh_level - 1].p_max_entries)
    {
      ret = ext3_extent_get (handle, EXT2_EXTENT_UP, &extent);
      if (ret != 0)
	goto end;
      ret = ext3_extent_node_split (handle, canexpand);
      if (ret != 0)
	goto end;
      ret = ext3_extent_goto (handle, orig_height, orig_block);
      if (ret != 0)
	goto end;
    }

  path = handle->eh_path + handle->eh_level;
  if (path->p_curr == NULL)
    return -ENOENT;
  no_balance = canexpand ? ext3_extent_splitting_eof (handle, path) : 0;
  eh = (Ext3ExtentHeader *) path->p_buffer;

  if (handle->eh_level == 0)
    {
      new_root = 1;
      to_copy = eh->eh_entries;
      new_path =
	kzalloc ((handle->eh_max_paths + 1) * sizeof (Ext3GenericExtent));
      if (unlikely (new_path == NULL))
	{
	  ret = -ENOMEM;
	  goto end;
	}
    }
  else
    {
      if (no_balance)
	to_copy = 1;
      else
	to_copy = eh->eh_entries / 2;
    }

  if (to_copy == 0 && !no_balance)
    {
      ret = -ENOSPC;
      goto end;
    }

  blockbuf = kmalloc (handle->eh_sb->sb_blksize);
  if (unlikely (blockbuf == NULL))
    {
      ret = -ENOMEM;
      goto end;
    }
  if (goal_block == 0)
    goal_block = ext2_find_inode_goal (handle->eh_sb, handle->eh_ino,
				       handle->eh_inode, 0);
  ret = ext2_alloc_block (handle->eh_sb, goal_block, blockbuf, &new_node_block,
			  NULL);
  if (ret != 0)
    goto end;

  new_eh = (Ext3ExtentHeader *) blockbuf;
  memcpy (new_eh, eh, sizeof (Ext3ExtentHeader));
  new_eh->eh_entries = to_copy;
  new_eh->eh_max = (handle->eh_sb->sb_blksize - sizeof (Ext3ExtentHeader)) /
    sizeof (Ext3Extent);
  memcpy (EXT2_FIRST_INDEX (new_eh), EXT2_FIRST_INDEX (eh) +
	  eh->eh_entries - to_copy, sizeof (Ext3ExtentIndex) * to_copy);
  new_node_start = EXT2_FIRST_INDEX (new_eh)->ei_block;
  ret =
    ext3_extent_block_checksum_update (handle->eh_sb, handle->eh_ino, new_eh);
  if (ret != 0)
    goto end;
  ret = ext2_write_blocks (blockbuf, handle->eh_sb, new_node_block, 1);
  if (ret != 0)
    goto end;

  if (handle->eh_level == 0)
    {
      memcpy (new_path, path, sizeof (Ext3GenericExtentPath) *
	      handle->eh_max_paths);
      handle->eh_path = new_path;
      new_path = path;
      path = handle->eh_path;
      path->p_entries = 1;
      path->p_left = path->p_max_entries - 1;
      handle->eh_max_depth++;
      handle->eh_max_paths++;
      eh->eh_depth = handle->eh_max_depth;
    }
  else
    {
      path->p_entries -= to_copy;
      path->p_left -= to_copy;
    }

  eh->eh_entries = path->p_entries;
  ret = ext3_extent_update_path (handle);
  if (ret != 0)
    goto end;

  if (new_root)
    {
      ret = ext3_extent_get (handle, EXT2_EXTENT_FIRST_SIB, &extent);
      if (ret != 0)
	goto end;
      extent.e_lblk = new_node_start;
      extent.e_pblk = new_node_block;
      extent.e_len = handle->eh_path[0].p_end_block - extent.e_lblk;
      ret = ext3_extent_replace (handle, 0, &extent);
      if (ret != 0)
	goto end;
    }
  else
    {
      uint32_t new_node_len;
      ret = ext3_extent_get (handle, EXT2_EXTENT_UP, &extent);
      new_node_len = new_node_start - extent.e_lblk;
      extent.e_len -= new_node_len;
      ret = ext3_extent_replace (handle, 0, &extent);
      if (ret != 0)
	goto end;
      extent.e_lblk = new_node_start;
      extent.e_pblk = new_node_block;
      extent.e_len = new_node_len;
      ret = ext3_extent_insert (handle, EXT2_EXTENT_INSERT_AFTER, &extent);
      if (ret != 0)
	goto end;
    }

  ret = ext3_extent_goto (handle, orig_height, orig_block);
  if (ret != 0)
    goto end;

  ext2_iblk_add_blocks (handle->eh_sb, handle->eh_inode, 1);
  ret = ext2_update_inode (handle->eh_sb, handle->eh_ino, handle->eh_inode);

 end:
  if (new_path != NULL)
    kfree (new_path);
  kfree (blockbuf);
  return ret;
}

int
ext3_extent_fix_parents (Ext3ExtentHandle *handle)
{
  Ext3GenericExtentPath *path;
  Ext3GenericExtent extent;
  Ext3ExtentInfo info;
  block_t start;
  int orig_height;
  int ret = 0;
  if (handle->eh_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  if (handle->eh_path == NULL)
    return -ENOENT;

  path = handle->eh_path + handle->eh_level;
  if (path->p_curr == NULL)
    return -ENOENT;

  ret = ext3_extent_get (handle, EXT2_EXTENT_CURRENT, &extent);
  if (ret != 0)
    return ret;
  start = extent.e_lblk;

  ret = ext3_extent_get_info (handle, &info);
  if (ret != 0)
    return ret;
  orig_height = info.i_max_depth - info.i_curr_level;

  while (handle->eh_level > 0 && path->p_left == path->p_entries - 1)
    {
      ret = ext3_extent_get (handle, EXT2_EXTENT_UP, &extent);
      if (ret != 0)
	return ret;
      if (extent.e_lblk == start)
	break;
      path = handle->eh_path + handle->eh_level;
      extent.e_len += extent.e_lblk - start;
      extent.e_lblk = start;
      ret = ext3_extent_replace (handle, 0, &extent);
      if (ret != 0)
	return ret;
      ext3_extent_update_path (handle);
    }
  return ext3_extent_goto (handle, orig_height, start);
}

int
ext3_extent_insert (Ext3ExtentHandle *handle, int flags,
		    Ext3GenericExtent *extent)
{
  Ext3GenericExtentPath *path;
  Ext3ExtentIndex *index;
  Ext3ExtentHeader *eh;
  int ret;
  if (handle->eh_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  if (handle->eh_path == NULL)
    return -ENOENT;

  path = handle->eh_path + handle->eh_level;
  if (path->p_entries >= path->p_max_entries)
    {
      if (flags & EXT2_EXTENT_INSERT_NOSPLIT)
	return -ENOSPC;
      else
	{
	  ret = ext3_extent_node_split (handle, 1);
	  if (ret != 0)
	    return ret;
	  path = handle->eh_path + handle->eh_level;
	}
    }

  eh = (Ext3ExtentHeader *) path->p_buffer;
  if (path->p_curr != NULL)
    {
      index = path->p_curr;
      if (flags & EXT2_EXTENT_INSERT_AFTER)
	{
	  index++;
	  path->p_left--;
	}
    }
  else
    {
      index = EXT2_FIRST_INDEX (eh);
      path->p_left = -1;
    }
  path->p_curr = index;

  if (path->p_left >= 0)
    memmove (index + 1, index, (path->p_left + 1) * sizeof (Ext3ExtentIndex));
  path->p_left++;
  path->p_entries++;

  eh = (Ext3ExtentHeader *) path->p_buffer;
  eh->eh_entries = path->p_entries;
  ret = ext3_extent_replace (handle, 0, extent);
  if (ret != 0)
    goto err;
  ret = ext3_extent_update_path (handle);
  if (ret != 0)
    goto err;
  return 0;

 err:
  ext3_extent_delete (handle, 0);
  return ret;
}

int
ext3_extent_replace (Ext3ExtentHandle *handle, int flags,
		     Ext3GenericExtent *extent)
{
  Ext3GenericExtentPath *path;
  Ext3ExtentIndex *index;
  Ext3Extent *ex;
  if (handle->eh_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  if (handle->eh_path == NULL)
    return -ENOENT;

  path = handle->eh_path + handle->eh_level;
  if (path->p_curr == NULL)
    return -ENOENT;

  if (handle->eh_level == handle->eh_max_depth)
    {
      ex = path->p_curr;
      ex->ee_block = extent->e_lblk;
      ex->ee_start = extent->e_pblk & 0xffffffff;
      ex->ee_start_hi = extent->e_pblk >> 32;
      if (extent->e_flags & EXT2_EXTENT_FLAGS_UNINIT)
	{
	  if (extent->e_len > EXT2_UNINIT_MAX_LEN)
	    return -EINVAL;
	  ex->ee_len = extent->e_len + EXT2_INIT_MAX_LEN;
	}
      else
	{
	  if (extent->e_len > EXT2_INIT_MAX_LEN)
	    return -EINVAL;
	  ex->ee_len = extent->e_len;
	}
    }
  else
    {
      index = path->p_curr;
      index->ei_leaf = extent->e_pblk & 0xffffffff;
      index->ei_leaf_hi = extent->e_pblk >> 32;
      index->ei_block = extent->e_lblk;
      index->ei_unused = 0;
    }
  ext3_extent_update_path (handle);
  return 0;
}

int
ext3_extent_delete (Ext3ExtentHandle *handle, int flags)
{
  Ext2Filesystem *fs = handle->eh_sb->sb_private;
  Ext3ExtentHeader *eh;
  Ext3GenericExtentPath *path;
  char *ptr;
  int ret = 0;
  if (handle->eh_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  if (handle->eh_path == NULL)
    return -ENOENT;

  path = handle->eh_path + handle->eh_level;
  if (path->p_curr == NULL)
    return -ENOENT;
  ptr = path->p_curr;

  if (path->p_left != 0)
    {
      memmove (ptr, ptr + sizeof (Ext3ExtentIndex),
	       path->p_left * sizeof (Ext3ExtentIndex));
      path->p_left--;
    }
  else
    {
      Ext3ExtentIndex *index = path->p_curr;
      index--;
      path->p_curr = index;
    }
  if (--path->p_entries == 0)
    path->p_curr = 0;

  if (path->p_entries == 0 && handle->eh_level != 0)
    {
      if (!(flags & EXT2_EXTENT_DELETE_KEEP_EMPTY))
	{
	  Ext3GenericExtent extent;
	  ret = ext3_extent_get (handle, EXT2_EXTENT_UP, &extent);
	  if (ret != 0)
	    return ret;
	  ret = ext3_extent_delete (handle, flags);
	  handle->eh_inode->i_blocks -=
	    (handle->eh_sb->sb_blksize * EXT2_CLUSTER_RATIO (fs)) / 512;
	  ret =
	    ext2_update_inode (handle->eh_sb, handle->eh_ino, handle->eh_inode);
	  ext2_block_alloc_stats (handle->eh_sb, extent.e_pblk, -1);
	}
    }
  else
    {
      eh = (Ext3ExtentHeader *) path->p_buffer;
      eh->eh_entries = path->p_entries;
      if (path->p_entries == 0 && handle->eh_level == 0)
	{
	  eh->eh_depth = 0;
	  handle->eh_max_depth = 0;
	}
      ret = ext3_extent_update_path (handle);
    }
  return ret;
}

int
ext3_extent_dealloc_blocks (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
			    block_t start, block_t end)
{
  Ext3ExtentHandle *handle = NULL;
  Ext3GenericExtent extent;
  block_t free_start;
  block_t lfree_start;
  block_t next;
  uint32_t free_count;
  uint32_t newlen;
  int freed = 0;
  int op;
  int ret = ext3_extent_open (sb, ino, inode, &handle);
  if (ret != 0)
    return ret;

  ext3_extent_goto (handle, 0, start);
  ret = ext3_extent_get (handle, EXT2_EXTENT_CURRENT, &extent);
  if (ret != 0)
    {
      if (ret == -ENOENT)
	ret = 0;
      goto end;
    }

  while (1)
    {
      op = EXT2_EXTENT_NEXT_LEAF;
      next = extent.e_lblk + extent.e_len;
      if (start >= extent.e_lblk)
	{
	  if (end < extent.e_lblk)
	    break;
	  free_start = extent.e_pblk;
	  lfree_start = extent.e_lblk;
	  if (next > end)
	    free_count = end - extent.e_lblk + 1;
	  else
	    free_count = extent.e_len;
	  extent.e_len -= free_count;
	  extent.e_lblk += free_count;
	  extent.e_pblk += free_count;
	}
      else if (end >= next - 1)
	{
	  if (start >= next)
	    goto next_extent;
	  newlen = start - extent.e_lblk;
	  free_start = extent.e_pblk + newlen;
	  lfree_start = extent.e_lblk + newlen;
	  free_count = extent.e_len - newlen;
	  extent.e_len = newlen;
	}
      else
	{
	  Ext3GenericExtent new_ex;
	  new_ex.e_pblk = extent.e_pblk + end + 1 - extent.e_lblk;
	  new_ex.e_lblk = end + 1;
	  new_ex.e_len = next - end - 1;
	  new_ex.e_flags = extent.e_flags;
	  extent.e_len = start - extent.e_lblk;
	  free_start = extent.e_pblk + extent.e_len;
	  lfree_start = extent.e_lblk + extent.e_len;
	  free_count = end - start + 1;

	  ret = ext3_extent_insert (handle, EXT2_EXTENT_INSERT_AFTER, &new_ex);
	  if (ret != 0)
	    goto end;
	  ret = ext3_extent_fix_parents (handle);
	  if (ret != 0)
	    goto end;
	  ret = ext3_extent_goto (handle, 0, extent.e_lblk);
	  if (ret != 0)
	    goto end;
	}

      if (extent.e_len != 0)
	{
	  ret = ext3_extent_replace (handle, 0, &extent);
	  if (ret != 0)
	    goto end;
	  ret = ext3_extent_fix_parents (handle);
	}
      else
	{
	  Ext3GenericExtent new_ex;
	  block_t old_block;
	  block_t next_block;
	  ret = ext3_extent_get (handle, EXT2_EXTENT_CURRENT, &new_ex);
	  if (ret != 0)
	    goto end;
	  old_block = new_ex.e_lblk;
	  ret = ext3_extent_get (handle, EXT2_EXTENT_NEXT_LEAF, &new_ex);
	  if (ret == -ESRCH)
	    next_block = old_block;
	  else if (ret != 0)
	    goto end;
	  else
	    next_block = new_ex.e_lblk;
	  ret = ext3_extent_goto (handle, 0, old_block);
	  if (ret != 0)
	    goto end;
	  ret = ext3_extent_delete (handle, 0);
	  if (ret != 0)
	    goto end;
	  ret = ext3_extent_fix_parents (handle);
	  if (ret != 0 && ret != -ENOENT)
	    goto end;
	  ret = 0;

	  ext3_extent_goto (handle, 0, next_block);
	  op = EXT2_EXTENT_CURRENT;
	}
      if (ret != 0)
	goto end;

      ret = ext3_extent_dealloc_range (sb, ino, inode, lfree_start, free_start,
				       free_count, &freed);
      if (ret != 0)
	goto end;

    next_extent:
      ret = ext3_extent_get (handle, op, &extent);
      if (ret == -ESRCH || ret == -ENOENT)
	break;
      if (ret != 0)
	goto end;
    }

  ret = ext2_iblk_sub_blocks (sb, inode, freed);
 end:
  ext3_extent_free (handle);
  return ret;
}

int
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

int
ext3_extent_set_bmap (Ext3ExtentHandle *handle, block_t logical,
		      block_t physical, int flags)
{
  int mapped = 1;
  int extent_uninit = 0;
  int prev_uninit = 0;
  int next_uninit = 0;
  int new_uninit = 0;
  int max_len = EXT2_INIT_MAX_LEN;
  int orig_height;
  int has_prev;
  int has_next;
  block_t orig_block;
  Ext3GenericExtentPath *path;
  Ext3GenericExtent extent;
  Ext3GenericExtent next_extent;
  Ext3GenericExtent prev_extent;
  Ext3GenericExtent new_extent;
  Ext3ExtentInfo info;
  int ec;
  int ret;
  if (handle->eh_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;

  if (handle->eh_path == NULL)
    return -EINVAL;
  path = handle->eh_path + handle->eh_level;

  if (flags & EXT2_EXTENT_SET_BMAP_UNINIT)
    {
      new_uninit = 1;
      max_len = EXT2_UNINIT_MAX_LEN;
    }

  if (physical != 0)
    {
      new_extent.e_len = 1;
      new_extent.e_pblk = physical;
      new_extent.e_lblk = logical;
      new_extent.e_flags = EXT2_EXTENT_FLAGS_LEAF;
      if (new_uninit)
	new_extent.e_flags |= EXT2_EXTENT_FLAGS_UNINIT;
    }
  if (handle->eh_max_depth == 0 && path->p_entries == 0)
    return ext3_extent_insert (handle, 0, &new_extent);

  ret = ext3_extent_get (handle, EXT2_EXTENT_CURRENT, &extent);
  if (ret != 0)
    {
      if (ret != -ENOENT)
	return ret;
      memset (&extent, 0, sizeof (Ext3GenericExtent));
    }
  ret = ext3_extent_get_info (handle, &info);
  if (ret != 0)
    return ret;
  orig_height = info.i_max_depth - info.i_curr_level;
  orig_block = extent.e_lblk;

  ret = ext3_extent_goto (handle, 0, logical);
  if (ret != 0)
    {
      if (ret == -ENOENT)
	{
	  ret = 0;
	  mapped = 0;
	  if (physical == 0)
	    goto end;
	}
      else
	goto end;
    }
  ret = ext3_extent_get (handle, EXT2_EXTENT_CURRENT, &extent);
  if (ret != 0)
    goto end;
  if (extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT)
    extent_uninit = 1;
  ret = ext3_extent_get (handle, EXT2_EXTENT_NEXT_LEAF, &next_extent);
  if (ret != 0)
    {
      has_next = 0;
      if (ret != -ESRCH)
	goto end;
    }
  else
    {
      has_next = 1;
      if (next_extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT)
	next_uninit = 1;
    }

  ret = ext3_extent_goto (handle, 0, logical);
  if (ret != 0 && ret != -ENOENT)
    goto end;
  ret = ext3_extent_get (handle, EXT2_EXTENT_PREV_LEAF, &prev_extent);
  if (ret != 0)
    {
      has_prev = 0;
      if (ret != -ESRCH)
	goto end;
    }
  else
    {
      has_prev = 1;
      if (prev_extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT)
	prev_uninit = 1;
    }
  ret = ext3_extent_goto (handle, 0, logical);
  if (ret != 0 && ret != -ENOENT)
    goto end;

  if (mapped && new_uninit == extent_uninit
      && extent.e_pblk + logical - extent.e_lblk == physical)
    goto end;
  if (!mapped)
    {
      if (logical == extent.e_lblk + extent.e_len
	  && physical == extent.e_pblk + extent.e_len
	  && new_uninit == extent_uninit && (int) extent.e_len < max_len - 1)
	{
	  extent.e_len++;
	  ret = ext3_extent_replace (handle, 0, &extent);
	}
      else if (logical == extent.e_lblk - 1 && physical == extent.e_pblk - 1
	       && new_uninit == extent_uninit
	       && (int) extent.e_len < max_len - 1)
	{
	  extent.e_len++;
	  extent.e_lblk--;
	  extent.e_pblk--;
	}
      else if (has_next && logical == next_extent.e_lblk - 1
	       && physical == next_extent.e_pblk - 1
	       && new_uninit == next_uninit
	       && (int) next_extent.e_len < max_len - 1)
	{
	  ret = ext3_extent_get (handle, EXT2_EXTENT_NEXT_LEAF, &next_extent);
	  if (ret != 0)
	    goto end;
	  next_extent.e_len++;
	  next_extent.e_lblk--;
	  next_extent.e_pblk--;
	  ret = ext3_extent_replace (handle, 0, &next_extent);
	}
      else if (logical < extent.e_lblk)
	ret = ext3_extent_insert (handle, 0, &new_extent);
      else
	ret = ext3_extent_insert (handle, EXT2_EXTENT_INSERT_AFTER,
				  &new_extent);
      if (ret != 0)
	goto end;
      ret = ext3_extent_fix_parents (handle);
      if (ret != 0)
	goto end;
    }
  else if (logical == extent.e_lblk && extent.e_len == 1)
    {
      if (physical != 0)
	ret = ext3_extent_replace (handle, 0, &new_extent);
      else
	{
	  ret = ext3_extent_delete (handle, 0);
	  if (ret != 0)
	    goto end;
	  ec = ext3_extent_fix_parents (handle);
	  if (ec != -ENOENT)
	    ret = ec;
	}
      if (ret != 0)
	goto end;
    }
  else if (logical == extent.e_lblk + extent.e_len - 1)
    {
      if (physical != 0)
	{
	  if (has_next && logical == next_extent.e_lblk - 1
	      && physical == next_extent.e_pblk - 1
	      && new_uninit == next_uninit
	      && (int) next_extent.e_len < max_len - 1)
	    {
	      ret = ext3_extent_get (handle, EXT2_EXTENT_NEXT_LEAF,
				     &next_extent);
	      if (ret != 0)
		goto end;
	      next_extent.e_len++;
	      next_extent.e_lblk--;
	      next_extent.e_pblk--;
	      ret = ext3_extent_replace (handle, 0, &next_extent);
	    }
	  else
	    ret = ext3_extent_insert (handle, EXT2_EXTENT_INSERT_AFTER,
				      &new_extent);
	  if (ret != 0)
	    goto end;
	  ret = ext3_extent_fix_parents (handle);
	  if (ret != 0)
	    goto end;

	  ret = ext3_extent_goto (handle, 0, logical);
	  if (ret != 0)
	    goto end;
	  ret = ext3_extent_get (handle, EXT2_EXTENT_CURRENT, &extent);
	  if (ret != 0)
	    goto end;
	}
      extent.e_len--;
      ret = ext3_extent_replace (handle, 0, &extent);
      if (ret != 0)
	goto end;
    }
  else if (logical == extent.e_lblk)
    {
      if (physical != 0)
	{
	  if (has_prev && logical == prev_extent.e_lblk + prev_extent.e_len
	      && physical == prev_extent.e_pblk + prev_extent.e_len
	      && new_uninit == prev_uninit
	      && (int) prev_extent.e_len < max_len - 1)
	    {
	      ret = ext3_extent_get (handle, EXT2_EXTENT_PREV_LEAF,
				     &prev_extent);
	      if (ret != 0)
		goto end;
	      prev_extent.e_len++;
	      ret = ext3_extent_replace (handle, 0, &prev_extent);
	    }
	  else
	    ret = ext3_extent_insert (handle, 0, &new_extent);
	  if (ret != 0)
	    goto end;
	  ret = ext3_extent_fix_parents (handle);
	  if (ret != 0)
	    goto end;
	  ret = ext3_extent_get (handle, EXT2_EXTENT_NEXT_LEAF, &extent);
	  if (ret != 0)
	    goto end;
	}
      extent.e_pblk++;
      extent.e_lblk--;
      extent.e_len--;
      ret = ext3_extent_replace (handle, 0, &extent);
      if (ret != 0)
	goto end;
      ret = ext3_extent_fix_parents (handle);
      if (ret != 0)
	goto end;
    }
  else
    {
      Ext3GenericExtent save_extent = extent;
      uint32_t save_len = extent.e_len;
      block_t save_block = extent.e_lblk;
      int ret2;
      extent.e_len = logical - extent.e_lblk;
      ret = ext3_extent_replace (handle, 0, &extent);
      if (ret != 0)
	goto end;
      if (physical != 0)
	{
	  ret = ext3_extent_insert (handle, EXT2_EXTENT_INSERT_AFTER,
				    &new_extent);
	  if (ret != 0)
	    {
	      ret2 = ext3_extent_goto (handle, 0, save_block);
	      if (ret2 == 0)
		ext3_extent_replace (handle, 0, &save_extent);
	      goto end;
	    }
	}
      extent.e_pblk += extent.e_len + 1;
      extent.e_lblk += extent.e_len + 1;
      extent.e_len = save_len - extent.e_len - 1;
      ret = ext3_extent_insert (handle, EXT2_EXTENT_INSERT_AFTER, &extent);
      if (ret != 0)
	{
	  if (physical != 0)
	    {
	      ret2 = ext3_extent_goto (handle, 0, new_extent.e_lblk);
	      if (ret2 == 0)
		ext3_extent_delete (handle, 0);
	    }
	  ret2 = ext3_extent_goto (handle, 0, save_block);
	  if (ret2 == 0)
	    ext3_extent_replace (handle, 0, &save_extent);
	  goto end;
	}
    }

 end:
  if (orig_height > handle->eh_max_depth)
    orig_height = handle->eh_max_depth;
  ext3_extent_goto (handle, orig_height, orig_block);
  return ret;
}

void
ext3_extent_free (Ext3ExtentHandle *handle)
{
  int i;
  if (handle == NULL)
    return;
  if (handle->eh_path != NULL)
    {
      for (i = 1; i < handle->eh_max_paths; i++)
	{
	  if (handle->eh_path[i].p_buffer != NULL)
	    kfree (handle->eh_path[i].p_buffer);
	}
      kfree (handle->eh_path);
    }
  kfree (handle);
}
