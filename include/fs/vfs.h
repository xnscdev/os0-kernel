/*************************************************************************
 * vfs.h -- This file is part of OS/0.                                   *
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

#ifndef _FS_VFS_H
#define _FS_VFS_H

#include <libk/libk.h>

typedef struct
{
  uid_t vi_uid;
  gid_t vi_gid;
  u32 vi_flags;
  u32 vi_ino;
  u32 vi_nlink;
  dev_t vi_rdev;
  loff_t vi_size;
  struct timespec vi_atime;
  struct timespec vi_mtime;
  struct timespec vi_ctime;
  blkcnt_t vi_blocks;
} VFSInode;

#endif
