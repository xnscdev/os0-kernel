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

#include <fs/ext2.h>
#include <libk/libk.h>
#include <sys/process.h>
#include <sys/syscall.h>
#include <vm/heap.h>

void *syscall_table[256];

static int
sys_path_rel_lookup (const char *path, VFSDirEntry *entry)
{
  VFSPath *vpath;
  VFSPath *rvpath;
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

  ret = vfs_lookup (entry, &mp->vfs_sb, rvpath);
  vfs_path_free (vpath);
  return ret;
}

/* Separates a path into a file and its parent directory */

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

  /* Get the relative filesystem path */
  mp = &mount_table[ret];
  ret = vfs_path_rel (&rvpath, vpath, mp);
  if (ret != 0)
    {
      vfs_path_free (vpath);
      return ret;
    }
  if (rvpath != NULL)
    {
      /* Point to the parent directory instead of the file */
      temp = rvpath;
      rvpath = rvpath->vp_prev;
      rvpath->vp_next = NULL;
    }
  else
    temp = vpath;

  ret = vfs_lookup (entry, &mp->vfs_sb, rvpath);
  if (rvpath != NULL)
    vfs_path_free (rvpath);
  if (ret != 0)
    *name = NULL;
  else
    *name = strdup (temp->vp_long == NULL ? temp->vp_short : temp->vp_long);
  vfs_path_free (vpath);
  return ret;
}

int
sys_fork (void)
{
  return task_fork ();
}

int
sys_read (int fd, void *buffer, size_t len)
{
  ProcessFile *file;
  int ret;
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT)
    return -EBADF;
  file = &process_table[task_getpid ()].p_files[fd];
  if (file->pf_inode == NULL || (file->pf_mode & O_ACCMODE) == O_WRONLY)
    return -EBADF;
  ret = vfs_read (file->pf_inode, buffer, len, file->pf_offset);
  if (ret != 0)
    return ret;
  file->pf_offset += len;
  return 0;
}

int
sys_write (int fd, void *buffer, size_t len)
{
  ProcessFile *file;
  int ret;
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT)
    return -EBADF;
  file = &process_table[task_getpid ()].p_files[fd];
  if (file->pf_inode == NULL || (file->pf_mode & O_ACCMODE) == O_RDONLY)
    return -EBADF;
  ret = vfs_write (file->pf_inode, buffer, len, file->pf_offset);
  if (ret != 0)
    return ret;
  file->pf_offset += len;
  return 0;
}

int
sys_open (const char *path, int flags, mode_t mode)
{
  ProcessFile *files = process_table[task_getpid ()].p_files;
  int i;
  for (i = 0; i < PROCESS_FILE_LIMIT; i++)
    {
      if (files[i].pf_inode == NULL)
	{
	  VFSDirEntry entry;
	  int ret = sys_path_rel_lookup (path, &entry);
	  if (ret != 0)
	    return ret; /* TODO Create file if O_CREAT in flags */
	  kfree (entry.d_name);
	  files[i].pf_inode = entry.d_inode;
	  switch (flags & O_ACCMODE)
	    {
	    case O_RDONLY:
	      files[i].pf_mode = O_RDONLY;
	      break;
	    case O_WRONLY:
	      if (S_ISDIR (files[i].pf_inode->vi_mode))
		{
		  vfs_destroy_inode (files[i].pf_inode);
		  files[i].pf_inode = NULL;
		  return -EISDIR;
		}
	      files[i].pf_mode = O_WRONLY;
	      break;
	    case O_RDWR:
	      if (S_ISDIR (files[i].pf_inode->vi_mode))
		{
		  vfs_destroy_inode (files[i].pf_inode);
		  files[i].pf_inode = NULL;
		  return -EISDIR;
		}
	      files[i].pf_mode = O_RDWR;
	      break;
	    default:
	      vfs_destroy_inode (files[i].pf_inode);
	      files[i].pf_inode = NULL;
	      return -EINVAL;
	    }
	  files[i].pf_offset =
	    flags & O_APPEND ? files[i].pf_inode->vi_size : 0;
	  return 0;
	}
    }
  return -EMFILE; /* No more file descriptors available */
}

int
sys_close (int fd)
{
  ProcessFile *file;
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT)
    return -EBADF;
  file = &process_table[task_getpid ()].p_files[fd];
  if (file->pf_inode == NULL)
    return -EBADF;
  vfs_destroy_inode (file->pf_inode);
  file->pf_mode = 0;
  file->pf_offset = 0;
  return 0;
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
  VFSDirEntry old_entry;
  VFSDirEntry new_entry;
  char *name;
  int ret = sys_path_sep (new, &new_entry, &name);
  if (ret != 0)
    return ret;
  ret = sys_path_rel_lookup (old, &old_entry);
  if (ret != 0)
    {
      vfs_destroy_inode (new_entry.d_inode);
      kfree (new_entry.d_name);
      kfree (name);
      return ret;
    }
  ret = vfs_link (old_entry.d_inode, new_entry.d_inode, name);
  vfs_destroy_inode (old_entry.d_inode);
  kfree (old_entry.d_name);
  vfs_destroy_inode (new_entry.d_inode);
  kfree (new_entry.d_name);
  kfree (name);
  return ret;
}

int
sys_unlink (const char *path)
{
  VFSDirEntry entry;
  char *name;
  int ret = sys_path_sep (path, &entry, &name);
  if (ret != 0)
    return ret;
  ret = vfs_unlink (entry.d_inode, name);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  kfree (name);
  return ret;
}

int
sys_mknod (const char *path, mode_t mode, dev_t dev)
{
  VFSDirEntry entry;
  char *name;
  int ret = sys_path_sep (path, &entry, &name);
  if (ret != 0)
    return ret;
  ret = vfs_mknod (entry.d_inode, name, mode, dev);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  kfree (name);
  return ret;
}

int
sys_chmod (const char *path, mode_t mode)
{
  VFSDirEntry entry;
  int ret = sys_path_rel_lookup (path, &entry);
  if (ret != 0)
    return ret;
  ret = vfs_chmod (entry.d_inode, mode);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  return ret;
}

int
sys_chown (const char *path, uid_t uid, gid_t gid)
{
  VFSDirEntry entry;
  int ret = sys_path_rel_lookup (path, &entry);
  if (ret != 0)
    return ret;
  ret = vfs_chown (entry.d_inode, uid, gid);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  return ret;
}

off_t
sys_lseek (int fd, off_t offset, int whence)
{
  ProcessFile *file;
  off_t real_offset;
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT)
    return -EBADF;
  file = &process_table[task_getpid ()].p_files[fd];
  if (file->pf_inode == NULL)
    return -EBADF;
  switch (whence)
    {
    case SEEK_SET:
      real_offset = offset;
      break;
    case SEEK_CUR:
      real_offset = file->pf_offset + offset;
      break;
    case SEEK_END:
      real_offset = file->pf_inode->vi_size + offset;
      break;
    default:
      return -EINVAL;
    }
  if (real_offset < 0)
    return -EINVAL;
  file->pf_offset = real_offset;
  return real_offset;
}

pid_t
sys_getpid (void)
{
  return task_getpid ();
}

int
sys_mount (const char *src, const char *dir, const char *type, int flags,
	   void *data)
{
  return -ENOSYS;
}

int
sys_unmount (const char *dir)
{
  return -ENOSYS;
}

int
sys_rename (const char *old, const char *new)
{
  VFSDirEntry old_entry;
  VFSDirEntry new_entry;
  char *old_name;
  char *new_name;
  int ret = sys_path_sep (old, &old_entry, &old_name);
  if (ret != 0)
    return ret;
  ret = sys_path_sep (new, &new_entry, &new_name);
  if (ret != 0)
    {
      vfs_destroy_inode (old_entry.d_inode);
      kfree (old_entry.d_name);
      kfree (old_name);
      return ret;
    }
  ret = vfs_rename (old_entry.d_inode, old_name, new_entry.d_inode, new_name);
  vfs_destroy_inode (old_entry.d_inode);
  kfree (old_entry.d_name);
  kfree (old_name);
  vfs_destroy_inode (new_entry.d_inode);
  kfree (new_entry.d_name);
  kfree (new_name);
  return ret;
}

int
sys_mkdir (const char *path, mode_t mode)
{
  VFSDirEntry entry;
  char *name;
  int ret = sys_path_sep (path, &entry, &name);
  if (ret != 0)
    return ret;
  ret = vfs_mkdir (entry.d_inode, name, mode);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  kfree (name);
  return ret;
}

int
sys_rmdir (const char *path)
{
  VFSDirEntry entry;
  char *name;
  int ret = sys_path_sep (path, &entry, &name);
  if (ret != 0)
    return ret;
  ret = vfs_rmdir (entry.d_inode, name);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  kfree (name);
  return ret;
}

int
sys_symlink (const char *old, const char *new)
{
  VFSDirEntry entry;
  char *name;
  int ret = sys_path_sep (new, &entry, &name);
  if (ret != 0)
    return ret;
  ret = vfs_symlink (entry.d_inode, old, name);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  kfree (name);
  return ret;
}

int
sys_readlink (const char *path, char *buffer, size_t len)
{
  VFSDirEntry entry;
  int ret = sys_path_rel_lookup (path, &entry);
  if (ret != 0)
    return ret;
  ret = vfs_readlink (&entry, buffer, len);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  return ret;
}

int
sys_truncate (const char *path, off_t len)
{
  VFSDirEntry entry;
  int ret = sys_path_rel_lookup (path, &entry);
  if (ret != 0)
    return ret;
  entry.d_inode->vi_size = len;
  ret = vfs_truncate (entry.d_inode);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  return ret;
}

int
sys_statvfs (const char *path, struct statvfs *st)
{
  VFSPath *vpath;
  int ret = vfs_namei (&vpath, path);
  if (ret != 0)
    return ret;
  ret = vfs_path_find_mount (vpath);
  vfs_path_free (vpath);
  if (ret < 0)
    return ret;
  return vfs_statvfs (&mount_table[ret].vfs_sb, st);
}

int
sys_stat (const char *path, struct stat *st)
{
  VFSDirEntry entry;
  VFSPath *vpath;
  VFSMount *mp;
  int ret = vfs_namei (&vpath, path);
  if (ret != 0)
    return ret;
  ret = vfs_path_find_mount (vpath);
  vfs_path_free (vpath);
  if (ret < 0)
    return ret;
  mp = &mount_table[ret];
  ret = sys_path_rel_lookup (path, &entry);
  if (ret != 0)
    return ret;
  ret = vfs_getattr (mp, &entry, st);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  return ret;
}

int
sys_setxattr (const char *path, const char *name, const void *value, size_t len,
	      int flags)
{
  VFSDirEntry entry;
  int ret = sys_path_rel_lookup (path, &entry);
  if (ret != 0)
    return ret;
  ret = vfs_setxattr (&entry, name, value, len, flags);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  return ret;
}

int
sys_getxattr (const char *path, const char *name, void *value, size_t len)
{
  VFSDirEntry entry;
  int ret = sys_path_rel_lookup (path, &entry);
  if (ret != 0)
    return ret;
  ret = vfs_getxattr (&entry, name, value, len);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  return ret;
}

int
sys_listxattr (const char *path, char *buffer, size_t len)
{
  VFSDirEntry entry;
  int ret = sys_path_rel_lookup (path, &entry);
  if (ret != 0)
    return ret;
  ret = vfs_listxattr (&entry, buffer, len);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  return ret;
}

int
sys_removexattr (const char *path, const char *name)
{
  VFSDirEntry entry;
  int ret = sys_path_rel_lookup (path, &entry);
  if (ret != 0)
    return ret;
  ret = vfs_removexattr (&entry, name);
  vfs_destroy_inode (entry.d_inode);
  kfree (entry.d_name);
  return ret;
}

void
syscall_init (void)
{
  syscall_table[SYS_fork] = sys_fork;
  syscall_table[SYS_read] = sys_read;
  syscall_table[SYS_write] = sys_write;
  syscall_table[SYS_open] = sys_open;
  syscall_table[SYS_close] = sys_close;
  syscall_table[SYS_creat] = sys_creat;
  syscall_table[SYS_link] = sys_link;
  syscall_table[SYS_unlink] = sys_unlink;
  syscall_table[SYS_mknod] = sys_mknod;
  syscall_table[SYS_chmod] = sys_chmod;
  syscall_table[SYS_chown] = sys_chown;
  syscall_table[SYS_lseek] = sys_lseek;
  syscall_table[SYS_getpid] = sys_getpid;
  syscall_table[SYS_mount] = sys_mount;
  syscall_table[SYS_unmount] = sys_unmount;
  syscall_table[SYS_rename] = sys_rename;
  syscall_table[SYS_mkdir] = sys_mkdir;
  syscall_table[SYS_rmdir] = sys_rmdir;
  syscall_table[SYS_symlink] = sys_symlink;
  syscall_table[SYS_readlink] = sys_readlink;
  syscall_table[SYS_truncate] = sys_truncate;
  syscall_table[SYS_statvfs] = sys_statvfs;
  syscall_table[SYS_stat] = sys_stat;
  syscall_table[SYS_setxattr] = sys_setxattr;
  syscall_table[SYS_getxattr] = sys_getxattr;
  syscall_table[SYS_listxattr] = sys_listxattr;
  syscall_table[SYS_removexattr] = sys_removexattr;
}
