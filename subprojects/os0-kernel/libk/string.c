/*************************************************************************
 * string.c -- This file is part of OS/0.                                *
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

#include <libk/libk.h>

void *
memcpy (void *__restrict dest, const void *__restrict src, size_t len)
{
  size_t i;
  if ((u32) dest % 2 == 0 && (u32) src % 2 == 0 && len % 2 == 0)
    {
      for (i = 0; i < len / 2; i++)
	((u16 *) dest)[i] = ((u16 *) src)[i];
    }
  else
    {
      for (i = 0; i < len; i++)
	((u8 *) dest)[i] = ((u8 *) src)[i];
    }
  return dest;
}

void *
memmove (void *dest, const void *src, size_t len)
{
  size_t i;
  if (dest < src)
    return memcpy (dest, src, len);
  if ((u32) dest % 2 == 0 && (u32) src % 2 == 0 && len % 2 == 0)
    {
      for (i = len / 2; i > 0; i--)
	((u16 *) dest)[i - 1] = ((u16 *) src)[i - 1];
    }
  else
    {
      for (i = len; i > 0; i--)
	((u8 *) dest)[i - 1] = ((u8 *) src)[i - 1];
    }
  return dest;
}

void *
memset (void *ptr, int c, size_t len)
{
  size_t i;
  for (i = 0; i < len; i++)
    ((u8 *) ptr)[i] = c;
  return ptr;
}

int
memcmp (const void *a, const void *b, size_t len)
{
  u8 *ca = (u8 *) a;
  u8 *cb = (u8 *) b;
  size_t i;
  for (i = 0; i < len; i++)
    {
      if (ca[i] > cb[i])
	return 1;
      else if (ca[i] < cb[i])
	return -1;
    }
  return 0;
}

size_t
strlen (const char *s)
{
  size_t i = 0;
  while (s[i] != '\0')
    i++;
  return i;
}
