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

void
vfs_destroy_dir_entry (VFSDirEntry *entry)
{
  if (entry == NULL)
    return;
  vfs_destroy_inode (entry->d_inode);
  kfree (entry->d_name);
  kfree (entry);
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

VFSInode *
vfs_alloc_inode (VFSSuperblock *sb)
{
  return sb->sb_ops->sb_alloc_inode (sb);
}

void
vfs_destroy_inode (VFSInode *inode)
{
  if (inode == NULL)
    return;
  inode->vi_sb->sb_ops->sb_destroy_inode (inode);
}

void
vfs_fill_inode (VFSInode *inode)
{
  inode->vi_sb->sb_ops->sb_fill_inode (inode);
}

void
vfs_write_inode (VFSInode *inode)
{
  inode->vi_sb->sb_ops->sb_write_inode (inode);
}

void
vfs_free_sb (VFSSuperblock *sb)
{
  sb->sb_ops->sb_free (sb);
}

void
vfs_update_sb (VFSSuperblock *sb)
{
  sb->sb_ops->sb_update (sb);
}

int
vfs_statvfs (VFSSuperblock *sb, struct statvfs *st)
{
  return sb->sb_ops->sb_statvfs (sb, st);
}

int
vfs_remount (VFSSuperblock *sb, int *flags, void *data)
{
  return sb->sb_ops->sb_remount (sb, flags, data);
}

int
vfs_create (VFSInode *dir, const char *name, mode_t mode)
{
  return dir->vi_ops->vfs_create (dir, name, mode);
}

int
vfs_lookup (VFSDirEntry *entry, VFSSuperblock *sb, VFSPath *path)
{
  return sb->sb_fstype->vfs_iops->vfs_lookup (entry, sb, path);
}

int
vfs_link (VFSInode *old, VFSInode *dir, const char *new)
{
  return dir->vi_ops->vfs_link (old, dir, new);
}

int
vfs_unlink (VFSInode *dir, const char *name)
{
  return dir->vi_ops->vfs_unlink (dir, name);
}

int
vfs_symlink (VFSInode *dir, const char *old, const char *new)
{
  return dir->vi_ops->vfs_symlink (dir, old, new);
}

int
vfs_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  return inode->vi_ops->vfs_read (inode, buffer, len, offset);
}

int
vfs_write (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  return inode->vi_ops->vfs_write (inode, buffer, len, offset);
}

int
vfs_readdir (VFSDirEntry **entries, VFSSuperblock *sb, VFSInode *dir)
{
  return sb->sb_fstype->vfs_iops->vfs_readdir (entries, sb, dir);
}

int
vfs_chmod (VFSInode *inode, mode_t mode)
{
  return inode->vi_ops->vfs_chmod (inode, mode);
}

int
vfs_chown (VFSInode *inode, uid_t uid, gid_t gid)
{
  return inode->vi_ops->vfs_chown (inode, uid, gid);
}

int
vfs_mkdir (VFSInode *dir, const char *name, mode_t mode)
{
  return dir->vi_ops->vfs_mkdir (dir, name, mode);
}

int
vfs_rmdir (VFSInode *dir, const char *name)
{
  return dir->vi_ops->vfs_rmdir (dir, name);
}

int
vfs_mknod (VFSInode *dir, const char *name, mode_t mode, dev_t rdev)
{
  return dir->vi_ops->vfs_mknod (dir, name, mode, rdev);
}

int
vfs_rename (VFSInode *olddir, const char *oldname, VFSInode *newdir,
	    const char *newname)
{
  return olddir->vi_ops->vfs_rename (olddir, oldname, newdir, newname);
}

int
vfs_readlink (VFSDirEntry *entry, char *buffer, size_t len)
{
  return entry->d_inode->vi_ops->
    vfs_readlink (entry, buffer, len);
}

int
vfs_truncate (VFSInode *inode)
{
  return inode->vi_ops->vfs_truncate (inode);
}

int
vfs_getattr (VFSMount *mp, VFSDirEntry *entry, struct stat *st)
{
  return mp->vfs_fstype->vfs_iops->vfs_getattr (mp, entry, st);
}

int
vfs_setxattr (VFSDirEntry *entry, const char *name, const void *value,
	      size_t len, int flags)
{
  return entry->d_inode->vi_ops->vfs_setxattr (entry, name, value, len, flags);
}

int
vfs_getxattr (VFSDirEntry *entry, const char *name, void *buffer, size_t len)
{
  return entry->d_inode->vi_ops->vfs_getxattr (entry, name, buffer, len);
}

int
vfs_listxattr (VFSDirEntry *entry, char *buffer, size_t len)
{
  return entry->d_inode->vi_ops->vfs_listxattr (entry, buffer, len);
}

int
vfs_removexattr (VFSDirEntry *entry, const char *name)
{
  return entry->d_inode->vi_ops->vfs_removexattr (entry, name);
}

int
vfs_compare_dir_entry (VFSDirEntry *entry, const char *a, const char *b)
{
  return entry->d_ops->d_compare (entry, a, b);
}

void
vfs_iput_dir_entry (VFSDirEntry *entry, VFSInode *inode)
{
  entry->d_ops->d_iput (entry, inode);
}
