/*************************************************************************
 * relvfs.c -- This file is part of OS/0.                                *
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
#include <vm/heap.h>

static int
__sys_utime_ns_valid (long nsec)
{
  return (nsec >= 0 && nsec < 1000000000) || nsec == UTIME_NOW
    || nsec == UTIME_OMIT;
}

int
sys_openat (int fd, const char *path, int flags, mode_t mode)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  char *cwdpath = proc->p_cwdpath;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
      proc->p_cwdpath = proc->p_files[fd]->pf_path;
    }
  ret = sys_open (path, flags, mode);
  proc->p_cwd = cwd;
  proc->p_cwdpath = cwdpath;
  return ret;
}

int
sys_mkdirat (int fd, const char *path, mode_t mode)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
    }
  ret = sys_mkdir (path, mode);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_mknodat (int fd, const char *path, mode_t mode, dev_t dev)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
    }
  ret = sys_mknod (path, mode, dev);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_fchownat (int fd, const char *path, uid_t uid, gid_t gid, int flags)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  VFSInode *inode;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
    }

  ret = vfs_open_file (&inode, path, flags & AT_SYMLINK_NOFOLLOW ? 0 : 1);
  if (ret != 0)
    return ret;
  ret = vfs_chown (inode, uid, gid);
  vfs_unref_inode (inode);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_futimesat (int fd, const char *path, const struct timeval times[2])
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  VFSInode *inode;
  int ret;
  if (times[0].tv_usec >= 1000000 || times[1].tv_usec >= 1000000)
    return -EINVAL;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
    }

  ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  inode->vi_atime.tv_sec = times[0].tv_sec;
  inode->vi_atime.tv_nsec = times[0].tv_usec * 1000;
  inode->vi_mtime.tv_sec = times[1].tv_sec;
  inode->vi_mtime.tv_nsec = times[1].tv_usec * 1000;
  ret = vfs_write_inode (inode);
  vfs_unref_inode (inode);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_unlinkat (int fd, const char *path, int flags)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
    }
  if (flags & AT_REMOVEDIR)
    ret = sys_rmdir (path);
  else
    ret = sys_unlink (path);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_renameat (int oldfd, const char *old, int newfd, const char *new)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  VFSInode *old_inode;
  VFSInode *new_inode;
  char *old_name;
  char *new_name;
  int ret;

  if (oldfd != AT_FDCWD)
    {
      if (oldfd < 0 || oldfd >= PROCESS_FILE_LIMIT
	  || proc->p_files[oldfd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[oldfd]->pf_inode;
    }
  ret = sys_path_sep (old, &old_inode, &old_name);
  if (ret != 0)
    {
      proc->p_cwd = cwd;
      return ret;
    }

  if (newfd != AT_FDCWD)
    {
      if (newfd < 0 || newfd >= PROCESS_FILE_LIMIT
	  || proc->p_files[newfd] == NULL)
	{
	  vfs_unref_inode (old_inode);
	  kfree (old_name);
	  proc->p_cwd = cwd;
	  return -EBADF;
	}
      proc->p_cwd = proc->p_files[newfd]->pf_inode;
    }
  ret = sys_path_sep (new, &new_inode, &new_name);
  if (ret != 0)
    {
      vfs_unref_inode (old_inode);
      kfree (old_name);
      proc->p_cwd = cwd;
      return ret;
    }

  ret = vfs_rename (old_inode, old_name, new_inode, new_name);
  vfs_unref_inode (old_inode);
  kfree (old_name);
  vfs_unref_inode (new_inode);
  kfree (new_name);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_linkat (int oldfd, const char *old, int newfd, const char *new, int flags)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  VFSInode *old_inode;
  VFSInode *new_inode;
  VFSInode *temp;
  char *name;
  int ret;

  if (oldfd != AT_FDCWD)
    {
      if (oldfd < 0 || oldfd >= PROCESS_FILE_LIMIT
	  || proc->p_files[oldfd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[oldfd]->pf_inode;
    }
  ret = vfs_open_file (&old_inode, old, flags & AT_SYMLINK_FOLLOW ? 1 : 0);
  if (ret != 0)
    {
      proc->p_cwd = cwd;
      return ret;
    }

  if (newfd != AT_FDCWD)
    {
      if (newfd < 0 || newfd >= PROCESS_FILE_LIMIT
	  || proc->p_files[newfd] == NULL)
	{
	  vfs_unref_inode (old_inode);
	  proc->p_cwd = cwd;
	  return -EBADF;
	}
      proc->p_cwd = proc->p_files[newfd]->pf_inode;
    }
  ret = sys_path_sep (new, &new_inode, &name);
  if (ret != 0)
    {
      vfs_unref_inode (old_inode);
      proc->p_cwd = cwd;
      return ret;
    }

  ret = vfs_lookup (&temp, new_inode, new_inode->vi_sb, name, 0);
  if (ret == 0)
    {
      vfs_unref_inode (old_inode);
      vfs_unref_inode (new_inode);
      kfree (name);
      vfs_unref_inode (temp);
      return -EEXIST;
    }

  ret = vfs_link (old_inode, new_inode, name);
  vfs_unref_inode (old_inode);
  vfs_unref_inode (new_inode);
  kfree (name);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_symlinkat (const char *old, int fd, const char *new)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
    }
  ret = sys_symlink (old, new);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_readlinkat (int fd, const char *__restrict path, char *__restrict buffer,
		size_t len)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
    }
  ret = sys_readlink (path, buffer, len);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_fchmodat (int fd, const char *path, mode_t mode, int flags)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  VFSInode *inode;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
    }

  ret = vfs_open_file (&inode, path, flags & AT_SYMLINK_NOFOLLOW ? 0 : 1);
  if (ret != 0)
    return ret;
  ret = vfs_chmod (inode, mode);
  vfs_unref_inode (inode);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_faccessat (int fd, const char *path, int mode, int flags)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  VFSInode *inode;
  int accmode = flags & AT_EACCESS ? 0 : 1;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
    }

  ret = vfs_open_file (&inode, path, flags & AT_SYMLINK_NOFOLLOW ? 0 : 1);
  if (ret != 0)
    return ret;
  switch (mode)
    {
    case F_OK:
      break; /* The file exists at this point */
    case X_OK:
      return vfs_perm_check_exec (inode, accmode);
    case R_OK:
      return vfs_perm_check_read (inode, accmode);
    case W_OK:
      return vfs_perm_check_write (inode, accmode);
    default:
      ret = -EINVAL;
    }
  vfs_unref_inode (inode);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_utimensat (int fd, const char *path, const struct timespec times[2],
	       int flags)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  VFSInode *inode;
  int ret;
  if (!__sys_utime_ns_valid (times[0].tv_nsec)
      || !__sys_utime_ns_valid (times[1].tv_nsec))
    return -EINVAL;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd]->pf_inode;
    }

  ret = vfs_open_file (&inode, path, flags & AT_SYMLINK_NOFOLLOW ? 0 : 1);
  if (ret != 0)
    return ret;
  switch (times[0].tv_nsec)
    {
    case UTIME_NOW:
      inode->vi_atime.tv_sec = time (NULL);
      inode->vi_atime.tv_nsec = 0;
      break;
    case UTIME_OMIT:
      break;
    default:
      inode->vi_atime = times[0];
    }
  switch (times[1].tv_nsec)
    {
    case UTIME_NOW:
      inode->vi_mtime.tv_sec = time (NULL);
      inode->vi_mtime.tv_nsec = 0;
      break;
    case UTIME_OMIT:
      break;
    default:
      inode->vi_mtime = times[1];
    }
  ret = vfs_write_inode (inode);
  vfs_unref_inode (inode);
  proc->p_cwd = cwd;
  return ret;
}
