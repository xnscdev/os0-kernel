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
#include <sys/io.h>
#include <sys/process.h>
#include <video/vga.h>

static char kbd_press_map[128];
static int modifies; /* If 0xe0 was seen previously */

/* US QWERTY keyboard layout */

static char kbd_scancode_map[128] = {
  0, '\33', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
  '\37', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
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

  /* Handle releasing a key */
  if (scancode > 0x80)
    {
      kbd_press_map[scancode - 0x80] = 0;
      return;
    }

  /* Mark the key as pressed */
  kbd_press_map[scancode] = 1;

  /* Send the key to the tty line discipline */
  if (kbd_scancode_map[scancode] != 0)
    {
      /* Replace characters if they are different with shift pressed */
      unsigned char c = kbd_scancode_map[scancode];
      if (SHIFT_DOWN && kbd_shift_chars[c] != '\0')
	c = kbd_shift_chars[c];
      /* Needs to come after shift replace check for ^? to work */
      if (CTRL_DOWN && kbd_ctrl_chars[c] != '\0')
	c = kbd_ctrl_chars[c];
      tty_process_input (CURRENT_TTY, c);
    }
}

int
kbd_key_pressed (int key)
{
  return kbd_press_map[key];
}

void
kbd_cpu_reset (void)
{
  unsigned char c = 2;
  while (c & 2)
    c = inb (KBD_PORT_STATUS);
  outb (0xfe, KBD_PORT_STATUS);
  halt ();
}
