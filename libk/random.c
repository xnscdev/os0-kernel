/*************************************************************************
 * random.c -- This file is part of OS/0.                                *
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

unsigned char entropy_pool[SHA256_DIGEST_SIZE];

void
add_entropy (const void *data, size_t len)
{
  SHA256Context ctx;
  unsigned char buffer[SHA256_DIGEST_SIZE];
  memcpy (buffer, entropy_pool, SHA256_DIGEST_SIZE);
  sha256_init (&ctx, buffer);
  sha256_write (&ctx, entropy_pool, SHA256_DIGEST_SIZE);
  sha256_write (&ctx, data, len);
  sha256_close (&ctx);
  memcpy (entropy_pool, buffer, SHA256_DIGEST_SIZE);
}

void
get_entropy (void *data, size_t len)
{
  unsigned char buffer[SHA256_DIGEST_SIZE];
  while (len > 0)
    {
      size_t l;
      sha256_data (buffer, entropy_pool, SHA256_DIGEST_SIZE);
      add_entropy (buffer, SHA256_DIGEST_SIZE);

      l = len < SHA256_DIGEST_SIZE ? len : SHA256_DIGEST_SIZE;
      memcpy (data, buffer, l);
      data += l;
      len -= l;
    }
}

void
random_init (void)
{
  time_t t = time (NULL);
  sha256_data (entropy_pool, &t, sizeof (time_t));
}
