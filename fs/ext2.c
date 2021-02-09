/*************************************************************************
 * ext2.c -- This file is part of OS/0.                                  *
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
#include <libk/stdlib.h>
#include <errno.h>

static int ext2_mount (VFSMount *mp, const char *path, uint32_t flags,
		       void *data);
static int ext2_unmount (VFSMount *mp, uint32_t flags);
static int ext2_statfs (VFSMount *mp, struct statfs *st);
static int ext2_create (VFSInode *dir, VFSDirEntry *entry, mode_t mode);
static VFSDirEntry *ext2_lookup (VFSInode *dir, VFSDirEntry *entry);
static int ext2_link (VFSDirEntry *old, VFSInode *dir, VFSDirEntry *new);
static int ext2_unlink (VFSInode *dir, VFSDirEntry *entry);
static int ext2_symlink (VFSInode *dir, VFSDirEntry *entry, const char *name);
static int ext2_mkdir (VFSInode *dir, VFSDirEntry *entry, mode_t mode);
static int ext2_rmdir (VFSInode *dir, VFSDirEntry *entry);
static int ext2_mknod (VFSInode *dir, VFSDirEntry *entry, mode_t mode,
		       dev_t rdev);
static int ext2_rename (VFSInode *olddir, VFSDirEntry *oldentry,
			VFSInode *newdir, VFSDirEntry *newentry);
static int ext2_readlink (VFSDirEntry *entry, char *buffer, size_t len);
static void ext2_truncate (VFSInode *inode);
static int ext2_permission (VFSInode *inode, mode_t mask);
static int ext2_getattr (VFSMount *mp, VFSDirEntry *entry, struct stat *st);
static int ext2_setxattr (VFSDirEntry *entry, const char *name,
			  const void *value, size_t len, int flags);
static int ext2_getxattr (VFSDirEntry *entry, const char *name, void *buffer,
			  size_t len);
static int ext2_listxattr (VFSDirEntry *entry, char *buffer, size_t len);
static int ext2_removexattr (VFSDirEntry *entry, const char *name);
static int ext2_compare (VFSDirEntry *entry, const char *a, const char *b);
static void ext2_iput (VFSDirEntry *entry, VFSInode *inode);

static const VFSMountOps ext2_mops = {
  .vfs_mount = ext2_mount,
  .vfs_unmount = ext2_unmount,
  .vfs_statfs = ext2_statfs
};

static const VFSInodeOps ext2_iops = {
  .vfs_create = ext2_create,
  .vfs_lookup = ext2_lookup,
  .vfs_link = ext2_link,
  .vfs_unlink = ext2_unlink,
  .vfs_symlink = ext2_symlink,
  .vfs_mkdir = ext2_mkdir,
  .vfs_rmdir = ext2_rmdir,
  .vfs_mknod = ext2_mknod,
  .vfs_rename = ext2_rename,
  .vfs_readlink = ext2_readlink,
  .vfs_truncate = ext2_truncate,
  .vfs_permission = ext2_permission,
  .vfs_getattr = ext2_getattr,
  .vfs_setxattr = ext2_setxattr,
  .vfs_getxattr = ext2_getxattr,
  .vfs_listxattr = ext2_listxattr,
  .vfs_removexattr = ext2_removexattr
};

static const VFSDirEntryOps ext2_dops = {
  .d_compare = ext2_compare,
  .d_iput = ext2_iput
};

static const VFSFilesystem ext2_vfs = {
  .vfs_name = "ext2",
  .vfs_flags = 0,
  .vfs_init = NULL,
  .vfs_destroy = NULL,
  .vfs_mops = &ext2_mops,
  .vfs_iops = &ext2_iops,
  .vfs_dops = &ext2_dops
};

void
ext2_init (void)
{
  if (fs_register (&ext2_vfs) != 0)
    panic ("Failed to register " EXT2_FS_NAME " filesystem");
}
