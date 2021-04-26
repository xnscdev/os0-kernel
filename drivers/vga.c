/*************************************************************************
 * vga.c -- This file is part of OS/0.                                   *
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
#include <video/vga.h>

static size_t vga_row;
static size_t vga_column;
static unsigned char vga_color;
static uint16_t *vga_buffer;
int vga_console;

void
vga_init (void)
{
  size_t x;
  size_t y;

  vga_color = vga_mkcolor (VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  vga_buffer = (uint16_t *) VGA_BUFFER;

  for (y = 0; y < VGA_SCREEN_HEIGHT; y++)
    {
      for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	vga_buffer[vga_getindex (x, y)] = vga_mkentry (' ', vga_color);
    }
  vga_console = 1;
}

unsigned char
vga_getcolor (void)
{
  return vga_color;
}

void
vga_setcolor (unsigned char color)
{
  vga_color = color;
}

void
vga_putentry (char c, size_t x, size_t y)
{
  vga_buffer[vga_getindex (x, y)] = vga_mkentry (c, vga_color);
}

void
vga_putchar (char c)
{
  __asm__ volatile ("cli");
  if (c == '\n')
    goto wrap;
  vga_putentry (c, vga_column, vga_row);
  if (++vga_column == VGA_SCREEN_WIDTH)
    {
    wrap:
      vga_column = 0;
      if (++vga_row == VGA_SCREEN_HEIGHT)
	{
	  size_t i;
	  for (i = VGA_SCREEN_WIDTH; i < VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT;
	       i++)
	    vga_buffer[i - VGA_SCREEN_WIDTH] = vga_buffer[i];
	  for (i = 0; i < VGA_SCREEN_WIDTH; i++)
	    vga_putentry (' ', i, VGA_SCREEN_HEIGHT - 1);
	  vga_row--;
	}
    }
  vga_setcurs (vga_column, vga_row);
  __asm__ volatile ("sti");
}

void
vga_write (const char *s, size_t size)
{
  int i;
  for (i = 0; i < size; i++)
    vga_putchar (s[i]);
}

void
vga_puts (const char *s)
{
  int written = 0;
  for (; *s != '\0'; s++, written++)
    vga_putchar (*s);
}

void
vga_setcurs (size_t x, size_t y)
{
  uint16_t pos = vga_getindex (x, y);
  outb (0x0f, VGA_PORT_INDEX);
  outb ((unsigned char) (pos & 0xff), VGA_PORT_DATA);
  outb (0x0e, VGA_PORT_INDEX);
  outb ((unsigned char) ((pos >> 8) & 0xff), VGA_PORT_DATA);
}
