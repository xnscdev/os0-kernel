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
  void **sa_elems;
  uint32_t sa_size;
  uint32_t sa_max;
  ComparePredicate sa_cmp;
} SortedArray;

__BEGIN_DECLS

int default_cmp (const void *a, const void *b);
int noop_cmp (const void *a, const void *b);

int sorted_array_place (SortedArray *array, void *addr, uint32_t maxsize,
			ComparePredicate cmp);
void sorted_array_destroy (SortedArray *array);
void sorted_array_insert (SortedArray *array, void *item);
void *sorted_array_lookup (SortedArray *array, uint32_t i);
void sorted_array_remove (SortedArray *array, uint32_t i);

__END_DECLS

#endif
