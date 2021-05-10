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

void *syscall_table[NR_syscalls] = {
  NULL,
  sys_exit,
  sys_fork,
  sys_read,
  sys_write,
  sys_open,
  sys_close,
  NULL,
  sys_creat,
  sys_link,
  sys_unlink,
  sys_execve,
  sys_chdir,
  NULL,
  sys_mknod,
  sys_chmod,
  sys_chown,
  NULL,
  NULL,
  sys_lseek,
  sys_getpid,
  sys_mount,
  sys_umount,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_rename,
  sys_mkdir,
  sys_rmdir,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_brk,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_fcntl,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_symlink,
  sys_readlink,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_truncate,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_statvfs,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_stat,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_fchdir,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sys_setxattr,
  NULL,
  NULL,
  sys_getxattr,
  NULL,
  NULL,
  sys_listxattr,
  NULL,
  NULL,
  sys_removexattr
};

extern uint32_t exit_task;

/* Separates a path into a file and its parent directory */

static int
sys_path_sep (const char *path, VFSInode **dir, char **name)
{
  char *buffer = strdup (path);
  char *sep;
  if (unlikely (buffer == NULL))
    return -ENOMEM;
  sep = strrchr (buffer, '/');

  if (sep != NULL)
    {
      if (sep[1] == '\0')
	*name = NULL;
      else
	*name = strdup (sep + 1);
    }

  if (buffer == sep)
    {
      /* Direct child of root directory */
      *dir = vfs_root_inode;
      vfs_ref_inode (*dir);
    }
  else if (sep == NULL)
    {
      /* File in current directory */
      *dir = process_table[task_getpid ()].p_cwd;
      vfs_ref_inode (*dir);
    }
  else
    {
      *sep = '\0';
      *dir = vfs_open_file (buffer);
      if (*dir == NULL)
	{
	  kfree (buffer);
	  return -ENOENT;
	}
    }
  return 0;
}

void
sys_exit (int code)
{
  pid_t pid = task_getpid ();
  if (pid == 0)
    panic ("Attempted to exit from kernel task");
  exit_task = pid;
}

int
sys_fork (void)
{
  int ret;
  __asm__ volatile ("cli");
  ret = task_fork ();
  __asm__ volatile ("sti");
  return ret;
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
	  VFSInode *inode = vfs_open_file (path);
	  if (inode == NULL)
	    return -ENOENT;
	  files[i].pf_inode = inode;
	  switch (flags & O_ACCMODE)
	    {
	    case O_RDONLY:
	      files[i].pf_mode = O_RDONLY;
	      break;
	    case O_WRONLY:
	      if (S_ISDIR (files[i].pf_inode->vi_mode))
		{
		  vfs_unref_inode (files[i].pf_inode);
		  files[i].pf_inode = NULL;
		  return -EISDIR;
		}
	      files[i].pf_mode = O_WRONLY;
	      break;
	    case O_RDWR:
	      if (S_ISDIR (files[i].pf_inode->vi_mode))
		{
		  vfs_unref_inode (files[i].pf_inode);
		  files[i].pf_inode = NULL;
		  return -EISDIR;
		}
	      files[i].pf_mode = O_RDWR;
	      break;
	    default:
	      vfs_unref_inode (files[i].pf_inode);
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
  vfs_unref_inode (file->pf_inode);
  file->pf_mode = 0;
  file->pf_offset = 0;
  return 0;
}

int
sys_creat (const char *path, mode_t mode)
{
  VFSInode *dir;
  char *name;
  int ret = sys_path_sep (path, &dir, &name);
  if (ret != 0)
    return ret;
  ret = vfs_create (dir, name, mode);
  vfs_unref_inode (dir);
  kfree (name);
  return ret;
}

int
sys_link (const char *old, const char *new)
{
  VFSInode *old_inode;
  VFSInode *new_inode;
  char *name;
  int ret = sys_path_sep (new, &new_inode, &name);
  if (ret != 0)
    return ret;
  old_inode = vfs_open_file (old);
  if (old_inode == NULL)
    {
      vfs_unref_inode (new_inode);
      return -ENOENT;
    }
  ret = vfs_link (old_inode, new_inode, name);
  vfs_unref_inode (old_inode);
  vfs_unref_inode (new_inode);
  kfree (name);
  return ret;
}

int
sys_unlink (const char *path)
{
  VFSInode *dir;
  char *name;
  int ret = sys_path_sep (path, &dir, &name);
  if (ret != 0)
    return ret;
  ret = vfs_unlink (dir, name);
  vfs_unref_inode (dir);
  kfree (name);
  return ret;
}

int
sys_execve (const char *path, char *const *argv, char *const *envp)
{
  uint32_t eip;
  int ret;
  VFSInode *inode = vfs_open_file (path);
  if (inode == NULL)
    return -ENOENT;
  ret = process_exec (inode, &eip);
  vfs_unref_inode (inode);
  if (ret != 0)
    return ret;
  task_exec (eip);
}

int
sys_chdir (const char *path)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *inode = vfs_open_file (path);
  if (inode == NULL)
    return -ENOENT;
  vfs_unref_inode (proc->p_cwd);
  proc->p_cwd = inode;
  return 0;
}

int
sys_mknod (const char *path, mode_t mode, dev_t dev)
{
  VFSInode *dir;
  char *name;
  int ret = sys_path_sep (path, &dir, &name);
  if (ret != 0)
    return ret;
  ret = vfs_mknod (dir, name, mode, dev);
  vfs_unref_inode (dir);
  kfree (name);
  return ret;
}

int
sys_chmod (const char *path, mode_t mode)
{
  int ret;
  VFSInode *inode = vfs_open_file (path);
  if (inode == NULL)
    return -ENOENT;
  ret = vfs_chmod (inode, mode);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_chown (const char *path, uid_t uid, gid_t gid)
{
  int ret;
  VFSInode *inode = vfs_open_file (path);
  if (inode == NULL)
    return -ENOENT;
  ret = vfs_chown (inode, uid, gid);
  vfs_unref_inode (inode);
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
sys_umount (const char *dir)
{
  return -ENOSYS;
}

int
sys_rename (const char *old, const char *new)
{
  VFSInode *old_inode;
  VFSInode *new_inode;
  char *old_name;
  char *new_name;
  int ret = sys_path_sep (old, &old_inode, &old_name);
  if (ret != 0)
    return ret;
  ret = sys_path_sep (new, &new_inode, &new_name);
  if (ret != 0)
    {
      vfs_unref_inode (old_inode);
      kfree (old_name);
      return ret;
    }
  ret = vfs_rename (old_inode, old_name, new_inode, new_name);
  vfs_unref_inode (old_inode);
  kfree (old_name);
  vfs_unref_inode (new_inode);
  kfree (new_name);
  return ret;
}

int
sys_mkdir (const char *path, mode_t mode)
{
  VFSInode *dir;
  char *name;
  int ret = sys_path_sep (path, &dir, &name);
  if (ret != 0)
    return ret;
  ret = vfs_mkdir (dir, name, mode);
  vfs_unref_inode (dir);
  kfree (name);
  return ret;
}

int
sys_rmdir (const char *path)
{
  VFSInode *dir;
  char *name;
  int ret = sys_path_sep (path, &dir, &name);
  if (ret != 0)
    return ret;
  ret = vfs_rmdir (dir, name);
  vfs_unref_inode (dir);
  kfree (name);
  return ret;
}

int
sys_brk (void *ptr)
{
  if (sys_getpid () == 0)
    return -EINVAL;
  return process_set_break ((uint32_t) ptr);
}

int
sys_fcntl (int fd, int cmd, int arg)
{
  return fcntl (fd, cmd, arg);
}

int
sys_symlink (const char *old, const char *new)
{
  VFSInode *dir;
  char *name;
  int ret = sys_path_sep (new, &dir, &name);
  if (ret != 0)
    return ret;
  ret = vfs_symlink (dir, old, name);
  vfs_unref_inode (dir);
  kfree (name);
  return ret;
}

int
sys_readlink (const char *path, char *buffer, size_t len)
{
  int ret;
  VFSInode *inode = vfs_open_file (path);
  if (inode == NULL)
    return -ENOENT;
  ret = vfs_readlink (inode, buffer, len);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_truncate (const char *path, off_t len)
{
  int ret;
  VFSInode *inode = vfs_open_file (path);
  if (inode == NULL)
    return -ENOENT;
  inode->vi_size = len;
  ret = vfs_truncate (inode);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_statvfs (const char *path, struct statvfs *st)
{
  VFSInode *inode = vfs_open_file (path);
  int ret;
  if (inode == NULL)
    return -ENOENT;
  ret = vfs_statvfs (inode->vi_sb, st);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_stat (const char *path, struct stat *st)
{
  VFSInode *inode = vfs_open_file (path);
  int ret;
  if (inode == NULL)
    return -ENOENT;
  ret = vfs_getattr (inode, st);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_fchdir (int fd)
{
  Process *proc = &process_table[task_getpid ()];
  vfs_unref_inode (proc->p_cwd);
  proc->p_cwd = proc->p_files[fd].pf_inode;
  vfs_ref_inode (proc->p_cwd);
  return 0;
}

int
sys_setxattr (const char *path, const char *name, const void *value, size_t len,
	      int flags)
{
  int ret;
  VFSInode *inode = vfs_open_file (path);
  if (inode == NULL)
    return -ENOENT;
  ret = vfs_setxattr (inode, name, value, len, flags);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_getxattr (const char *path, const char *name, void *value, size_t len)
{
  int ret;
  VFSInode *inode = vfs_open_file (path);
  if (inode == NULL)
    return -ENOENT;
  ret = vfs_getxattr (inode, name, value, len);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_listxattr (const char *path, char *buffer, size_t len)
{
  int ret;
  VFSInode *inode = vfs_open_file (path);
  if (inode == NULL)
    return -ENOENT;
  ret = vfs_listxattr (inode, buffer, len);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_removexattr (const char *path, const char *name)
{
  int ret;
  VFSInode *inode = vfs_open_file (path);
  if (inode == NULL)
    return -ENOENT;
  ret = vfs_removexattr (inode, name);
  vfs_unref_inode (inode);
  return ret;
}
