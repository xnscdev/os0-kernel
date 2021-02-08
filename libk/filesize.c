/*************************************************************************
 * filesize.c -- This file is part of OS/0.                              *
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

static char itoa_buffer[32];

static char filesize_prefixes[] = {
  'B',
  'K',
  'M',
  'G',
  'T',
  'P',
  'E'
};

char *
format_filesize (u64 size, char *buffer)
{
  char *ptr;
  u16 value;
  int i;
  int p;

  if (size == 0)
    {
      strcpy (buffer, "0B");
      return buffer;
    }

  for (i = 6, p = 60; p >= 0; i--, p -= 10)
    {
      if (size >= 1 << p)
	{
	  value = size >> p;
	  break;
	}
    }

  utoa (value, itoa_buffer, 10);
  ptr = stpcpy (buffer, itoa_buffer);
  *ptr = filesize_prefixes[i];
  *++ptr = '\0';
  return buffer;
}
