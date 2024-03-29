/*************************************************************************
 * stdio.h -- This file is part of OS/0.                                 *
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

#ifndef _STDIO_H
#define _STDIO_H

#include <sys/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

__BEGIN_DECLS

int __vprintk (void (*write) (const char *, size_t), const char *fmt,
	       va_list args);
int printk (const char *__restrict fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
int vprintk (const char *fmt, va_list args);

__END_DECLS

#endif
