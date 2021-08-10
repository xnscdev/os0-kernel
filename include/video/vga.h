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
#define vga_setfg(color, fg) ((unsigned char) ((color & 0xf0) | fg))
#define vga_setbg(color, bg) ((unsigned char) ((color & 0x0f) | (bg << 4)))
#define vga_mkentry(c, color) ((uint16_t) c | (uint16_t) color << 8)
#define vga_getindex(x, y) (y * VGA_SCREEN_WIDTH + x)

#define VGA_DEFAULT_FG    VGA_COLOR_LIGHT_GREY
#define VGA_DEFAULT_BG    VGA_COLOR_BLACK
#define VGA_DEFAULT_COLOR vga_mkcolor (VGA_DEFAULT_FG, VGA_DEFAULT_BG)

#define DEFAULT_IFLAG (BRKINT | ISTRIP | ICRNL | IMAXBEL | IXON | IXANY)
#define DEFAULT_OFLAG (OPOST | ONLCR | XTABS)
#define DEFAULT_CFLAG (B38400 | CREAD | CS7 | PARENB | HUPCL)
#define DEFAULT_LFLAG (ECHO | ICANON | ISIG | IEXTEN | ECHOE | ECHOKE | ECHOCTL)

#define VT_FLAG_REVERSE 0x0001

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

typedef enum
{
  TERMINAL_ESC_SEQ_ESC = 0, /* Initial state */
  TERMINAL_ESC_SEQ_CSI      /* Control sequence */
} TerminalEscSeqMode;

typedef struct
{
  unsigned int vte_flags;
  unsigned int vte_curr;
  unsigned int vte_num[4];
  TerminalEscSeqMode vte_mode;
} TerminalEscSeqState;

typedef struct
{
  uint16_t vt_data[VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT]; /* Screen data */
  KbdBuffer vt_kbdbuf;           /* Keyboard input buffer for terminal */
  TerminalEscSeqState vt_escseq; /* State for parsing escape sequences */
  size_t vt_row;                 /* Cursor row */
  size_t vt_column;              /* Cursor column */
  size_t vt_saved_row;           /* Saved cursor row */
  size_t vt_saved_col;           /* Saved cursor column */
  pid_t vt_fgpgid;               /* Foreground process group ID */
  unsigned char vt_color;        /* Current color */
  unsigned int vt_flags;
  struct termios vt_termios;     /* Termios structure */
} Terminal;

#define CURRENT_TERMINAL (terminals[active_terminal])

__BEGIN_DECLS

extern Terminal *terminals[PROCESS_LIMIT];
extern int active_terminal;
extern uint16_t *vga_hdw_buf;
extern VFSSuperblock vga_tty_sb;
extern int parsing_escseq;

void vga_init (void);
void vga_putentry (Terminal *term, char c, size_t x, size_t y);
void vga_putchar (Terminal *term, char c);
void vga_delchar (Terminal *term);
void vga_delline (Terminal *term, size_t len);
void vga_write (Terminal *term, const char *s, size_t size);
void vga_puts (Terminal *term, const char *s);
void vga_display_putchar (Terminal *term, char c);
void vga_clear (Terminal *term);
void vga_setcurs (size_t x, size_t y);

int vga_tty_read (VFSInode *inode, void *buffer, size_t len, off_t offset);
int vga_tty_write (VFSInode *inode, const void *buffer, size_t len,
		   off_t offset);
int vga_tty_getattr (VFSInode *inode, struct stat64 *st);

void vga_terminal_cancel_escseq (Terminal *term);
void vga_terminal_parse_escseq (Terminal *term, char c);
void set_active_terminal (int term);

__END_DECLS

#endif
