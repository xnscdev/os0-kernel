/*************************************************************************
 * vfs.c -- This file is part of OS/0.                                   *
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
__sys_xchown (const char *path, uid_t uid, gid_t gid, int follow_symlinks)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, follow_symlinks);
  if (ret != 0)
    return ret;
  ret = vfs_chown (inode, uid, gid);
  vfs_unref_inode (inode);
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
  if (ret < 0)
    return ret;
  file->pf_offset += len;
  return ret;
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
  if (ret < 0)
    return ret;
  file->pf_offset += len;
  return ret;
}

int
sys_open (const char *path, int flags, mode_t mode)
{
  Process *proc = &process_table[task_getpid ()];
  ProcessFile *files = proc->p_files;
  int i;
  mode &= ~proc->p_umask;
  for (i = 0; i < PROCESS_FILE_LIMIT; i++)
    {
      if (files[i].pf_inode == NULL)
	{
	  VFSInode *inode;
	  int ret = vfs_open_file (&inode, path, 1);

	  if (flags & O_CREAT)
	    {
	      if (flags & O_EXCL && ret == 0)
		{
		  /* The file already exists */
		  vfs_unref_inode (inode);
		  return -EEXIST;
		}
	      if (ret == -ENOENT)
		{
		  /* Try to create a new file */
		  VFSInode *dir;
		  char *name;
		  ret = sys_path_sep (path, &dir, &name);
		  if (ret != 0)
		    return ret;
		  ret = vfs_create (dir, name, mode);
		  if (ret != 0)
		    {
		      vfs_unref_inode (dir);
		      kfree (name);
		      return ret;
		    }

		  /* Try to open the file again */
		  ret = vfs_lookup (&inode, dir, dir->vi_sb, name, 1);
		  vfs_unref_inode (dir);
		  kfree (name);
		}
	    }
	  if (ret != 0)
	    return ret;

	  files[i].pf_inode = inode;
	  switch (flags & O_ACCMODE)
	    {
	    case O_RDONLY:
	      if (S_ISDIR (files[i].pf_inode->vi_mode))
		{
		  files[i].pf_dir =
		    vfs_alloc_dir (files[i].pf_inode, files[i].pf_inode->vi_sb);
		  if (unlikely (files[i].pf_dir == NULL))
		    {
		      vfs_unref_inode (files[i].pf_inode);
		      files[i].pf_inode = NULL;
		      return -EIO;
		    }
		}
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

	  files[i].pf_path = vfs_path_resolve (path);
	  if (files[i].pf_path == NULL)
	    {
	      vfs_unref_inode (files[i].pf_inode);
	      files[i].pf_inode = NULL;
	      return -ENOMEM;
	    }
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
  file->pf_inode = NULL;
  vfs_destroy_dir (file->pf_dir);
  file->pf_dir = NULL;
  kfree (file->pf_path);
  file->pf_path = NULL;
  file->pf_mode = 0;
  file->pf_offset = 0;
  return 0;
}

int
sys_creat (const char *path, mode_t mode)
{
  return sys_open (path, O_CREAT | O_TRUNC | O_WRONLY, mode);
}

int
sys_link (const char *old, const char *new)
{
  VFSInode *old_inode;
  VFSInode *new_inode;
  VFSInode *temp;
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
sys_chdir (const char *path)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *inode;
  char *cwd;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  if (!S_ISDIR (inode->vi_mode))
    {
      vfs_unref_inode (inode);
      return -ENOTDIR;
    }
  cwd = vfs_path_resolve (path);
  if (cwd == NULL)
    {
      vfs_unref_inode (inode);
      return -ENOMEM;
    }
  vfs_unref_inode (proc->p_cwd);
  proc->p_cwd = inode;
  kfree (proc->p_cwdpath);
  proc->p_cwdpath = cwd;
  return 0;
}

int
sys_mknod (const char *path, mode_t mode, dev_t dev)
{
  VFSInode *dir;
  VFSInode *temp;
  char *name;
  int ret = sys_path_sep (path, &dir, &name);
  if (ret != 0)
    return ret;

  ret = vfs_lookup (&temp, dir, dir->vi_sb, name, 0);
  if (ret == 0)
    {
      vfs_unref_inode (dir);
      kfree (name);
      vfs_unref_inode (temp);
      return -EEXIST;
    }

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
sys_lchown (const char *path, uid_t uid, gid_t gid)
{
  return __sys_xchown (path, uid, gid, 0);
}

off_t
sys_lseek (int fd, off_t offset, int whence)
{
  ProcessFile *file;
  off_t real_offset;
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  file = &process_table[task_getpid ()].p_files[fd];
  if (S_ISSOCK (inode->vi_mode) || S_ISFIFO (inode->vi_mode))
    return -ESPIPE;
  switch (whence)
    {
    case SEEK_SET:
      real_offset = offset;
      break;
    case SEEK_CUR:
      real_offset = file->pf_offset + offset;
      break;
    case SEEK_END:
      real_offset = inode->vi_size + offset;
      break;
    default:
      return -EINVAL;
    }
  if (real_offset < 0)
    return -EINVAL;
  file->pf_offset = real_offset;

  /* TODO Update directory structure on seek */
  return real_offset;
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
      ret = vfs_perm_check_exec (inode, 1);
      break;
    case R_OK:
      ret = vfs_perm_check_read (inode, 1);
      break;
    case W_OK:
      ret = vfs_perm_check_write (inode, 1);
      break;
    default:
      ret = -EINVAL;
    }
  vfs_unref_inode (inode);
  return ret;
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
  VFSInode *temp;
  char *name;
  int ret = sys_path_sep (path, &dir, &name);
  if (ret != 0)
    return ret;

  ret = vfs_lookup (&temp, dir, dir->vi_sb, name, 0);
  if (ret == 0)
    {
      vfs_unref_inode (dir);
      kfree (name);
      vfs_unref_inode (temp);
      return -EEXIST;
    }

  ret = vfs_mkdir (dir, name, mode & ~process_table[task_getpid ()].p_umask);
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

mode_t
sys_umask (mode_t mask)
{
  Process *proc = &process_table[task_getpid ()];
  mode_t old = proc->p_umask;
  proc->p_umask = mask;
  return old;
}

int
sys_symlink (const char *old, const char *new)
{
  VFSInode *dir;
  VFSInode *temp;
  char *name;
  int ret = sys_path_sep (new, &dir, &name);
  if (ret != 0)
    return ret;

  ret = vfs_lookup (&temp, dir, dir->vi_sb, name, 0);
  if (ret == 0)
    {
      vfs_unref_inode (dir);
      kfree (name);
      vfs_unref_inode (temp);
      return -EEXIST;
    }

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
  if (!S_ISLNK (inode->vi_mode))
    {
      vfs_unref_inode (inode);
      return -EINVAL;
    }
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
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  inode->vi_size = len;
  return vfs_truncate (inode);
}

int
sys_fchmod (int fd, mode_t mode)
{
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  return vfs_chmod (inode, mode);
}

int
sys_fchown (int fd, uid_t uid, gid_t gid)
{
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  return vfs_chown (inode, uid, gid);
}

int
sys_statfs (const char *path, struct statfs *st)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;
  ret = vfs_statfs (inode->vi_sb, st);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_fstatfs (int fd, struct statfs *st)
{
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  return vfs_statfs (inode->vi_sb, st);
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
sys_lstat (const char *path, struct stat *st)
{
  VFSInode *inode;
  int ret = vfs_open_file (&inode, path, 0);
  if (ret != 0)
    return ret;
  ret = vfs_getattr (inode, st);
  vfs_unref_inode (inode);
  return ret;
}

int
sys_fstat (int fd, struct stat *st)
{
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  return vfs_getattr (inode, st);
}

int
sys_fchdir (int fd)
{
  Process *proc = &process_table[task_getpid ()];
  VFSInode *inode = inode_from_fd (fd);
  if (inode == NULL)
    return -EBADF;
  if (!S_ISDIR (inode->vi_mode))
    return -ENOTDIR;
  proc->p_cwdpath = strdup (proc->p_files[fd].pf_path);
  if (proc->p_cwdpath == NULL)
    return -ENOMEM;
  vfs_unref_inode (proc->p_cwd);
  proc->p_cwd = inode;
  vfs_ref_inode (proc->p_cwd);
  return 0;
}

int
sys_getdents (int fd, struct dirent *dirp, unsigned int count)
{
  Process *proc = &process_table[task_getpid ()];
  VFSDirectory *dir;
  VFSDirEntry *entry;
  unsigned int i;
  int ret;
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT)
    return -EBADF;
  dir = proc->p_files[fd].pf_dir;
  if (dir == NULL)
    return -ENOTDIR;
  for (i = 0; i < count; i++)
    {
      ret = vfs_readdir (&entry, dir, dir->vd_inode->vi_sb);
      switch (ret)
	{
	case 1:
	  if (i == count - 1)
	    break;
	  return 0;
	case 0:
	  dirp[i].d_ino = entry->d_inode->vi_ino;
	  dirp[i].d_reclen = sizeof (struct dirent);
	  dirp[i].d_type = DT_UNKNOWN;
	  strncpy (dirp[i].d_name, entry->d_name, 256);
	  vfs_destroy_dir_entry (entry);
	  break;
	default:
	  return ret;
	}
    }
  return sizeof (struct dirent) * count;
}

int
sys_chown (const char *path, uid_t uid, gid_t gid)
{
  return __sys_xchown (path, uid, gid, 1);
}

int
sys_getcwd (char *buffer, size_t len)
{
  char *cwd = process_table[task_getpid ()].p_cwdpath;
  size_t cwdlen = strlen (cwd);
  if (buffer == NULL)
    return cwdlen;
  if (len == 0)
    return -EINVAL;
  if (len <= cwdlen)
    return -ERANGE;
  strcpy (buffer, cwd);
  return 0;
}