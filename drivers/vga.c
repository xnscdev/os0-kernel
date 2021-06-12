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
#include <string.h>

uint16_t *vga_hdw_buf = (uint16_t *) VGA_BUFFER;

void
vga_putentry (Terminal *term, char c, size_t x, size_t y)
{
  term->vt_data[vga_getindex (x, y)] = vga_mkentry (c, term->vt_color);
  if (term == CURRENT_TERMINAL)
    vga_hdw_buf[vga_getindex (x, y)] = vga_mkentry (c, term->vt_color);
}

void
vga_putchar (Terminal *term, char c)
{
  __asm__ volatile ("cli");
  if (c == '\n')
    goto wrap;
  vga_putentry (term, c, term->vt_column, term->vt_row);
  if (++term->vt_column == VGA_SCREEN_WIDTH)
    {
    wrap:
      term->vt_column = 0;
      if (++term->vt_row == VGA_SCREEN_HEIGHT)
	{
	  size_t i;
	  for (i = VGA_SCREEN_WIDTH; i < VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT;
	       i++)
	    term->vt_data[i - VGA_SCREEN_WIDTH] = term->vt_data[i];
	  for (i = 0; i < VGA_SCREEN_WIDTH; i++)
	    vga_putentry (term, ' ', i, VGA_SCREEN_HEIGHT - 1);
	  term->vt_row--;
	  if (term == CURRENT_TERMINAL)
	    memcpy (vga_hdw_buf, term->vt_data,
		    2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
	}
    }
  vga_setcurs (term->vt_column, term->vt_row);
  __asm__ volatile ("sti");
}

void
vga_delchar (Terminal *term)
{
  if (term->vt_column > 0)
    {
      term->vt_column--;
      vga_putentry (term, ' ', term->vt_column, term->vt_row);
      vga_setcurs (term->vt_column, term->vt_row);
    }
}

void
vga_write (Terminal *term, const char *s, size_t size)
{
  int i;
  for (i = 0; i < size; i++)
    vga_putchar (term, s[i]);
}

void
vga_puts (Terminal *term, const char *s)
{
  int written = 0;
  for (; *s != '\0'; s++, written++)
    vga_putchar (term, *s);
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
