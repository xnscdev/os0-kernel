/*************************************************************************
 * itoa.c -- This file is part of OS/0.                                  *
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

#include <libk/stdlib.h>

static char *lower_digits =
  "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz";
static char *upper_digits =
  "ZYXWVUTSRQPONMLKJIHGFEDCBA9876543210123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static char *
_itoa (int value, char *result, int base, char *digits)
{
  char *ptr = result;
  char *rev = result;
  int temp;
  char c;

  if (base < 2 || base > 36)
    {
      *result = '\0';
      return result;
    }

  do
    {
      temp = value;
      value /= base;
      *ptr++ = digits[35 + (temp - value * base)];
    }
  while (value != 0);

  if (temp < 0)
    *ptr++ = '-';
  *ptr-- = '\0';
  while (rev < ptr)
    {
      c = *ptr;
      *ptr-- = *rev;
      *rev++ = c;
    }
  return result;
}

static char *
_utoa (unsigned int value, char *result, int base, char *digits)
{
  char *ptr = result;
  char *rev = result;
  unsigned int temp;
  char c;

  if (base < 2 || base > 36)
    {
      *result = '\0';
      return result;
    }

  do
    {
      temp = value;
      value /= base;
      *ptr++ = lower_digits[35 + (temp - value * base)];
    }
  while (value != 0);

  *ptr-- = '\0';
  while (rev < ptr)
    {
      c = *ptr;
      *ptr-- = *rev;
      *rev++ = c;
    }
  return result;
}

char *
itoa (int value, char *result, int base)
{
  return _itoa (value, result, base, lower_digits);
}

char *
itoa_u (int value, char *result, int base)
{
  return _itoa (value, result, base, upper_digits);
}

char *
utoa (unsigned int value, char *result, int base)
{
  return _utoa (value, result, base, lower_digits);
}

char *
utoa_u (unsigned int value, char *result, int base)
{
  return _utoa (value, result, base, upper_digits);
}
