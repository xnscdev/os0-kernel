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

static int
ext2_process_readdir (VFSInode *dir, int entry, Ext2DirEntry *dirent,
		      int offset, int blocksize, char *buffer, void *priv)
{
  Ext2Readdir *r = priv;
  if (offset < r->r_offset)
    r->r_block++;
  r->r_offset = offset;
  r->r_err = r->r_func (dirent->d_name, dirent->d_name_len & 0xff,
			dirent->d_inode, (dirent->d_name_len >> 8) & 0xff,
			r->r_block * dir->vi_sb->sb_blksize + r->r_offset,
			r->r_private);
  if (r->r_err != 0)
    {
      if (r->r_err > 0)
	r->r_err = 0;
      return DIRENT_ABORT;
    }
  return 0;
}

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
  return ext2_unlink_dirent (dir->vi_sb, dir, name, 0);
}

int
ext2_symlink (VFSInode *dir, const char *old, const char *new)
{
  Process *proc = &process_table[task_getpid ()];
  Ext2Filesystem *fs = dir->vi_sb->sb_private;
  blksize_t blksize = dir->vi_sb->sb_blksize;
  block_t block;
  unsigned int target_len;
  ino64_t ino;
  Ext2Inode inode;
  char *blockbuf = NULL;
  int fast_link;
  int inline_link;
  int drop_ref = 0;
  int ret = ext2_read_bitmaps (dir->vi_sb);
  if (ret != 0)
    return ret;

  target_len = strnlen (old, blksize + 1);
  if (target_len >= blksize)
    return -ENAMETOOLONG;

  blockbuf = kzalloc (blksize);
  if (unlikely (blockbuf == NULL))
    return -ENOMEM;
  strncpy (blockbuf, old, blksize);

  fast_link = target_len < 60;
  if (!fast_link)
    {
      ret = ext2_new_block (dir->vi_sb,
			    ext2_find_inode_goal (dir->vi_sb, dir->vi_ino, NULL,
						  0), NULL, &block, NULL);
      if (ret != 0)
	goto end;
    }

  memset (&inode, 0, sizeof (Ext2Inode));
  ret = ext2_new_inode (dir->vi_sb, dir->vi_ino, NULL, &ino);
  if (ret != 0)
    goto end;

  inode.i_mode = SYMLINK_MODE;
  inode.i_uid = proc->p_euid;
  inode.i_gid = proc->p_egid;
  inode.i_links_count = 1;
  ext2_inode_set_size (dir->vi_sb, &inode, target_len);

  inline_link = !fast_link
    && (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_INLINE_DATA);
  if (fast_link)
    strcpy ((char *) &inode.i_block, old);
  else if (inline_link)
    return -ENOTSUP;
  else
    {
      ext2_iblk_set (dir->vi_sb, &inode, 1);
      if (fs->f_super.s_feature_incompat & EXT3_FT_INCOMPAT_EXTENTS)
	inode.i_flags |= EXT4_EXTENTS_FL;
    }

  /* Write new inode */
  if (inline_link)
    ret = ext2_update_inode (dir->vi_sb, ino, &inode, sizeof (Ext2Inode));
  else
    ret = ext2_write_new_inode (dir->vi_sb, ino, &inode);
  if (ret != 0)
    goto end;

  if (!fast_link && !inline_link)
    {
      ret = ext2_bmap (dir->vi_sb, ino, &inode, NULL, BMAP_SET, 0, NULL,
		       &block);
      if (ret != 0)
	goto end;
      ret = ext2_write_blocks (blockbuf, dir->vi_sb, block, 1);
      if (ret != 0)
	goto end;
      ext2_block_alloc_stats (dir->vi_sb, block, 1);
    }
  ext2_inode_alloc_stats (dir->vi_sb, ino, 1, 0);
  drop_ref = 1;

  ret = ext2_add_link (dir->vi_sb, dir, new, ino, EXT2_DIRTYPE_LINK);
  if (ret != 0)
    goto end;
  drop_ref = 0;

 end:
  kfree (blockbuf);
  if (drop_ref)
    {
      if (!fast_link && !inline_link)
	ext2_block_alloc_stats (dir->vi_sb, block, -1);
      ext2_inode_alloc_stats (dir->vi_sb, ino, -1, 0);
    }
  return ret;
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
ext2_readdir (VFSInode *inode, VFSDirEntryFillFunc func, void *private)
{
  Ext2Readdir r;
  int ret;
  r.r_func = func;
  r.r_private = private;
  r.r_block = 0;
  r.r_offset = 0;
  r.r_err = 0;
  ret = ext2_dir_iterate (inode->vi_sb, inode, DIRENT_FLAG_EMPTY, NULL,
			  ext2_process_readdir, &r);
  if (ret != 0)
    return ret;
  return r.r_err;
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
  Ext2Filesystem *fs = dir->vi_sb->sb_private;
  Process *proc = &process_table[task_getpid ()];
  Ext3ExtentHandle *handle;
  Ext2Inode inode;
  VFSInode *temp;
  block_t b;
  ino64_t ino;
  char *block = NULL;
  int drop_ref = 0;
  int ret = ret = ext2_read_bitmaps (dir->vi_sb);
  if (ret != 0)
    goto end;

  ret = ext2_new_inode (dir->vi_sb, dir->vi_ino, NULL, &ino);
  if (ret != 0)
    goto end;

  memset (&inode, 0, sizeof (Ext2Inode));
  ret = ext2_new_block (dir->vi_sb,
			ext2_find_inode_goal (dir->vi_sb, ino, &inode, 0),
			NULL, &b, NULL);
  if (ret != 0)
    goto end;

  ret = ext2_new_dir_block (dir->vi_sb, ino, dir->vi_ino, &block);
  if (ret != 0)
    goto end;

  inode.i_mode = S_IFDIR | mode;
  inode.i_uid = proc->p_euid;
  inode.i_gid = proc->p_egid;
  if (fs->f_super.s_feature_incompat & EXT3_FT_INCOMPAT_EXTENTS)
    inode.i_flags |= EXT4_EXTENTS_FL;
  else
    inode.i_block[0] = b;
  inode.i_size = dir->vi_sb->sb_blksize;
  ext2_iblk_set (dir->vi_sb, &inode, 1);
  inode.i_links_count = 2;

  ret = ext2_write_new_inode (dir->vi_sb, ino, &inode);
  if (ret != 0)
    goto end;

  temp = vfs_alloc_inode (dir->vi_sb);
  if (unlikely (temp == NULL))
    {
      ret = -ENOMEM;
      goto end;
    }
  temp->vi_private = kzalloc (sizeof (Ext2File));
  if (unlikely (temp == NULL))
    {
      ret = -ENOMEM;
      vfs_unref_inode (temp);
      goto end;
    }
  temp->vi_ino = ino;
  ret = ext2_open_file (dir->vi_sb, ino, temp->vi_private);
  if (ret != 0)
    {
      vfs_unref_inode (temp);
      goto end;
    }
  ret = ext2_write_dir_block (dir->vi_sb, b, block, 0, temp);
  vfs_unref_inode (temp);
  if (ret != 0)
    goto end;

  if (fs->f_super.s_feature_incompat & EXT3_FT_INCOMPAT_EXTENTS)
    {
      ret = ext3_extent_open (dir->vi_sb, ino, &inode, &handle);
      if (ret != 0)
	goto end;
      ret = ext3_extent_set_bmap (handle, 0, b, 0);
      if (ret != 0)
	goto end;
    }

  ext2_block_alloc_stats (dir->vi_sb, b, 1);
  ext2_inode_alloc_stats (dir->vi_sb, ino, 1, 1);
  drop_ref = 1;

  ret = ext2_add_link (dir->vi_sb, dir, name, ino, EXT2_DIRTYPE_DIR);
  if (ret != 0)
    goto end;

  if (dir->vi_ino != ino)
    {
      dir->vi_nlink++;
      ret = ext2_write_inode (dir);
      if (ret != 0)
	goto end;
    }
  drop_ref = 0;

 end:
  if (block != NULL)
    kfree (block);
  if (drop_ref)
    {
      ext2_block_alloc_stats (dir->vi_sb, b, -1);
      ext2_inode_alloc_stats (dir->vi_sb, ino, -1, 1);
    }
  return ret;
}

int
ext2_rmdir (VFSInode *dir, const char *name)
{
  return ext2_unlink_dirent (dir->vi_sb, dir, name, 0);
}

int
ext2_mknod (VFSInode *dir, const char *name, mode_t mode, dev_t rdev)
{
  if (!(mode & S_IFMT))
    mode |= S_IFREG;
  if (S_ISCHR (mode) || S_ISBLK (mode))
    {
      if (process_table[task_getpid ()].p_euid != 0)
	return -EPERM;
    }
  else if (!S_ISREG (mode) && !S_ISFIFO (mode) && !S_ISSOCK (mode))
    return -EINVAL;
  return ext2_new_file (dir, name, mode, NULL);
}

int
ext2_rename (VFSInode *olddir, const char *oldname, VFSInode *newdir,
	     const char *newname)
{
  ino64_t ino;
  Ext2Inode inode;
  int ret = ext2_lookup_inode (olddir->vi_sb, olddir, oldname, strlen (oldname),
			       NULL, &ino);
  if (ret != 0)
    return ret;

  /* Remove existing link, if any */
  ret = ext2_unlink_dirent (newdir->vi_sb, newdir, newname, 0);
  if (ret != 0 && ret != -ENOENT)
    return ret;

  /* Determine mode and replace link */
  ret = ext2_read_inode (newdir->vi_sb, ino, &inode);
  if (ret != 0)
    return ret;
  ret = ext2_add_link (newdir->vi_sb, newdir, newname, ino, inode.i_mode);
  if (ret != 0)
    return ret;

  /* Remove old entry */
  return ext2_unlink_dirent (olddir->vi_sb, olddir, oldname, 0);
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
      return 0;
    }
  else
    {
      if (size < len)
	len = size;
      return ext2_read (inode, buffer, len, 0);
    }
}

int
ext2_truncate (VFSInode *inode)
{
  return ext2_file_set_size (inode->vi_private, inode->vi_size);
}

int
ext2_getattr (VFSInode *inode, struct stat64 *st)
{
  st->st_dev = makedev (inode->vi_sb->sb_dev->sd_major,
			inode->vi_sb->sb_dev->sd_minor);
  st->st_blksize = inode->vi_sb->sb_blksize;
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
