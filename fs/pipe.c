/*************************************************************************
 * pipe.c -- This file is part of OS/0.                                  *
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

#include <fs/pipe.h>
#include <libk/libk.h>
#include <sys/process.h>
#include <sys/syscall.h>
#include <vm/heap.h>

const VFSSuperblockOps pipe_sops = {
  .sb_alloc_inode = pipe_alloc_inode,
  .sb_destroy_inode = pipe_destroy_inode
};

const VFSInodeOps pipe_iops = {
  .vfs_read = pipe_read,
  .vfs_write = pipe_write,
  .vfs_getattr = pipe_getattr
};

VFSSuperblock pipe_sb = {
  NULL,
  PIPE_BLKSIZE,
  PIPE_LENGTH,
  &pipe_sops,
  0,
  0,
  -1,
  0,
  NULL,
  NULL,
  NULL
};

VFSInode *
pipe_alloc_inode (VFSSuperblock *sb)
{
  VFSInode *inode = kzalloc (sizeof (VFSInode));
  time_t t;
  if (unlikely (inode == NULL))
    return NULL;
  inode->vi_ops = &pipe_iops;
  inode->vi_sb = sb;

  /* Set reasonable defaults */
  t = time (NULL);
  inode->vi_mode = S_IFIFO | S_IRWXU | S_IRWXG | S_IRWXO;
  inode->vi_atime.tv_sec = t;
  inode->vi_mtime.tv_sec = t;
  inode->vi_ctime.tv_sec = t;
  inode->vi_blocks = PIPE_BLOCKS;
  return inode;
}

void
pipe_destroy_inode (VFSInode *inode)
{
  Pipe *pipe = inode->vi_private;
  if (inode->vi_flags & PIPE_WRITE_END)
    pipe->p_flags |= PIPE_WRITE_CLOSED;
  else
    pipe->p_flags |= PIPE_READ_CLOSED;

  /* Free the pipe if both ends are closed */
  if ((pipe->p_flags & PIPE_READ_CLOSED) && (pipe->p_flags & PIPE_WRITE_CLOSED))
    {
      kfree (pipe->p_data);
      kfree (pipe);
    }
}

int
pipe_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  Pipe *pipe = inode->vi_private;
  if (pipe->p_flags & PIPE_DATA_END)
    return 0;
  while (pipe->p_readptr + len > pipe->p_writeptr)
    {
      /* Wait for enough data to be available, and check if the pipe
	 has been closed while waiting */
      if (pipe->p_flags & PIPE_WRITE_CLOSED)
	{
	  pipe->p_flags |= PIPE_DATA_END;
	  len = pipe->p_writeptr - pipe->p_readptr;
	  break;
	}
    }
  memcpy (buffer, pipe->p_data + pipe->p_readptr, len);
  pipe->p_readptr += len;
  return len;
}

int
pipe_write (VFSInode *inode, const void *buffer, size_t len, off_t offset)
{
  Pipe *pipe = inode->vi_private;
  if (pipe->p_flags & PIPE_READ_CLOSED)
    goto err;
  if (pipe->p_writeptr + len > PIPE_LENGTH)
    {
      /* Wait for enough data to be read, and check if the read end has
	 been closed while waiting */
      while (pipe->p_readptr < PIPE_LENGTH - pipe->p_writeptr)
	{
	  if (pipe->p_flags & PIPE_READ_CLOSED)
	    goto err;
	}
      memmove (pipe->p_data, pipe->p_data + pipe->p_readptr,
	       PIPE_LENGTH - pipe->p_readptr);
      pipe->p_writeptr -= pipe->p_readptr;
      pipe->p_readptr = 0;
    }
  memcpy (pipe->p_data + pipe->p_writeptr, buffer, len);
  pipe->p_writeptr += len;
  return len;

 err:
  process_send_signal (task_getpid (), SIGPIPE);
  return -EPIPE;
}

int
pipe_getattr (VFSInode *inode, struct stat64 *st)
{
  st->st_dev = 0;
  st->st_blksize = PIPE_BLKSIZE;
  return 0;
}
