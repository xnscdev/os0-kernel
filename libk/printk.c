/*************************************************************************
 * printk.c -- This file is part of OS/0.                                *
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
#include <video/vga.h>
#include <limits.h>

static char itoa_buffer[32];

#ifndef TEST

static void
__vga_write (const char *str, size_t len)
{
  vga_write (CURRENT_TTY, str, len);
}

#endif

int
__vprintk (void (*write) (const char *, size_t), const char *fmt, va_list args)
{
  int written = 0;
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
	  write (fmt, amt);
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
	  write (&c, 1);
	  fmt++;
	}
      else if (*fmt == 's')
	{
	  const char *s = va_arg (args, const char *);
	  size_t len = strlen (s);
	  if (maxrem < len)
	    return -1;
	  write (s, len);
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
	  write (itoa_buffer, len);
	  fmt++;
	}
      else if (*fmt == 'o')
	{
	  unsigned int n = va_arg (args, unsigned int);
	  size_t len;
	  utoa (n, itoa_buffer, 8);
	  len = strlen (itoa_buffer);
	  if (maxrem < len)
	    return -1;
	  write (itoa_buffer, len);
	  fmt++;
	}
      else if (*fmt == 'p')
	{
	  void *ptr = va_arg (args, void *);
	  uint32_t addr = (uint32_t) ptr;
	  size_t len;
	  utoa (addr, itoa_buffer, 16);
	  len = strlen (itoa_buffer);
	  if (maxrem < len + 2)
	    return -1;
	  write ("0x", 2);
	  write (itoa_buffer, len);
	  fmt++;
	}
      else if (*fmt == 'u')
	{
	  unsigned int n = va_arg (args, unsigned int);
	  size_t len;
	  utoa (n, itoa_buffer, 10);
	  len = strlen (itoa_buffer);
	  if (maxrem < len)
	    return -1;
	  write (itoa_buffer, len);
	  fmt++;
	}
      else if (*fmt == 'x' || *fmt == 'X')
	{
	  unsigned int n = va_arg (args, unsigned int);
	  size_t len;
	  if (*fmt == 'x')
	    utoa (n, itoa_buffer, 16);
	  else
	    utoa_u (n, itoa_buffer, 16);
	  len = strlen (itoa_buffer);
	  if (maxrem < len)
	    return -1;
	  write (itoa_buffer, len);
	  fmt++;
	}
      else if (strncmp (fmt, "ld", 2) == 0 || strncmp (fmt, "li", 2) == 0)
	{
	  long n = va_arg (args, long);
	  size_t len;
	  itoa (n, itoa_buffer, 10);
	  len = strlen (itoa_buffer);
	  if (maxrem < len)
	    return -1;
	  write (itoa_buffer, len);
	  fmt += 2;
	}
      else if (strncmp (fmt, "lo", 2) == 0)
	{
	  unsigned long n = va_arg (args, unsigned long);
	  size_t len;
	  utoa (n, itoa_buffer, 8);
	  len = strlen (itoa_buffer);
	  if (maxrem < len)
	    return -1;
	  write (itoa_buffer, len);
	  fmt += 2;
	}
      else if (strncmp (fmt, "lu", 2) == 0)
	{
	  unsigned long n = va_arg (args, unsigned long);
	  size_t len;
	  utoa (n, itoa_buffer, 10);
	  len = strlen (itoa_buffer);
	  if (maxrem < len)
	    return -1;
	  write (itoa_buffer, len);
	  fmt += 2;
	}
      else if (strncmp (fmt, "lx", 2) == 0)
	{
	  unsigned long n = va_arg (args, unsigned long);
	  size_t len;
	  if (*fmt == 'x')
	    utoa (n, itoa_buffer, 16);
	  else
	    utoa_u (n, itoa_buffer, 16);
	  len = strlen (itoa_buffer);
	  if (maxrem < len)
	    return -1;
	  write (itoa_buffer, len);
	  fmt += 2;
	}
      else
	{
	  size_t len;
	  fmt = fmt_begin;
	  len = strlen (fmt);
	  if (maxrem < len)
	    return -1;
	  write (fmt, len);
	  written += len;
	  fmt += len;
	}
    }
  return written;
}

#ifndef TEST

int
printk (const char *__restrict fmt, ...)
{
  int ret;
  va_list args;
  va_start (args, fmt);
  ret = vprintk (fmt, args);
  va_end (args);
  return ret;
}

int
vprintk (const char *fmt, va_list args)
{
  return __vprintk (__vga_write, fmt, args);
}

#endif
