/*************************************************************************
 * process.h -- This file is part of OS/0.                               *
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

#ifndef _SYS_PROCESS_H
#define _SYS_PROCESS_H

#include <fs/vfs.h>
#include <sys/cdefs.h>
#include <sys/task.h>

#define PROCESS_LIMIT 256
#define PROCESS_FILE_LIMIT 64

typedef struct
{
  VFSInode *pf_inode;
  int pf_mode;
} ProcessFile;

typedef struct
{
  ProcessFile p_files[PROCESS_FILE_LIMIT];
  volatile ProcessTask *p_task;
} Process;

__BEGIN_DECLS

extern Process process_table[PROCESS_LIMIT];

__END_DECLS

#endif
