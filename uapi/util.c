/*************************************************************************
 * util.c -- This file is part of OS/0.                                  *
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

int
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

  if (sep == NULL)
    {
      /* File in current directory */
      *dir = process_table[task_getpid ()].p_cwd;
      vfs_ref_inode (*dir);
      *name = strdup (buffer);
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
      *name = strdup (sep + 1);
    }
  return 0;
}

VFSInode *
inode_from_fd (int fd)
{
  ProcessFile *file;
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT)
    return NULL;
  file = process_table[task_getpid ()].p_files[fd];
  return file == NULL ? NULL : file->pf_inode;
}
