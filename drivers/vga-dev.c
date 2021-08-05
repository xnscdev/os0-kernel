/*************************************************************************
 * vga-dev.c -- This file is part of OS/0.                               *
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
#include <sys/kbd.h>
#include <sys/process.h>
#include <video/vga.h>
#include <vm/heap.h>

static const VFSInodeOps vga_tty_iops = {
  .vfs_read = vga_tty_read,
  .vfs_write = vga_tty_write,
  .vfs_getattr = vga_tty_getattr
};

static VFSInode *
vga_tty_alloc_inode (VFSSuperblock *sb)
{
  VFSInode *inode = kzalloc (sizeof (VFSInode));
  if (inode == NULL)
    return NULL;
  inode->vi_mode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH
    | S_IWOTH;
  inode->vi_ops = &vga_tty_iops;
  inode->vi_sb = &vga_tty_sb;
  return inode;
}

static const VFSSuperblockOps vga_tty_sb_ops = {
  .sb_alloc_inode = vga_tty_alloc_inode,
  .sb_destroy_inode = (void (*) (VFSInode *)) kfree
};

VFSSuperblock vga_tty_sb = {
  .sb_ops = &vga_tty_sb_ops
};

int
vga_tty_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  return kbd_get_input (buffer, len,
			inode->vi_flags & VI_FLAG_NONBLOCK ? 0 : 1);
}

int
vga_tty_write (VFSInode *inode, const void *buffer, size_t len, off_t offset)
{
  if (CURRENT_TERMINAL->vt_termios.c_lflag & TOSTOP)
    {
      pid_t pid = task_getpid ();
      if (process_table[pid].p_pgid != CURRENT_TERMINAL->vt_fgpgid)
	process_send_signal (pid, SIGTTOU);
    }
  vga_write (CURRENT_TERMINAL, buffer, len);
  return len;
}

int
vga_tty_getattr (VFSInode *inode, struct stat64 *st)
{
  st->st_dev = 0;
  st->st_blksize = 512;
  return 0;
}
