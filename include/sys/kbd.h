/*************************************************************************
 * kbd.h -- This file is part of OS/0.                                   *
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

#ifndef _SYS_KBD_H
#define _SYS_KBD_H

#include <sys/cdefs.h>
#include <stddef.h>

#define KBD_PORT_DATA    0x60
#define KBD_PORT_STATUS  0x64
#define KBD_PORT_COMMAND 0x64

#define KBD_BUFSIZ 1024

#define KEY_ESC      0x01
#define KEY_BACKSP   0x0e
#define KEY_ENTER    0x1c
#define KEY_LCTRL    0x1d
#define KEY_LSHIFT   0x2a
#define KEY_RSHIFT   0x36
#define KEY_LALT     0x38
#define KEY_CAPSLOCK 0x3a
#define KEY_NUMLOCK  0x45
#define KEY_SCRLLOCK 0x46

#define KEY_EXTENDED 0xe0

#define SHIFT_DOWN							\
  (kbd_key_pressed (KEY_LSHIFT) || kbd_key_pressed (KEY_RSHIFT))
#define CAPSLOCK_DOWN kbd_key_pressed (KEY_CAPSLOCK)
#define CTRL_DOWN     kbd_key_pressed (KEY_LCTRL)

__BEGIN_DECLS

void kbd_handle (int scancode);
int kbd_key_pressed (int key);
void kbd_cpu_reset (void) __attribute__ ((noreturn));

__END_DECLS

#endif
