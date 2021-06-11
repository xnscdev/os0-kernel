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

static const char *errno_names[] = {
  NULL,
  "Operation not permitted",
  "No such file or directory",
  "No such process",
  "Interrupted system call",
  "Input/output error",
  "No such device or address",
  "Argument list too long",
  "Exec format error",
  "Bad file descriptor",
  "No child processes",
  "Resource temporarily unavailable",
  "Cannot allocate memory",
  "Permission denied",
  "Bad address",
  "Block device required",
  "Device or resource busy",
  "File exists",
  "Invalid cross-device link",
  "No such device",
  "Not a directory",
  "Is a directory",
  "Invalid argument",
  "Too many open files",
  "Too many open files in system",
  "Inappropriate ioctl for device",
  "Text file busy",
  "File too large",
  "No space left on device",
  "Illegal seek",
  "Read-only file system",
  "Too many links",
  "Broken pipe",
  "Numerical argument out of domain",
  "Numerical result out of range",
  "Resource deadlock avoided",
  "File name too long",
  "No locks available",
  "Function not implemented",
  "Directory not empty",
  "Too many levels of symbolic links",
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  "Device not a stream",
  "No data available",
  "Timer expired",
  "Out of streams resources",
  "Machine is not on the network",
  "Package not installed",
  "Remote I/O error",
  "Link has been severed",
  "Advertise error",
  "Srmount error",
  "Communication error on send",
  "Protocol error",
  "Multihop attempted",
  NULL,
  NULL,
  "Value too large for defined data type",
  "Name not unique on the network",
  "File descriptor in bad state",
  "Remote address changed",
  "Can not access a needed shared library" /* sic */
  "Accessing a corrupt shared library",
  NULL,
  "Attempting to link in too many shared libraries",
  "Cannot exec a shared library directly",
  "Invalid or incomplete multibyte or wide character",
  "Interrupted system call should be restarted",
  "Streams pipe error",
  "Too many users",
  "Socket operation on non-socket",
  "Destination address required",
  "Message too long",
  "Protocol wrong type for socket",
  "Protocol not available",
  "Protocol not supported",
  "Socket type not supported",
  "Operation not supported",
  "Protocol family not supported",
  "Address family not supported by protocol",
  "Address already in use",
  "Cannot assign requested address",
  "Network is down",
  "Network is unreachable",
  "Network dropped connection on reset",
  "Software caused connection abort",
  "Connection reset by peer",
  "No buffer space available",
  "Transport endpoint is already connected",
  "Transport endpoint is not connected",
  "Cannot send after transport endpoint shutdown",
  "Too many references: cannot splice",
  "Connection timed out",
  "Connection refused",
  "Host is down",
  "No route to host",
  "Operation already in progress",
  "Operation now in progress",
  "Stale file handle",
  "Structure needs cleaning",
  NULL,
  NULL,
  NULL,
  "Remote I/O error",
  "Disk quota exceeded"
};

const char *const sys_signames[NR_signals] = {
  "SIGHUP",
  "SIGINT",
  "SIGQUIT",
  "SIGILL",
  "SIGTRAP",
  "SIGABRT",
  "SIGBUS",
  "SIGFPE",
  "SIGKILL",
  "SIGUSR1",
  "SIGSEGV",
  "SIGUSR2",
  "SIGPIPE",
  "SIGALRM",
  "SIGTERM",
  "SIGSTKFLT",
  "SIGCHLD",
  "SIGCONT",
  "SIGSTOP",
  "SIGTSTP",
  "SIGTTIN",
  "SIGTTOU",
  "SIGURG",
  "SIGXCPU",
  "SIGXFSZ",
  "SIGVTALRM",
  "SIGPROF",
  "SIGWINCH",
  "SIGIO",
  "SIGPWR",
  "SIGSYS"
};

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
  else
    s = strtok_ptr;
  if (strtok_save != '\0')
    *strtok_ptr = strtok_save; /* Restore state from previous call */

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
	  strtok_ptr = end;
	  *end = '\0';
	  return ptr;
	}
    }
  strtok_save = '\0';
  strtok_ptr = end;
  return ptr;
}

const char *
strerror (int errno)
{
  if (errno >= 0 || errno < -(sizeof (errno_names) / sizeof (errno_names[0])))
    return NULL;
  return errno_names[-errno];
}
