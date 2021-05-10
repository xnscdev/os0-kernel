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

#include <fs/devfs.h>
#include <fs/ext2.h>
#include <fs/vfs.h>
#include <libk/libk.h>
#include <sys/process.h>
#include <sys/syscall.h>
#include <vm/heap.h>

VFSFilesystem fs_table[VFS_FS_TABLE_SIZE];
VFSMount mount_table[VFS_MOUNT_TABLE_SIZE];
VFSInode *vfs_root_inode;

static VFSInode *vfs_default_lookup (VFSInode *dir, VFSSuperblock *sb,
				     const char *name, int follow_symlinks);
static VFSDirEntry *vfs_default_readdir (VFSDirectory *dir, VFSSuperblock *sb);

static VFSInodeOps vfs_default_iops = {
  .vfs_lookup = vfs_default_lookup,
  .vfs_readdir = vfs_default_readdir
};

static VFSSuperblockOps vfs_default_sops = {
  .sb_destroy_inode = (void (*) (VFSInode *)) kfree
};

static VFSSuperblock vfs_default_sb = {
  .sb_ops = &vfs_default_sops
};

static VFSInode *
vfs_default_lookup (VFSInode *dir, VFSSuperblock *sb, const char *name,
		    int follow_symlinks)
{
  /* Must be root directory */
  if (dir->vi_ino != 0)
    return NULL;
  if (strcmp (name, "dev") == 0)
    {
      VFSInode *dev = kzalloc (sizeof (VFSInode));
      if (unlikely (dev == NULL))
	return NULL;
      dev->vi_ino = 1;
      dev->vi_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
      dev->vi_nlink = 2;
      dev->vi_ops = &vfs_default_iops;
      dev->vi_sb = sb;
      dev->vi_refcnt = 1;
      return dev;
    }
  return NULL;
}

static VFSDirEntry *
vfs_default_readdir (VFSDirectory *dir, VFSSuperblock *sb)
{
  if (dir->vd_inode->vi_ino == 0)
    {
      VFSDirEntry *entry;
      if (dir->vd_count > 0)
	return NULL;
      entry = kzalloc (sizeof (VFSDirEntry));
      entry->d_inode = vfs_default_lookup (dir->vd_inode, sb, "dev", 0);
      entry->d_name = strdup ("dev");
      return entry;
    }
  return NULL;
}

void
vfs_init (void)
{
  vfs_root_inode = kzalloc (sizeof (VFSInode));
  if (unlikely (vfs_root_inode == NULL))
    panic ("Failed to initialize VFS");
  vfs_root_inode->vi_nlink = 3;
  vfs_root_inode->vi_ops = &vfs_default_iops;
  vfs_root_inode->vi_sb = &vfs_default_sb;
  vfs_root_inode->vi_refcnt = 1;

  devfs_init ();
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
  vfs_unref_inode (entry->d_inode);
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
      mp->vfs_mntpoint = vfs_open_file (dir);
      if (mp->vfs_mntpoint == NULL)
	{
	  kfree (mp);
	  return -EPERM;
	}

      /* Find a slot in the mount table */
      for (j = 0; j < VFS_MOUNT_TABLE_SIZE; j++)
	{
	  if (mount_table[j].vfs_fstype == NULL)
	    break;
	}
      if (j == VFS_MOUNT_TABLE_SIZE)
	{
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
  VFSInode *inode;
  if (sb->sb_ops->sb_alloc_inode == NULL)
    return NULL;
  inode = sb->sb_ops->sb_alloc_inode (sb);
  if (inode == NULL)
    return NULL;
  inode->vi_refcnt = 1;
  return inode;
}

void
vfs_ref_inode (VFSInode *inode)
{
  if (inode != NULL)
    inode->vi_refcnt++;
}

void
vfs_unref_inode (VFSInode *inode)
{
  if (inode == NULL)
    return;
  if (--inode->vi_refcnt == 0 && inode->vi_sb->sb_ops->sb_destroy_inode != NULL)
    inode->vi_sb->sb_ops->sb_destroy_inode (inode);
}

void
vfs_fill_inode (VFSInode *inode)
{
  if (inode->vi_sb->sb_ops->sb_fill_inode != NULL)
    inode->vi_sb->sb_ops->sb_fill_inode (inode);
}

void
vfs_write_inode (VFSInode *inode)
{
  if (inode->vi_sb->sb_ops->sb_write_inode != NULL)
    inode->vi_sb->sb_ops->sb_write_inode (inode);
}

void
vfs_free_sb (VFSSuperblock *sb)
{
  if (sb->sb_ops->sb_free != NULL)
    sb->sb_ops->sb_free (sb);
}

void
vfs_update_sb (VFSSuperblock *sb)
{
  if (sb->sb_ops->sb_update != NULL)
    sb->sb_ops->sb_update (sb);
}

int
vfs_statvfs (VFSSuperblock *sb, struct statvfs *st)
{
  if (sb->sb_ops->sb_statvfs != NULL)
    return sb->sb_ops->sb_statvfs (sb, st);
  return -ENOSYS;
}

int
vfs_remount (VFSSuperblock *sb, int *flags, void *data)
{
  if (sb->sb_ops->sb_remount != NULL)
    return sb->sb_ops->sb_remount (sb, flags, data);
  return -ENOSYS;
}

int
vfs_create (VFSInode *dir, const char *name, mode_t mode)
{
  if (dir->vi_ops->vfs_create != NULL)
    return dir->vi_ops->vfs_create (dir, name, mode);
  return -ENOSYS;
}

VFSInode *
vfs_lookup (VFSInode *dir, VFSSuperblock *sb, const char *name,
	    int follow_symlinks)
{
  if (dir->vi_ops->vfs_lookup != NULL)
    return dir->vi_ops->vfs_lookup (dir, sb, name, follow_symlinks);
  return NULL;
}

int
vfs_link (VFSInode *old, VFSInode *dir, const char *new)
{
  if (dir->vi_ops->vfs_link != NULL)
    return dir->vi_ops->vfs_link (old, dir, new);
  return -ENOSYS;
}

int
vfs_unlink (VFSInode *dir, const char *name)
{
  if (dir->vi_ops->vfs_unlink != NULL)
    return dir->vi_ops->vfs_unlink (dir, name);
  return -ENOSYS;
}

int
vfs_symlink (VFSInode *dir, const char *old, const char *new)
{
  if (dir->vi_ops->vfs_symlink != NULL)
    return dir->vi_ops->vfs_symlink (dir, old, new);
  return -ENOSYS;
}

int
vfs_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  if (inode->vi_ops->vfs_read != NULL)
    return inode->vi_ops->vfs_read (inode, buffer, len, offset);
  return -ENOSYS;
}

int
vfs_write (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  if (inode->vi_ops->vfs_write != NULL)
    return inode->vi_ops->vfs_write (inode, buffer, len, offset);
  return -ENOSYS;
}

VFSDirEntry *
vfs_readdir (VFSDirectory *dir, VFSSuperblock *sb)
{
  if (dir->vd_inode->vi_ops->vfs_readdir != NULL)
    return dir->vd_inode->vi_ops->vfs_readdir (dir, sb);
  return NULL;
}

int
vfs_chmod (VFSInode *inode, mode_t mode)
{
  if (inode->vi_ops->vfs_chmod != NULL)
    return inode->vi_ops->vfs_chmod (inode, mode);
  return -ENOSYS;
}

int
vfs_chown (VFSInode *inode, uid_t uid, gid_t gid)
{
  if (inode->vi_ops->vfs_chown != NULL)
    return inode->vi_ops->vfs_chown (inode, uid, gid);
  return -ENOSYS;
}

int
vfs_mkdir (VFSInode *dir, const char *name, mode_t mode)
{
  if (dir->vi_ops->vfs_mkdir != NULL)
    return dir->vi_ops->vfs_mkdir (dir, name, mode);
  return -ENOSYS;
}

int
vfs_rmdir (VFSInode *dir, const char *name)
{
  if (dir->vi_ops->vfs_rmdir != NULL)
    return dir->vi_ops->vfs_rmdir (dir, name);
  return -ENOSYS;
}

int
vfs_mknod (VFSInode *dir, const char *name, mode_t mode, dev_t rdev)
{
  if (dir->vi_ops->vfs_mknod != NULL)
    return dir->vi_ops->vfs_mknod (dir, name, mode, rdev);
  return -ENOSYS;
}

int
vfs_rename (VFSInode *olddir, const char *oldname, VFSInode *newdir,
	    const char *newname)
{
  /* TODO Support cross-filesystem renaming */
  if (olddir->vi_ops->vfs_rename != NULL)
    return olddir->vi_ops->vfs_rename (olddir, oldname, newdir, newname);
  return -ENOSYS;
}

int
vfs_readlink (VFSInode *inode, char *buffer, size_t len)
{
  if (inode->vi_ops->vfs_readlink != NULL)
    return inode->vi_ops->vfs_readlink (inode, buffer, len);
  return -ENOSYS;
}

int
vfs_truncate (VFSInode *inode)
{
  if (inode->vi_ops->vfs_truncate != NULL)
    return inode->vi_ops->vfs_truncate (inode);
  return -ENOSYS;
}

int
vfs_getattr (VFSInode *inode, struct stat *st)
{
  if (inode->vi_ops->vfs_getattr != NULL)
    return inode->vi_ops->vfs_getattr (inode, st);
  return -ENOSYS;
}

int
vfs_setxattr (VFSInode *inode, const char *name, const void *value,
	      size_t len, int flags)
{
  if (inode->vi_ops->vfs_setxattr != NULL)
    return inode->vi_ops->vfs_setxattr (inode, name, value, len, flags);
  return -ENOSYS;
}

int
vfs_getxattr (VFSInode *inode, const char *name, void *buffer, size_t len)
{
  if (inode->vi_ops->vfs_getxattr != NULL)
    return inode->vi_ops->vfs_getxattr (inode, name, buffer, len);
  return -ENOSYS;
}

int
vfs_listxattr (VFSInode *inode, char *buffer, size_t len)
{
  if (inode->vi_ops->vfs_listxattr != NULL)
    return inode->vi_ops->vfs_listxattr (inode, buffer, len);
  return -ENOSYS;
}

int
vfs_removexattr (VFSInode *inode, const char *name)
{
  if (inode->vi_ops->vfs_removexattr != NULL)
    return inode->vi_ops->vfs_removexattr (inode, name);
  return -ENOSYS;
}

int
vfs_compare_dir_entry (VFSDirEntry *entry, const char *a, const char *b)
{
  if (entry->d_ops->d_compare != NULL)
    return entry->d_ops->d_compare (entry, a, b);
  return -ENOSYS;
}

void
vfs_iput_dir_entry (VFSDirEntry *entry, VFSInode *inode)
{
  if (entry->d_ops->d_iput != NULL)
    entry->d_ops->d_iput (entry, inode);
}

VFSInode *
vfs_open_file (const char *path)
{
  VFSInode *dir;
  char *buffer = strdup (path);
  char *ptr;
  int dont_unref = 0;
  if (unlikely (buffer == NULL))
    return NULL;

  if (*path == '/')
    {
      dir = vfs_root_inode;
      ptr = buffer + 1;
    }
  else
    {
      dir = process_table[task_getpid ()].p_cwd;
      ptr = buffer;
    }
  vfs_ref_inode (dir);

  while (*ptr != '\0')
    {
      char *end = strchr (ptr, '/');
      if (end != NULL)
        *end = '\0';

      if (*ptr != '\0' && strcmp (ptr, ".") != 0)
	{
	  VFSInode *inode = vfs_lookup (dir, dir->vi_sb, ptr, 1);
	  int i;
	  if (inode == NULL)
	    {
	      vfs_unref_inode (dir);
	      kfree (buffer);
	      return NULL;
	    }
	  vfs_unref_inode (dir);
	  dir = inode;
	  dont_unref = 1;

	  /* Check if the inode the root of a mounted filesystem and replace
	     it with the root inode of the mount if it is */
	  for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++)
	    {
	      VFSInode *mpt = mount_table[i].vfs_mntpoint;
	      if (mpt == NULL)
		continue;
	      if (dir->vi_ino == mpt->vi_ino && dir->vi_sb == mpt->vi_sb)
		{
		  vfs_unref_inode (dir);
		  dir = mount_table[i].vfs_sb.sb_root;
		  vfs_ref_inode (dir);
		  break;
		}
	    }
	}

      if (end == NULL)
	break;
      ptr = end + 1;
    }

  kfree (buffer);
  if (!dont_unref)
    vfs_unref_inode (dir); /* Decrease refcount since dir never was changed */
  return dir;
}
