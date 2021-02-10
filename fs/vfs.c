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

#include <fs/ext2.h>
#include <fs/vfs.h>
#include <libk/libk.h>
#include <vm/heap.h>
#include <errno.h>

VFSFilesystem fs_table[VFS_FS_TABLE_SIZE];

void
vfs_init (void)
{
  ext2_init ();
}

int
vfs_register (const VFSFilesystem *fs)
{
  int i;
  if (fs == NULL || *fs->vfs_name == '\0')
    return -EINVAL;
  for (i = 0; i < VFS_FS_TABLE_SIZE; i++)
    {
      if (*fs_table[i].vfs_name != '\0')
	continue;
      memcpy (&fs_table[i], fs, sizeof (VFSFilesystem));
      return 0;
    }
  return -ENOSPC;
}

int
vfs_mount (const char *type, const char *dir, int flags, void *data)
{
  int i;
  if (type == NULL || dir == NULL)
    return -EINVAL;
  for (i = 0; i < VFS_FS_TABLE_SIZE; i++)
    {
      VFSMount *mp;
      if (strcmp (fs_table[i].vfs_name, type) != 0)
        continue;
      mp = kmalloc (sizeof (VFSMount));
      mp->vfs_fstype = &fs_table[i];
      /* TODO Fill vfs_parent and vfs_mntpoint with mount point table */
      mp->vfs_parent = NULL;
      mp->vfs_mntpoint = NULL;
      mp->vfs_private = NULL;
      return fs_table[i].vfs_mount (mp, flags, data);
    }
  return -EINVAL; /* No such filesystem type */
}

VFSPath *
vfs_path_add_component (VFSPath *path, const char *name)
{
  VFSPath *new;
  if (path == NULL || name == NULL || strchr (name, '/') != NULL)
    return NULL;
  new = kmalloc (sizeof (VFSPath));
  new->vp_parent = path;
  if (strlen (name) < 16)
    {
      strcpy (new->vp_short, name);
      new->vp_long = NULL;
    }
  else
    new->vp_long = strdup (name);
  return new;
}

VFSPath *
vfs_namei (const char *path)
{
  VFSPath *dir = NULL;
  char *buffer;
  char *temp;

  if (path == NULL || *path != '/')
    return NULL;

  buffer = strdup (path);
  for (temp = strtok (buffer, "/"); temp != NULL; temp = strtok (NULL, "/"))
    dir = vfs_path_add_component (dir, temp);
  kfree (buffer);
  return dir;
}
