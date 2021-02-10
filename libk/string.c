/*************************************************************************
 * string.c -- This file is part of OS/0.                                *
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
#include <vm/heap.h>

static char strtok_save;
static char *strtok_ptr;
static char *strtok_end;

void *
memcpy (void *__restrict dest, const void *__restrict src, size_t len)
{
  size_t i;
  if ((uint32_t) dest % 2 == 0 && (uint32_t) src % 2 == 0 && len % 2 == 0)
    {
      for (i = 0; i < len / 2; i++)
	((uint16_t *) dest)[i] = ((uint16_t *) src)[i];
    }
  else
    {
      for (i = 0; i < len; i++)
	((unsigned char *) dest)[i] = ((unsigned char *) src)[i];
    }
  return dest;
}

void *
memmove (void *dest, const void *src, size_t len)
{
  size_t i;
  if (dest < src)
    return memcpy (dest, src, len);
  if ((uint32_t) dest % 2 == 0 && (uint32_t) src % 2 == 0 && len % 2 == 0)
    {
      for (i = len / 2; i > 0; i--)
	((uint16_t *) dest)[i - 1] = ((uint16_t *) src)[i - 1];
    }
  else
    {
      for (i = len; i > 0; i--)
	((unsigned char *) dest)[i - 1] = ((unsigned char *) src)[i - 1];
    }
  return dest;
}

void *
memset (void *ptr, int c, size_t len)
{
  size_t i;
  for (i = 0; i < len; i++)
    ((unsigned char *) ptr)[i] = c;
  return ptr;
}

int
memcmp (const void *a, const void *b, size_t len)
{
  unsigned char *ca = (unsigned char *) a;
  unsigned char *cb = (unsigned char *) b;
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

char *
strdup (const char *s)
{
  size_t len = strlen (s);
  char *buffer = kmalloc (len + 1);
  if (unlikely (buffer == NULL))
    return NULL;
  strcpy (buffer, s);
  return buffer;
}

char *
stpcpy (char *dest, const char *src)
{
  while (1)
    {
      *dest = *src++;
      if (*dest == '\0')
	return dest;
      dest++;
    }
}

char *
stpncpy (char *dest, const char *src, size_t len)
{
  size_t i = 0;
  while (i < len)
    {
      *dest = *src++;
      if (*dest == '\0')
        break;
      dest++;
      i++;
    }
  while (i < len)
    dest[i++] = '\0';
  return dest;
}

char *
strcpy (char *dest, const char *src)
{
  size_t i = 0;
  while (1)
    {
      dest[i] = src[i];
      if (dest[i] == '\0')
	return dest;
      i++;
    }
}

char *
strncpy (char *dest, const char *src, size_t len)
{
  size_t i = 0;
  while (i < len)
    {
      dest[i] = src[i];
      if (dest[i] == '\0')
        break;
      i++;
    }
  while (i < len)
    dest[i++] = '\0';
  return dest;
}

int
strcmp (const char *a, const char *b)
{
  size_t i = 0;
  while (1)
    {
      if (a[i] > b[i])
	return 1;
      if (a[i] < b[i])
	return -1;
      if (a[i] == '\0' && b[i] == '\0')
	return 0;
      i++;
    }
}

int
strncmp (const char *a, const char *b, size_t len)
{
  size_t i;
  for (i = 0; i < len; i++)
    {
      if (a[i] > b[i])
	return 1;
      if (a[i] < b[i])
	return -1;
    }
  return 0;
}

char *
strchr (const char *s, int c)
{
  const char *ptr = s;
  if (s == NULL)
    return NULL;
  while (1)
    {
      if (*ptr == c)
	return (char *) ptr;
      if (*ptr == '\0')
	return NULL;
      ptr++;
    }
}

char *
strrchr (const char *s, int c)
{
  const char *ptr = s;
  const char *save = NULL;
  if (s == NULL)
    return NULL;
  while (1)
    {
      if (*ptr == c)
	{
	  if (c == '\0')
	    return (char *) ptr;
	  save = ptr;
	}
      else if (*ptr == '\0')
	return (char *) save;
      ptr++;
    }
}

char *
strtok (char *__restrict s, const char *__restrict delims)
{
  size_t i;
  char *ptr = NULL;
  char *end;
  if (s != NULL)
    strtok_ptr = s;
  else if (strtok_ptr == NULL)
    return NULL; /* No string initialized */
  if (strtok_save != '\0')
    *strtok_end = strtok_save; /* Restore state from previous call */

  for (end = s; *end != '\0'; end++)
    {
      int is_delim = 0;
      for (i = 0; delims[i] != '\0'; i++)
	{
	  if (*end == delims[i])
	    {
	      is_delim = 1;
	      break;
	    }
	}

      if (!is_delim && ptr == NULL)
	ptr = end;
      else if (is_delim && ptr != NULL)
	{
	  strtok_save = *end;
	  strtok_ptr = ptr;
	  strtok_end = end;
	  *end = '\0';
	  return ptr;
	}
    }
  return NULL;
}
