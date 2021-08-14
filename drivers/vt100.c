/*************************************************************************
 * vt100.c -- This file is part of OS/0.                                 *
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
#include <sys/tty.h>
#include <video/vga.h>

/* Character sets */

enum
{
  VT100_G0_UK,
  VT100_G1_UK,
  VT100_G0_US,
  VT100_G1_US,
  VT100_G0_SPEC,
  VT100_G1_SPEC,
  VT100_G0_ALT,
  VT100_G1_ALT,
  VT100_G0_ALT_SPEC,
  VT100_G1_ALT_SPEC
};

/* VT100 states */

enum
{
  VT100_STATE_ESC = 1, /* ^[ */
  VT100_STATE_CSI,     /* ^[[ */
  VT100_STATE_LP,      /* ^[( */
  VT100_STATE_RP,      /* ^[) */
  VT100_STATE_BQ       /* ^[[? */
};

static void
vt100_set_dec (TTY *tty)
{
  tty_reset_state (tty);
}

static void
vt100_reset_dec (TTY *tty)
{
  tty_reset_state (tty);
}

static void
vt100_set_sgr (TTY *tty)
{
  size_t i;
  for (i = 0; i < tty->t_curritem + 1; i++)
    {
      switch (tty->t_statebuf[i])
	{
	case 0:
	  tty->t_flags &= ~TTY_REVERSE_VIDEO;
	  tty->t_color = VGA_DEFAULT_COLOR;
	  break;
	case 7:
	  tty->t_flags |= TTY_REVERSE_VIDEO;
	  break;
	case 30:
	  tty->t_color = vga_setfg (tty->t_color, VGA_COLOR_BLACK);
	  break;
	case 31:
	  tty->t_color = vga_setfg (tty->t_color, VGA_COLOR_RED);
	  break;
	case 32:
	  tty->t_color = vga_setfg (tty->t_color, VGA_COLOR_GREEN);
	  break;
	case 33:
	  tty->t_color = vga_setfg (tty->t_color, VGA_COLOR_YELLOW);
	  break;
	case 34:
	  tty->t_color = vga_setfg (tty->t_color, VGA_COLOR_BLUE);
	  break;
	case 35:
	  tty->t_color = vga_setfg (tty->t_color, VGA_COLOR_MAGENTA);
	  break;
	case 36:
	  tty->t_color = vga_setfg (tty->t_color, VGA_COLOR_CYAN);
	  break;
	case 37:
	  tty->t_color = vga_setfg (tty->t_color, VGA_COLOR_WHITE);
	  break;
	case 40:
	  tty->t_color = vga_setbg (tty->t_color, VGA_COLOR_BLACK);
	  break;
	case 41:
	  tty->t_color = vga_setbg (tty->t_color, VGA_COLOR_RED);
	  break;
	case 42:
	  tty->t_color = vga_setbg (tty->t_color, VGA_COLOR_GREEN);
	  break;
	case 43:
	  tty->t_color = vga_setbg (tty->t_color, VGA_COLOR_YELLOW);
	  break;
	case 44:
	  tty->t_color = vga_setbg (tty->t_color, VGA_COLOR_BLUE);
	  break;
	case 45:
	  tty->t_color = vga_setbg (tty->t_color, VGA_COLOR_MAGENTA);
	  break;
	case 46:
	  tty->t_color = vga_setbg (tty->t_color, VGA_COLOR_CYAN);
	  break;
	case 47:
	  tty->t_color = vga_setbg (tty->t_color, VGA_COLOR_WHITE);
	  break;
	}
    }
  tty_reset_state (tty);
}

static void
vt100_set_cursor_pos (TTY *tty)
{
  if (tty->t_statebuf[0] > 0)
    tty->t_statebuf[0]--;
  if (tty->t_statebuf[0] >= VGA_SCREEN_HEIGHT)
    tty->t_statebuf[0] = VGA_SCREEN_HEIGHT - 1;
  if (tty->t_statebuf[1] > 0)
    tty->t_statebuf[1]--;
  if (tty->t_statebuf[1] >= VGA_SCREEN_WIDTH)
    tty->t_statebuf[1] = VGA_SCREEN_WIDTH - 1;
  tty->t_row = tty->t_statebuf[0];
  tty->t_column = tty->t_statebuf[1];
  vga_setcurs (tty->t_column, tty->t_row);
  tty_reset_state (tty);
}

static void
vt100_clear_screen (TTY *tty)
{
  size_t y;
  size_t x;
  switch (tty->t_statebuf[0])
    {
    case 0:
      for (x = tty->t_column; x < VGA_SCREEN_WIDTH; x++)
	vga_putentry (tty, ' ', x, tty->t_row);
      for (y = tty->t_row + 1; y < VGA_SCREEN_HEIGHT; y++)
	{
	  for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	    vga_putentry (tty, ' ', x, y);
	}
      break;
    case 1:
      for (x = 0; x < tty->t_column; x++)
	vga_putentry (tty, ' ', x, tty->t_row);
      for (y = 0; y < tty->t_row; y++)
	{
	  for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	    vga_putentry (tty, ' ', x, y);
	}
      break;
    case 2:
      for (y = 0; y < VGA_SCREEN_HEIGHT; y++)
	{
	  for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	    vga_putentry (tty, ' ', x, y);
	}
      break;
    }
  vga_update_display (tty);
  tty_reset_state (tty);
}

static void
vt100_clear_line (TTY *tty)
{
  size_t x;
  switch (tty->t_statebuf[0])
    {
    case 0:
      for (x = tty->t_column; x < VGA_SCREEN_WIDTH; x++)
	vga_putentry (tty, ' ', x, tty->t_row);
      break;
    case 1:
      for (x = 0; x < tty->t_column; x++)
	vga_putentry (tty, ' ', x, tty->t_row);
      break;
    case 2:
      for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	vga_putentry (tty, ' ', x, tty->t_row);
      break;
    }
  vga_update_display (tty);
  tty_reset_state (tty);
}

static void
vt100_set_charset (TTY *tty, int set)
{
  /* TODO Implement multiple character sets */
  tty_reset_state (tty);
}

static void
vt100_handle_escaped_char (TTY *tty, char c)
{
  switch (tty->t_state)
    {
    case VT100_STATE_ESC:
      switch (c)
	{
	case '[':
	  DO_RET (tty->t_state = VT100_STATE_CSI);
	case '(':
	  DO_RET (tty->t_state = VT100_STATE_LP);
	case ')':
	  DO_RET (tty->t_state = VT100_STATE_RP);
	case '=':
	  DO_RET (tty_set_alt_keypad (tty, 1));
	case '>':
	  DO_RET (tty_set_alt_keypad (tty, 0));
	}
      break;
    case VT100_STATE_CSI:
      switch (c)
	{
	case '?':
	  DO_RET (tty->t_state = VT100_STATE_BQ);
	case 'h':
	  DO_RET (vt100_set_dec (tty));
	case 'l':
	  DO_RET (vt100_reset_dec (tty));
	case 'm':
	  DO_RET (vt100_set_sgr (tty));
	case 'r':
	  DO_RET (tty_reset_state (tty));
	case 'H':
	  DO_RET (vt100_set_cursor_pos (tty));
	case 'J':
	  DO_RET (vt100_clear_screen (tty));
	case 'K':
	  DO_RET (vt100_clear_line (tty));
	case ';':
	  if (++tty->t_curritem == 8)
	    tty_reset_state (tty); /* Too many numbers */
	  return;
	default:
	  if (isdigit (c))
	    DO_RET (tty_add_digit_char (tty, c));
	}
      break;
    case VT100_STATE_LP:
      switch (c)
	{
	case 'A':
	  DO_RET (vt100_set_charset (tty, VT100_G0_UK));
	case 'B':
	  DO_RET (vt100_set_charset (tty, VT100_G0_US));
	case '0':
	  DO_RET (vt100_set_charset (tty, VT100_G0_SPEC));
	case '1':
	  DO_RET (vt100_set_charset (tty, VT100_G0_ALT));
	case '2':
	  DO_RET (vt100_set_charset (tty, VT100_G0_ALT_SPEC));
	}
      break;
    case VT100_STATE_RP:
      switch (c)
	{
	case 'A':
	  DO_RET (vt100_set_charset (tty, VT100_G1_UK));
	case 'B':
	  DO_RET (vt100_set_charset (tty, VT100_G1_US));
	case '0':
	  DO_RET (vt100_set_charset (tty, VT100_G1_SPEC));
	case '1':
	  DO_RET (vt100_set_charset (tty, VT100_G1_ALT));
	case '2':
	  DO_RET (vt100_set_charset (tty, VT100_G1_ALT_SPEC));
	}
      break;
    case VT100_STATE_BQ:
      switch (c)
	{
	default:
	  if (isdigit (c))
	    DO_RET (tty_add_digit_char (tty, c));
	}
      break;
    }
  tty_reset_state (tty); /* Invalid character, stop parsing */
}

void
vt100_write_char (TTY *tty, char c)
{
  if (tty->t_state != 0)
    vt100_handle_escaped_char (tty, c);
  else if (c == '\33')
    tty->t_state = VT100_STATE_ESC;
  else if (isspace (c) || isgraph (c))
    vga_putchar (tty, c);
}
