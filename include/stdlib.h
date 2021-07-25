/*************************************************************************
 * stdlib.h -- This file is part of OS/0.                                *
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

#ifndef _STDLIB_H
#define _STDLIB_H

#include <libk/compile.h>
#include <libk/types.h>
#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

int atoi (const char *str);
long atol (const char *str);
long long atoll (const char *str);

char *itoa (int value, char *result, int base);
char *itoa_u (int value, char *result, int base);
char *utoa (unsigned int value, char *result, int base);
char *utoa_u (unsigned int value, char *result, int base);

void qsort (void *const pbase, size_t len, size_t size, ComparePredicate cmp);

uint16_t crc16 (uint16_t seed, const void *data, size_t len);
uint32_t crc32 (uint32_t seed, const void *data, size_t len);

void low_abort (const char *msg) __low_text __attribute__ ((noreturn));
void panic (const char *__restrict fmt, ...)
  __attribute__ ((noreturn, cold, format (printf, 1, 2)));
void halt (void) __attribute__ ((noreturn, cold));

__END_DECLS

#endif
