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
  .vfs_write = vga_tty_write
};

static VFSInode *
vga_tty_alloc_inode (VFSSuperblock *sb)
{
  VFSInode *inode = kzalloc (sizeof (VFSInode));
  if (inode == NULL)
    return NULL;
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
vga_dev_read (SpecDevice *dev, void *buffer, size_t len, off_t offset)
{
  int nonblock =
    process_table[task_getpid ()].p_files[STDIN_FILENO].pf_flags & O_NONBLOCK;
  int ret = kbd_get_input (buffer, len, !nonblock);
  return ret < 0 ? ret : len;
}

int
vga_dev_write (SpecDevice *dev, const void *buffer, size_t len, off_t offset)
{
  vga_write (CURRENT_TERMINAL, buffer, len);
  return len;
}

int
vga_tty_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  return vga_dev_read (inode->vi_private, buffer, len, offset);
}

int
vga_tty_write (VFSInode *inode, const void *buffer, size_t len, off_t offset)
{
  return vga_dev_write (inode->vi_private, buffer, len, offset);
}
