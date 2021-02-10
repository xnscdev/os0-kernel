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

#include <fs/vfs.h>
#include <libk/compile.h>
#include <vm/heap.h>
#include <errno.h>
#include <string.h>

int
vfs_path_add_component (VFSPath **result, VFSPath *path, const char *name)
{
  VFSPath *new;

  /* Special names */
  if (strcmp (name, ".") == 0)
    {
      *result = path;
      return 0;
    }
  else if (strcmp (name, "..") == 0)
    {
      VFSPath *temp = path->vp_parent;
      if (temp == NULL)
	{
	  /* Root directory .. leads to itself */
	  *result = path;
	  return 0;
	}
      if (path->vp_long != NULL)
	kfree (path->vp_long);
      kfree (path);
      *result = temp;
      return 0;
    }

  if (name == NULL || strchr (name, '/') != NULL)
    {
      *result = NULL;
      return -EINVAL;
    }
  new = kmalloc (sizeof (VFSPath));
  if (unlikely (new == NULL))
    return -ENOMEM;
  new->vp_parent = path;
  if (strlen (name) < 16)
    {
      strcpy (new->vp_short, name);
      new->vp_long = NULL;
    }
  else
    new->vp_long = strdup (name);
  *result = new;
  return 0;
}

void
vfs_path_free (VFSPath *path)
{
  VFSPath *temp = path;
  if (path == NULL)
    return;
  while (path != NULL)
    {
      if (path->vp_long != NULL)
	kfree (path->vp_long);
      path = temp->vp_parent;
      kfree (temp);
      temp = path;
    }
}

int
vfs_namei (VFSPath **result, const char *path)
{
  VFSPath *dir = NULL;
  char *buffer;
  char *temp;

  if (path == NULL || *path != '/')
    return -EINVAL;

  buffer = strdup (path);
  for (temp = strtok (buffer, "/"); temp != NULL; temp = strtok (NULL, "/"))
    {
      VFSPath *new;
      int ret = vfs_path_add_component (&new, dir, temp);
      if (ret != 0)
	{
	  kfree (buffer);
	  vfs_path_free (dir);
	  *result = NULL;
	  return ret;
	}
      dir = new;
    }
  kfree (buffer);
  *result = dir;
  return 0;
}
