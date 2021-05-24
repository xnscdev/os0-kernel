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

#include <libk/libk.h>
#include <sys/process.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <video/vga.h>
#include <vm/heap.h>

extern int exit_task;

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
      int ret;
      *sep = '\0';
      ret = vfs_open_file (dir, buffer, 1);
      if (ret != 0)
	{
	  kfree (buffer);
	  return ret;
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
  process_table[pid].p_term = 1;
  process_table[pid].p_waitstat = (code & 0xff) << 8;
  if (process_table[pid].p_refcnt == 0)
    exit_task = pid;
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
	  VFSInode *inode;
	  int ret = vfs_open_file (&inode, path, 1);
	  if (ret != 0)
	    return ret;
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
	  return i;
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

pid_t
sys_waitpid (pid_t pid, int *status, int options)
{
  return wait4 (pid, status, options, NULL);
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
  ret = vfs_open_file (&old_inode, old, 1);
  if (ret != 0)
    {
      vfs_unref_inode (new_inode);
      return ret;
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
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = process_exec (inode, &eip);
  vfs_unref_inode (inode);
  if (ret != 0)
    return ret;
  task_exec (eip, argv, envp);
}

int
sys_chdir (const char *path)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
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
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = vfs_chmod (inode, mode);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_chown (const char *path, uid_t uid, gid_t gid)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
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
sys_access (const char *path, int mode)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  switch (mode)
    {
    case F_OK:
      break; /* The file exists at this point */
    case X_OK:
    case R_OK:
    case W_OK:
      ret = -ENOSYS; /* TODO Implement */
      break;
    default:
      ret = -EINVAL;
    }
  vfs_unref_inode (inode);
  return ret;
}

int
sys_kill (pid_t pid, int sig)
{
  int exit = sig == SIGKILL;
  ProcessSignal *signal;
  if (pid == 0 || pid == -1)
    return -ENOSYS;
  if (pid < 0)
    pid = -pid;
  if (pid >= PROCESS_LIMIT)
    return -EINVAL;
  if (sig < 0 || sig >= NR_signals)
    return -EINVAL;
  signal = &process_table[pid].p_signals[sig];

  if (sig == SIGFPE || sig == SIGILL || sig == SIGSEGV || sig == SIGBUS
      || sig == SIGABRT || sig == SIGTRAP || sig == SIGSYS)
    {
      if (!signal->ps_enabled || (!(signal->ps_act.sa_flags & SA_SIGINFO)
				  && signal->ps_act.sa_handler == SIG_DFL))
	exit = 1; /* Default action is to terminate process */
    }

  if (exit)
    {
      process_table[pid].p_term = 1;
      process_table[pid].p_waitstat = sig;
    }

  if (signal->ps_act.sa_flags & SA_SIGINFO)
    ; /* TODO Call signal->ps_act.sa_sigaction */
  else
    signal->ps_act.sa_handler (sig);
  return 0;
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

sighandler_t
sys_signal (int sig, sighandler_t func)
{
  struct sigaction old;
  struct sigaction act;
  if (sig < 0 || sig >= NR_signals || sig == SIGKILL || sig == SIGSTOP)
    return SIG_ERR;
  act.sa_handler = func;
  act.sa_sigaction = NULL;
  act.sa_flags = 0;
  memset (&act.sa_mask, 0, sizeof (sigset_t));
  if (sys_sigaction (sig, &act, &old) == -1)
    return SIG_ERR;
  return old.sa_handler;
}

int
sys_ioctl (int fd, unsigned long req, void *data)
{
  return ioctl (fd, req, data);
}

int
sys_fcntl (int fd, int cmd, int arg)
{
  return fcntl (fd, cmd, arg);
}

pid_t
sys_getppid (void)
{
  return task_getppid ();
}

int
sys_sigaction (int sig, const struct sigaction *__restrict act,
	       struct sigaction *__restrict old)
{
  Process *proc;
  if (sig < 0 || sig >= NR_signals || sig == SIGKILL || sig == SIGSTOP)
    return -EINVAL;
  proc = &process_table[task_getpid ()];
  if (old != NULL)
    memcpy (old, &proc->p_signals[sig].ps_act, sizeof (struct sigaction));
  if (act != NULL)
    {
      memcpy (&proc->p_signals[sig].ps_act, act, sizeof (struct sigaction));
      proc->p_signals[sig].ps_enabled = 1;
    }
  return 0;
}

int
sys_gettimeofday (struct timeval *__restrict tv, struct timezone *__restrict tz)
{
  if (tv != NULL)
    {
      tv->tv_sec = time (NULL);
      tv->tv_usec = 0;
    }
  if (tz != NULL)
    {
      tz->tz_minuteswest = 0;
      tz->tz_dsttime = 0;
    }
  return 0;
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
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = vfs_readlink (inode, buffer, len);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_truncate (const char *path, off_t len)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  inode->vi_size = len;
  ret = vfs_truncate (inode);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_ftruncate (int fd, off_t len)
{
  VFSInode *inode;
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT)
    return -EBADF;
  inode = process_table[task_getpid ()].p_files[fd].pf_inode;
  if (inode == NULL)
    return -EBADF;
  inode->vi_size = len;
  return vfs_truncate (inode);
}

int
sys_fchmod (int fd, mode_t mode)
{
  VFSInode *inode;
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT)
    return -EBADF;
  inode = process_table[task_getpid ()].p_files[fd].pf_inode;
  if (inode == NULL)
    return -EBADF;
  return vfs_chmod (inode, mode);
}

int
sys_fchown (int fd, uid_t uid, gid_t gid)
{
  VFSInode *inode;
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT)
    return -EBADF;
  inode = process_table[task_getpid ()].p_files[fd].pf_inode;
  if (inode == NULL)
    return -EBADF;
  return vfs_chown (inode, uid, gid);
}

int
sys_statvfs (const char *path, struct statvfs *st)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = vfs_statvfs (inode->vi_sb, st);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_stat (const char *path, struct stat *st)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = vfs_getattr (inode, st);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_fstat (int fd, struct stat *st)
{
  VFSInode *inode;
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT)
    return -EBADF;
  inode = process_table[task_getpid ()].p_files[fd].pf_inode;
  if (inode == NULL)
    return -EBADF;
  return vfs_getattr (inode, st);
}

pid_t
sys_wait4 (pid_t pid, int *status, int options, struct rusage *usage)
{
  return wait4 (pid, status, options, usage);
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
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = vfs_setxattr (inode, name, value, len, flags);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_getxattr (const char *path, const char *name, void *value, size_t len)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = vfs_getxattr (inode, name, value, len);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_listxattr (const char *path, char *buffer, size_t len)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = vfs_listxattr (inode, buffer, len);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_removexattr (const char *path, const char *name)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = vfs_removexattr (inode, name);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_openat (int fd, const char *path, int flags, mode_t mode)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT
	  || proc->p_files[fd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd].pf_inode;
    }
  ret = sys_open (path, flags, mode);
  proc->p_cwd = cwd;
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
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT
	  || proc->p_files[fd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd].pf_inode;
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
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT
	  || proc->p_files[fd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd].pf_inode;
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
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT
	  || proc->p_files[fd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd].pf_inode;
    }
  ret = sys_chown (path, uid, gid);
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
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT
	  || proc->p_files[fd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd].pf_inode;
    }
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
	  || proc->p_files[oldfd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[oldfd].pf_inode;
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
	  || proc->p_files[newfd].pf_inode == NULL)
	{
	  vfs_unref_inode (old_inode);
	  kfree (old_name);
	  proc->p_cwd = cwd;
	  return -EBADF;
	}
      proc->p_cwd = proc->p_files[newfd].pf_inode;
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
sys_linkat (int oldfd, const char *old, int newfd, const char *new)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  VFSInode *old_inode;
  VFSInode *new_inode;
  char *name;
  int ret;

  if (oldfd != AT_FDCWD)
    {
      if (oldfd < 0 || oldfd >= PROCESS_FILE_LIMIT
	  || proc->p_files[oldfd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[oldfd].pf_inode;
    }
  ret = vfs_open_file (&old_inode, old, 1);
  if (ret != 0)
    {
      proc->p_cwd = cwd;
      return ret;
    }

  if (newfd != AT_FDCWD)
    {
      if (newfd < 0 || newfd >= PROCESS_FILE_LIMIT
	  || proc->p_files[newfd].pf_inode == NULL)
	{
	  vfs_unref_inode (old_inode);
	  proc->p_cwd = cwd;
	  return -EBADF;
	}
      proc->p_cwd = proc->p_files[newfd].pf_inode;
    }
  ret = sys_path_sep (new, &new_inode, &name);
  if (ret != 0)
    {
      vfs_unref_inode (old_inode);
      proc->p_cwd = cwd;
      return ret;
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
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT
	  || proc->p_files[fd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd].pf_inode;
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
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT
	  || proc->p_files[fd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd].pf_inode;
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
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT
	  || proc->p_files[fd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd].pf_inode;
    }
  ret = sys_chmod (path, mode);
  proc->p_cwd = cwd;
  return ret;
}

int
sys_faccessat (int fd, const char *path, int mode, int flags)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *cwd = proc->p_cwd;
  int ret;
  if (fd != AT_FDCWD)
    {
      if (fd < 0 || fd >= PROCESS_FILE_LIMIT
	  || proc->p_files[fd].pf_inode == NULL)
	return -EBADF;
      proc->p_cwd = proc->p_files[fd].pf_inode;
    }
  ret = sys_access (path, mode);
  proc->p_cwd = cwd;
  return ret;
}
