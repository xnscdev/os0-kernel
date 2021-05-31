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

#include <sys/process.h>
#include <sys/kbd.h>
#include <video/vga.h>
#include <ctype.h>
#include <unistd.h>

static char kbd_press_map[128];

char kbd_buffer[KBD_BUFSIZ];
size_t kbd_bufpos;

/* US QWERTY keyboard layout */

static char kbd_print_chars[128] = {
  0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
  0, 0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
  '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
  0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

void
kbd_handle (int scancode)
{
  if (scancode > 0x80)
    {
      kbd_press_map[scancode - 0x80] = 0;
      return;
    }

  kbd_press_map[scancode] = 1;
  if (kbd_print_chars[scancode] != 0)
    {
      struct termios *term;
      char c = kbd_print_chars[scancode];
      if (kbd_bufpos == KBD_BUFSIZ)
	return;
      if (kbd_key_pressed (KEY_LSHIFT) || kbd_key_pressed (KEY_RSHIFT))
	c = toupper (c);
      term = process_table[task_getpid ()].p_files[STDIN_FILENO].pf_termios;
      kbd_buffer[kbd_bufpos++] = c;
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
