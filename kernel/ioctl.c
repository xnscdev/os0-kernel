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

static int
ioctl_tcgets (Process *proc, int fd, struct termios *data)
{
  SpecDevice *dev = proc->p_files[fd].pf_inode->vi_private;
  if (dev == NULL || dev->sd_major != 1 || (dev->sd_minor != STDIN_FILENO
					    && dev->sd_minor != STDOUT_FILENO
					    && dev->sd_minor != STDERR_FILENO))
    return -ENOTTY;
  data->c_iflag = BRKINT | ISTRIP | ICRNL | IMAXBEL | IXON | IXANY;
  data->c_oflag = OPOST | ONLCR | XTABS;
  data->c_cflag = B9600 | CREAD | CS7 | PARENB | HUPCL;
  data->c_lflag = ECHO | ICANON | ISIG | IEXTEN | ECHOE | ECHOKE | ECHOCTL;
  return 0;
}

int
ioctl (int fd, unsigned long req, void *data)
{
  Process *proc = &process_table[task_getpid ()];
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd].pf_inode == NULL)
    return -EBADF;
  switch (req)
    {
    case TCGETS:
      return ioctl_tcgets (proc, fd, data);
    }
  return -EINVAL;
}
