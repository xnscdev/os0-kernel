/*************************************************************************
 * vfs.c -- This file is part of OS/0.                                   *
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
#include <fs/vfs.h>
#include <libk/libk.h>
#include <vm/heap.h>
#include <errno.h>

VFSFilesystem fs_table[VFS_FS_TABLE_SIZE];
VFSMount mount_table[VFS_MOUNT_TABLE_SIZE];

void
vfs_init (void)
{
  ext2_init ();
}

int
vfs_register (const VFSFilesystem *fs)
{
  int i;
  if (fs == NULL || *fs->vfs_name == '\0')
    return -EINVAL;
  for (i = 0; i < VFS_FS_TABLE_SIZE; i++)
    {
      if (*fs_table[i].vfs_name != '\0')
	continue;
      memcpy (&fs_table[i], fs, sizeof (VFSFilesystem));
      return 0;
    }
  return -ENOSPC;
}

int
vfs_mount (const char *type, const char *dir, int flags, void *data)
{
  int i;
  if (type == NULL || dir == NULL)
    return -EINVAL;
  for (i = 0; i < VFS_FS_TABLE_SIZE; i++)
    {
      VFSMount *mp;
      int ret;
      int j;
      if (strcmp (fs_table[i].vfs_name, type) != 0)
        continue;
      mp = kzalloc (sizeof (VFSMount));
      if (unlikely (mp == NULL))
	return -ENOMEM;
      mp->vfs_fstype = &fs_table[i];

      /* Get mount point as a path and set parent mount */
      ret = vfs_namei (&mp->vfs_mntpoint, dir);
      if (ret != 0)
	{
	  kfree (mp);
	  return ret;
	}
      for (j = 0; j < VFS_MOUNT_TABLE_SIZE; j++)
	{
	  if (mount_table[j].vfs_mntpoint == NULL)
	    continue;
	  if (vfs_path_subdir (mp->vfs_mntpoint, mount_table[j].vfs_mntpoint))
	    {
	      mp->vfs_parent = &mount_table[j];
	      break;
	    }
	}

      /* Find a slot in the mount table */
      for (j = 0; j < VFS_MOUNT_TABLE_SIZE; j++)
	{
	  if (mount_table[j].vfs_fstype == NULL)
	    break;
	}
      if (j == VFS_MOUNT_TABLE_SIZE)
	{
	  vfs_path_free (mp->vfs_mntpoint);
	  kfree (mp);
	  return -ENOSPC;
	}

      /* Filesystem-specific mount */
      ret = fs_table[i].vfs_mount (mp, flags, data);
      if (ret != 0)
	return ret;

      /* Fill mount table */
      memcpy (&mount_table[j], mp, sizeof (VFSMount));
      return 0;
    }
  return -EINVAL; /* No such filesystem type */
}

void
vfs_inode_destroy (VFSInode *inode)
{
  if (inode == NULL)
    return;
  inode->vi_sb->sb_ops->sb_destroy_inode (inode);
}

void
vfs_dir_entry_destroy (VFSDirEntry *entry)
{
  if (entry == NULL)
    return;
  vfs_inode_destroy (entry->d_inode);
  kfree (entry->d_name);
  kfree (entry);
}
