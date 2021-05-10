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

#include <sys/process.h>
#include <video/vga.h>
#include <vm/heap.h>
#include <errno.h>

static int
vga_dev_write (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  vga_write (buffer, len);
  return 0;
}

static const VFSInodeOps vga_tty_iops = {
  .vfs_write = vga_dev_write
};

static VFSInode *
vga_stdout_alloc_inode (VFSSuperblock *sb)
{
  VFSInode *inode = kzalloc (sizeof (VFSInode));
  if (inode == NULL)
    return NULL;
  inode->vi_ops = &vga_tty_iops;
  inode->vi_sb = &vga_stdout_sb;
  return inode;
}

static VFSInode *
vga_stderr_alloc_inode (VFSSuperblock *sb)
{
  VFSInode *inode = kzalloc (sizeof (VFSInode));
  if (inode == NULL)
    return NULL;
  inode->vi_ops = &vga_tty_iops;
  inode->vi_sb = &vga_stderr_sb;
  return inode;
}

static const VFSSuperblockOps vga_stdout_sb_ops = {
  .sb_alloc_inode = vga_stdout_alloc_inode,
  .sb_destroy_inode = (void (*) (VFSInode *)) kfree
};

static const VFSSuperblockOps vga_stderr_sb_ops = {
  .sb_alloc_inode = vga_stderr_alloc_inode,
  .sb_destroy_inode = (void (*) (VFSInode *)) kfree
};

VFSSuperblock vga_stdout_sb = {
  .sb_ops = &vga_stdout_sb_ops
};

VFSSuperblock vga_stderr_sb = {
  .sb_ops = &vga_stderr_sb_ops
};

int
is_vga_tty (int fd)
{
  Process *proc = &process_table[task_getpid ()];
  if (fd < 0 || fd >= PROCESS_FILE_LIMIT || proc->p_files[fd].pf_inode == NULL)
    return -EBADF;
  return proc->p_files[fd].pf_inode->vi_sb == &vga_stdout_sb
    || proc->p_files[fd].pf_inode->vi_sb == &vga_stderr_sb;
}
