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

#include <kconfig.h> /* Define process limit macros */

#include <fs/vfs.h>
#include <libk/array.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/task.h>
#include <termios.h>

#define PROCESS_BREAK_LIMIT   0xb0000000

#define DEFAULT_IFLAG (BRKINT | ISTRIP | ICRNL | IMAXBEL | IXON | IXANY)
#define DEFAULT_OFLAG (OPOST | ONLCR | XTABS)
#define DEFAULT_CFLAG (B9600 | CREAD | CS7 | PARENB | HUPCL)
#define DEFAULT_LFLAG (ECHO | ICANON | ISIG | IEXTEN | ECHOE | ECHOKE | ECHOCTL)

typedef struct
{
  VFSInode *pf_inode;
  int pf_mode;
  int pf_flags;
  off_t pf_offset;
  struct termios *pf_termios;
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
  ProcessFile p_files[PROCESS_FILE_LIMIT]; /* File descriptor table */
  ProcessSignal p_signals[NR_signals];     /* Signal handler table */
  volatile ProcessTask *p_task;            /* Scheduler task */
  Array *p_segments;                       /* List of segments in memory */
  VFSInode *p_cwd;                         /* Working directory */
  uint32_t p_break;                        /* Location of program break */
  struct rusage p_rusage;                  /* Resource usage */
  struct rusage p_cusage;                  /* Resourge usage of child
					      processes */
  int p_term;                              /* If process is terminated */
  int p_waitstat;                          /* Value to set wait status to */
  uid_t p_uid;                             /* Real user id */
  uid_t p_euid;                            /* Effective user id */
  uid_t p_suid;                            /* Saved set-user-id */
  gid_t p_gid;                             /* Real group id */
  gid_t p_egid;                            /* Effective group id */
  gid_t p_sgid;                            /* Saved set-group-id */
  volatile unsigned int p_refcnt;          /* Reference count, used by wait to 
					      postpone freeing a process */
} Process;

__BEGIN_DECLS

extern Process process_table[PROCESS_LIMIT];

int process_exec (VFSInode *inode, uint32_t *entry);
void process_free (pid_t pid);
int process_setup_std_streams (pid_t pid);
uint32_t process_set_break (uint32_t addr);
int process_terminated (pid_t pid);

__END_DECLS

#endif
