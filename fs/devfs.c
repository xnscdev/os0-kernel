/*************************************************************************
 * devfs.c -- This file is part of OS/0.                                 *
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

#include <fs/devfs.h>
#include <libk/libk.h>
#include <sys/ata.h>
#include <sys/process.h>
#include <sys/sysmacros.h>
#include <vm/heap.h>
#include <errno.h>

const VFSSuperblockOps devfs_sops = {
  .sb_alloc_inode = devfs_alloc_inode,
  .sb_destroy_inode = devfs_destroy_inode,
  .sb_free = devfs_free
};

const VFSInodeOps devfs_iops = {
  .vfs_lookup = devfs_lookup,
  .vfs_read = devfs_read,
  .vfs_readdir = devfs_readdir,
  .vfs_readlink = devfs_readlink,
  .vfs_getattr = devfs_getattr
};

const VFSDirEntryOps devfs_dops = {
  .d_compare = devfs_compare
};

const VFSFilesystem devfs_vfs = {
  .vfs_name = DEVFS_FS_NAME,
  .vfs_flags = 0,
  .vfs_mount = devfs_mount,
  .vfs_unmount = devfs_unmount,
  .vfs_sops = &devfs_sops,
  .vfs_iops = &devfs_iops,
  .vfs_dops = &devfs_dops
};

int
devfs_mount (VFSMount *mp, int flags, void *data)
{
  VFSDirEntry *root;
  mp->vfs_sb.sb_fstype = mp->vfs_fstype;
  mp->vfs_sb.sb_ops = &devfs_sops;

  /* Set root dir entry and inode */
  root = kzalloc (sizeof (VFSDirEntry));
  if (unlikely (root == NULL))
    return -ENOMEM;
  root->d_mounted = 1;
  root->d_name = strdup ("/");
  if (unlikely (root->d_name == NULL))
    {
      kfree (root);
      return -ENOMEM;
    }

  mp->vfs_sb.sb_root = devfs_alloc_inode (&mp->vfs_sb);
  if (unlikely (mp->vfs_sb.sb_root == NULL))
    return -ENOMEM;
  mp->vfs_sb.sb_root->vi_ino = DEVFS_ROOT_INODE;
  mp->vfs_sb.sb_root->vi_mode =
    S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
  mp->vfs_sb.sb_root->vi_nlink = 3;
  mp->vfs_sb.sb_root->vi_refcnt = 1;
  return 0;
}

int
devfs_unmount (VFSMount *mp, int flags)
{
  return 0;
}

VFSInode *
devfs_alloc_inode (VFSSuperblock *sb)
{
  VFSInode *inode = kzalloc (sizeof (VFSInode));
  if (unlikely (inode == NULL))
    return NULL;
  inode->vi_ops = &devfs_iops;
  inode->vi_sb = sb;
  inode->vi_refcnt = 1;
  return inode;
}

void
devfs_destroy_inode (VFSInode *inode)
{
  kfree (inode);
}

void
devfs_free (VFSSuperblock *sb)
{
  vfs_unref_inode (sb->sb_root);
}

int
devfs_lookup (VFSInode **inode, VFSInode *dir, VFSSuperblock *sb,
	      const char *name, int follow_symlinks)
{
  int i;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  if (dir->vi_ino == DEVFS_ROOT_INODE)
    {
      for (i = 0; i < DEVICE_TABLE_SIZE; i++)
	{
	  if (strcmp (device_table[i].sd_name, name) == 0)
	    {
	      *inode = vfs_alloc_inode (sb);
	      if (unlikely (*inode == NULL))
		return -ENOMEM;
	      if (device_table[i].sd_type == DEVICE_TYPE_BLOCK)
		(*inode)->vi_mode = S_IFBLK;
	      else
		(*inode)->vi_mode = S_IFCHR;
	      (*inode)->vi_mode |= S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	      (*inode)->vi_nlink = 1;
	      (*inode)->vi_rdev =
		makedev (device_table[i].sd_major, device_table[i].sd_minor);
	      (*inode)->vi_private = &device_table[i];
	      return 0;
	    }
	}
      return -ENOENT;
    }
  else if (dir->vi_ino == DEVFS_FD_INODE)
    {
      int fd = 0;
      /* Convert string to file descriptor */
      while (isdigit (*name))
	{
	  fd *= 10;
	  fd += *name - '0';
	  name++;
	}
      if (*name != '\0')
	return -ENOENT;
      *inode = process_table[task_getpid ()].p_files[fd].pf_inode;
      vfs_ref_inode (*inode);
      return 0;
    }
  return -ENOENT;
}

int
devfs_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  return -ENOSYS;
}

VFSDirEntry *
devfs_readdir (VFSDirectory *dir, VFSSuperblock *sb)
{
  return NULL;
}

int
devfs_readlink (VFSInode *inode, char *buffer, size_t len)
{
  return -ENOSYS;
}

int
devfs_getattr (VFSInode *inode, struct stat *st)
{
  return -ENOSYS;
}

int
devfs_compare (VFSDirEntry *entry, const char *a, const char *b)
{
  return strcmp (a, b);
}

void
devfs_init (void)
{
  if (vfs_register (&devfs_vfs) != 0)
    panic ("Failed to register " DEVFS_FS_NAME " filesystem");
}
