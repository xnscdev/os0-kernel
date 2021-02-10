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

int
vfs_path_cmp (const VFSPath *a, const VFSPath *b)
{
  size_t la;
  size_t lb;
  const VFSPath *ta;
  const VFSPath *tb;

  /* Special value checks */
  if (a == b)
    return 0;
  if (a == NULL || b == NULL)
    {
      if (a == NULL)
	return b != NULL;
      return -1;
    }

  for (la = 0, ta = a; ta != NULL; ta = ta->vp_parent)
    la++;
  for (lb = 0, tb = b; tb != NULL; tb = tb->vp_parent)
    lb++;
  if (lb > la)
    return 1;
  if (lb < la)
    return -1;

  for (ta = a, tb = b; ta != NULL && tb != NULL;
       ta = ta->vp_parent, tb = tb->vp_parent)
    {
      int ret = vfs_path_component_cmp (ta, tb);
      if (ret != 0)
	return ret;
    }
  return 0;
}

int
vfs_path_subdir (const VFSPath *path, const VFSPath *dir)
{
  const VFSPath *temp;
  if (dir == NULL)
    return path != NULL;
  for (temp = path; temp != NULL; temp++)
    {
      if (vfs_path_cmp (temp, dir) == 0)
	return 1;
    }
  return 0;
}
