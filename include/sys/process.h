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
#include <libk/array.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/task.h>

#define PROCESS_LIMIT         256
#define PROCESS_FILE_LIMIT    64
#define PROCESS_SEGMENT_LIMIT 32
#define PROCESS_BREAK_LIMIT   0xb0000000

typedef struct
{
  VFSInode *pf_inode;
  int pf_mode;
  int pf_flags;
  off_t pf_offset;
} ProcessFile;

typedef struct
{
  uint32_t ps_addr;
  uint32_t ps_size;
} ProcessSegment;

typedef struct
{
  unsigned char ps_enabled;
  unsigned char ps_blocked;
  struct sigaction ps_act;
} ProcessSignal;

typedef struct
{
  ProcessFile p_files[PROCESS_FILE_LIMIT];
  ProcessSignal p_signals[NR_signals];
  volatile ProcessTask *p_task;
  Array *p_segments;
  VFSInode *p_cwd;
  uint32_t p_break;
  struct rusage p_rusage;
  struct rusage p_cusage;
  int p_term;
  int p_waitstat;
  volatile unsigned int p_refcnt;
} Process;

__BEGIN_DECLS

extern Process process_table[PROCESS_LIMIT];

int process_exec (VFSInode *inode, uint32_t *entry);
void process_free (pid_t pid);
int process_setup_std_streams (pid_t pid);
uint32_t process_set_break (uint32_t addr);

__END_DECLS

#endif
