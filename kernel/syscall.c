/*************************************************************************
 * syscall.c -- This file is part of OS/0.                               *
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

#include <fs/vfs.h>
#include <sys/syscall.h>
#include <errno.h>

int
sys_creat (const char *path, mode_t mode)
{
  return -ENOSYS;
}

int
sys_link (const char *old, const char *new)
{
  return -ENOSYS;
}

int
sys_unlink (const char *path)
{
  return -ENOSYS;
}

int
sys_mknod (const char *path, mode_t mode, dev_t dev)
{
  return -ENOSYS;
}

int
sys_chmod (const char *path, mode_t mode)
{
  return -ENOSYS;
}

int
sys_chown (const char *path, uid_t uid, gid_t gid)
{
  return -ENOSYS;
}

int
sys_mount (const char *src, const char *dir, const char *type, int flags,
	   void *data)
{
  return -ENOSYS;
}

int
sys_umount (const char *dir)
{
  return -ENOSYS;
}

int
sys_rename (const char *old, const char *new)
{
  return -ENOSYS;
}

int
sys_mkdir (const char *path, mode_t mode)
{
  return -ENOSYS;
}

int
sys_rmdir (const char *path)
{
  return -ENOSYS;
}

int
sys_symlink (const char *old, const char *new)
{
  return -ENOSYS;
}

int
sys_readlink (const char *path, char *buffer, size_t len)
{
  return -ENOSYS;
}

int
sys_truncate (const char *path, off_t len)
{
  return -ENOSYS;
}

int
sys_statvfs (const char *path, struct statvfs *st)
{
  return -ENOSYS;
}

int
sys_stat (const char *path, struct stat *st)
{
  return -ENOSYS;
}

int
sys_setxattr (const char *path, const char *name, const void *value, size_t len,
	      int flags)
{
  return -ENOSYS;
}

int
sys_getxattr (const char *path, const char *name, void *value, size_t len)
{
  return -ENOSYS;
}

int
sys_listxattr (const char *path, char *buffer, size_t len)
{
  return -ENOSYS;
}

int
sys_removexattr (const char *path, const char *name)
{
  return -ENOSYS;
}
