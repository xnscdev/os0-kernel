/*************************************************************************
 * array.c -- This file is part of OS/0.                                 *
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
#include <vm/heap.h>
#include <errno.h>

int
default_cmp (const void *a, const void *b)
{
  return (uint32_t) a < (uint32_t) b;
}

Array *
array_new (uint32_t max)
{
  Array *array = kmalloc (sizeof (Array));
  if (array == NULL)
    return NULL;
  array->a_elems = kmalloc (sizeof (void *) * max);
  if (array->a_elems == NULL)
    {
      kfree (array);
      return NULL;
    }
  array->a_size = 0;
  array->a_max = max;
  return array;
}

int
array_append (Array *array, void *elem)
{
  if (array == NULL || array->a_size == array->a_max)
    return -1;
  array->a_elems[array->a_size++] = elem;
  return 0;
}

void
array_destroy (Array *array, ReleasePredicate func, void *data)
{
  if (array == NULL)
    return;
  if (func != NULL)
    {
      uint32_t i;
      for (i = 0; i < array->a_size; i++)
	func (array->a_elems[i], data);
    }
  kfree (array->a_elems);
  kfree (array);
}

SortedArray *
sorted_array_new (uint32_t max, ComparePredicate cmp)
{
  SortedArray *array = kmalloc (sizeof (SortedArray));
  if (unlikely (array == NULL))
    return NULL;
  array->sa_elems = kmalloc (sizeof (void *) * max);
  if (unlikely (array->sa_elems == NULL))
    {
      kfree (array);
      return NULL;
    }
  memset (array->sa_elems, 0, sizeof (void *) * max);
  array->sa_size = 0;
  array->sa_max = max;
  array->sa_cmp = cmp;
  array->sa_alloc = 1;
  return array;
}

int
sorted_array_place (SortedArray *array, void *addr, uint32_t max,
		    ComparePredicate cmp)
{
  if (array == NULL || addr == NULL || cmp == NULL)
    return -EINVAL;
  array->sa_elems = (void **) addr;
  memset (array->sa_elems, 0, sizeof (void *) * max);
  array->sa_size = 0;
  array->sa_max = max;
  array->sa_cmp = cmp;
  array->sa_alloc = 0;
  return 0;
}

void
sorted_array_destroy (SortedArray *array, ReleasePredicate func, void *data)
{
  if (array == NULL)
    return;
  if (func != NULL)
    {
      uint32_t i;
      for (i = 0; i < array->sa_size; i++)
	func (array->sa_elems[i], data);
    }
  if (array->sa_alloc)
    kfree (array->sa_elems);
  kfree (array);
}

void
sorted_array_insert (SortedArray *array, void *item)
{
  uint32_t i = 0;
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
sorted_array_lookup (SortedArray *array, uint32_t i)
{
  if (array == NULL || i >= array->sa_size)
    return NULL;
  return array->sa_elems[i];
}

void
sorted_array_remove (SortedArray *array, uint32_t i)
{
  while (i < array->sa_size)
    {
      array->sa_elems[i] = array->sa_elems[i + 1];
      i++;
    }
  array->sa_size--;
}
