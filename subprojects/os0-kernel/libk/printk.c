/*************************************************************************
 * printk.c -- This file is part of OS/0.                                *
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
#include <video/vga.h>
#include <limits.h>
#include <stdarg.h>

static char itoa_buffer[32];

int
printk (const char *__restrict fmt, ...)
{
  va_list args;
  int written = 0;
  va_start (args, fmt);

  while (*fmt != '\0')
    {
      size_t maxrem = INT_MAX - written;
      const char *fmt_begin;
      if (fmt[0] != '%' || fmt[1] == '%')
	{
	  size_t amt = 1;
	  if (*fmt == '%')
	    fmt++;
	  while (fmt[amt] != '\0' && fmt[amt] != '%')
	    amt++;
	  if (maxrem < amt)
	    return -1;
	  vga_write (fmt, amt);
	  fmt += amt;
	  written += amt;
	  continue;
	}

      fmt_begin = fmt++;
      if (*fmt == 'c')
	{
	  char c = (char) va_arg (args, int);
	  if (maxrem == 0)
	    return -1;
	  vga_putchar (c);
	  fmt++;
	}
      else if (*fmt == 's')
	{
	  const char *s = va_arg (args, const char *);
	  size_t len = strlen (s);
	  if (maxrem < len)
	    return -1;
	  vga_write (s, len);
	  fmt++;
	}
      else if (*fmt == 'd' || *fmt == 'i')
	{
	  int n = va_arg (args, int);
	  size_t len;
	  itoa (n, itoa_buffer, 10);
	  len = strlen (itoa_buffer);
	  if (maxrem < len)
	    return -1;
	  vga_write (itoa_buffer, len);
	  fmt++;
	}
      else if (*fmt == 'x')
	{
	  int n = va_arg (args, int);
	  size_t len;
	  itoa (n, itoa_buffer, 16);
	  len = strlen (itoa_buffer);
	  if (maxrem < len)
	    return -1;
	  vga_write (itoa_buffer, len);
	  fmt++;
	}
      else
	{
	  size_t len;
	  fmt = fmt_begin;
	  len = strlen (fmt);
	  if (maxrem < len)
	    return -1;
	  vga_write (fmt, len);
	  written += len;
	  fmt += len;
	}
    }

  va_end (args);
  return written;
}
