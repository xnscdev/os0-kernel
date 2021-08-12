/*************************************************************************
 * terminal.c -- This file is part of OS/0.                              *
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

#include <video/vga.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

void
vga_init (void)
{
  size_t x;
  size_t y;
  for (y = 0; y < VGA_SCREEN_HEIGHT; y++)
    {
      for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	default_tty.t_screenbuf[vga_getindex (x, y)] =
	  vga_mkentry (' ', default_tty.t_color);
    }
  memcpy (vga_hdw_buf, default_tty.t_screenbuf,
	  2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
}

void
set_active_tty (int term)
{
  active_tty = term;
  memcpy ((void *) VGA_BUFFER, CURRENT_TTY->t_screenbuf,
	  2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
}
