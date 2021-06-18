/*************************************************************************
 * perm.c -- This file is part of OS/0.                                  *
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
vfs_perm_check_read (VFSInode *inode, int real)
{
  uid_t uid = real ? sys_getuid () : sys_geteuid ();
  if (uid == 0)
    return 0; /* Super-user has all permissions */
  if (inode->vi_uid == uid)
    {
      if (!(inode->vi_mode & S_IRUSR))
	return -EACCES;
      if (S_ISDIR (inode->vi_mode) && !(inode->vi_mode & S_IXUSR))
	return -EACCES;
    }
  else if (inode->vi_gid == (real ? sys_getgid () : sys_getegid ()))
    {
      if (!(inode->vi_mode & S_IRGRP))
	return -EACCES;
      if (S_ISDIR (inode->vi_mode) && !(inode->vi_mode & S_IXGRP))
	return -EACCES;
    }
  else if (!(inode->vi_mode & S_IROTH))
    return -EACCES;
  else if (S_ISDIR (inode->vi_mode) && !(inode->vi_mode & S_IXOTH))
    return -EACCES;
  return 0;
}

int
vfs_perm_check_write (VFSInode *inode, int real)
{
  uid_t uid = real ? sys_getuid () : sys_geteuid ();
  if (uid == 0)
    return 0; /* Super-user has all permissions */
  if (inode->vi_uid == uid)
    {
      if (!(inode->vi_mode & S_IWUSR))
	return -EACCES;
      if (S_ISDIR (inode->vi_mode) && !(inode->vi_mode & S_IXUSR))
	return -EACCES;
    }
  else if (inode->vi_gid == (real ? sys_getgid () : sys_getegid ()))
    {
      if (!(inode->vi_mode & S_IWGRP))
	return -EACCES;
      if (S_ISDIR (inode->vi_mode) && !(inode->vi_mode & S_IXGRP))
	return -EACCES;
    }
  else if (!(inode->vi_mode & S_IWOTH))
    return -EACCES;
  else if (S_ISDIR (inode->vi_mode) && !(inode->vi_mode & S_IXOTH))
    return -EACCES;
  return 0;
}

int
vfs_perm_check_exec (VFSInode *inode, int real)
{
  uid_t uid = real ? sys_getuid () : sys_geteuid ();
  if (uid == 0)
    return inode->vi_mode & (S_IXUSR | S_IXGRP | S_IXOTH) ? 0 : -EACCES;
  if (inode->vi_uid == uid)
    {
      if (!(inode->vi_mode & S_IXUSR))
	return -EACCES;
    }
  else if (inode->vi_gid == (real ? sys_getgid () : sys_getegid ()))
    {
      if (!(inode->vi_mode & S_IXGRP))
	return -EACCES;
    }
  else if (!(inode->vi_mode & S_IXOTH))
    return -EACCES;
  return 0;
}
