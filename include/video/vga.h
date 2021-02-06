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

#include <libk/types.h>
#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

#define VGA_SCREEN_WIDTH 80
#define VGA_SCREEN_HEIGHT 25
#define VGA_BUFFER 0xc00b8000

#define VGA_PORT_INDEX 0x3d4
#define VGA_PORT_DATA 0x3d5

#define vga_mkcolor(fg, bg) ((u8) (fg | bg << 4))
#define vga_mkentry(c, color) ((u16) c | (u16) color << 8)
#define vga_getindex(x, y) (y * VGA_SCREEN_WIDTH + x)

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
  VGA_COLOR_LIGHT_BROWN,
  VGA_COLOR_WHITE
} VGAColor;

__BEGIN_DECLS

void vga_init (void);
u8 vga_getcolor (void);
void vga_setcolor (u8 color);
void vga_putentry (char c, size_t x, size_t y);
void vga_putchar (char c);
void vga_write (const char *s, size_t size);
void vga_puts (const char *s);
void vga_setcurs (size_t x, size_t y);

__END_DECLS

#endif
