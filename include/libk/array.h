/*************************************************************************
 * array.h -- This file is part of OS/0.                                 *
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

#ifndef _LIBK_ARRAY_H
#define _LIBK_ARRAY_H

#include <libk/types.h>
#include <sys/cdefs.h>

typedef struct
{
  void **a_elems;
  uint32_t a_size;
  uint32_t a_max;
} Array;

typedef struct
{
  void **sa_elems;
  uint32_t sa_size;
  uint32_t sa_max;
  ComparePredicate sa_cmp;
  unsigned char sa_alloc;
} SortedArray;

__BEGIN_DECLS

int default_cmp (const void *a, const void *b);

Array *array_new (uint32_t max);
int array_append (Array *array, void *elem);
void array_destroy (Array *array, ReleasePredicate func, void *data);

SortedArray *sorted_array_new (uint32_t max, ComparePredicate cmp);
int sorted_array_place (SortedArray *array, void *addr, uint32_t max,
			ComparePredicate cmp);
void sorted_array_destroy (SortedArray *array, ReleasePredicate func,
			   void *data);
void sorted_array_insert (SortedArray *array, void *item);
void *sorted_array_lookup (SortedArray *array, uint32_t i);
void sorted_array_remove (SortedArray *array, uint32_t i);

__END_DECLS

#endif
