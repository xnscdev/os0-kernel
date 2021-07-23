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

const char *const sys_errlist[] = {
  [EPERM] = "Operation not permitted",
  [ENOENT] = "No such file or directory",
  [ESRCH] = "No such process",
  [EINTR] = "Interrupted system call",
  [EIO] = "Input/output error",
  [ENXIO] = "No such device or address",
  [E2BIG] = "Argument list too long",
  [ENOEXEC] = "Exec format error",
  [EBADF] = "Bad file descriptor",
  [ECHILD] = "No child processes",
  [EAGAIN] = "Resource temporarily unavailable",
  [ENOMEM] = "Cannot allocate memory",
  [EACCES] = "Permission denied",
  [EFAULT] = "Bad address",
  [ENOTBLK] = "Block device required",
  [EBUSY] = "Device or resource busy",
  [EEXIST] = "File exists",
  [EXDEV] = "Invalid cross-device link",
  [ENODEV] = "No such device",
  [ENOTDIR] = "Not a directory",
  [EISDIR] = "Is a directory",
  [EINVAL] = "Invalid argument",
  [ENFILE] = "Too many open files",
  [EMFILE] = "Too many open files in system",
  [ENOTTY] = "Inappropriate ioctl for device",
  [ETXTBSY] = "Text file busy",
  [EFBIG] = "File too large",
  [ENOSPC] = "No space left on device",
  [ESPIPE] = "Illegal seek",
  [EROFS] = "Read-only file system",
  [EMLINK] = "Too many links",
  [EPIPE] = "Broken pipe",
  [EDOM] = "Numerical argument out of domain",
  [ERANGE] = "Numerical result out of range",
  [EDEADLK] = "Resource deadlock avoided",
  [ENAMETOOLONG] = "File name too long",
  [ENOLCK] = "No locks available",
  [ENOSYS] = "Function not implemented",
  [ENOTEMPTY] = "Directory not empty",
  [ELOOP] = "Too many levels of symbolic links",
  [ENOSTR] = "Device not a stream",
  [ENODATA] = "No data available",
  [ETIME] = "Timer expired",
  [ENOSR] = "Out of streams resources",
  [ENONET] = "Machine is not on the network",
  [ENOPKG] = "Package not installed",
  [EREMOTE] = "Remote I/O error",
  [ENOLINK] = "Link has been severed",
  [EADV] = "Advertise error",
  [ESRMNT] = "Srmount error",
  [ECOMM] = "Communication error on send",
  [EPROTO] = "Protocol error",
  [EMULTIHOP] = "Multihop attempted",
  [EOVERFLOW] = "Value too large for defined data type",
  [ENOTUNIQ] = "Name not unique on the network",
  [EBADFD] = "File descriptor in bad state",
  [EREMCHG] = "Remote address changed",
  [ELIBACC] = "Can not access a needed shared library", /* sic */
  [ELIBBAD] = "Accessing a corrupt shared library",
  [ELIBMAX] = "Attempting to link in too many shared libraries",
  [ELIBEXEC] = "Cannot exec a shared library directly",
  [EILSEQ] = "Invalid or incomplete multibyte or wide character",
  [ERESTART] = "Interrupted system call should be restarted",
  [ESTRPIPE] = "Streams pipe error",
  [EUSERS] = "Too many users",
  [ENOTSOCK] = "Socket operation on non-socket",
  [EDESTADDRREQ] = "Destination address required",
  [EMSGSIZE] = "Message too long",
  [EPROTOTYPE] = "Protocol wrong type for socket",
  [ENOPROTOOPT] = "Protocol not available",
  [EPROTONOSUPPORT] = "Protocol not supported",
  [ESOCKTNOSUPPORT] = "Socket type not supported",
  [EOPNOTSUPP] = "Operation not supported",
  [EPFNOSUPPORT] = "Protocol family not supported",
  [EAFNOSUPPORT] = "Address family not supported by protocol",
  [EADDRINUSE] = "Address already in use",
  [EADDRNOTAVAIL] = "Cannot assign requested address",
  [ENETDOWN] = "Network is down",
  [ENETUNREACH] = "Network is unreachable",
  [ENETRESET] = "Network dropped connection on reset",
  [ECONNABORTED] = "Software caused connection abort",
  [ECONNRESET] = "Connection reset by peer",
  [ENOBUFS] = "No buffer space available",
  [EISCONN] = "Transport endpoint is already connected",
  [ENOTCONN] = "Transport endpoint is not connected",
  [ESHUTDOWN] = "Cannot send after transport endpoint shutdown",
  [ETOOMANYREFS] = "Too many references: cannot splice",
  [ETIMEDOUT] = "Connection timed out",
  [ECONNREFUSED] = "Connection refused",
  [EHOSTDOWN] = "Host is down",
  [EHOSTUNREACH] = "No route to host",
  [EALREADY] = "Operation already in progress",
  [EINPROGRESS] = "Operation now in progress",
  [ESTALE] = "Stale file handle",
  [EUCLEAN] = "Structure needs cleaning",
  [EREMOTEIO] = "Remote I/O error",
  [EDQUOT] = "Disk quota exceeded"
};

const int sys_nerr = __NR_errno;

const char *const sys_siglist[NSIG] = {
  NULL,
  "Hangup",
  "Interrupt",
  "Quit",
  "Illegal instruction",
  "Trace/breakpoint trap",
  "Aborted",
  "Bus error",
  "Floating point exception",
  "Killed",
  "User defined signal 1",
  "Segmentation fault",
  "User defined signal 2",
  "Broken pipe",
  "Alarm clock",
  "Stack fault",
  "Child exited",
  "Continued",
  "Stopped (signal)",
  "Stopped",
  "Stopped (tty input)",
  "Stopped (tty output)",
  "Urgent I/O condition",
  "CPU time limit exceeded",
  "File size limit exceeded",
  "Virtual timer expired",
  "Profiling timer expired",
  "Window changed",
  "I/O possible",
  "Power failure",
  "Bad system call"
};

const char *const sys_signame[NSIG] = {
  NULL,
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
strerror (int err)
{
  err = -err;
  if (err < 0 || err > sys_nerr)
    return NULL;
  return sys_errlist[err];
}
