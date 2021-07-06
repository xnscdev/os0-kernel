/*************************************************************************
 * pipe.h -- This file is part of OS/0.                                  *
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

#ifndef _FS_PIPE_H
#define _FS_PIPE_H

/* Pseudo-filesystem for pipes */

#include <fs/vfs.h>
#include <sys/cdefs.h>
#include <sys/memory.h>

#define PIPE_BLKSIZE    PAGE_SIZE
#define PIPE_BLOCKS     4
#define PIPE_LENGTH     (PIPE_BLKSIZE * PIPE_BLOCKS)

/* VFS inode flags */

#define PIPE_WRITE_END 0x01

/* Pipe flags */

#define PIPE_READ_CLOSED  0x01
#define PIPE_WRITE_CLOSED 0x02

typedef struct
{
  size_t p_readptr;  /* Offset in data where next read starts */
  size_t p_writeptr; /* Offset in data where next write starts */
  void *p_data;      /* Data area reserved for pipe */
  int p_flags;       /* Pipe flags */
} Pipe;

__BEGIN_DECLS

extern const VFSSuperblockOps pipe_sops;
extern const VFSInodeOps pipe_iops;
extern VFSSuperblock pipe_sb;

VFSInode *pipe_alloc_inode (VFSSuperblock *sb);
void pipe_destroy_inode (VFSInode *inode);

int pipe_read (VFSInode *inode, void *buffer, size_t len, off_t offset);
int pipe_write (VFSInode *inode, const void *buffer, size_t len, off_t offset);
int pipe_getattr (VFSInode *inode, struct stat64 *st);

__END_DECLS

#endif
