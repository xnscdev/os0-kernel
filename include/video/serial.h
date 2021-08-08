/*************************************************************************
 * serial.h -- This file is part of OS/0.                                *
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

#ifndef _VIDEO_SERIAL_H
#define _VIDEO_SERIAL_H

#include <sys/cdefs.h>
#include <stddef.h>

#define SERIAL_COM1 0x3f8
#define SERIAL_COM2 0x2f8
#define SERIAL_COM3 0x3e8
#define SERIAL_COM4 0x2e8

#define SERIAL_REG_DATA          0
#define SERIAL_REG_INTERRUPT     1
#define SERIAL_REG_CONTROL       2
#define SERIAL_REG_LINE_CONTROL  3
#define SERIAL_REG_MODEM_CONTROL 4
#define SERIAL_REG_LINE_STATUS   5
#define SERIAL_REG_MODEM_STATUS  6
#define SERIAL_REG_SCRATCH       7

#define SERIAL_REG(com, reg) (SERIAL_COM ## com + (reg))

__BEGIN_DECLS

void serial_init (void);
char serial_read_byte (void);
void serial_write_byte (char c);
void serial_write_data (const void *data, size_t len);
int serial_printf (const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));

__END_DECLS

#endif
