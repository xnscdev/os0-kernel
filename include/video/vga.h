/*************************************************************************
 * vga.h -- This file is part of OS/0.                                   *
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

#ifndef _VIDEO_VGA_H
#define _VIDEO_VGA_H

#include <kconfig.h>

#include <sys/device.h>
#include <sys/kbd.h>
#include <sys/tty.h>
#include <termios.h>

#define VGA_BUFFER 0xc00b8000

#define VGA_PORT_INDEX 0x3d4
#define VGA_PORT_DATA 0x3d5

#define vga_mkcolor(fg, bg) ((unsigned char) (fg | bg << 4))
#define vga_setfg(color, fg) ((unsigned char) ((color & 0xf0) | fg))
#define vga_setbg(color, bg) ((unsigned char) ((color & 0x0f) | (bg << 4)))
#define vga_mkentry(c, color) ((uint16_t) c | (uint16_t) color << 8)
#define vga_getindex(x, y) (y * VGA_SCREEN_WIDTH + x)

#define VGA_DEFAULT_FG    VGA_COLOR_LIGHT_GREY
#define VGA_DEFAULT_BG    VGA_COLOR_BLACK
#define VGA_DEFAULT_COLOR vga_mkcolor (VGA_DEFAULT_FG, VGA_DEFAULT_BG)

#define VTE_FLAG_ERR 0x0001
#define VTE_FLAG_NUM 0x0002

typedef enum
{
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE,
  VGA_COLOR_GREEN,
  VGA_COLOR_CYAN,
  VGA_COLOR_RED,
  VGA_COLOR_MAGENTA,
  VGA_COLOR_BROWN,
  VGA_COLOR_LIGHT_GREY,
  VGA_COLOR_DARK_GREY,
  VGA_COLOR_LIGHT_BLUE,
  VGA_COLOR_LIGHT_GREEN,
  VGA_COLOR_LIGHT_CYAN,
  VGA_COLOR_LIGHT_RED,
  VGA_COLOR_LIGHT_MAGENTA,
  VGA_COLOR_YELLOW,
  VGA_COLOR_WHITE
} VGAColor;

__BEGIN_DECLS

extern uint16_t *vga_hdw_buf;

void vga_init (void);
void vga_putentry (TTY *tty, char c, size_t x, size_t y);
void vga_putchar (TTY *tty, char c);
void vga_erase_char (TTY *tty);
void vga_erase_line (TTY *tty, size_t len);
void vga_write (TTY *tty, const char *s, size_t size);
void vga_puts (TTY *tty, const char *s);
void vga_display_putchar (TTY *tty, char c);
void vga_clear (TTY *tty);
void vga_update_display (TTY *tty);
void vga_setcurs (size_t x, size_t y);

__END_DECLS

#endif
