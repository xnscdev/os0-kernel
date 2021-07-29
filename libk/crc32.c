/*************************************************************************
 * crc32.c -- This file is part of OS/0.                                 *
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

#include <libk/hash.h>

static uint32_t crc32_table[0x100];
static uint32_t crc32_wtable[0x400];

static uint32_t
crc32_byte (uint32_t b)
{
  int i;
  for (i = 0; i < 8; i++)
    b = (b & 1 ? 0 : 0x82f63b78) ^ b >> 1;
  return b ^ 0xff000000;
}

static void
crc32_init_tables (uint32_t *table, uint32_t *wtable)
{
  size_t i;
  size_t k;
  size_t w;
  size_t j;
  for (i = 0; i < 0x100; i++)
    table[i] = crc32_byte (i);
  for (k = 0; k < 4; k++)
    {
      for (w = 0, i = 0; i < 0x100; i++)
	{
	  for (j = 0, w = 0; j < 4; j++)
	    w = table[(unsigned char) (j == k ? w ^ i : w)] ^ w >> 8;
	  wtable[(k << 8) + i] = w ^ (k ? wtable[0] : 0);
	}
    }
}

uint32_t
crc32 (uint32_t seed, const void *data, size_t len)
{
  size_t naccum = len / 4;
  size_t i;
  uint32_t crc = seed;
  if (crc32_table[0] == 0)
    crc32_init_tables (crc32_table, crc32_wtable);
  for (i = 0; i < naccum; i++)
    {
      uint32_t a = crc ^ ((uint32_t *) data)[i];
      size_t j;
      for (j = 0, crc = 0; j < 4; j++)
	crc ^= crc32_wtable[(j << 8) + (unsigned char) (a >> 8 * j)];
    }
  for (i = naccum * 4; i < len; i++)
    crc = crc32_table[(unsigned char) crc + ((unsigned char *) data)[i]] ^
      crc >> 8;
  return crc;
}
