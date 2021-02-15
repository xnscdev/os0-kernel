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
#include <vm/heap.h>
#include <errno.h>
#include <string.h>

static int
sys_path_sep (const char *path, VFSDirEntry *entry, char **name)
{
  VFSPath *vpath;
  VFSPath *rvpath;
  VFSPath *temp;
  VFSMount *mp;
  int ret = vfs_namei (&vpath, path);
  if (ret != 0)
    return ret;

  ret = vfs_path_find_mount (vpath);
  if (ret < 0)
    {
      vfs_path_free (vpath);
      return ret;
    }

  mp = &mount_table[ret];
  ret = vfs_path_rel (&rvpath, vpath, mp);
  if (ret != 0)
    {
      vfs_path_free (vpath);
      return ret;
    }
  temp = rvpath;
  if (rvpath != NULL)
    {
      rvpath = rvpath->vp_prev;
      rvpath->vp_next = NULL;
    }

  ret = vfs_lookup (entry, &mp->vfs_sb, rvpath);
  if (rvpath != NULL)
    vfs_path_free (rvpath);
  if (ret == 0)
    *name = strdup (temp->vp_long == NULL ? temp->vp_short : temp->vp_long);
  else
    *name = NULL;
  vfs_path_free (vpath);
  return ret;
}

int
sys_creat (const char *path, mode_t mode)
{
  VFSDirEntry entry;
  char *name;
  int ret = sys_path_sep (path, &entry, &name);
  if (ret != 0)
    return ret;
  ret = vfs_create (entry.d_inode, name, mode);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  kfree (name);
  return ret;
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
