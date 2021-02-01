/*************************************************************************
 * array.h -- This file is part of OS/0.                                 *
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

#ifndef _LIBK_ARRAY_H
#define _LIBK_ARRAY_H

#include <libk/types.h>
#include <sys/cdefs.h>

typedef struct
{
  void **sa_elems;
  u32 sa_size;
  u32 sa_max;
  ComparePredicate sa_cmp;
} SortedArray;

__BEGIN_DECLS

SortedArray sorted_array_place (void *addr, u32 maxsize, ComparePredicate cmp);
void sorted_array_destroy (SortedArray *array);

__END_DECLS

#endif
