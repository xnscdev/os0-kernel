/*************************************************************************
 * ext2-inode.c -- This file is part of OS/0.                            *
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

#include <fs/ext2.h>
#include <libk/compile.h>
#include <sys/device.h>
#include <sys/sysmacros.h>
#include <vm/heap.h>
#include <errno.h>
#include <string.h>

int
ext2_read_blocks (void *buffer, VFSSuperblock *sb, uint32_t block,
		  size_t nblocks)
{
  return sb->sb_dev->sd_read (sb->sb_dev, buffer, nblocks * sb->sb_blksize,
			      block * sb->sb_blksize);
}

off_t
ext2_data_block (Ext2Inode *inode, VFSSuperblock *sb, off_t block)
{
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  off_t realblock;

  /* First 12 data blocks stored directly in inode */
  if (block < EXT2_STORED_INODES)
    return inode->ei_bptr0[block];

  /* Read indirect block pointer */
  bptr1 = kmalloc (sb->sb_blksize);
  if (block < sb->sb_blksize + EXT2_STORED_INODES)
    {
      if (ext2_read_blocks (bptr1, sb, inode->ei_bptr1, 1) != 0)
        goto err;
      realblock = bptr1[block - EXT2_STORED_INODES];
      kfree (bptr1);
      return realblock;
    }

  /* Read doubly indirect block pointer */
  bptr2 = kmalloc (sb->sb_blksize);
  if (block < sb->sb_blksize * sb->sb_blksize + sb->sb_blksize +
      EXT2_STORED_INODES)
    {
      realblock = block - sb->sb_blksize - EXT2_STORED_INODES;
      if (ext2_read_blocks (bptr1, sb, inode->ei_bptr2, 1) != 0)
	goto err;
      if (ext2_read_blocks (bptr2, sb,
			    bptr1[realblock / sb->sb_blksize], 1) != 0)
	goto err;
      realblock = bptr2[realblock % sb->sb_blksize];
      kfree (bptr1);
      kfree (bptr2);
      return realblock;
    }

  /* Read triply indirect block pointer */
  bptr3 = kmalloc (sb->sb_blksize);
  if (block < sb->sb_blksize * sb->sb_blksize * sb->sb_blksize +
      sb->sb_blksize * sb->sb_blksize + sb->sb_blksize + EXT2_STORED_INODES)
    {
      realblock = block - sb->sb_blksize * sb->sb_blksize - sb->sb_blksize -
	EXT2_STORED_INODES;
      if (ext2_read_blocks (bptr1, sb, inode->ei_bptr3, 1) != 0)
	goto err;
      if (ext2_read_blocks (bptr2, sb, bptr1[realblock / (sb->sb_blksize *
							  sb->sb_blksize)],
			    1) != 0)
	goto err;
      if (ext2_read_blocks (bptr3, sb, bptr2[realblock % (sb->sb_blksize *
							  sb->sb_blksize) /
					     sb->sb_blksize], 1) != 0)
	goto err;
      realblock = bptr3[realblock % sb->sb_blksize];
      kfree (bptr1);
      kfree (bptr2);
      kfree (bptr3);
      return realblock;
    }

  /* Block offset too large */
  return -EINVAL;

  /* Read error */
 err:
  if (bptr1 != NULL)
    kfree (bptr1);
  if (bptr2 != NULL)
    kfree (bptr2);
  if (bptr3 != NULL)
    kfree (bptr3);
  return -EIO;
}

Ext2Inode *
ext2_read_inode (VFSSuperblock *sb, ino_t inode)
{
  Ext2Superblock *esb = (Ext2Superblock *) sb->sb_private;
  Ext2BGD *bgdt = (Ext2BGD *) (sb->sb_private + sizeof (Ext2Superblock));
  uint32_t inosize = esb->esb_versmaj > 0 ? esb->esb_inosize : 128;
  uint32_t inotbl = bgdt[(inode - 1) / esb->esb_ipg].eb_inotbl;
  uint32_t index = (inode - 1) % esb->esb_ipg;
  uint32_t block = inotbl + index * inosize / sb->sb_blksize;
  uint32_t offset = index % (sb->sb_blksize / inosize);
  void *buffer;
  Ext2Inode *result;

  if (inode == 0)
    return NULL;

  buffer = kmalloc (sb->sb_blksize);
  if (unlikely (buffer == NULL))
    return NULL;

  if (ext2_read_blocks (buffer, sb, block, 1) != 0)
    {
      kfree (buffer);
      return NULL;
    }

  result = kmalloc (sizeof (Ext2Inode));
  if (unlikely (result == NULL))
    {
      kfree (buffer);
      return NULL;
    }
  memcpy (result, buffer + offset * inosize, sizeof (Ext2Inode));

  kfree (buffer);
  return result;
}

int
ext2_create (VFSInode *dir, VFSDirEntry *entry, mode_t mode)
{
  return -ENOSYS;
}

int
ext2_lookup (VFSDirEntry *entry, VFSSuperblock *sb, VFSPath *path)
{
  VFSDirEntry *dir = sb->sb_root;
  int ret;

  for (path = vfs_path_first (path); path != NULL; path = path->vp_next)
    {
      VFSDirEntry *entries;
      ret = ext2_readdir (&entries, sb, dir->d_inode);
      if (ret != 0)
	return ret;
      for (; entries != NULL; entries = entries->d_next)
	{
	  char *name = path->vp_long == NULL ? path->vp_short : path->vp_long;
	  vfs_destroy_dir_entry (entries->d_prev);
	  entries->d_prev = NULL;
	  if (strcmp (entries->d_name, name) == 0)
	    goto found;
	}
      vfs_destroy_dir_entry (entries);
      if (dir != sb->sb_root)
	vfs_destroy_dir_entry (dir);
      return -ENOENT;

    found:
      dir = entries;
      dir->d_next = NULL;
      /* Clean up all unchecked entries */
      for (entries = entries->d_next; entries != NULL;
	   entries = entries->d_next)
        vfs_destroy_dir_entry (entries);

      if (path->vp_next != NULL && !S_ISDIR (dir->d_inode->vi_mode))
	{
	  vfs_destroy_dir_entry (dir);
	  return -ENOTDIR;
	}
      if (strlen (dir->d_name) > EXT2_MAX_NAME_LEN)
	{
	  vfs_destroy_dir_entry (dir);
	  return -ENAMETOOLONG;
	}
    }

  memcpy (entry, dir, sizeof (VFSDirEntry));
  kfree (dir);
  return 0;
}

int
ext2_link (VFSDirEntry *old, VFSInode *dir, VFSDirEntry *new)
{
  return -ENOSYS;
}

int
ext2_unlink (VFSInode *dir, VFSDirEntry *entry)
{
  return -ENOSYS;
}

int
ext2_symlink (VFSInode *dir, VFSDirEntry *entry, const char *name)
{
  return -ENOSYS;
}

int
ext2_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  return -ENOSYS;
}

int
ext2_write (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  return -ENOSYS;
}

int
ext2_readdir (VFSDirEntry **entries, VFSSuperblock *sb, VFSInode *dir)
{
  Ext2Inode *ei;
  VFSDirEntry *result = NULL;
  VFSDirEntry *temp;
  void *buffer;
  int blocks;
  int i;

  if (dir->vi_ino == 0)
    return -EINVAL;
  ei = ext2_read_inode (sb, dir->vi_ino);
  if (ei == NULL)
    return -EINVAL;

  if ((ei->ei_mode & 0xf000) != EXT2_TYPE_DIR)
    {
      kfree (ei);
      return -ENOTDIR;
    }

  blocks = (dir->vi_size + sb->sb_blksize - 1) / sb->sb_blksize;
  buffer = kmalloc (sb->sb_blksize);

  for (i = 0; i < blocks; i++)
    {
      int bytes = 0;
      if (ext2_read_blocks (buffer, sb, ext2_data_block (ei, sb, i), 1) != 0)
	{
	  kfree (buffer);
	  kfree (ei);
	  return -EIO;
	}
      while (bytes < sb->sb_blksize)
	{
	  Ext2DirEntry *guess = (Ext2DirEntry *) (buffer + bytes);
	  if (guess->ed_inode == 0 || guess->ed_size == 0)
	    {
	      bytes += 4;
	      continue;
	    }

	  temp = kmalloc (sizeof (VFSDirEntry));
	  temp->d_flags = 0;
	  temp->d_inode = ext2_alloc_inode (sb);
	  temp->d_inode->vi_ino = guess->ed_inode;
	  ext2_fill_inode (temp->d_inode);
	  temp->d_mounted = 0;
	  temp->d_name = kmalloc (guess->ed_namelen + 1);
	  strncpy (temp->d_name, buffer + bytes + 8, guess->ed_namelen);
	  temp->d_name[guess->ed_namelen] = '\0';
	  temp->d_ops = &ext2_dops;
	  temp->d_prev = result;
	  if (result != NULL)
	    result->d_next = temp;
	  temp->d_next = NULL;
	  result = temp;

	  bytes += guess->ed_size;
	}
    }

  kfree (buffer);
  kfree (ei);
  for (; result->d_prev != NULL; result = result->d_prev)
    ;
  *entries = result;
  return 0;
}

int
ext2_mkdir (VFSInode *dir, VFSDirEntry *entry, mode_t mode)
{
  return -ENOSYS;
}

int
ext2_rmdir (VFSInode *dir, VFSDirEntry *entry)
{
  return -ENOSYS;
}

int
ext2_mknod (VFSInode *dir, VFSDirEntry *entry, mode_t mode, dev_t rdev)
{
  return -ENOSYS;
}

int
ext2_rename (VFSInode *olddir, VFSDirEntry *oldentry, VFSInode *newdir,
	     VFSDirEntry *newentry)
{
  return -ENOSYS;
}

int
ext2_readlink (VFSDirEntry *entry, char *buffer, size_t len)
{
  int i;
  loff_t size = entry->d_inode->vi_size;
  Ext2Inode *inode = entry->d_inode->vi_private;

  if (size <= 60)
    {
      /* Path stored directly in block pointers */
      for (i = 0; i < len && i < size && i < 48; i++)
	buffer[i] = ((char *) inode->ei_bptr0)[i];
      for (; i < len && i < size && i < 52; i++)
	buffer[i] = ((char *) &inode->ei_bptr1)[i - 48];
      for (; i < len && i < size && i < 56; i++)
	buffer[i] = ((char *) &inode->ei_bptr2)[i - 52];
      for (; i < len && i < size && i < 60; i++)
	buffer[i] = ((char *) &inode->ei_bptr3)[i - 56];
    }
  else
    {
      uint32_t block = 0;
      VFSSuperblock *sb = entry->d_inode->vi_sb;
      uint32_t realblock = ext2_data_block (inode, sb, block);
      void *currblock = kmalloc (sb->sb_blksize);
      if (unlikely (currblock == NULL))
	return -ENOMEM;

      while (1)
	{
	  if (ext2_read_blocks (currblock, sb, realblock, 1) != 0)
	    {
	      kfree (currblock);
	      return -EIO;
	    }
	  if (i + sb->sb_blksize > len)
	    {
	      /* Copy this block to the max length */
	      memcpy (buffer + i, currblock, len - i);
	      break;
	    }

	  memcpy (buffer + i, currblock, sb->sb_blksize);
	  i += sb->sb_blksize;
	  realblock = ext2_data_block (inode, sb, ++block);
	}
      kfree (currblock);
    }
  return 0;
}

int
ext2_truncate (VFSInode *inode)
{
  return -ENOSYS;
}

int
ext2_permission (VFSInode *inode, mode_t mask)
{
  return -ENOSYS;
}

int
ext2_getattr (VFSMount *mp, VFSDirEntry *entry, struct stat *st)
{
  VFSInode *inode = entry->d_inode;
  if (inode == NULL)
    return -EINVAL;
  st->st_dev = makedev (inode->vi_sb->sb_dev->sd_major,
			inode->vi_sb->sb_dev->sd_minor);
  st->st_ino = inode->vi_ino;
  st->st_mode = inode->vi_mode;
  st->st_nlink = inode->vi_nlink;
  st->st_uid = inode->vi_uid;
  st->st_gid = inode->vi_gid;
  st->st_rdev = inode->vi_rdev;
  st->st_size = inode->vi_size;
  st->st_atime = inode->vi_atime.tv_sec;
  st->st_mtime = inode->vi_mtime.tv_sec;
  st->st_ctime = inode->vi_ctime.tv_sec;
  st->st_blksize = mp->vfs_sb.sb_blksize;
  st->st_blocks = inode->vi_blocks;
  return 0;
}

int
ext2_setxattr (VFSDirEntry *entry, const char *name, const void *value,
	       size_t len, int flags)
{
  return -ENOSYS;
}

int
ext2_getxattr (VFSDirEntry *entry, const char *name, void *buffer, size_t len)
{
  return -ENOSYS;
}

int
ext2_listxattr (VFSDirEntry *entry, char *buffer, size_t len)
{
  return -ENOSYS;
}

int
ext2_removexattr (VFSDirEntry *entry, const char *name)
{
  return -ENOSYS;
}
