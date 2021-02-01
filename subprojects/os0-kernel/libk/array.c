/*************************************************************************
 * array.c -- This file is part of OS/0.                                 *
 * Copyright (C) 2020 XNSC                                               *
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

SortedArray
sorted_array_place (void *addr, u32 max, ComparePredicate cmp)
{
  SortedArray self;
  assert (addr != NULL);
  assert (cmp != NULL);
  self.sa_elems = (void **) addr;
  memset (self.sa_elems, 0, max * sizeof (void *));
  self.sa_size = 0;
  self.sa_max = max;
  self.sa_cmp = cmp;
  return self;
}

void
sorted_array_destroy (SortedArray *array)
{
}

void
sorted_array_insert (SortedArray *array, void *item)
{
  u32 i = 0;
  while (i < array->sa_size && array->sa_cmp (array->sa_elems[i], item))
    i++;
  if (i == array->sa_size)
    array->sa_elems[array->sa_size++] = item;
  else
    {
      void *temp = array->sa_elems[i];
      array->sa_elems[i] = item;
      while (i < array->sa_size)
	{
	  void *elem = array->sa_elems[++i];
	  array->sa_elems[i] = temp;
	  temp = elem;
	}
      array->sa_size++;
    }
}

void *
sorted_array_lookup (SortedArray *array, u32 i)
{
  if (array == NULL || i >= array->sa_size)
    return NULL;
  return array->sa_elems[i];
}

void
sorted_array_remove (SortedArray *array, u32 i)
{
  while (i < array->sa_size)
    {
      array->sa_elems[i] = array->sa_elems[i + 1];
      i++;
    }
  array->sa_size--;
}
