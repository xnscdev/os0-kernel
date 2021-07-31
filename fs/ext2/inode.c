/*************************************************************************
 * inode.c -- This file is part of OS/0.                                 *
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
#include <sys/param.h>
#include <sys/process.h>
#include <sys/sysmacros.h>
#include <vm/heap.h>

int
ext2_create (VFSInode *dir, const char *name, mode_t mode)
{
  return ext2_new_file (dir, name, (mode & ~S_IFMT) | S_IFREG, NULL);
}

int
ext2_lookup (VFSInode **inode, VFSInode *dir, VFSSuperblock *sb,
	     const char *name, int symcount)
{
  ino64_t ino;
  Ext2File *file;
  VFSInode *vi;
  int ret = ext2_lookup_inode (sb, dir, name, strlen (name), NULL, &ino);
  if (ret != 0)
    return ret;

  vi = vfs_alloc_inode (sb);
  if (unlikely (vi == NULL))
    return -ENOMEM;
  file = kzalloc (sizeof (Ext2File));
  if (unlikely (file == NULL))
    {
      kfree (vi);
      return -ENOMEM;
    }
  vi->vi_ino = ino;
  vi->vi_private = file;
  ret = ext2_fill_inode (vi);
  if (ret != 0)
    {
      vfs_unref_inode (vi);
      return ret;
    }

  if (symcount >= 0 && S_ISLNK (vi->vi_mode))
    {
      /* Change working directory for relative symlinks */
      Process *proc = &process_table[task_getpid ()];
      VFSInode *cwd = proc->p_cwd;
      char *buffer = kzalloc (PATH_MAX);
      if (unlikely (buffer == NULL))
	{
	  vfs_unref_inode (vi);
	  return -ENOMEM;
	}
      proc->p_cwd = dir;

      /* Read symlink contents */
      ret = ext2_readlink (vi, buffer, PATH_MAX);
      if (ret < 0)
	goto end;

      /* Open symlink path */
      ret = vfs_open_file (inode, buffer, 0);
    end:
      vfs_unref_inode (vi);
      kfree (buffer);
      proc->p_cwd = cwd;
      return ret;
    }
  else
    *inode = vi;
  return 0;
}

int
ext2_link (VFSInode *old, VFSInode *dir, const char *new)
{
  if (old->vi_sb != dir->vi_sb)
    return -EXDEV;
  return ext2_add_link (dir->vi_sb, dir, new, old->vi_ino,
			ext2_dir_type (old->vi_mode));
}

int
ext2_unlink (VFSInode *dir, const char *name)
{
  VFSSuperblock *sb = dir->vi_sb;
  Ext2Inode *ei = dir->vi_private;
  int blocks = (dir->vi_size + sb->sb_blksize - 1) / sb->sb_blksize;
  void *buffer;
  char *guessname;
  VFSInode *temp;
  block_t *realblocks;
  int ret;
  int i;

  ret = vfs_lookup (&temp, dir, dir->vi_sb, name, 0);
  if (ret != 0)
    return ret;
  if (process_table[task_getpid ()].p_euid != 0 && S_ISDIR (temp->vi_mode))
    {
      /* Unlinking a directory requires root */
      vfs_unref_inode (temp);
      return -EPERM;
    }

  buffer = kmalloc (sb->sb_blksize);
  realblocks = kmalloc (sizeof (block_t) * blocks);
  if (unlikely (buffer == NULL || realblocks == NULL))
    {
      kfree (buffer);
      vfs_unref_inode (temp);
      return -ENOMEM;
    }
  if (ext2_data_blocks (ei, sb, 0, blocks, realblocks) < 0)
    {
      kfree (buffer);
      kfree (realblocks);
      vfs_unref_inode (temp);
      return -ENOMEM;
    }

  for (i = 0; i < blocks; i++)
    {
      int bytes = 0;
      Ext2DirEntry *last = NULL;
      if (ext2_read_blocks (buffer, sb, realblocks[i], 1) != 0)
	{
	  kfree (buffer);
	  kfree (realblocks);
	  vfs_unref_inode (temp);
	  return -EIO;
	}
      while (bytes < sb->sb_blksize)
	{
	  Ext2DirEntry *guess = (Ext2DirEntry *) (buffer + bytes);
	  uint16_t namelen;
	  if (guess->d_inode == 0 || guess->d_rec_len == 0)
	    {
	      bytes += 4;
	      continue;
	    }

	  namelen = guess->d_name_len & 0xff;
	  guessname = (char *) guess + sizeof (Ext2DirEntry);
	  if (strlen (name) == namelen
	      && strncmp (guessname, name, namelen) == 0)
	    {
	      if (last != NULL)
	        last->d_rec_len += guess->d_rec_len;
	      memset (guess, 0, sizeof (Ext2DirEntry));
	      ret = ext2_write_blocks (buffer, sb, realblocks[i], 1);
	      kfree (buffer);
	      kfree (realblocks);
	      if (ret != 0)
		{
		  vfs_unref_inode (temp);
		  return ret;
		}
	      ret = ext2_unref_inode (sb, temp);
	      vfs_unref_inode (temp);
	      return ret;
	    }

	  bytes += guess->d_rec_len;
	  last = guess;
	}
    }

  kfree (buffer);
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
  Ext2File *file = inode->vi_private;
  unsigned int count = 0;
  unsigned int start;
  unsigned int c;
  uint64_t left;
  char *ptr = buffer;
  int ret;
  file->f_pos = offset;

  if (file->f_inode.i_flags & EXT4_INLINE_DATA_FL)
    return -ENOTSUP;

  while (file->f_pos < EXT2_I_SIZE (file->f_inode) && len > 0)
    {
      ret = ext2_sync_file_buffer_pos (file);
      if (ret != 0)
	return ret;
      ret = ext2_load_file_buffer (file, 0);
      if (ret != 0)
	return ret;
      start = file->f_pos % inode->vi_sb->sb_blksize;
      c = inode->vi_sb->sb_blksize - start;
      if (c > len)
	c = len;
      left = EXT2_I_SIZE (file->f_inode) - file->f_pos;
      if (c > left)
	c = left;
      memcpy (ptr, file->f_buffer + start, c);
      file->f_pos += c;
      ptr += c;
      count += c;
      len -= c;
    }
  return count;
}

int
ext2_write (VFSInode *inode, const void *buffer, size_t len, off_t offset)
{
  Ext2File *file = inode->vi_private;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  const char *ptr = buffer;
  unsigned int count = 0;
  unsigned int start;
  unsigned int c;
  int ret;

  if (file->f_inode.i_flags & EXT4_INLINE_DATA_FL)
    return -ENOTSUP;

  while (len > 0)
    {
      ret = ext2_sync_file_buffer_pos (file);
      if (ret != 0)
	goto end;
      start = file->f_pos % blksize;
      c = blksize - start;
      if (c > len)
	c = len;
      ret = ext2_load_file_buffer (file, c == blksize);
      if (ret != 0)
	goto end;
      file->f_flags |= EXT2_FILE_BUFFER_DIRTY;
      memcpy (file->f_buffer + start, ptr, c);

      if (file->f_physblock == 0)
	{
	  ret = ext2_bmap (inode->vi_sb, file->f_ino, &file->f_inode,
			   file->f_buffer + blksize,
			   file->f_ino == 0 ? 0 : BMAP_ALLOC, file->f_block, 0,
			   &file->f_physblock);
	  if (ret != 0)
	    goto end;
	}

      file->f_pos += c;
      ptr += c;
      count += c;
      len -= c;
    }

 end:
  if (count != 0 && EXT2_I_SIZE (file->f_inode) < file->f_pos)
    {
      int ret2 = ext2_file_set_size (file, file->f_pos);
      if (ret == 0)
	ret = ret2;
    }
  return ret == 0 ? count : ret;
}

int
ext2_readdir (VFSDirEntry **entry, VFSDirectory *dir, VFSSuperblock *sb)
{
  Ext2Inode *ei = dir->vd_inode->vi_private;
  while (1)
    {
      Ext2DirEntry *guess = (Ext2DirEntry *) (dir->vd_buffer + dir->vd_offset);
      VFSDirEntry *result;
      uint16_t namelen;
      block_t realblock;
      int ret;

      if (guess->d_inode == 0 || guess->d_rec_len == 0)
	{
	  dir->vd_offset += 4;
	  if (dir->vd_offset >= sb->sb_blksize)
	    {
	      if (++dir->vd_block >=
		  (dir->vd_inode->vi_size + sb->sb_blksize - 1) /
		  sb->sb_blksize)
		return 1; /* Finished reading all directory entries */
	      ret = ext2_data_blocks (ei, sb, dir->vd_block, 1, &realblock);
	      if (ret < 0)
		return ret;
	      ret = ext2_read_blocks (dir->vd_buffer, sb, realblock, 1);
	      if (ret < 0)
		return ret;
	      dir->vd_offset = 0;
	    }
	  continue;
	}

      namelen = guess->d_name_len & 0xff;
      result = kmalloc (sizeof (VFSDirEntry));
      result->d_flags = 0;
      result->d_inode = vfs_alloc_inode (sb);
      result->d_inode->vi_ino = guess->d_inode;
      ext2_fill_inode (result->d_inode);
      result->d_mounted = 0;
      result->d_name = kmalloc (namelen + 1);
      strncpy (result->d_name, dir->vd_buffer + dir->vd_offset + 8, namelen);
      result->d_name[namelen] = '\0';
      result->d_ops = &ext2_dops;
      dir->vd_offset += guess->d_rec_len;
      *entry = result;
      return 0;
    }
}

int
ext2_chmod (VFSInode *inode, mode_t mode)
{
  Ext2Inode *ei = inode->vi_private;
  inode->vi_mode = (inode->vi_mode & S_IFMT) | (mode & 07777);
  ei->i_mode = (ei->i_mode & 0xf000) | (mode & 07777);
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
  VFSInode *inode;
  int ret = ext2_new_file (dir, name, (mode & ~S_IFMT) | S_IFDIR, &inode);
  if (ret != 0)
    return ret;

  /* Build `.' and `..' entries */
  ext2_add_link (inode->vi_sb, inode, ".", inode->vi_ino, EXT2_DIRTYPE_DIR);
  ext2_add_link (inode->vi_sb, inode, "..", dir->vi_ino, EXT2_DIRTYPE_DIR);

  /* Increase refcount of parent directory */
  dir->vi_nlink++;
  ext2_write_inode (dir);

  vfs_unref_inode (inode);
  return 0;
}

int
ext2_rmdir (VFSInode *dir, const char *name)
{
  VFSSuperblock *sb = dir->vi_sb;
  Ext2Inode *ei = dir->vi_private;
  int blocks = (dir->vi_size + sb->sb_blksize - 1) / sb->sb_blksize;
  void *buffer;
  char *guessname;
  block_t *realblocks;
  VFSInode *temp;
  VFSDirectory *d;
  int ret;
  int i = 0;

  ret = vfs_lookup (&temp, dir, dir->vi_sb, name, 0);
  if (ret != 0)
    return ret;
  if (!S_ISDIR (temp->vi_mode))
    {
      vfs_unref_inode (temp);
      return -ENOTDIR;
    }

  /* Make sure the directory is empty */
  d = ext2_alloc_dir (dir, sb);
  if (d == NULL)
    return -ENOMEM;
  while (1)
    {
      VFSDirEntry *entry;
      int bad;
      ret = ext2_readdir (&entry, d, sb);
      if (ret == 1)
	break;
      else if (ret < 0)
	{
	  ext2_destroy_dir (d);
	  return ret;
	}
      bad = strcmp (entry->d_name, ".") != 0
	&& strcmp (entry->d_name, "..") != 0;
      vfs_destroy_dir_entry (entry);
      if (bad)
	{
	  ext2_destroy_dir (d);
	  return -ENOTEMPTY;
	}
    }
  ext2_destroy_dir (d);

  buffer = kmalloc (sb->sb_blksize);
  realblocks = kmalloc (sizeof (block_t) * blocks);
  if (unlikely (buffer == NULL || realblocks == NULL))
    {
      kfree (buffer);
      vfs_unref_inode (temp);
      return -ENOMEM;
    }
  if (ext2_data_blocks (ei, sb, 0, blocks, realblocks) < 0)
    {
      kfree (buffer);
      kfree (realblocks);
      vfs_unref_inode (temp);
      return -ENOMEM;
    }

  for (i = 0; i < blocks; i++)
    {
      int bytes = 0;
      Ext2DirEntry *last = NULL;
      if (ext2_read_blocks (buffer, sb, realblocks[i], 1) != 0)
	{
	  kfree (buffer);
	  kfree (realblocks);
	  vfs_unref_inode (temp);
	  return -EIO;
	}
      while (bytes < sb->sb_blksize)
	{
	  Ext2DirEntry *guess = (Ext2DirEntry *) (buffer + bytes);
	  uint16_t namelen;
	  if (guess->d_inode == 0 || guess->d_rec_len == 0)
	    {
	      bytes += 4;
	      continue;
	    }

	  namelen = guess->d_name_len;
	  guessname = (char *) guess + sizeof (Ext2DirEntry);
	  if (strlen (name) == namelen
	      && strncmp (guessname, name, namelen) == 0)
	    {
	      if (last != NULL)
	        last->d_rec_len += guess->d_rec_len;
	      memset (guess, 0, sizeof (Ext2DirEntry));
	      ret = ext2_write_blocks (buffer, sb, realblocks[i], 1);
	      kfree (buffer);
	      kfree (realblocks);
	      if (ret != 0)
		{
		  vfs_unref_inode (temp);
		  return ret;
		}
	      ret = ext2_unref_inode (sb, temp);
	      vfs_unref_inode (temp);
	      return ret;
	    }

	  bytes += guess->d_rec_len;
	  last = guess;
	}
    }

  kfree (buffer);
  return -ENOENT;
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
ext2_readlink (VFSInode *inode, char *buffer, size_t len)
{
  int i;
  block_t size = inode->vi_size;
  Ext2Inode *ei = inode->vi_private;

  if (size <= 60)
    {
      /* Path stored directly in block pointers */
      for (i = 0; i < len && i < size && i < 48; i++)
	buffer[i] = ((char *) ei->i_block)[i];
      for (; i < len && i < size && i < 52; i++)
	buffer[i] = ((char *) &ei->i_block[EXT2_IND_BLOCK])[i - 48];
      for (; i < len && i < size && i < 56; i++)
	buffer[i] = ((char *) &ei->i_block[EXT2_DIND_BLOCK])[i - 52];
      for (; i < len && i < size && i < 60; i++)
	buffer[i] = ((char *) &ei->i_block[EXT2_TIND_BLOCK])[i - 56];
    }
  else
    {
      uint32_t block = 0;
      VFSSuperblock *sb = inode->vi_sb;
      block_t realblock;
      void *currblock;
      int ret = ext2_data_blocks (ei, sb, block, 1, &realblock);
      if (ret < 0)
	return ret;

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
	  if (i + sb->sb_blksize > inode->vi_size)
	    {
	      memcpy (buffer + i, currblock, inode->vi_size - i);
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
	  ret = ext2_data_blocks (ei, sb, ++block, 1, &realblock);
	  if (ret < 0)
	    {
	      kfree (currblock);
	      return ret;
	    }
	}
    }
  return 0;
}

int
ext2_truncate (VFSInode *inode)
{
  Ext2Superblock *esb = &((Ext2Filesystem *) inode->vi_sb->sb_private)->f_super;
  Ext2Inode *ei = inode->vi_private;
  uint64_t origsize = ei->i_size;
  uint64_t newsize = inode->vi_size & 0xffffffff;
  blksize_t blksize = inode->vi_sb->sb_blksize;

  if (S_ISDIR (inode->vi_mode))
    return -EISDIR;

  if (esb->s_rev_level > 0 && S_ISREG (inode->vi_mode)
      && esb->s_feature_ro_compat & EXT2_FT_RO_COMPAT_LARGE_FILE)
    {
      origsize |= (uint64_t) ei->i_size_high << 32;
      ei->i_size_high = inode->vi_size >> 32;
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
      blkcnt64_t nblocks = (origsize + blksize - 1) / blksize - start;
      if (nblocks != 0)
	return ext2_unalloc_data_blocks (inode, start, nblocks);
      else
	return 0;
    }
  else
    {
      blkcnt64_t origblocks = (origsize + blksize - 1) / blksize;
      blkcnt64_t newblocks = (newsize + blksize - 1) / newsize;
      return ext2_extend_inode (inode, origblocks, newblocks);
    }
}

int
ext2_getattr (VFSInode *inode, struct stat64 *st)
{
  st->st_dev = makedev (inode->vi_sb->sb_dev->sd_major,
			inode->vi_sb->sb_dev->sd_minor);
  st->st_ino = inode->vi_ino;
  st->st_mode = inode->vi_mode;
  st->st_nlink = inode->vi_nlink;
  st->st_uid = inode->vi_uid;
  st->st_gid = inode->vi_gid;
  st->st_rdev = inode->vi_rdev;
  st->st_size = inode->vi_size;
  st->st_atim = inode->vi_atime;
  st->st_mtim = inode->vi_mtime;
  st->st_ctim = inode->vi_ctime;
  st->st_blksize = inode->vi_sb->sb_blksize;
  st->st_blocks = inode->vi_blocks;
  return 0;
}

int
ext2_setxattr (VFSInode *inode, const char *name, const void *value,
	       size_t len, int flags)
{
  return -ENOSYS;
}

int
ext2_getxattr (VFSInode *inode, const char *name, void *buffer, size_t len)
{
  return -ENOSYS;
}

int
ext2_listxattr (VFSInode *inode, char *buffer, size_t len)
{
  return -ENOSYS;
}

int
ext2_removexattr (VFSInode *inode, const char *name)
{
  return -ENOSYS;
}
