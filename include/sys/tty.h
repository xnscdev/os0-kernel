/*************************************************************************
 * tty.h -- This file is part of OS/0.                                   *
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

#ifndef _SYS_TTY_H
#define _SYS_TTY_H

#include <fs/vfs.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <termios.h>

#define TTY_BUFFER_SIZE 4096

#define VGA_SCREEN_WIDTH  80
#define VGA_SCREEN_HEIGHT 25

#define TTY_INPUT_READY   0x0001
#define TTY_REVERSE_VIDEO 0x0002
#define TTY_QUOTE_INPUT   0x0004

#define DEFAULT_IFLAG (BRKINT | ISTRIP | ICRNL | IMAXBEL | IXON | IXANY)
#define DEFAULT_OFLAG (OPOST | ONLCR | XTABS)
#define DEFAULT_CFLAG (B38400 | CREAD | CS7 | PARENB | HUPCL)
#define DEFAULT_LFLAG (ECHO | ICANON | ISIG | IEXTEN | ECHOE | ECHOKE | ECHOCTL)

typedef struct
{
  char tb_data[TTY_BUFFER_SIZE];
  size_t tb_start;
  size_t tb_end;
} TTYInputBuffer;

typedef struct
{
  uint16_t t_screenbuf[VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT]; /* Screen data */
  TTYInputBuffer t_inbuf;       /* Input buffer for terminal */
  size_t t_row;                 /* Cursor row */
  size_t t_column;              /* Cursor column */
  size_t t_saved_row;           /* Saved cursor row */
  size_t t_saved_col;           /* Saved cursor column */
  pid_t t_fg_pgid;              /* Foreground process group ID */
  unsigned char t_color;        /* Current color */
  unsigned int t_flags;         /* Flags */
  struct termios t_termios;     /* Termios structure */
} TTY;

#define CURRENT_TTY (ttys[active_tty])

__BEGIN_DECLS

extern TTY default_tty;
extern TTY *ttys[PROCESS_LIMIT];
extern int active_tty;
extern VFSSuperblock tty_sb;

void tty_input_buffer_add_char (TTY *tty, char c);
void tty_flush_input_line (TTY *tty, char delim);
size_t tty_erase_input (TTY *tty);
size_t tty_erase_input_word (TTY *tty);
size_t tty_kill_input (TTY *tty);
void tty_reprint_input (TTY *tty);
void tty_process_input (TTY *tty, char c);
void tty_process_output (TTY *tty, char c, size_t arg);

int tty_read (VFSInode *inode, void *buffer, size_t len, off_t offset);
int tty_write (VFSInode *inode, const void *buffer, size_t len, off_t offset);
int tty_getattr (VFSInode *inode, struct stat64 *st);

__END_DECLS

#endif
