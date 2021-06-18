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

#include <fs/vfs.h>
#include <sys/device.h>
#include <sys/kbd.h>
#include <termios.h>

#define VGA_SCREEN_WIDTH 80
#define VGA_SCREEN_HEIGHT 25
#define VGA_BUFFER 0xc00b8000

#define VGA_PORT_INDEX 0x3d4
#define VGA_PORT_DATA 0x3d5

#define vga_mkcolor(fg, bg) ((unsigned char) (fg | bg << 4))
#define vga_mkentry(c, color) ((uint16_t) c | (uint16_t) color << 8)
#define vga_getindex(x, y) (y * VGA_SCREEN_WIDTH + x)

#define DEFAULT_IFLAG (BRKINT | ISTRIP | ICRNL | IMAXBEL | IXON | IXANY)
#define DEFAULT_OFLAG (OPOST | ONLCR | XTABS)
#define DEFAULT_CFLAG (B9600 | CREAD | CS7 | PARENB | HUPCL)
#define DEFAULT_LFLAG (ECHO | ICANON | ISIG | IEXTEN | ECHOE | ECHOKE | ECHOCTL)

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

typedef struct
{
  uint16_t vt_data[VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT]; /* Screen data */
  KbdBuffer vt_kbdbuf;       /* Keyboard input buffer for terminal */
  size_t vt_row;             /* Cursor row */
  size_t vt_column;          /* Cursor column */
  pid_t vt_fgpgid;           /* Foreground process group ID */
  unsigned char vt_color;    /* Current color */
  struct termios vt_termios; /* Termios structure */
} Terminal;

#define CURRENT_TERMINAL (terminals[active_terminal])

__BEGIN_DECLS

extern Terminal *terminals[PROCESS_LIMIT];
extern int active_terminal;
extern uint16_t *vga_hdw_buf;
extern VFSSuperblock vga_tty_sb;

void vga_init (void);
void vga_putentry (Terminal *term, char c, size_t x, size_t y);
void vga_putchar (Terminal *term, char c);
void vga_delchar (Terminal *term);
void vga_write (Terminal *term, const char *s, size_t size);
void vga_puts (Terminal *term, const char *s);
void vga_setcurs (size_t x, size_t y);

int vga_dev_read (SpecDevice *dev, void *buffer, size_t len, off_t offset);
int vga_dev_write (SpecDevice *dev, const void *buffer, size_t len,
		   off_t offset);
int vga_tty_read (VFSInode *inode, void *buffer, size_t len, off_t offset);
int vga_tty_write (VFSInode *inode, const void *buffer, size_t len,
		   off_t offset);

void set_active_terminal (int term);

__END_DECLS

#endif
