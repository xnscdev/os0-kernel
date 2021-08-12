/*************************************************************************
 * ioctl.c -- This file is part of OS/0.                                 *
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
#include <video/vga.h>

static struct winsize vga_display_winsize = {
  VGA_SCREEN_HEIGHT,
  VGA_SCREEN_WIDTH,
  9,
  16
};

static int
ioctl_isatty (Process *proc, int fd)
{
  return ttys[proc->p_sid] != NULL
    && proc->p_files[fd]->pf_inode->vi_sb == &tty_sb;
}

static int
ioctl_tcgets (Process *proc, int fd, struct termios *data)
{
  if (!ioctl_isatty (proc, fd))
    return -ENOTTY;
  memcpy (data, &ttys[proc->p_sid]->t_termios, sizeof (struct termios));
  return 0;
}

static int
ioctl_tcsets (Process *proc, int fd, struct termios *data)
{
  if (!ioctl_isatty (proc, fd))
    return -ENOTTY;
  memcpy (&ttys[proc->p_sid]->t_termios, data, sizeof (struct termios));
  return 0;
}

static int
ioctl_tcsetsf (Process *proc, int fd, struct termios *data)
{
  if (!ioctl_isatty (proc, fd))
    return -ENOTTY;
  ttys[proc->p_sid]->t_inbuf.tb_start = 0;
  ttys[proc->p_sid]->t_inbuf.tb_end = 0;
  memcpy (&ttys[proc->p_sid]->t_termios, data, sizeof (struct termios));
  return 0;
}

static int
ioctl_tcflush (Process *proc, int fd, int arg)
{
  if (!ioctl_isatty (proc, fd))
    return -ENOTTY;
  switch (arg)
    {
    case TCOFLUSH:
      return 0;
    case TCIFLUSH:
    case TCIOFLUSH:
      ttys[proc->p_sid]->t_inbuf.tb_start = 0;
      ttys[proc->p_sid]->t_inbuf.tb_end = 0;
      return 0;
    default:
      return -EINVAL;
    }
}

int
ioctl (int fd, unsigned long req, void *data)
{
  Process *proc = &process_table[task_getpid ()];
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd] == NULL)
    return -EBADF;
  switch (req)
    {
    case TCGETS:
    case TCGETA:
      return ioctl_tcgets (proc, fd, data);
    case TCSETS:
    case TCSETSW:
    case TCSETA:
    case TCSETAW:
      return ioctl_tcsets (proc, fd, data);
    case TCSETSF:
    case TCSETAF:
      return ioctl_tcsetsf (proc, fd, data);
    case TIOCGWINSZ:
      if (!ioctl_isatty (proc, fd))
	return -ENOTTY;
      memcpy (data, &vga_display_winsize, sizeof (struct winsize));
      break;
    case TIOCSWINSZ:
      return ioctl_isatty (proc, fd) ? -ENOTSUP : -ENOTTY;
    case TCSBRK:
    case TCSBRKP:
    case TIOCSBRK:
    case TIOCCBRK:
    case TCXONC:
      if (!ioctl_isatty (proc, fd))
	return -ENOTTY;
      break;
    case TIOCINQ:
      if (!ioctl_isatty (proc, fd))
	return -ENOTTY;
      *((int *) data) = ttys[proc->p_sid]->t_inbuf.tb_end -
	ttys[proc->p_sid]->t_inbuf.tb_start;
    case TIOCOUTQ:
      if (!ioctl_isatty (proc, fd))
	return -ENOTTY;
      *((int *) data) = 0;
      break;
    case TCFLSH:
      return ioctl_tcflush (proc, fd, (int) data);
    case TIOCSTI:
      return -ENOTSUP; /* TODO Implement */
    case TIOCGPGRP:
      if (!ioctl_isatty (proc, fd))
	return -ENOTTY;
      *((pid_t *) data) = ttys[proc->p_sid]->t_fg_pgid;
      break;
    case TIOCSPGRP:
      if (!ioctl_isatty (proc, fd))
	return -ENOTTY;
      ttys[proc->p_sid]->t_fg_pgid = *((pid_t *) data);
      break;
    case TIOCGSID:
      if (!ioctl_isatty (proc, fd))
	return -ENOTTY;
      *((pid_t *) data) = proc->p_sid;
      break;
    default:
      return -EINVAL;
    }
  return 0;
}
