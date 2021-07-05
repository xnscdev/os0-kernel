/*************************************************************************
 * kbd.c -- This file is part of OS/0.                                   *
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
#include <sys/process.h>
#include <video/vga.h>

static char kbd_press_map[128];
static int kbd_flush_input;
static int kbd_eof;
static int modifies; /* If 0xe0 was seen previously */

/* US QWERTY keyboard layout */

static char kbd_print_chars[128] = {
  0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
  0, 0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
  '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
  0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

static char kbd_shift_chars[128] = {
  ['-'] = '_',
  ['='] = '+',
  [','] = '<',
  ['.'] = '>',
  ['/'] = '?',
  [';'] = ':',
  ['\''] = '"',
  ['['] = '{',
  [']'] = '}',
  ['\\'] = '|',
  ['`'] = '~',
  ['0'] = ')', '!', '@', '#', '$', '%', '^', '&', '*', '(',
  ['a'] = 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
};

static char kbd_ctrl_chars[128] = {
  [' '] = 000,
  ['a'] = 001, 002, 003, 004, 005, 006, 007, 010, 011, 012, 013, 014, 015,
  016, 017, 020, 021, 022, 023, 024, 025, 026, 027, 030, 031, 032,
  ['['] = 033, 034, 035,
  ['~'] = 036,
  ['?'] = 037
};

static char kbd_unctrl_chars[] = {
  ' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']',
  '~', '?'
};

static int
kbd_char_is_eol (char c)
{
  struct termios *term = &CURRENT_TERMINAL->vt_termios;
  return c == '\n' || (c == term->c_cc[VEOL] && c != 0xff);
}

static void
kbd_write_char (char c)
{
  KbdBuffer *buffer = &CURRENT_TERMINAL->vt_kbdbuf;
  tcflag_t lflag = CURRENT_TERMINAL->vt_termios.c_lflag;
  if (buffer->kbd_bufpos == KBD_BUFSIZ)
    {
      if (buffer->kbd_currpos > 0)
	{
	  memmove (buffer->kbd_buffer, buffer->kbd_buffer + buffer->kbd_currpos,
		   KBD_BUFSIZ - buffer->kbd_currpos);
	  buffer->kbd_bufpos -= buffer->kbd_currpos;
	  buffer->kbd_currpos = 0;
	}
      else
	return; /* No space left, ignore keystroke */
    }
  buffer->kbd_buffer[buffer->kbd_bufpos++] = c;

  if (lflag & ECHO)
    {
      if ((lflag & ECHOCTL) && c != '\n' && c >= 0
	  && c < sizeof (kbd_unctrl_chars))
	{
	  vga_display_putchar (CURRENT_TERMINAL, '^');
	  vga_display_putchar (CURRENT_TERMINAL, kbd_unctrl_chars[(int) c]);
	}
      else
	vga_putchar (CURRENT_TERMINAL, c);
    }
  else if ((lflag & ECHONL) && c == '\n')
    vga_display_putchar (CURRENT_TERMINAL, '\n');

  if (kbd_char_is_eol (c))
    kbd_flush_input = 1;
}

static void
kbd_delchar (void)
{
  KbdBuffer *buffer = &CURRENT_TERMINAL->vt_kbdbuf;
  if (!kbd_char_is_eol (buffer->kbd_buffer[buffer->kbd_bufpos])
      && buffer->kbd_bufpos > buffer->kbd_currpos)
    {
      buffer->kbd_bufpos--;
      if (CURRENT_TERMINAL->vt_termios.c_lflag & ECHO)
	vga_delchar (CURRENT_TERMINAL);
    }
}

static void
kbd_delline (void)
{
  KbdBuffer *buffer = &CURRENT_TERMINAL->vt_kbdbuf;
  if (buffer->kbd_bufpos <= buffer->kbd_currpos)
    return;
  if (CURRENT_TERMINAL->vt_termios.c_lflag & ECHO)
    {
      size_t len = buffer->kbd_bufpos - buffer->kbd_currpos;
      vga_delline (CURRENT_TERMINAL, len);
    }
  buffer->kbd_bufpos = buffer->kbd_currpos = 0;
}

static int
kbd_parse_ctlseq (char c)
{
  KbdBuffer *buffer = &CURRENT_TERMINAL->vt_kbdbuf;
  struct termios *term = &CURRENT_TERMINAL->vt_termios;
  int sig = 0;
  if (c == 0xff)
    return 0;

  if (c == term->c_cc[VINTR])
    sig = SIGINT;
  else if (c == term->c_cc[VQUIT])
    sig = SIGQUIT;
  else if (c == term->c_cc[VSUSP])
    sig = SIGTSTP;
  if (sig)
    {
      if (term->c_lflag & ISIG)
	{
	  int i;
	  if (!(term->c_lflag & NOFLSH))
	    buffer->kbd_bufpos = buffer->kbd_currpos = 0;
	  for (i = 1; i < PROCESS_LIMIT; i++)
	    {
	      if (process_table[i].p_task != NULL
		  && process_table[i].p_pgid == CURRENT_TERMINAL->vt_fgpgid)
		process_send_signal (i, sig);
	    }
	  return 1;
	}
      else
	return 0;
    }

  if (term->c_lflag & ICANON)
    {
      if (c == term->c_cc[VSTART] || c == term->c_cc[VSTOP])
	return 1;
      else if (c == term->c_cc[VERASE])
	{
	  kbd_delchar ();
	  return 1;
	}
      else if (c == term->c_cc[VKILL])
	{
	  kbd_delline ();
	  return 1;
	}
      else if (c == term->c_cc[VEOF])
	{
	  kbd_flush_input = 1;
	  kbd_eof = 1;
	  return 1;
	}
      else if (c == term->c_cc[VEOL])
	kbd_flush_input = 1;
    }
  return 0;
}

void
kbd_handle (int scancode)
{
  if (modifies)
    {
      modifies = 0;
      return;
    }

  if (scancode == KEY_EXTENDED)
    {
      modifies = 1;
      return;
    }
  if (scancode > 0x80)
    {
      kbd_press_map[scancode - 0x80] = 0;
      return;
    }
  kbd_press_map[scancode] = 1;

  switch (scancode)
    {
    case KEY_BACKSP:
      /* Remove character if backspace pressed */
      kbd_delchar ();
      return;
    case KEY_ESC:
      /* Send ESC char to terminal to begin parsing terminal escape
	 sequence */
      vga_putchar (CURRENT_TERMINAL, '\33');
      return;
    }

  /* If the character is printable, add it to the input buffer and echo
     it to the display if necessary */
  if (kbd_print_chars[scancode] != 0)
    {
      unsigned char c = kbd_print_chars[scancode];
      if (SHIFT_DOWN && kbd_shift_chars[c] != '\0')
	c = kbd_shift_chars[c];

      /* Needs to come after shift replace check for ^? to work */
      if (CTRL_DOWN && kbd_ctrl_chars[c] != '\0')
	{
	  c = kbd_ctrl_chars[c];
	  if (kbd_parse_ctlseq (c))
	    return;
	}

      if (CURRENT_TERMINAL->vt_termios.c_iflag & ISTRIP)
	c &= 0x7f;
      kbd_write_char (c);
    }
}

void
kbd_await_press (int key)
{
  while (!kbd_key_pressed (key))
    ;
  kbd_press_map[key] = 0;
}

int
kbd_key_pressed (int key)
{
  return kbd_press_map[key];
}

int
kbd_get_input (void *buffer, size_t len, int block)
{
  KbdBuffer *kbdbuf = &CURRENT_TERMINAL->vt_kbdbuf;
  int await = 0;

  if (kbd_eof)
    return 0;

  if (CURRENT_TERMINAL->vt_termios.c_lflag & ICANON)
    {
      char *check = strchr (kbdbuf->kbd_buffer + kbdbuf->kbd_currpos, '\n');
      if (check == NULL)
	await = 1;
    }
  else if (kbdbuf->kbd_bufpos <= kbdbuf->kbd_currpos)
    await = 1; /* TODO Support VTIME and VMIN */

  if (await)
    {
      if (block)
        {
	  if (CURRENT_TERMINAL->vt_termios.c_lflag & ICANON)
	    {
	      while (!kbd_flush_input)
		;
	    }
	  else
	    {
	      while (kbdbuf->kbd_bufpos <= kbdbuf->kbd_currpos)
		;
	    }
	  len = kbdbuf->kbd_bufpos - kbdbuf->kbd_currpos;
	}
      else
	return -EAGAIN;
    }
  memcpy (buffer, kbdbuf->kbd_buffer + kbdbuf->kbd_currpos, len);
  kbdbuf->kbd_currpos += len;
  kbd_flush_input = 0;
  return len;
}
