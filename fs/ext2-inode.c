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
#include <libk/libk.h>
#include <sys/ata.h>
#include <sys/device.h>
#include <sys/sysmacros.h>
#include <vm/heap.h>

int
ext2_create (VFSInode *dir, const char *name, mode_t mode)
{
  Ext2Superblock *esb = dir->vi_sb->sb_private;
  VFSInode *inode;
  Ext2Inode *ei;
  ino_t ino;
  time_t newtime = time (NULL);
  int ret;

  inode = ext2_alloc_inode (dir->vi_sb);
  if (unlikely (inode == NULL))
    return -ENOMEM;
  ino = ext2_create_inode (dir->vi_sb, dir->vi_ino / esb->esb_ipg);
  if (ino == 0)
    {
      kfree (inode);
      return -ENOSPC;
    }

  ei = kmalloc (sizeof (Ext2Inode));
  if (unlikely (ei == NULL))
    {
      kfree (inode);
      return -ENOMEM;
    }

  /* TODO Pre-allocate data blocks */
  ei->ei_mode = EXT2_TYPE_FILE | (mode & 07777);
  ei->ei_uid = 0;
  ei->ei_sizel = 0;
  ei->ei_atime = newtime;
  ei->ei_ctime = newtime;
  ei->ei_mtime = newtime;
  ei->ei_dtime = 0;
  ei->ei_gid = 0;
  ei->ei_nlink = 1;
  ei->ei_sectors = 0;
  ei->ei_flags = 0;
  ei->ei_oss1 = 0;
  memset (ei->ei_bptr0, 0, EXT2_STORED_INODES << 2);
  ei->ei_bptr1 = 0;
  ei->ei_bptr2 = 0;
  ei->ei_bptr3 = 0;
  ei->ei_gen = 0; /* ??? */
  ei->ei_acl = 0;
  ei->ei_sizeh = 0;
  ei->ei_fragbaddr = 0; /* ??? */
  memset (ei->ei_oss2, 0, 3);

  inode->vi_ino = ino;
  inode->vi_mode = mode | S_IFREG;
  inode->vi_uid = ei->ei_uid;
  inode->vi_gid = ei->ei_gid;
  inode->vi_flags = ei->ei_flags;
  inode->vi_nlink = ei->ei_nlink;
  inode->vi_size = ei->ei_sizel;
  if ((ei->ei_mode & 0xf000) == EXT2_TYPE_FILE && esb->esb_versmaj > 0
      && esb->esb_roft & EXT2_FT_RO_FILESIZE64)
    inode->vi_size |= (loff_t) ei->ei_sizeh << 32;
  inode->vi_atime.tv_sec = ei->ei_atime;
  inode->vi_atime.tv_nsec = 0;
  inode->vi_mtime.tv_sec = ei->ei_mtime;
  inode->vi_mtime.tv_nsec = 0;
  inode->vi_ctime.tv_sec = ei->ei_ctime;
  inode->vi_ctime.tv_nsec = 0;
  inode->vi_sectors = ei->ei_sectors;
  inode->vi_blocks = ei->ei_sectors * ATA_SECTSIZE / inode->vi_sb->sb_blksize;
  inode->vi_private = ei;

  ext2_write_inode (inode);
  ret = ext2_add_entry (dir, inode, name);
  vfs_destroy_inode (inode);
  return ret;
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

      /* Clean up all unchecked entries */
      for (entries = entries->d_next; entries != NULL;
	   entries = entries->d_next)
        vfs_destroy_dir_entry (entries);
      dir->d_next = NULL;

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
ext2_link (VFSInode *old, VFSInode *dir, const char *new)
{
  return old->vi_sb == dir->vi_sb ? ext2_add_entry (dir, old, new) : -EXDEV;
}

int
ext2_unlink (VFSInode *dir, const char *name)
{
  VFSSuperblock *sb = dir->vi_sb;
  Ext2Superblock *esb;
  Ext2Inode *ei;
  void *buffer;
  char *guessname;
  int blocks;
  int ret;
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

  esb = sb->sb_private;
  blocks = (dir->vi_size + sb->sb_blksize - 1) / sb->sb_blksize;
  buffer = kmalloc (sb->sb_blksize);

  for (i = 0; i < blocks; i++)
    {
      int bytes = 0;
      loff_t block = ext2_data_block (ei, sb, i);
      Ext2DirEntry *last = NULL;
      if (ext2_read_blocks (buffer, sb, block, 1) != 0)
	{
	  kfree (buffer);
	  kfree (ei);
	  return -EIO;
	}
      while (bytes < sb->sb_blksize)
	{
	  Ext2DirEntry *guess = (Ext2DirEntry *) (buffer + bytes);
	  uint16_t namelen;
	  if (guess->ed_inode == 0 || guess->ed_size == 0)
	    {
	      bytes += 4;
	      continue;
	    }

	  namelen = guess->ed_namelenl;
	  if ((esb->esb_reqft & EXT2_FT_REQ_DIRTYPE) == 0)
	    namelen |= guess->ed_namelenh << 8;

	  guessname = (char *) guess + sizeof (Ext2DirEntry);
	  if (strlen (name) == namelen
	      && strncmp (guessname, name, namelen) == 0)
	    {
	      ino_t temp = guess->ed_inode;
	      kfree (ei);
	      if (last != NULL)
	        last->ed_size += guess->ed_size;
	      memset (guess, 0, sizeof (Ext2DirEntry));
	      ret = ext2_write_blocks (buffer, sb, block, 1);
	      kfree (buffer);
	      if (ret != 0)
		return ret;
	      return ext2_unref_inode (sb, temp);
	    }

	  bytes += guess->ed_size;
	  last = guess;
	}
    }

  kfree (buffer);
  kfree (ei);
  return -ENOENT;
}

int
ext2_symlink (VFSInode *dir, const char *old, const char *new)
{
  return -ENOSYS;
}

int
ext2_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  char *temp;
  uint32_t realblock;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  off_t start_block = offset / blksize;
  off_t mid_block = start_block + (offset % blksize != 0);
  off_t end_block = (offset + len) / blksize;
  size_t blocks = end_block - mid_block;
  size_t start_diff = mid_block * blksize - offset;
  size_t end_diff = offset + len - end_block * blksize;
  size_t i;
  int ret;

  if (S_ISDIR (inode->vi_mode))
    return -EISDIR;
  if (offset > inode->vi_size)
    return -EINVAL;

  if (mid_block > end_block)
    {
      /* Completely contained in a single block */
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, start_block);
      if (realblock < 0)
	return realblock;
      temp = kmalloc (blksize);
      if (unlikely (temp == NULL))
	return -ENOMEM;
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (buffer, temp + blksize - start_diff, len);
      kfree (temp);
      return 0;
    }

  for (i = 0; i < blocks; i++)
    {
      realblock = ext2_data_block (inode->vi_private, inode->vi_sb, i);
      if (realblock < 0)
	return realblock;
      ret = ext2_read_blocks (buffer + start_diff + i * blksize, inode->vi_sb,
			      realblock, 1);
      if (ret != 0)
        return ret;
    }

  /* Read unaligned starting bytes */
  if (start_diff != 0)
    {
      temp = kmalloc (blksize);
      if (unlikely (temp == NULL))
	return -ENOMEM;
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, start_block);
      if (realblock < 0)
	{
	  kfree (temp);
	  return realblock;
	}
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (buffer, temp + blksize - start_diff, start_diff);
    }

  /* Read unaligned ending bytes */
  if (end_diff != 0)
    {
      if (temp == NULL)
	{
	  temp = kmalloc (blksize);
	  if (unlikely (temp == NULL))
	    return -ENOMEM;
	}
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, end_block);
      if (realblock < 0)
	{
	  kfree (temp);
	  return realblock;
	}
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (buffer + start_diff + blocks * blksize, temp, end_diff);
    }

  kfree (temp);
  return 0;
}

int
ext2_write (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  char *temp;
  uint32_t realblock;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  off_t start_block = offset / blksize;
  off_t mid_block = start_block + (offset % blksize != 0);
  off_t end_block = (offset + len) / blksize;
  size_t blocks = end_block - mid_block;
  size_t start_diff = mid_block * blksize - offset;
  size_t end_diff = offset + len - end_block * blksize;
  size_t i;
  int ret;

  if (S_ISDIR (inode->vi_mode))
    return -EISDIR;
  if (offset > inode->vi_size)
    {
      inode->vi_size = offset + len;
      ret = ext2_truncate (inode);
      if (ret != 0)
	return ret;
    }

  if (mid_block > end_block)
    {
      /* Completely contained in a single block */
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, start_block);
      if (realblock < 0)
	return realblock;
      temp = kmalloc (blksize);
      if (unlikely (temp == NULL))
	return -ENOMEM;
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (temp + blksize - start_diff, buffer, len);
      ret = ext2_write_blocks (temp, inode->vi_sb, realblock, 1);
      kfree (temp);
      return ret;
    }

  for (i = 0; i < blocks; i++)
    {
      realblock = ext2_data_block (inode->vi_private, inode->vi_sb, i);
      if (realblock < 0)
	return realblock;
      ret = ext2_write_blocks (buffer + start_diff + i * blksize, inode->vi_sb,
			       realblock, 1);
      if (ret != 0)
        return ret;
    }

  /* Write unaligned starting bytes */
  if (start_diff != 0)
    {
      temp = kmalloc (blksize);
      if (unlikely (temp == NULL))
	return -ENOMEM;
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, start_block);
      if (realblock < 0)
	{
	  kfree (temp);
	  return realblock;
	}
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (temp + blksize - start_diff, buffer, start_diff);
      ret = ext2_write_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
    }

  /* Write unaligned ending bytes */
  if (end_diff != 0)
    {
      if (temp == NULL)
	{
	  temp = kmalloc (blksize);
	  if (unlikely (temp == NULL))
	    return -ENOMEM;
	}
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, end_block);
      if (realblock < 0)
	{
	  kfree (temp);
	  return realblock;
	}
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (temp, buffer + start_diff + blocks * blksize, end_diff);
      ret = ext2_write_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
    }

  kfree (temp);
  return 0;
}

int
ext2_readdir (VFSDirEntry **entries, VFSSuperblock *sb, VFSInode *dir)
{
  Ext2Superblock *esb;
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

  esb = sb->sb_private;
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
	  uint16_t namelen;
	  if (guess->ed_inode == 0 || guess->ed_size == 0)
	    {
	      bytes += 4;
	      continue;
	    }

	  namelen = guess->ed_namelenl;
	  if ((esb->esb_reqft & EXT2_FT_REQ_DIRTYPE) == 0)
	    namelen |= guess->ed_namelenh << 8;

	  temp = kmalloc (sizeof (VFSDirEntry));
	  temp->d_flags = 0;
	  temp->d_inode = ext2_alloc_inode (sb);
	  temp->d_inode->vi_ino = guess->ed_inode;
	  ext2_fill_inode (temp->d_inode);
	  temp->d_mounted = 0;
	  temp->d_name = kmalloc (namelen + 1);
	  strncpy (temp->d_name, buffer + bytes + 8, namelen);
	  temp->d_name[namelen] = '\0';
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
ext2_chmod (VFSInode *inode, mode_t mode)
{
  Ext2Inode *ei = inode->vi_private;
  inode->vi_mode = (inode->vi_mode & S_IFMT) | (mode & 07777);
  ei->ei_mode = (ei->ei_mode & 0xf000) | (mode & 07777);
  ext2_write_inode (inode);
  return 0;
}

int
ext2_chown (VFSInode *inode, uid_t uid, gid_t gid)
{
  inode->vi_uid = uid;
  inode->vi_gid = gid;
  ext2_write_inode (inode);
  return 0;
}

int
ext2_mkdir (VFSInode *dir, const char *name, mode_t mode)
{
  return -ENOSYS;
}

int
ext2_rmdir (VFSInode *dir, const char *name)
{
  return -ENOSYS;
}

int
ext2_mknod (VFSInode *dir, const char *name, mode_t mode, dev_t rdev)
{
  return -ENOSYS;
}

int
ext2_rename (VFSInode *olddir, const char *oldname, VFSInode *newdir,
	     const char *newname)
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
      uint32_t realblock;
      void *currblock;

      realblock = ext2_data_block (inode, sb, block);
      if (realblock < 0)
        return -EIO;

      currblock = kmalloc (sb->sb_blksize);
      if (unlikely (currblock == NULL))
	return -ENOMEM;

      while (1)
	{
	  if (ext2_read_blocks (currblock, sb, realblock, 1) != 0)
	    {
	      kfree (currblock);
	      return -EIO;
	    }
	  if (i + sb->sb_blksize > entry->d_inode->vi_size)
	    {
	      memcpy (buffer + i, currblock, entry->d_inode->vi_size - i);
	      kfree (currblock);
	      return i;
	    }
	  if (i + sb->sb_blksize > len)
	    {
	      memcpy (buffer + i, currblock, len - i);
	      kfree (currblock);
	      return i;
	    }

	  memcpy (buffer + i, currblock, sb->sb_blksize);
	  i += sb->sb_blksize;
	  realblock = ext2_data_block (inode, sb, ++block);
	  if (realblock < 0)
	    {
	      kfree (currblock);
	      return -EIO;
	    }
	}
    }
  return 0;
}

int
ext2_truncate (VFSInode *inode)
{
  Ext2Superblock *esb = inode->vi_sb->sb_private;
  Ext2Inode *ei = inode->vi_private;
  uint64_t origsize = ei->ei_sizel;
  uint64_t newsize = inode->vi_size & 0xffffffff;
  blksize_t blksize = inode->vi_sb->sb_blksize;

  if (S_ISDIR (inode->vi_mode))
    return -EISDIR;

  if (esb->esb_versmaj > 0 && S_ISREG (inode->vi_mode)
      && esb->esb_roft & EXT2_FT_RO_FILESIZE64)
    {
      origsize |= (uint64_t) ei->ei_sizeh << 32;
      ei->ei_sizeh = inode->vi_size >> 32;
      newsize |= inode->vi_size >> 32 << 32;
    }
  inode->vi_size = newsize;
  inode->vi_sectors = (newsize + blksize - 1) / blksize * ATA_SECTSIZE;
  ext2_write_inode (inode);

  if (origsize == newsize)
    return 0;
  else if (origsize > newsize)
    {
      off_t start = (newsize + blksize - 1) / blksize;
      blkcnt_t nblocks = (origsize + blksize - 1) / blksize - start;
      if (nblocks != 0)
	return ext2_unalloc_data_blocks (inode, start, nblocks);
      else
	return 0;
    }
  else
    {
      blkcnt_t origblocks = (origsize + blksize - 1) / blksize;
      blkcnt_t newblocks = (newsize + blksize - 1) / newsize;
      return ext2_extend_inode (inode, origblocks, newblocks);
    }
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
