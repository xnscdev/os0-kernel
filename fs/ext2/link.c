/*************************************************************************
 * link.c -- This file is part of OS/0.                                  *
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
#include <vm/heap.h>

static int
ext2_check_empty (VFSInode *dir, int entry, Ext2DirEntry *dirent, int offset,
		  int blocksize, char *buffer, void *priv)
{
  if (strcmp (dirent->d_name, ".") != 0 && strcmp (dirent->d_name, "..") != 0)
    {
      *((int *) priv) = 0;
      return DIRENT_ABORT;
    }
  return 0;
}

static int
ext2_process_link (VFSInode *dir, int entry, Ext2DirEntry *dirent, int offset,
		   int blocksize, char *buffer, void *priv)
{
  Ext2Filesystem *fs = dir->vi_sb->sb_private;
  Ext2Link *l = priv;
  Ext2DirEntry *next;
  unsigned int rec_len;
  unsigned int min_rec_len;
  unsigned int curr_rec_len;
  int csum_size = 0;
  int ret = 0;
  if (l->l_done)
    return DIRENT_ABORT;

  rec_len = ext2_dir_rec_len (l->l_namelen, 0);
  l->l_err = ext2_get_rec_len (l->l_sb, dirent, &curr_rec_len);
  if (l->l_err != 0)
    return DIRENT_ABORT;

  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM)
    csum_size = sizeof (Ext2DirEntryTail);

  next = (Ext2DirEntry *) (buffer + offset + curr_rec_len);
  if (offset + curr_rec_len < blocksize - csum_size - 8 && next->d_inode == 0
      && offset + curr_rec_len + next->d_rec_len <= blocksize)
    {
      curr_rec_len += next->d_rec_len;
      l->l_err = ext2_set_rec_len (l->l_sb, curr_rec_len, dirent);
      if (l->l_err != 0)
	return DIRENT_ABORT;
      ret = DIRENT_CHANGED;
    }

  if (dirent->d_inode != 0)
    {
      min_rec_len = ext2_dir_rec_len (dirent->d_name_len & 0xff, 0);
      if (curr_rec_len < min_rec_len + rec_len)
	return ret;
      rec_len = curr_rec_len - min_rec_len;
      l->l_err = ext2_set_rec_len (l->l_sb, min_rec_len, dirent);
      if (l->l_err != 0)
	return DIRENT_ABORT;

      next = (Ext2DirEntry *) (buffer + offset + dirent->d_rec_len);
      next->d_inode = 0;
      next->d_name_len = 0;
      l->l_err = ext2_set_rec_len (l->l_sb, rec_len, next);
      if (l->l_err != 0)
	return DIRENT_ABORT;
      return DIRENT_CHANGED;
    }

  if (curr_rec_len < rec_len)
    return ret;
  dirent->d_inode = l->l_inode;
  dirent->d_name_len = (dirent->d_name_len & 0xff00) | (l->l_namelen & 0xff);
  strncpy (dirent->d_name, l->l_name, l->l_namelen);
  if (fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_FILETYPE)
    dirent->d_name_len = (dirent->d_name_len & 0xff) | ((l->l_flags & 7) << 8);
  l->l_done = 1;
  return DIRENT_ABORT | DIRENT_CHANGED;
}

static int
ext2_process_unlink (VFSInode *dir, int entry, Ext2DirEntry *dirent, int offset,
		     int blocksize, char *buffer, void *priv)
{
  Ext2Link *l = priv;
  Ext2DirEntry *prev = l->l_prev;
  Ext2Inode inode;
  int ret;
  l->l_prev = dirent;
  if (l->l_name != NULL)
    {
      if ((dirent->d_name_len & 0xff) != l->l_namelen)
	return 0;
      if (strncmp (l->l_name, dirent->d_name, dirent->d_name_len & 0xff) != 0)
	return 0;
    }
  if (dirent->d_inode == 0)
    return 0;

  /* Remove a link from the inode link count and unallocate it if there are
     no more remaining links */
  ret = ext2_read_inode (l->l_sb, dirent->d_inode, &inode);
  if (ret != 0)
    goto end;
  if (S_ISDIR (inode.i_mode))
    {
      /* Make sure the directory is empty */
      int empty = 1;
      ext2_dir_iterate (l->l_sb, dir, DIRENT_FLAG_EMPTY, NULL, ext2_check_empty,
			&empty);
      if (!empty)
	{
	  l->l_err = -ENOTEMPTY;
	  return DIRENT_ABORT;
	}
    }
  if (--inode.i_links_count == 0)
    {
      inode.i_dtime = time (NULL);
      ext2_inode_alloc_stats (l->l_sb, dirent->d_inode, -1,
			      S_ISDIR (inode.i_mode));
      ext2_dealloc_blocks (l->l_sb, dirent->d_inode, &inode, NULL, 0, ~0ULL);
    }
  ext2_update_inode (l->l_sb, dirent->d_inode, &inode, sizeof (Ext2Inode));

 end:
  if (offset != 0)
    prev->d_rec_len += dirent->d_rec_len;
  else
    dirent->d_inode = 0;
  l->l_done = 1;
  return DIRENT_ABORT | DIRENT_CHANGED;
}

int
ext2_add_index_link (VFSSuperblock *sb, VFSInode *dir, const char *name,
		     ino64_t ino, int flags)
{
  return -ENOTSUP;
}

int
ext2_add_link (VFSSuperblock *sb, VFSInode *dir, const char *name, ino64_t ino,
	       int flags)
{
  Ext2Inode *inode = dir->vi_private;
  Ext2Link l;
  int ret;
  if (sb->sb_mntflags & MS_RDONLY)
    return -EROFS;

  if (inode->i_flags & EXT2_INDEX_FL)
    return ext2_add_index_link (sb, dir, name, ino, flags);

  l.l_sb = sb;
  l.l_name = name;
  l.l_namelen = name == NULL ? 0 : strlen (name);
  l.l_inode = ino;
  l.l_flags = flags;
  l.l_done = 0;
  ret =
    ext2_dir_iterate (sb, dir, DIRENT_FLAG_EMPTY, NULL, ext2_process_link, &l);
  if (ret != 0)
    return ret;
  if (l.l_err != 0)
    return l.l_err;
  if (l.l_done)
    return 0;

  /* Couldn't add the entry, expand the directory and try again */
  ret = ext2_expand_dir (dir);
  if (ret != 0)
    return -ENOSPC;
  ret =
    ext2_dir_iterate (sb, dir, DIRENT_FLAG_EMPTY, NULL, ext2_process_link, &l);
  if (ret != 0)
    return ret;
  if (l.l_err != 0)
    return l.l_err;
  return l.l_done ? 0 : -ENOSPC;
}

int
ext2_unlink_dirent (VFSSuperblock *sb, VFSInode *dir, const char *name,
		    int flags)
{
  Ext2Link l;
  int ret;
  if (sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  l.l_sb = sb;
  l.l_name = name;
  l.l_namelen = name == NULL ? 0 : strlen (name);
  l.l_flags = flags;
  l.l_done = 0;
  l.l_err = 0;
  l.l_prev = NULL;
  ret = ext2_dir_iterate (sb, dir, DIRENT_FLAG_EMPTY, NULL, ext2_process_unlink,
			  &l);
  if (ret != 0)
    return ret;
  if (l.l_err != 0)
    return l.l_err;
  return l.l_done ? 0 : -ENOENT;
}

int
ext2_dir_type (mode_t mode)
{
  if (S_ISREG (mode))
    return EXT2_DIRTYPE_REG;
  if (S_ISDIR (mode))
    return EXT2_DIRTYPE_DIR;
  if (S_ISCHR (mode))
    return EXT2_DIRTYPE_CHRDEV;
  if (S_ISBLK (mode))
    return EXT2_DIRTYPE_BLKDEV;
  if (S_ISFIFO (mode))
    return EXT2_DIRTYPE_FIFO;
  if (S_ISSOCK (mode))
    return EXT2_DIRTYPE_SOCKET;
  if (S_ISLNK (mode))
    return EXT2_DIRTYPE_LINK;
  return EXT2_DIRTYPE_NONE;
}
