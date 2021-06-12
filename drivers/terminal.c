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
#include <string.h>

static Terminal default_terminal;

Terminal *terminals[TERMINAL_LIMIT] = {&default_terminal};
int active_terminal;

void
vga_init (void)
{
  size_t x;
  size_t y;

  default_terminal.vt_color =
    vga_mkcolor (VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  for (y = 0; y < VGA_SCREEN_HEIGHT; y++)
    {
      for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	default_terminal.vt_data[vga_getindex (x, y)] =
	  vga_mkentry (' ', default_terminal.vt_color);
    }
  memcpy (vga_hdw_buf, default_terminal.vt_data,
	  2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);

  /* Initialize termios */
  default_terminal.vt_termios.c_iflag = DEFAULT_IFLAG;
  default_terminal.vt_termios.c_oflag = DEFAULT_OFLAG;
  default_terminal.vt_termios.c_cflag = DEFAULT_CFLAG;
  default_terminal.vt_termios.c_lflag = DEFAULT_LFLAG;
  default_terminal.vt_termios.c_ispeed = B9600;
  default_terminal.vt_termios.c_ospeed = B9600;
}

void
set_active_terminal (int term)
{
  active_terminal = term;
  memcpy ((void *) VGA_BUFFER, CURRENT_TERMINAL->vt_data,
	  2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
}
