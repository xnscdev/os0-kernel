/*************************************************************************
 * path.c -- This file is part of OS/0.                                  *
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
#include <libk/libk.h>
#include <sys/process.h>
#include <vm/heap.h>

/* Replace starting directory if it is the root inode of a mounted filesystem */

#define CHECK_ROOT_MOUNT do						\
    {									\
      for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++)			\
	{								\
	  VFSInode *mpt = mount_table[i].vfs_mntpoint;			\
	  if (mpt == NULL)						\
	    continue;							\
	  if (dir->vi_ino == mpt->vi_ino && dir->vi_sb == mpt->vi_sb)	\
	    {								\
	      dir = mount_table[i].vfs_sb.sb_root;			\
	      vfs_ref_inode (dir);					\
	      dont_unref = 1;						\
	      goto search;						\
	    }								\
	}								\
    }									\
  while (0)

/* Check if the inode is the root of a mounted filesystem and replace
   it with the root inode of the mount if it is */

#define CHECK_PATH_MOUNT do						\
    {									\
      for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++)			\
	{								\
	  VFSInode *mpt = mount_table[i].vfs_mntpoint;			\
	  if (mpt == NULL)						\
	    continue;							\
	  if (dir->vi_ino == mpt->vi_ino && dir->vi_sb == mpt->vi_sb)	\
	    {								\
	      vfs_unref_inode (dir);					\
	      dir = mount_table[i].vfs_sb.sb_root;			\
	      vfs_ref_inode (dir);					\
	      break;							\
	    }								\
	}								\
    }									\
  while (0)

int
vfs_open_file (VFSInode **inode, const char *path, int follow_symlinks)
{
  VFSInode *dir;
  char *buffer = strdup (path);
  char *ptr;
  int dont_unref = 0;
  int i;
  if (unlikely (buffer == NULL))
    return -ENOMEM;

  if (*path == '/')
    {
      dir = vfs_root_inode;
      ptr = buffer + 1;
    }
  else
    {
      dir = process_table[task_getpid ()].p_cwd;
      ptr = buffer;
    }

  CHECK_ROOT_MOUNT;
  vfs_ref_inode (dir);
  dont_unref = 1;

 search:
  while (*ptr != '\0')
    {
      char *end = strchr (ptr, '/');
      if (end != NULL)
        *end = '\0';

      if (*ptr != '\0' && strcmp (ptr, ".") != 0)
	{
	  VFSInode *inode;
	  int ret = vfs_lookup (&inode, dir, dir->vi_sb, ptr,
				end == NULL ? follow_symlinks : 1);
	  if (ret != 0)
	    {
	      vfs_unref_inode (dir);
	      kfree (buffer);
	      return ret;
	    }
	  vfs_unref_inode (dir);
	  dir = inode;
	  dont_unref = 1;

	  if (end != NULL && !S_ISDIR (dir->vi_mode))
	    {
	      vfs_unref_inode (dir);
	      kfree (buffer);
	      return -ENOTDIR;
	    }

	  CHECK_PATH_MOUNT;
	}

      if (end == NULL)
	break;
      ptr = end + 1;
    }

  kfree (buffer);
  if (!dont_unref)
    vfs_unref_inode (dir); /* Decrease refcount since dir never was changed */
  *inode = dir;
  return 0;
}

char *
vfs_path_resolve (const char *path)
{
  char *buffer;
  char *result;
  char *resptr;
  char *bufptr;
  char *end;

  if (*path == '/')
    {
      buffer = strdup (path);
      if (buffer == NULL)
	return NULL;
      result = strdup (path);
      if (result == NULL)
	{
	  kfree (buffer);
	  return NULL;
	}
    }
  else
    {
      char *cwd = process_table[task_getpid ()].p_cwdpath;
      size_t cwdlen = strlen (cwd);
      size_t pathlen = strlen (path);
      buffer = kmalloc (cwdlen + pathlen + 2);
      if (buffer == NULL)
	return NULL;
      strncpy (buffer, cwd, cwdlen);
      buffer[cwdlen] = '/';
      strncpy (buffer + cwdlen + 1, path, pathlen);
      buffer[cwdlen + pathlen + 1] = '\0';
      result = kmalloc (cwdlen + pathlen + 2);
      if (result == NULL)
	{
	  kfree (buffer);
	  return NULL;
	}
    }
  bufptr = buffer;
  resptr = result;
  *resptr++ = '/';
  *resptr = '\0';

  while (1)
    {
      end = strchr (bufptr, '/');
      if (end != NULL)
	*end = '\0';
      if (*bufptr != '\0' && strcmp (bufptr, ".") != 0)
	{
	  if (strcmp (bufptr, "..") == 0)
	    {
	      char *start = strrchr (result, '/');
	      if (start != NULL && start != result)
		{
		  char *ptr;
		  for (ptr = start - 1; *ptr != '\0' && *ptr != '/'; ptr--)
		    ;
		  if (*ptr == '/')
		    {
		      resptr = ptr + 1;
		      *resptr = '\0';
		    }
		}
	    }
	  else
	    {
	      strcpy (resptr, bufptr);
	      resptr += strlen (bufptr);
	      *resptr++ = '/';
	      *resptr = '\0';
	    }
	}

      if (end == NULL)
	break;
      bufptr = end + 1;
    }

  if (resptr != result + 1 && resptr[-1] == '/')
    resptr[-1] = '\0';
  kfree (buffer);
  return result;
}
