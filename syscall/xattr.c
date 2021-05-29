/*************************************************************************
 * xattr.c -- This file is part of OS/0.                                 *
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

#include <libk/libk.h>
#include <sys/process.h>
#include <sys/syscall.h>

static int
__sys_xsetxattr (const char *path, const char *name, const void *value,
		 size_t len, int flags, int follow_symlinks)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, follow_symlinks);
  if (ret != 0)
    return ret;
  ret = vfs_setxattr (inode, name, value, len, flags);
  vfs_unref_inode (inode);
  return ret;
}

static int
__sys_xgetxattr (const char *path, const char *name, void *value, size_t len,
		 int follow_symlinks)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, follow_symlinks);
  if (ret != 0)
    return ret;
  ret = vfs_getxattr (inode, name, value, len);
  vfs_unref_inode (inode);
  return ret;
}

static int
__sys_xlistxattr (const char *path, char *buffer, size_t len,
		  int follow_symlinks)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, follow_symlinks);
  if (ret != 0)
    return ret;
  ret = vfs_listxattr (inode, buffer, len);
  vfs_unref_inode (inode);
  return ret;
}

static int
__sys_xremovexattr (const char *path, const char *name, int follow_symlinks)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, follow_symlinks);
  if (ret != 0)
    return ret;
  ret = vfs_removexattr (inode, name);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_setxattr (const char *path, const char *name, const void *value, size_t len,
	      int flags)
{
  return __sys_xsetxattr (path, name, value, len, flags, 1);
}

int
sys_lsetxattr (const char *path, const char *name, const void *value,
	       size_t len, int flags)
{
  return __sys_xsetxattr (path, name, value, len, flags, 0);
}

int
sys_fsetxattr (int fd, const char *name, const void *value, size_t len,
	       int flags)
{
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  return vfs_setxattr (inode, name, value, len, flags);
}

int
sys_getxattr (const char *path, const char *name, void *value, size_t len)
{
  return __sys_xgetxattr (path, name, value, len, 1);
}

int
sys_lgetxattr (const char *path, const char *name, void *value, size_t len)
{
  return __sys_xgetxattr (path, name, value, len, 0);
}

int
sys_fgetxattr (int fd, const char *name, void *value, size_t len)
{
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  return vfs_getxattr (inode, name, value, len);
}

int
sys_listxattr (const char *path, char *buffer, size_t len)
{
  return __sys_xlistxattr (path, buffer, len, 1);
}

int
sys_llistxattr (const char *path, char *buffer, size_t len)
{
  return __sys_xlistxattr (path, buffer, len, 0);
}

int
sys_flistxattr (int fd, char *buffer, size_t len)
{
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  return vfs_listxattr (inode, buffer, len);
}

int
sys_removexattr (const char *path, const char *name)
{
  return __sys_xremovexattr (path, name, 1);
}

int
sys_lremovexattr (const char *path, const char *name)
{
  return __sys_xremovexattr (path, name, 0);
}

int
sys_fremovexattr (int fd, const char *name)
{
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  return vfs_removexattr (inode, name);
}
