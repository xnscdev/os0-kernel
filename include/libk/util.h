/*************************************************************************
 * util.h -- This file is part of OS/0.                                  *
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

#ifndef _LIBK_UTIL_H
#define _LIBK_UTIL_H

#include <sys/cdefs.h>
#include <stdint.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

__BEGIN_DECLS

static inline uint32_t
div32_ceil (uint32_t a, uint32_t b)
{
  if (a == 0)
    return 0;
  return (a - 1) / b + 1;
}

static inline uint64_t
div64_ceil (uint64_t a, uint64_t b)
{
  if (a == 0)
    return 0;
  return (a - 1) / b + 1;
}

static inline void
fast_set_bit (void *ptr, uint64_t bit)
{
  unsigned char *addr = ptr;
  addr[bit >> 3] |= (unsigned char) (1 << (bit & 7));
}

static inline void
fast_clear_bit (void *ptr, uint64_t bit)
{
  unsigned char *addr = ptr;
  addr[bit >> 3] &= (unsigned char) ~(1 << (bit & 7));
}

static inline int
fast_test_bit (const void *ptr, uint64_t bit)
{
  const unsigned char *addr = ptr;
  return (1 << (bit & 7)) & addr[bit >> 3];
}

__END_DECLS

#endif
