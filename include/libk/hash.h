/*************************************************************************
 * hash.h -- This file is part of OS/0.                                  *
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

#ifndef _LIBK_HASH_H
#define _LIBK_HASH_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_SIZE 32
#define SHA256_CHUNK_SIZE  64

typedef struct
{
  unsigned char *s_digest;
  unsigned char s_chunk[SHA256_CHUNK_SIZE];
  unsigned char *s_ptr;
  size_t s_rem;
  size_t s_len;
  uint32_t s_h[8];
} SHA256Context;

__BEGIN_DECLS

uint16_t crc16 (uint16_t seed, const void *data, size_t len);
uint32_t crc32 (uint32_t seed, const void *data, size_t len);

void sha256_init (SHA256Context *ctx, unsigned char digest[SHA256_DIGEST_SIZE]);
void sha256_write (SHA256Context *ctx, const void *data, size_t len);
unsigned char *sha256_close (SHA256Context *ctx);
void sha256_data (unsigned char digest[SHA256_DIGEST_SIZE], const void *data,
		  size_t len);

__END_DECLS

#endif
