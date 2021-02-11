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
#include <errno.h>

int
ext2_create (VFSInode *dir, VFSDirEntry *entry, mode_t mode)
{
  return -ENOSYS;
}

int
ext2_lookup (VFSDirEntry *entry, VFSSuperblock *sb, VFSPath *path)
{
  return -ENOSYS;
}

int
ext2_link (VFSDirEntry *old, VFSInode *dir, VFSDirEntry *new)
{
  return -ENOSYS;
}

int
ext2_unlink (VFSInode *dir, VFSDirEntry *entry)
{
  return -ENOSYS;
}

int
ext2_symlink (VFSInode *dir, VFSDirEntry *entry, const char *name)
{
  return -ENOSYS;
}

VFSDirEntry *
ext2_readdir (VFSSuperblock *sb, VFSInode *dir)
{
  return NULL;
}

int
ext2_mkdir (VFSInode *dir, VFSDirEntry *entry, mode_t mode)
{
  return -ENOSYS;
}

int
ext2_rmdir (VFSInode *dir, VFSDirEntry *entry)
{
  return -ENOSYS;
}

int
ext2_mknod (VFSInode *dir, VFSDirEntry *entry, mode_t mode, dev_t rdev)
{
  return -ENOSYS;
}

int
ext2_rename (VFSInode *olddir, VFSDirEntry *oldentry, VFSInode *newdir,
	     VFSDirEntry *newentry)
{
  return -ENOSYS;
}

int
ext2_readlink (VFSDirEntry *entry, char *buffer, size_t len)
{
  return -ENOSYS;
}

int
ext2_truncate (VFSInode *inode)
{
  return -ENOSYS;
}

int
ext2_permission (VFSInode *inode, mode_t mask)
{
  return -ENOSYS;
}

int
ext2_getattr (VFSMount *mp, VFSDirEntry *entry, struct stat *st)
{
  return -ENOSYS;
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
