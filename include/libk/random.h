/*************************************************************************
 * random.h -- This file is part of OS/0.                                *
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

#ifndef _LIBK_RANDOM_H
#define _LIBK_RANDOM_H

#include <libk/hash.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

extern unsigned char entropy_pool[SHA256_DIGEST_SIZE];

void add_entropy (const void *data, size_t len);
void get_entropy (void *data, size_t len);
void random_init (void);

__END_DECLS

#endif
