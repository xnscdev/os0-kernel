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

#include <sys/task.h>
#include <video/vga.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

static Terminal default_terminal;

Terminal *terminals[PROCESS_LIMIT] = {&default_terminal};
int active_terminal;
int parsing_escseq;

static void
vga_csi_cursor_up (Terminal *term)
{
  if (term->vt_escseq.vte_flags & VTE_FLAG_ERR)
    return;
  if (term->vt_escseq.vte_num[0] == 0)
    term->vt_escseq.vte_num[0] = 1;
  if (term->vt_row < term->vt_escseq.vte_num[0])
    term->vt_row = 0;
  else
    term->vt_row -= term->vt_escseq.vte_num[0];
  vga_setcurs (term->vt_column, term->vt_row);
}

static void
vga_csi_cursor_down (Terminal *term)
{
  if (term->vt_escseq.vte_flags & VTE_FLAG_ERR)
    return;
  if (term->vt_escseq.vte_num[0] == 0)
    term->vt_escseq.vte_num[0] = 1;
  if (term->vt_row > VGA_SCREEN_HEIGHT - term->vt_escseq.vte_num[0] - 1)
    term->vt_row = VGA_SCREEN_HEIGHT - 1;
  else
    term->vt_row += term->vt_escseq.vte_num[0];
  vga_setcurs (term->vt_column, term->vt_row);
}

static void
vga_csi_cursor_right (Terminal *term)
{
  if (term->vt_escseq.vte_flags & VTE_FLAG_ERR)
    return;
  if (term->vt_escseq.vte_num[0] == 0)
    term->vt_escseq.vte_num[0] = 1;
  if (term->vt_column > VGA_SCREEN_WIDTH - term->vt_escseq.vte_num[0] - 1)
    term->vt_column = VGA_SCREEN_WIDTH - 1;
  else
    term->vt_column += term->vt_escseq.vte_num[0];
  vga_setcurs (term->vt_column, term->vt_row);
}

static void
vga_csi_cursor_left (Terminal *term)
{
  if (term->vt_escseq.vte_flags & VTE_FLAG_ERR)
    return;
  if (term->vt_escseq.vte_num[0] == 0)
    term->vt_escseq.vte_num[0] = 1;
  if (term->vt_column < term->vt_escseq.vte_num[0])
    term->vt_column = 0;
  else
    term->vt_column -= term->vt_escseq.vte_num[0];
  vga_setcurs (term->vt_column, term->vt_row);
}

static void
vga_csi_cursor_col_set (Terminal *term)
{
  if (term->vt_escseq.vte_flags & VTE_FLAG_ERR)
    return;
  if (term->vt_escseq.vte_num[0] > VGA_SCREEN_WIDTH)
    term->vt_escseq.vte_num[0] = VGA_SCREEN_WIDTH - 1;
  else if (term->vt_escseq.vte_num[0] > 0)
    term->vt_escseq.vte_num[0]--;
  term->vt_column = term->vt_escseq.vte_num[0];
  vga_setcurs (term->vt_column, term->vt_row);
}

static void
vga_csi_cursor_pos_set (Terminal *term)
{
  if (term->vt_escseq.vte_flags & VTE_FLAG_ERR)
    return;
  if (term->vt_escseq.vte_num[0] > VGA_SCREEN_WIDTH)
    term->vt_escseq.vte_num[0] = VGA_SCREEN_WIDTH - 1;
  else if (term->vt_escseq.vte_num[0] > 0)
    term->vt_escseq.vte_num[0]--;
  if (term->vt_escseq.vte_num[1] > VGA_SCREEN_HEIGHT)
    term->vt_escseq.vte_num[1] = VGA_SCREEN_HEIGHT - 1;
  else if (term->vt_escseq.vte_num[1] > 0)
    term->vt_escseq.vte_num[1]--;
  term->vt_row = term->vt_escseq.vte_num[0];
  term->vt_column = term->vt_escseq.vte_num[1];
  vga_setcurs (term->vt_column, term->vt_row);
}

static void
vga_csi_cursor_tab (Terminal *term)
{
  int i;
  if (term->vt_escseq.vte_flags & VTE_FLAG_ERR)
    return;
  if (term->vt_escseq.vte_num[0] > VGA_SCREEN_WIDTH / 8)
    term->vt_escseq.vte_num[0] = VGA_SCREEN_WIDTH / 8;
  else if (term->vt_escseq.vte_num[0] == 0)
    term->vt_escseq.vte_num[0] = 1;
  for (i = 0; i < term->vt_escseq.vte_num[0]; i++)
    vga_display_putchar (term, '\t');
}

static void
vga_erase_line (Terminal *term)
{
  size_t x;
  switch (term->vt_escseq.vte_num[0])
    {
    case 0:
      for (x = term->vt_column; x < VGA_SCREEN_WIDTH; x++)
	term->vt_data[vga_getindex (term->vt_row, x)] =
	  vga_mkentry (' ', vga_mkcolor (VGA_COLOR_BLACK, VGA_COLOR_BLACK));
      break;
    case 1:
      for (x = 0; x < term->vt_column; x++)
	term->vt_data[vga_getindex (term->vt_row, x)] =
	  vga_mkentry (' ', vga_mkcolor (VGA_COLOR_BLACK, VGA_COLOR_BLACK));
      break;
    case '2':
      for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	term->vt_data[vga_getindex (term->vt_row, x)] =
	  vga_mkentry (' ', vga_mkcolor (VGA_COLOR_BLACK, VGA_COLOR_BLACK));
      break;
    }
  if (term == CURRENT_TERMINAL)
    {
      DISABLE_TASK_SWITCH;
      memcpy (vga_hdw_buf, term->vt_data,
	      2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
      ENABLE_TASK_SWITCH;
    }
}

static void
vga_erase_display (Terminal *term)
{
  size_t y;
  size_t x;
  switch (term->vt_escseq.vte_num[0])
    {
    case 0:
      vga_erase_line (term);
      for (y = term->vt_row + 1; y < VGA_SCREEN_HEIGHT; y++)
	{
	  for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	    term->vt_data[vga_getindex (x, y)] =
	      vga_mkentry (' ', vga_mkcolor (VGA_COLOR_BLACK, VGA_COLOR_BLACK));
	}
      break;
    case 1:
      vga_erase_line (term);
      for (y = 0; y < term->vt_row; y++)
	{
	  for (x = 0; x < VGA_SCREEN_WIDTH; x++)
	    term->vt_data[vga_getindex (x, y)] =
	      vga_mkentry (' ', vga_mkcolor (VGA_COLOR_BLACK, VGA_COLOR_BLACK));
	}
      break;
    case '2':
      for (y = 0; y < VGA_SCREEN_HEIGHT; y++)
	vga_erase_line (term);
      break;
    }
  if (term == CURRENT_TERMINAL)
    {
      DISABLE_TASK_SWITCH;
      memcpy (vga_hdw_buf, term->vt_data,
	      2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
      ENABLE_TASK_SWITCH;
    }
}

static void
vga_csi_set_sgr (Terminal *term)
{
  int i;
  for (i = 0; i <= term->vt_escseq.vte_curr; i++)
    {
      switch (term->vt_escseq.vte_num[i])
	{
	case 0:
	case 100:
	  term->vt_color = VGA_DEFAULT_COLOR;
	  break;
	case 7:
	  term->vt_flags |= VT_FLAG_REVERSE;
	  break;
	case 30:
	  term->vt_color = vga_setfg (term->vt_color, VGA_COLOR_BLACK);
	  break;
	case 31:
	  term->vt_color = vga_setfg (term->vt_color, VGA_COLOR_RED);
	  break;
	case 32:
	  term->vt_color = vga_setfg (term->vt_color, VGA_COLOR_GREEN);
	  break;
	case 33:
	  term->vt_color = vga_setfg (term->vt_color, VGA_COLOR_YELLOW);
	  break;
	case 34:
	  term->vt_color = vga_setfg (term->vt_color, VGA_COLOR_BLUE);
	  break;
	case 35:
	  term->vt_color = vga_setfg (term->vt_color, VGA_COLOR_MAGENTA);
	  break;
	case 36:
	  term->vt_color = vga_setfg (term->vt_color, VGA_COLOR_CYAN);
	  break;
	case 37:
	  term->vt_color = vga_setfg (term->vt_color, VGA_COLOR_LIGHT_GREY);
	  break;
	case 39:
	  term->vt_color = vga_setfg (term->vt_color, VGA_DEFAULT_FG);
	  break;
	case 40:
	  term->vt_color = vga_setbg (term->vt_color, VGA_COLOR_BLACK);
	  break;
	case 41:
	  term->vt_color = vga_setbg (term->vt_color, VGA_COLOR_RED);
	  break;
	case 42:
	  term->vt_color = vga_setbg (term->vt_color, VGA_COLOR_GREEN);
	  break;
	case 43:
	  term->vt_color = vga_setbg (term->vt_color, VGA_COLOR_YELLOW);
	  break;
	case 44:
	  term->vt_color = vga_setbg (term->vt_color, VGA_COLOR_BLUE);
	  break;
	case 45:
	  term->vt_color = vga_setbg (term->vt_color, VGA_COLOR_MAGENTA);
	  break;
	case 46:
	  term->vt_color = vga_setbg (term->vt_color, VGA_COLOR_CYAN);
	  break;
	case 47:
	  term->vt_color = vga_setbg (term->vt_color, VGA_COLOR_LIGHT_GREY);
	  break;
	case 49:
	  term->vt_color = vga_setbg (term->vt_color, VGA_DEFAULT_BG);
	  break;
	}
    }
}

static void
vga_csi_parse_nums (Terminal *term)
{
  if (++term->vt_escseq.vte_curr == 4)
    {
      term->vt_escseq.vte_flags |= VTE_FLAG_ERR;
      term->vt_escseq.vte_curr = 0;
    }
}

static void
vga_terminal_parse_escseq_esc (Terminal *term, char c)
{
  switch (c)
    {
    case 'E':
      vga_display_putchar (term, '\n');
      break;
    case '[':
      term->vt_escseq.vte_mode = TERMINAL_ESC_SEQ_CSI;
      break;
    case 'c':
      term->vt_color = vga_mkcolor (VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
      vga_clear (term);
      term->vt_row = 0;
      term->vt_column = 0;
      vga_setcurs (0, 0);
      break;
    case '7':
      term->vt_saved_row = term->vt_row;
      term->vt_saved_col = term->vt_column;
      break;
    case '8':
      term->vt_row = term->vt_saved_row;
      term->vt_column = term->vt_saved_col;
      if (term == CURRENT_TERMINAL)
	vga_setcurs (term->vt_column, term->vt_row);
      break;
    default:
      vga_terminal_cancel_escseq (term);
    }
}

static void
vga_terminal_parse_escseq_csi (Terminal *term, char c)
{
  if (isdigit (c))
    {
      if (term->vt_escseq.vte_flags & VTE_FLAG_ERR)
	return;
      if (term->vt_escseq.vte_num[term->vt_escseq.vte_curr] > UINT_MAX / 10)
	{
	  term->vt_escseq.vte_flags |= VTE_FLAG_ERR;
	  return;
	}
      term->vt_escseq.vte_num[term->vt_escseq.vte_curr] *= 10;
      if (term->vt_escseq.vte_num[term->vt_escseq.vte_curr] >
	  UINT_MAX - c + '0')
	{
	  term->vt_escseq.vte_flags |= VTE_FLAG_ERR;
	  return;
	}
      term->vt_escseq.vte_num[term->vt_escseq.vte_curr] += c - '0';
      term->vt_escseq.vte_flags |= VTE_FLAG_NUM;
      return;
    }

  switch (c)
    {
    case 'A':
    case 'E':
      vga_csi_cursor_up (term);
      break;
    case 'B':
    case 'F':
      vga_csi_cursor_down (term);
      break;
    case 'C':
      vga_csi_cursor_right (term);
      break;
    case 'D':
      vga_csi_cursor_left (term);
      break;
    case 'G':
      vga_csi_cursor_col_set (term);
      break;
    case 'H':
      vga_csi_cursor_pos_set (term);
      break;
    case 'I':
      vga_csi_cursor_tab (term);
      break;
    case 'J':
      vga_erase_display (term);
      break;
    case 'K':
      vga_erase_line (term);
      break;
    case 'm':
      vga_csi_set_sgr (term);
      break;
    case ';':
      vga_csi_parse_nums (term);
      break;
    }
  vga_terminal_cancel_escseq (term);
}

void
vga_init (void)
{
  size_t x;
  size_t y;

  default_terminal.vt_color = VGA_DEFAULT_COLOR;
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
  default_terminal.vt_termios.c_cc[VINTR] = '\3';
  default_terminal.vt_termios.c_cc[VQUIT] = '\34';
  default_terminal.vt_termios.c_cc[VERASE] = '\37';
  default_terminal.vt_termios.c_cc[VKILL] = '\25';
  default_terminal.vt_termios.c_cc[VEOF] = '\4';
  default_terminal.vt_termios.c_cc[VTIME] = 0;
  default_terminal.vt_termios.c_cc[VMIN] = 1;
  default_terminal.vt_termios.c_cc[VSTART] = '\21';
  default_terminal.vt_termios.c_cc[VSTOP] = '\23';
  default_terminal.vt_termios.c_cc[VSUSP] = '\32';
  default_terminal.vt_termios.c_cc[VEOL] = 0xff;
  default_terminal.vt_termios.c_ispeed = B9600;
  default_terminal.vt_termios.c_ospeed = B9600;
}

void
vga_terminal_cancel_escseq (Terminal *term)
{
  term->vt_escseq.vte_flags = 0;
  term->vt_escseq.vte_curr = 0;
  memset (term->vt_escseq.vte_num, 0, sizeof (unsigned int) * 4);
  term->vt_escseq.vte_mode = TERMINAL_ESC_SEQ_ESC;
  parsing_escseq = 0;
}

void
vga_terminal_parse_escseq (Terminal *term, char c)
{
  if (c == '\033')
    {
      vga_terminal_cancel_escseq (term);
      return;
    }

  switch (term->vt_escseq.vte_mode)
    {
    case TERMINAL_ESC_SEQ_ESC:
      vga_terminal_parse_escseq_esc (term, c);
      break;
    case TERMINAL_ESC_SEQ_CSI:
      vga_terminal_parse_escseq_csi (term, c);
      break;
    }
}

void
set_active_terminal (int term)
{
  active_terminal = term;
  memcpy ((void *) VGA_BUFFER, CURRENT_TERMINAL->vt_data,
	  2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
}
