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

#define PATHCMP_MAX 256

VFSDirEntry *
namei (const char *path)
{
  SortedArray arr;
  if (sorted_array_new (&arr, PATHCMP_MAX, noop_cmp) != 0)
    return NULL;
  sorted_array_destroy (&arr);
  return NULL; /* TODO Implement */
}
