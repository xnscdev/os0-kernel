/*************************************************************************
 * serial.c -- This file is part of OS/0.                                *
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

#include <sys/io.h>
#include <video/serial.h>
#include <stdarg.h>
#include <stdio.h>

static int
serial_transmit_received (void)
{
  return inb (SERIAL_REG (1, SERIAL_REG_LINE_STATUS)) & 0x01;
}

static int
serial_transmit_empty (void)
{
  return inb (SERIAL_REG (1, SERIAL_REG_LINE_STATUS)) & 0x20;
}

void
serial_init (void)
{
  outb (0x00, SERIAL_REG (1, SERIAL_REG_INTERRUPT));
  outb (0x80, SERIAL_REG (1, SERIAL_REG_LINE_CONTROL));
  outb (0x03, SERIAL_REG (1, SERIAL_REG_DATA));
  outb (0x00, SERIAL_REG (1, SERIAL_REG_INTERRUPT));
  outb (0x03, SERIAL_REG (1, SERIAL_REG_LINE_CONTROL));
  outb (0xc7, SERIAL_REG (1, SERIAL_REG_CONTROL));
  outb (0x0b, SERIAL_REG (1, SERIAL_REG_MODEM_CONTROL));
  outb (0x1e, SERIAL_REG (1, SERIAL_REG_MODEM_CONTROL));
  outb (0xae, SERIAL_REG (1, SERIAL_REG_DATA));
  if (inb (SERIAL_REG (1, SERIAL_REG_DATA)) != 0xae)
    return;
  outb (0x0f, SERIAL_REG (1, SERIAL_REG_MODEM_CONTROL));
}

char
serial_read_byte (void)
{
  while (!serial_transmit_received ())
    ;
  return inb (SERIAL_REG (1, SERIAL_REG_DATA));
}

void
serial_write_byte (char c)
{
  while (!serial_transmit_empty ())
    ;
  outb (c, SERIAL_REG (1, SERIAL_REG_DATA));
}

void
serial_write_data (const void *data, size_t len)
{
  const char *buffer = data;
  while (len-- > 0)
    serial_write_byte (*buffer++);
}

int
serial_printf (const char *fmt, ...)
{
  int ret;
  va_list args;
  va_start (args, fmt);
  ret =
    __vprintk ((void (*) (const char *, size_t)) serial_write_data, fmt, args);
  va_end (args);
  return ret;
}
