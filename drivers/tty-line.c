/*************************************************************************
 * tty-line.c -- This file is part of OS/0.                              *
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
#include <sys/timer.h>
#include <video/vga.h>
#include <vm/heap.h>

#define DO_END(x) do { (x); goto end; } while (0)
#define DO_RET(x) do { (x); return; } while (0)

#define CHAR_MATCH(match) (c == term->c_cc[match] && term->c_cc[match] != 0xff)

TTY default_tty = {
  .t_termios = {
    .c_iflag = DEFAULT_IFLAG,
    .c_oflag = DEFAULT_OFLAG,
    .c_cflag = DEFAULT_CFLAG,
    .c_lflag = DEFAULT_LFLAG,
    .c_cc = {
      [VINTR]    = '\3',
      [VQUIT]    = '\34',
      [VERASE]   = '\37',
      [VKILL]    = '\25',
      [VEOF]     = '\4',
      [VTIME]    = 0,
      [VMIN]     = 1,
      [VSTART]   = '\21',
      [VSTOP]    = '\23',
      [VSUSP]    = '\32',
      [VEOL]     = 0xff,
      [VREPRINT] = '\22',
      [VDISCARD] = '\17',
      [VWERASE]  = '\27',
      [VLNEXT]   = '\26',
      [VEOL2]    = 0xff
    },
    .c_ispeed = B38400,
    .c_ospeed = B38400
  },
  .t_color = VGA_DEFAULT_COLOR
};

TTY *ttys[PROCESS_LIMIT] = {&default_tty};
int active_tty;

static const VFSInodeOps tty_iops = {
  .vfs_read = tty_read,
  .vfs_write = tty_write,
  .vfs_getattr = tty_getattr
};

static VFSInode *
tty_alloc_inode (VFSSuperblock *sb)
{
  VFSInode *inode = kzalloc (sizeof (VFSInode));
  if (inode == NULL)
    return NULL;
  inode->vi_mode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH
    | S_IWOTH;
  inode->vi_ops = &tty_iops;
  inode->vi_sb = &tty_sb;
  return inode;
}

static const VFSSuperblockOps tty_sb_ops = {
  .sb_alloc_inode = tty_alloc_inode,
  .sb_destroy_inode = (void (*) (VFSInode *)) kfree
};

VFSSuperblock tty_sb = {
  .sb_ops = &tty_sb_ops
};

void
tty_input_buffer_add_char (TTY *tty, char c)
{
  TTYInputBuffer *buffer = &tty->t_inbuf;
  if (buffer->tb_end >= TTY_BUFFER_SIZE - 1)
    {
      /* If data was read in raw mode from the start of the buffer, try
	 to make more room for the new data */
      if (buffer->tb_start > 0)
	{
	  memmove (buffer->tb_data, &buffer->tb_data[buffer->tb_start],
		   buffer->tb_end - buffer->tb_start);
	  buffer->tb_end -= buffer->tb_start;
	}
    }
  if (buffer->tb_end >= TTY_BUFFER_SIZE - 1
      && (!(tty->t_termios.c_lflag & ICANON) || c != '\n'))
    {
      /* Not enough space, discard input. Sound the system bell if desired. */
      if (tty->t_termios.c_iflag & IMAXBEL)
	speaker_beep ();
      return;
    }
  buffer->tb_data[buffer->tb_end++] = c;
}

void
tty_flush_input_line (TTY *tty, char delim)
{
  if (delim != '\0')
    tty_input_buffer_add_char (tty, delim);
  tty->t_flags |= TTY_INPUT_READY;
}

size_t
tty_erase_input (TTY *tty)
{
  /* Erase the last character in the input buffer if there is one and it is
     not an EOF character */
  TTYInputBuffer *buffer = &tty->t_inbuf;
  if (buffer->tb_end > 0
      && (buffer->tb_data[buffer->tb_end - 1] != tty->t_termios.c_cc[VEOF]
	  || tty->t_termios.c_cc[VEOF] == 0xff))
    {
      buffer->tb_end--;
      return 1;
    }
  else
    return 0;
}

size_t
tty_erase_input_word (TTY *tty)
{
  /* Erase input to the start of the previous word or EOF character */
  TTYInputBuffer *buffer = &tty->t_inbuf;
  char eof = tty->t_termios.c_cc[VEOF];
  long i = buffer->tb_end - 1;
  size_t len;
  while (i >= buffer->tb_start && isspace (buffer->tb_data[i])
	 && (buffer->tb_data[i] != eof || eof == 0xff))
    i--;
  while (i >= buffer->tb_start && !isspace (buffer->tb_data[i])
	 && (buffer->tb_data[i] != eof || eof == 0xff))
    i--;
  if (i < 0)
    i = 0;
  else
    i++;
  len = buffer->tb_end - i;
  buffer->tb_end = i;
  return len;
}

size_t
tty_kill_input (TTY *tty)
{
  /* Set the end pointer of the input buffer to the last occurence of the
     EOF character */
  TTYInputBuffer *buffer = &tty->t_inbuf;
  size_t len;
  char *ptr = tty->t_termios.c_cc[VEOF] == 0xff ? NULL :
    memrchr (buffer->tb_data, tty->t_termios.c_cc[VEOF], buffer->tb_end);
  if (ptr == NULL || ptr < buffer->tb_data + buffer->tb_start)
    {
      len = buffer->tb_end - buffer->tb_start;
      buffer->tb_start = 0;
      buffer->tb_end = 0;
    }
  else
    {
      len = buffer->tb_data + buffer->tb_end - ptr;
      buffer->tb_end = ptr - buffer->tb_data;
    }
  return len;
}

void
tty_reprint_input (TTY *tty)
{
  /* Copy the unread input into the output buffer */
  TTYInputBuffer *buffer = &tty->t_inbuf;
  size_t i;
  for (i = buffer->tb_start; i < buffer->tb_end; i++)
    tty_process_output (tty, buffer->tb_data[i], 0);
}

void
tty_process_input (TTY *tty, char c)
{
  struct termios *term;
  size_t arg = 0;
  if (tty == NULL)
    return;
  if (tty->t_flags & TTY_QUOTE_INPUT)
    {
      tty->t_flags &= ~TTY_QUOTE_INPUT;
      goto raw;
    }

  term = &tty->t_termios;
  if (term->c_lflag & ICANON)
    {
      if (c == '\n' || CHAR_MATCH (VEOL) || CHAR_MATCH (VEOL2))
	DO_END (tty_flush_input_line (tty, c));
      else if (CHAR_MATCH (VEOF) && tty->t_inbuf.tb_end == 0)
	DO_END (tty_flush_input_line (tty, '\0'));
      else if (CHAR_MATCH (VERASE))
	DO_END (arg = tty_erase_input (tty));
      else if (CHAR_MATCH (VKILL))
	DO_END (arg = tty_kill_input (tty));
      else if (term->c_lflag & IEXTEN)
	{
	  if (CHAR_MATCH (VWERASE))
	    DO_END (arg = tty_erase_input_word (tty));
	  else if (CHAR_MATCH (VREPRINT))
	    DO_END (tty_reprint_input (tty));
	  else if (CHAR_MATCH (VLNEXT))
	    DO_END (tty->t_flags |= TTY_QUOTE_INPUT);
	}
    }

  if (term->c_lflag & ISIG)
    {
      int sig = 0;
      if (CHAR_MATCH (VINTR))
	sig = SIGINT;
      else if (CHAR_MATCH (VSUSP))
	sig = SIGTSTP;
      else if (CHAR_MATCH (VQUIT))
	sig = SIGQUIT;
      if (sig != 0)
	{
	  int i;
	  for (i = 1; i < PROCESS_LIMIT; i++)
	    {
	      if (process_table[i].p_pgid == tty->t_fg_pgid)
		process_send_signal (i, sig);
	    }
	  return;
	}
    }

 raw:
  tty_input_buffer_add_char (tty, c);

 end:
  if (term->c_lflag & ECHO)
    tty_process_output (tty, c, arg);
  else if (c == '\n' && (term->c_lflag & ECHONL))
    tty_process_output (tty, '\n', arg);
}

void
tty_process_output (TTY *tty, char c, size_t arg)
{
  struct termios *term = &tty->t_termios;
  if (term->c_lflag & ICANON)
    {
      if (CHAR_MATCH (VERASE) && (term->c_lflag & ECHOE))
	{
	  if (arg)
	    vga_erase_char (tty);
	  return;
	}
      else if (CHAR_MATCH (VWERASE) && (term->c_lflag & ECHOE))
	DO_RET (vga_erase_line (tty, arg));
      else if (CHAR_MATCH (VKILL) && (term->c_lflag & (ECHOK | ECHOKE)))
	DO_RET (vga_erase_line (tty, arg));
    }

  switch (c)
    {
    case '\n':
    case '\t':
      vga_putchar (tty, c);
      break;
    default:
      if (iscntrl (c) && (term->c_lflag & ECHOCTL))
	{
	  if (!CHAR_MATCH (VEOF) || tty->t_column > 0)
	    {
	      vga_putchar (tty, '^');
	      vga_putchar (tty, c + 0x40);
	    }
	}
      else
	vga_putchar (tty, c);
    }
}

void
tty_wait_input_ready (TTY *tty)
{
  while (!(tty->t_flags & TTY_INPUT_READY))
    ;
}

void
tty_set_active (int term)
{
  active_tty = term;
  memcpy ((void *) VGA_BUFFER, CURRENT_TTY->t_screenbuf,
	  2 * VGA_SCREEN_WIDTH * VGA_SCREEN_HEIGHT);
}

int
tty_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  TTYInputBuffer *inbuf = &CURRENT_TTY->t_inbuf;
  if (CURRENT_TTY->t_termios.c_lflag & ICANON)
    {
      tty_wait_input_ready (CURRENT_TTY);
    data_ready:
      if (inbuf->tb_end - inbuf->tb_start < len)
	len = inbuf->tb_end - inbuf->tb_start;
      memcpy (buffer, &inbuf->tb_data[inbuf->tb_start], len);
      inbuf->tb_start += len;
      if (inbuf->tb_start == inbuf->tb_end)
	{
	  inbuf->tb_start = 0;
	  inbuf->tb_end = 0;
	  CURRENT_TTY->t_flags &= ~TTY_INPUT_READY;
	}
      return len;
    }
  else
    {
      cc_t min = CURRENT_TTY->t_termios.c_cc[VMIN];
      cc_t time = CURRENT_TTY->t_termios.c_cc[VTIME];
      if (inode->vi_flags & VI_FLAG_NONBLOCK)
	goto data_ready;
      else if (min == 0 && time == 0)
	{
	  if (inbuf->tb_end > inbuf->tb_start)
	    goto data_ready;
	  else
	    return 0;
	}
      else if (min > 0 && time == 0)
	{
	  while (inbuf->tb_end - inbuf->tb_start < min)
	    ;
	  goto data_ready;
	}
      else if (min == 0 && time > 0)
	{
	  unsigned long tick = timer_poll ();
	  while (inbuf->tb_start == inbuf->tb_end
		 && tick + time * 100 < timer_poll ())
	    ;
	  if (inbuf->tb_start == inbuf->tb_end)
	    return 0;
	  else
	    goto data_ready;
	}
      else
	{
	  size_t bytes = inbuf->tb_end - inbuf->tb_start;
	  unsigned long tick = timer_poll ();
	  while (bytes < min && bytes < len
		 && tick + time * 100 < timer_poll ())
	    {
	      if (inbuf->tb_start - inbuf->tb_end > bytes)
		bytes = inbuf->tb_start - inbuf->tb_end;
	    }
	  goto data_ready;
	}
    }
}

int
tty_write (VFSInode *inode, const void *buffer, size_t len, off_t offset)
{
  if (CURRENT_TTY->t_termios.c_lflag & TOSTOP)
    {
      pid_t pid = task_getpid ();
      if (process_table[pid].p_pgid != CURRENT_TTY->t_fg_pgid)
	process_send_signal (pid, SIGTTOU);
    }
  vga_write (CURRENT_TTY, buffer, len);
  return len;
}

int
tty_getattr (VFSInode *inode, struct stat64 *st)
{
  st->st_dev = 0;
  st->st_blksize = 512;
  return 0;
}
