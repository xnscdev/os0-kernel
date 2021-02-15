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

static int
vfs_path_component_cmp (const VFSPath *a, const VFSPath *b)
{
  if (a == b)
    return 0; /* Save strcmp() call */
  return strcmp (a->vp_long == NULL ? a->vp_short : a->vp_long,
		 b->vp_long == NULL ? b->vp_short : b->vp_long);
}

int
vfs_path_add_component (VFSPath **result, VFSPath *path, const char *name)
{
  VFSPath *new;
  path = vfs_path_last (path);

  /* Special names */
  if (strcmp (name, ".") == 0)
    {
      *result = path;
      return 0;
    }
  else if (strcmp (name, "..") == 0)
    {
      VFSPath *temp;
      if (path == NULL)
	{
	  /* Root directory .. leads to itself */
	  *result = NULL;
	  return 0;
	}
      temp = path->vp_prev;
      if (path->vp_long != NULL)
	kfree (path->vp_long);
      kfree (path);
      temp->vp_next = NULL;
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
  new->vp_prev = path;
  if (path != NULL)
    path->vp_next = new;
  new->vp_next = NULL;
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
  VFSPath *temp;
  if (path == NULL)
    return;
  path = vfs_path_first (path);
  temp = path;
  while (path != NULL)
    {
      if (path->vp_long != NULL)
	kfree (path->vp_long);
      path = temp->vp_next;
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

int
vfs_path_cmp (const VFSPath *a, const VFSPath *b)
{
  for (a = vfs_path_first (a), b = vfs_path_first (b); a != NULL && b != NULL;
       a = a->vp_next, b = b->vp_next)
    {
      int ret = vfs_path_component_cmp (a, b);
      if (ret != 0)
	return ret;
    }
  if (a == NULL || b == NULL)
    {
      if (a == NULL)
	return b != NULL;
      return -1;
    }
  return 0;
}

int
vfs_path_subdir (const VFSPath *path, const VFSPath *dir)
{
  const VFSPath *temp;
  if (dir == NULL)
    return path != NULL;
  for (temp = vfs_path_last (path); temp != NULL; temp = temp->vp_prev)
    {
      if (vfs_path_cmp (temp, dir) == 0)
	return 1;
    }
  return 0;
}

VFSPath *
vfs_path_first (const VFSPath *path)
{
  if (path == NULL)
    return NULL;
  while (path->vp_prev != NULL)
    path = path->vp_prev;
  return (VFSPath *) path;
}

VFSPath *
vfs_path_last (const VFSPath *path)
{
  if (path == NULL)
    return NULL;
  while (path->vp_next != NULL)
    path = path->vp_next;
  return (VFSPath *) path;
}

int
vfs_path_find_mount (const VFSPath *path)
{
  int i;
  for (i = 0; i < VFS_MOUNT_TABLE_SIZE; i++)
    {
      if (vfs_path_subdir (path, mount_table[i].vfs_mntpoint))
	return i;
    }
  return -1;
}

int
vfs_path_rel (VFSPath **result, VFSPath *path, const VFSMount *mp)
{
  VFSPath *temp;
  VFSPath *mntpoint;

  if (mp == NULL)
    {
      *result = NULL;
      return -1;
    }
  mntpoint = mp->vfs_mntpoint;

  /* Check if path is root dir relative to mount */
  if (path == mntpoint)
    return vfs_namei (result, "/");

  for (temp = vfs_path_last (path); temp != NULL; temp = temp->vp_prev)
    {
      if (vfs_path_cmp (temp, mntpoint) == 0)
	{
	  /* Disconnect and free the previous path components */
	  temp->vp_next->vp_prev = NULL;
	  temp->vp_next = NULL;
	  vfs_path_free (temp);
	  *result = vfs_path_last (path);
	  return 0;
	}
    }

  *result = NULL;
  return -1;
}
