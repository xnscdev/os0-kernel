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
#include <sys/kbd.h>
#include <video/vga.h>

static char kbd_press_map[128];
static int modifies; /* If 0xe0 was seen previously */

char kbd_buffer[KBD_BUFSIZ];
size_t kbd_bufpos;
size_t kbd_currpos;

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

  /* Remove character if backspace pressed */
  if (scancode == KEY_BACKSP)
    {
      if (kbd_buffer[kbd_bufpos] != '\n' && kbd_bufpos > 0)
	{
	  kbd_bufpos--;
	  vga_delchar ();
	}
      return;
    }

  /* If the character is printable, add it to the input buffer and echo
     it to the display if necessary */
  if (kbd_print_chars[scancode] != 0)
    {
      struct termios *term;
      unsigned char c = kbd_print_chars[scancode];
      if (c != '\0' && kbd_shift_chars[c] != '\0'
	  && (kbd_key_pressed (KEY_LSHIFT) || kbd_key_pressed (KEY_RSHIFT)))
	c = kbd_shift_chars[c];

      if (kbd_bufpos == KBD_BUFSIZ)
	{
	  if (kbd_currpos > 0)
	    {
	      memmove (kbd_buffer, kbd_buffer + kbd_currpos,
		       KBD_BUFSIZ - kbd_currpos);
	      kbd_bufpos -= kbd_currpos;
	      kbd_currpos = 0;
	    }
	  else
	    return; /* No space left, ignore keystroke */
	}
      kbd_buffer[kbd_bufpos++] = c;

      term = process_table[task_getpid ()].p_files[STDIN_FILENO].pf_termios;
      if (term != NULL && term->c_lflag & ECHO)
	vga_putchar (c);
    }
}

void
kbd_await_press (int key)
{
  while (!kbd_key_pressed (key))
    ;
}

int
kbd_key_pressed (int key)
{
  return kbd_press_map[key];
}

int
kbd_get_input (void *buffer, size_t len, int block)
{
  char *check = strchr (kbd_buffer + kbd_currpos, '\n');
  if (check == NULL)
    goto wait;
  while (kbd_bufpos < kbd_currpos + len)
    {
    wait:
      if (block)
        {
	  __asm__ volatile ("sti");
	  kbd_await_press (KEY_ENTER);
	  __asm__ volatile ("cli");
	}
      else
	return -EAGAIN;
    }
  memcpy (buffer, kbd_buffer + kbd_currpos, len);
  kbd_currpos += len;
  return 0;
}
