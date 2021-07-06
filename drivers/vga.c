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
#include <sys/task.h>
#include <video/vga.h>
#include <string.h>

uint16_t *vga_hdw_buf = (uint16_t *) VGA_BUFFER;

void
vga_putentry (Terminal *term, char c, size_t x, size_t y)
{
  unsigned char color = term->vt_color;
  if (term->vt_flags & VT_FLAG_REVERSE)
    color = ((color & 0xf0) >> 4) | ((color & 0x0f) << 4);
  term->vt_data[vga_getindex (x, y)] = vga_mkentry (c, color);
  if (term == CURRENT_TERMINAL)
    vga_hdw_buf[vga_getindex (x, y)] = vga_mkentry (c, color);
}

void
vga_putchar (Terminal *term, char c)
{
  if (parsing_escseq)
    vga_terminal_parse_escseq (term, c);
  else if (c == '\033')
    parsing_escseq = 1;
  else
    vga_display_putchar (term, c);
}

void
vga_delchar (Terminal *term)
{
  if (term->vt_column > 0)
    {
      if (term->vt_termios.c_lflag & ECHOE)
	{
	  term->vt_column--;
	  vga_putentry (term, ' ', term->vt_column, term->vt_row);
	  vga_setcurs (term->vt_column, term->vt_row);
	}
      else
	{
	  char c = term->vt_data[vga_getindex (term->vt_column - 1,
					       term->vt_row)] & 0xff;
	  vga_display_putchar (term, c);
	}
    }
}

void
vga_delline (Terminal *term, size_t len)
{
  tcflag_t lflag = term->vt_termios.c_lflag;
  if (lflag & ECHOKE)
    {
      size_t i;
      if (len > term->vt_column)
	len = term->vt_column;
      for (i = 0; i < len; i++)
	{
	  term->vt_column--;
	  vga_putentry (term, ' ', term->vt_column, term->vt_row);
	}
      vga_setcurs (term->vt_column, term->vt_row);
    }
  else
    {
      vga_display_putchar (term, term->vt_termios.c_cc[VKILL]);
      if (lflag & ECHOK)
	vga_display_putchar (term, '\n');
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
vga_display_putchar (Terminal *term, char c)
{
  tcflag_t iflag = CURRENT_TERMINAL->vt_termios.c_iflag;
  int active;
  if (c == '\0')
    return;
  active = term == CURRENT_TERMINAL;
  DISABLE_TASK_SWITCH;

  switch (c)
    {
    case '\a':
      goto end; /* TODO Sound the system bell */
    case '\n':
      term->vt_column = 0;
      if (iflag & INLCR)
	goto end;
    case '\v':
    case '\f':
      goto wrap;
    case '\r':
      if (iflag & IGNCR)
	goto end;
      term->vt_column = 0;
      if (iflag & ICRNL)
	goto wrap;
      else
	goto end;
    case '\t':
      term->vt_column |= 7;
      break;
    case '\b':
      vga_delchar (term);
      goto end;
    default:
      vga_putentry (term, c, term->vt_column, term->vt_row);
    }

  if (++term->vt_column == VGA_SCREEN_WIDTH)
    {
      term->vt_column = 0;
    wrap:
      if (++term->vt_row == VGA_SCREEN_HEIGHT)
	{
	  size_t i;
	  for (i = VGA_SCREEN_WIDTH; i < VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT;
	       i++)
	    term->vt_data[i - VGA_SCREEN_WIDTH] = term->vt_data[i];
	  for (i = 0; i < VGA_SCREEN_WIDTH; i++)
	    vga_putentry (term, ' ', i, VGA_SCREEN_HEIGHT - 1);
	  term->vt_row--;
	  if (active)
	    memcpy (vga_hdw_buf, term->vt_data,
		    2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
	}
    }

 end:
  vga_setcurs (term->vt_column, term->vt_row);
  ENABLE_TASK_SWITCH;
}

void
vga_clear (Terminal *term)
{
  size_t y;
  size_t x;
  for (y = 0; y < VGA_SCREEN_HEIGHT; y++)
    {
      for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	term->vt_data[vga_getindex (x, y)] =
	  vga_mkentry (' ', term->vt_color);
    }
  if (term == CURRENT_TERMINAL)
    {
      DISABLE_TASK_SWITCH;
      memcpy (vga_hdw_buf, term->vt_data,
	      2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
      ENABLE_TASK_SWITCH;
    }
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
