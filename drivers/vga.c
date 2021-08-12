/*************************************************************************
 * vga.c -- This file is part of OS/0.                                   *
 * Copyright (C) 2021 XNSC                                               *
 *                                                                       *
 * OS/0 is free software: you can redistribute it and/or modify          *
 * it under the ttys of the GNU General Public License as published by  *
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
#include <sys/io.h>
#include <sys/task.h>
#include <sys/timer.h>
#include <video/vga.h>

uint16_t *vga_hdw_buf = (uint16_t *) VGA_BUFFER;

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
vga_putentry (TTY *tty, char c, size_t x, size_t y)
{
  unsigned char color = tty->t_color;
  if (tty->t_flags & TTY_REVERSE_VIDEO)
    color = ((color & 0xf0) >> 4) | ((color & 0x0f) << 4);
  tty->t_screenbuf[vga_getindex (x, y)] = vga_mkentry (c, color);
  if (tty == CURRENT_TTY)
    vga_hdw_buf[vga_getindex (x, y)] = vga_mkentry (c, color);
}

void
vga_putchar (TTY *tty, char c)
{
  vga_display_putchar (tty, c);
}

void
vga_erase_char (TTY *tty)
{
  if (tty->t_column > 0)
    {
      tty->t_column--;
      vga_putentry (tty, ' ', tty->t_column, tty->t_row);
      vga_setcurs (tty->t_column, tty->t_row);
    }
}

void
vga_erase_line (TTY *tty, size_t len)
{
  size_t i;
  if (len > tty->t_column)
    len = tty->t_column;
  for (i = 0; i < len; i++)
    vga_putentry (tty, ' ', --tty->t_column, tty->t_row);
  vga_setcurs (tty->t_column, tty->t_row);
}

void
vga_write (TTY *tty, const char *s, size_t size)
{
  int i;
  for (i = 0; i < size; i++)
    vga_putchar (tty, s[i]);
}

void
vga_puts (TTY *tty, const char *s)
{
  int written = 0;
  for (; *s != '\0'; s++, written++)
    vga_putchar (tty, *s);
}

void
vga_display_putchar (TTY *tty, char c)
{
  tcflag_t iflag = CURRENT_TTY->t_termios.c_iflag;
  int active;
  if (c == '\0')
    return;
  active = tty == CURRENT_TTY;
  DISABLE_TASK_SWITCH;

  switch (c)
    {
    case '\a':
      speaker_beep ();
      goto end;
    case '\n':
      tty->t_column = 0;
      if (iflag & INLCR)
	goto end;
    case '\v':
    case '\f':
      goto wrap;
    case '\r':
      if (iflag & IGNCR)
	goto end;
      tty->t_column = 0;
      if (iflag & ICRNL)
	goto wrap;
      else
	goto end;
    case '\t':
      tty->t_column |= 7;
      break;
    default:
      vga_putentry (tty, c, tty->t_column, tty->t_row);
    }

  if (++tty->t_column == VGA_SCREEN_WIDTH)
    {
      tty->t_column = 0;
    wrap:
      if (++tty->t_row == VGA_SCREEN_HEIGHT)
	{
	  size_t i;
	  for (i = VGA_SCREEN_WIDTH; i < VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT;
	       i++)
	    tty->t_screenbuf[i - VGA_SCREEN_WIDTH] = tty->t_screenbuf[i];
	  for (i = 0; i < VGA_SCREEN_WIDTH; i++)
	    vga_putentry (tty, ' ', i, VGA_SCREEN_HEIGHT - 1);
	  tty->t_row--;
	  if (active)
	    memcpy (vga_hdw_buf, tty->t_screenbuf,
		    2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
	}
    }

 end:
  vga_setcurs (tty->t_column, tty->t_row);
  ENABLE_TASK_SWITCH;
}

void
vga_clear (TTY *tty)
{
  size_t y;
  size_t x;
  for (y = 0; y < VGA_SCREEN_HEIGHT; y++)
    {
      for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	tty->t_screenbuf[vga_getindex (x, y)] =
	  vga_mkentry (' ', tty->t_color);
    }
  if (tty == CURRENT_TTY)
    {
      DISABLE_TASK_SWITCH;
      memcpy (vga_hdw_buf, tty->t_screenbuf,
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
