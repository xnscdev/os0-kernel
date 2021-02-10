/*************************************************************************
 * namei.c -- This file is part of OS/0.                                 *
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

#include <libk/compile.h>
#include <sys/namei.h>
#include <vm/heap.h>
#include <string.h>

static NameiData *
namei_new (NameiData *parent, const char *path, const char *name, int level)
{
  NameiData *ni;
  if (name == NULL)
    return NULL;
  ni = kzalloc (sizeof (NameiData));
  if (unlikely (ni == NULL))
    return NULL;
  ni->nd_level = level;
  ni->nd_name = strdup (name);

  /* TODO Fill stat structure */
  return ni;
}

NameiData *
namei (NameiData *parent, const char *path, int start, NameiData **last)
{
  NameiData *ni = NULL;
  NameiData *first = NULL;
  char *name;
  char *end;
  char *temp;
  int level = 0;

  if (path == NULL)
    return NULL;
  if (parent != NULL)
    {
      ni = parent;
      level = parent->nd_level + 1;
    }
  temp = strdup (path);
  name = temp + start;

  if (*name == '/')
    {
      while (*name == '/')
	name++;
      first = ni = namei_new (ni, "/", "/", level);
    }

  for (end = name; name != NULL && end != NULL;)
    {
      if (*name != '\0')
	{
	  end = strchr (name, '/');
	  if (end != NULL)
	    *end = '\0';
	  ni = namei_new (ni, temp, name, level);
	}
      else
	end = NULL;
      if (first == NULL)
	first = ni;
      if (end != NULL)
	{
	  *end++ = '/';
	  while (*end == '/')
	    end++;
	}
      name = end;
    }

  if (last != NULL)
    *last = ni;
  kfree (temp);
  return first;
}
