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

#include <bits/mount.h>
#include <fs/devfs.h>
#include <fs/ext2.h>
#include <fs/vfs.h>
#include <libk/libk.h>
#include <sys/param.h>
#include <sys/process.h>
#include <sys/syscall.h>
#include <vm/heap.h>

VFSFilesystem fs_table[VFS_FS_TABLE_SIZE];
VFSMount mount_table[VFS_MOUNT_TABLE_SIZE];
VFSInode *vfs_root_inode;

static int vfs_default_lookup (VFSInode **inode, VFSInode *dir,
			       VFSSuperblock *sb, const char *name,
			       int symcount);
static int vfs_default_readdir (VFSInode *inode, VFSDirEntryFillFunc func,
				void *private);

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

static int
vfs_default_lookup (VFSInode **inode, VFSInode *dir, VFSSuperblock *sb,
		    const char *name, int symcount)
{
  /* Must be root directory */
  if (dir->vi_ino != 0)
    return -ENOENT;
  if (strcmp (name, "dev") == 0)
    {
      VFSInode *dev = kzalloc (sizeof (VFSInode));
      if (unlikely (dev == NULL))
	return -ENOMEM;
      dev->vi_ino = 1;
      dev->vi_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
      dev->vi_nlink = 2;
      dev->vi_ops = &vfs_default_iops;
      dev->vi_sb = sb;
      dev->vi_refcnt = 1;
      *inode = dev;
      return 0;
    }
  return -ENOENT;
}

static int
vfs_default_readdir (VFSInode *inode, VFSDirEntryFillFunc func, void *private)
{
  return func ("dev", 3, 1, DT_DIR, 0, private);
}

void
vfs_init (void)
{
  vfs_root_inode = kzalloc (sizeof (VFSInode));
  if (unlikely (vfs_root_inode == NULL))
    panic ("Failed to initialize VFS");
  vfs_root_inode->vi_mode =
    S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
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

int
vfs_mount (const char *type, const char *dir, int flags, void *data)
{
  int i;
  if (type == NULL || dir == NULL)
    return -EINVAL;
  for (i = 0; i < VFS_FS_TABLE_SIZE; i++)
    {
      int ret;
      int j;
      if (strcmp (fs_table[i].vfs_name, type) != 0)
        continue;

      /* Find a slot in the mount table */
      for (j = 0; j < VFS_MOUNT_TABLE_SIZE; j++)
	{
	  if (mount_table[j].vfs_fstype == NULL)
	    break;
	}
      if (j == VFS_MOUNT_TABLE_SIZE)
	return -ENOSPC;
      mount_table[j].vfs_fstype = &fs_table[i];
      mount_table[j].vfs_sb.sb_mntflags = flags;
      mount_table[j].vfs_sb.sb_mntslot = j;
      mount_table[j].vfs_sb.sb_fstype = &fs_table[i];

      /* Filesystem-specific mount */
      ret = fs_table[i].vfs_mount (&mount_table[j], flags, data);
      if (ret != 0)
	goto err;

      /* Get mount point as a path and set parent mount */
      ret = vfs_open_file (&mount_table[j].vfs_mntpoint, dir, 1);
      if (ret != 0)
	goto err;
      mount_table[j].vfs_mntpath = vfs_path_resolve (dir);
      if (unlikely (mount_table[j].vfs_mntpath == NULL))
	goto err;

      /* Previously mounted filesystems will have a mount point pointing to
	 an inode on the parent filesystem of the new mount, we need to update
	 them to point to the corresponding directories on the new filesystem */
      for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++)
	{
	  if (i == j || mount_table[i].vfs_fstype == NULL
	      || mount_table[i].vfs_mntpoint->vi_sb !=
	      mount_table[j].vfs_mntpoint->vi_sb)
	    continue;
	  ret = vfs_open_file (&mount_table[i].vfs_mntpoint,
			       mount_table[i].vfs_mntpath, 1);
	  if (ret != 0)
	    goto err;
	}
      return 0;

    err:
      kfree (mount_table[j].vfs_mntpath);
      memset (&mount_table[j], 0, sizeof (VFSMount));
      return ret;
    }
  return -EINVAL; /* No such filesystem type */
}

int
vfs_unmount (VFSSuperblock *sb, int flags)
{
  int ret;
  int slot;
  int i;

  /* Check if any file descriptors using the filesystem are still active */
  for (i = 0; i < PROCESS_SYS_FILE_LIMIT; i++)
    {
      if (process_fd_table[i].pf_inode != NULL
	  && process_fd_table[i].pf_inode->vi_sb == sb)
	return -EBUSY;
    }

  /* Run file-specific unmount function */
  ret = sb->sb_fstype->vfs_unmount (&mount_table[sb->sb_mntslot], flags);
  if (ret != 0)
    return ret;
  if (sb->sb_ops->sb_free != NULL)
    sb->sb_ops->sb_free (sb);

  /* Fix mount points of mounted filesystems under the one being unmounted */
  slot = sb->sb_mntslot;
  memset (&mount_table[sb->sb_mntslot], 0, sizeof (VFSMount));
  for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++)
    {
      if (i == slot || mount_table[i].vfs_fstype == NULL
	  || mount_table[i].vfs_mntpoint->vi_sb != sb)
	continue;
      ret = vfs_open_file (&mount_table[i].vfs_mntpoint,
			   mount_table[i].vfs_mntpath, 1);
      if (ret != 0)
	return ret;
    }
  return 0;
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
  inode->vi_sb = sb;
  inode->vi_flags = 0;
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

int
vfs_fill_inode (VFSInode *inode)
{
  if (inode->vi_sb->sb_ops->sb_fill_inode != NULL)
    return inode->vi_sb->sb_ops->sb_fill_inode (inode);
  return -ENOSYS;
}

int
vfs_write_inode (VFSInode *inode)
{
  if (inode->vi_sb->sb_ops->sb_write_inode != NULL)
    return inode->vi_sb->sb_ops->sb_write_inode (inode);
  return -ENOSYS;
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
vfs_statfs (VFSSuperblock *sb, struct statfs64 *st)
{
  if (sb->sb_ops->sb_statfs != NULL)
    return sb->sb_ops->sb_statfs (sb, st);
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
  int ret;
  if (dir->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  ret = vfs_perm_check_write (dir, 0);
  if (ret != 0)
    return ret;
  if (dir->vi_ops->vfs_create != NULL)
    return dir->vi_ops->vfs_create (dir, name, mode);
  return -ENOSYS;
}

int
vfs_lookup (VFSInode **inode, VFSInode *dir, VFSSuperblock *sb,
	    const char *name, int symcount)
{
  int ret;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  if (symcount >= MAXSYMLINKS)
    return -ELOOP;
  ret = vfs_perm_check_read (dir, 0);
  if (ret != 0)
    return ret;
  if (dir->vi_ops->vfs_lookup != NULL)
    return dir->vi_ops->vfs_lookup (inode, dir, sb, name, symcount);
  return -ENOSYS;
}

int
vfs_link (VFSInode *old, VFSInode *dir, const char *new)
{
  int ret;
  if (dir->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  ret = vfs_perm_check_write (dir, 0);
  if (ret != 0)
    return ret;
  if (dir->vi_ops->vfs_link != NULL)
    return dir->vi_ops->vfs_link (old, dir, new);
  return -ENOSYS;
}

int
vfs_unlink (VFSInode *dir, const char *name)
{
  int ret;
  if (dir->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  ret = vfs_perm_check_write (dir, 0);
  if (ret != 0)
    return ret;
  if (dir->vi_ops->vfs_unlink != NULL)
    return dir->vi_ops->vfs_unlink (dir, name);
  return -ENOSYS;
}

int
vfs_symlink (VFSInode *dir, const char *old, const char *new)
{
  int ret;
  if (dir->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  ret = vfs_perm_check_write (dir, 0);
  if (ret != 0)
    return ret;
  if (dir->vi_ops->vfs_symlink != NULL)
    return dir->vi_ops->vfs_symlink (dir, old, new);
  return -ENOSYS;
}

int
vfs_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  int ret = vfs_perm_check_read (inode, 0);
  if (ret != 0)
    return ret;
  if (vfs_can_seek (inode))
    {
      if (offset > inode->vi_size)
	return -EINVAL;
      else if (offset + len > inode->vi_size)
	len = inode->vi_size - offset;
    }
  if (inode->vi_ops->vfs_read != NULL)
    return inode->vi_ops->vfs_read (inode, buffer, len, offset);
  return -ENOSYS;
}

int
vfs_write (VFSInode *inode, const void *buffer, size_t len, off_t offset)
{
  int ret;
  if (inode->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  ret = vfs_perm_check_write (inode, 0);
  if (ret != 0)
    return ret;
  if (inode->vi_ops->vfs_write != NULL)
    return inode->vi_ops->vfs_write (inode, buffer, len, offset);
  return -ENOSYS;
}

int
vfs_readdir (VFSInode *inode, VFSDirEntryFillFunc func, void *private)
{
  int ret = vfs_perm_check_read (inode, 0);
  if (ret != 0)
    return ret;
  if (!S_ISDIR (inode->vi_mode))
    return -ENOTDIR;
  if (inode->vi_ops->vfs_readdir != NULL)
    return inode->vi_ops->vfs_readdir (inode, func, private);
  return -ENOSYS;
}

int
vfs_chmod (VFSInode *inode, mode_t mode)
{
  uid_t euid;
  if (inode->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  euid = sys_geteuid ();
  if (euid != 0 && euid != inode->vi_uid)
    return -EPERM;
  if (inode->vi_ops->vfs_chmod != NULL)
    return inode->vi_ops->vfs_chmod (inode, mode);
  return -ENOSYS;
}

int
vfs_chown (VFSInode *inode, uid_t uid, gid_t gid)
{
  uid_t euid;
  if (inode->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  euid = sys_geteuid ();
  if (euid != 0 && euid != inode->vi_uid)
    return -EPERM;
  if (inode->vi_ops->vfs_chown != NULL)
    return inode->vi_ops->vfs_chown (inode, uid, gid);
  return -ENOSYS;
}

int
vfs_mkdir (VFSInode *dir, const char *name, mode_t mode)
{
  int ret;
  if (dir->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  ret = vfs_perm_check_write (dir, 0);
  if (ret != 0)
    return ret;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  if (dir->vi_ops->vfs_mkdir != NULL)
    return dir->vi_ops->vfs_mkdir (dir, name, mode);
  return -ENOSYS;
}

int
vfs_rmdir (VFSInode *dir, const char *name)
{
  int ret;
  if (dir->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  ret = vfs_perm_check_write (dir, 0);
  if (ret != 0)
    return ret;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  if (dir->vi_ops->vfs_rmdir != NULL)
    return dir->vi_ops->vfs_rmdir (dir, name);
  return -ENOSYS;
}

int
vfs_mknod (VFSInode *dir, const char *name, mode_t mode, dev_t rdev)
{
  int ret;
  if (dir->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  ret = vfs_perm_check_write (dir, 0);
  if (ret != 0)
    return ret;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  if (dir->vi_ops->vfs_mknod != NULL)
    return dir->vi_ops->vfs_mknod (dir, name, mode, rdev);
  return -ENOSYS;
}

int
vfs_rename (VFSInode *olddir, const char *oldname, VFSInode *newdir,
	    const char *newname)
{
  int ret;
  if (olddir->vi_sb->sb_mntflags & MS_RDONLY
      || newdir->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  ret = vfs_perm_check_write (olddir, 0);
  if (ret != 0)
    return ret;
  ret = vfs_perm_check_write (newdir, 0);
  if (ret != 0)
    return ret;
  if (!S_ISDIR (olddir->vi_mode))
    return -ENOTDIR;
  if (!S_ISDIR (newdir->vi_mode))
    return -ENOTDIR;
  if (olddir->vi_sb != newdir->vi_sb)
    return -EXDEV;
  if (newdir->vi_ops->vfs_rename != NULL)
    return newdir->vi_ops->vfs_rename (olddir, oldname, newdir, newname);
  return -ENOSYS;
}

int
vfs_readlink (VFSInode *inode, char *buffer, size_t len)
{
  int ret = vfs_perm_check_read (inode, 0);
  if (ret != 0)
    return ret;
  if (!S_ISLNK (inode->vi_mode))
    return -EINVAL;
  if (inode->vi_ops->vfs_readlink != NULL)
    return inode->vi_ops->vfs_readlink (inode, buffer, len);
  return -ENOSYS;
}

int
vfs_truncate (VFSInode *inode)
{
  int ret;
  if (inode->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  ret = vfs_perm_check_write (inode, 0);
  if (ret != 0)
    return ret;
  if (inode->vi_ops->vfs_truncate != NULL)
    return inode->vi_ops->vfs_truncate (inode);
  return -ENOSYS;
}

int
vfs_getattr (VFSInode *inode, struct stat64 *st)
{
  if (inode->vi_ops->vfs_getattr != NULL)
    {
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
      st->st_blocks = inode->vi_blocks;
      return inode->vi_ops->vfs_getattr (inode, st);
    }
  return -ENOSYS;
}

int
vfs_setxattr (VFSInode *inode, const char *name, const void *value,
	      size_t len, int flags)
{
  int ret;
  if (inode->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  ret = vfs_perm_check_write (inode, 0);
  if (ret != 0)
    return ret;
  if (inode->vi_ops->vfs_setxattr != NULL)
    return inode->vi_ops->vfs_setxattr (inode, name, value, len, flags);
  return -ENOSYS;
}

int
vfs_getxattr (VFSInode *inode, const char *name, void *buffer, size_t len)
{
  int ret = vfs_perm_check_read (inode, 0);
  if (ret != 0)
    return ret;
  if (inode->vi_ops->vfs_getxattr != NULL)
    return inode->vi_ops->vfs_getxattr (inode, name, buffer, len);
  return -ENOSYS;
}

int
vfs_listxattr (VFSInode *inode, char *buffer, size_t len)
{
  int ret = vfs_perm_check_read (inode, 0);
  if (ret != 0)
    return ret;
  if (inode->vi_ops->vfs_listxattr != NULL)
    return inode->vi_ops->vfs_listxattr (inode, buffer, len);
  return -ENOSYS;
}

int
vfs_removexattr (VFSInode *inode, const char *name)
{
  int ret;
  if (inode->vi_sb->sb_mntflags & MS_RDONLY)
    return -EROFS;
  ret = vfs_perm_check_write (inode, 0);
  if (ret != 0)
    return ret;
  if (inode->vi_ops->vfs_removexattr != NULL)
    return inode->vi_ops->vfs_removexattr (inode, name);
  return -ENOSYS;
}

void
vfs_unmount_all (void)
{
  int i;
  for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++)
    {
      if (mount_table[i].vfs_fstype != NULL)
	vfs_unmount (&mount_table[i].vfs_sb, 0);
    }
}
