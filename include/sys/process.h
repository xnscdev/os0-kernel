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

#define PROCESS_BREAK_LIMIT    0x40000000

/* XXX Process file descriptors are shared on fork */

typedef struct
{
  VFSInode *pf_inode;     /* File descriptor inode */
  char *pf_path;          /* Path used to open fd */
  int pf_mode;            /* Access mode */
  off64_t pf_offset;      /* Current offset position */
  int pf_refcnt;          /* Reference count */
} ProcessFile;

typedef struct
{
  uint32_t pm_base; /* Base address */
  uint32_t pm_len;  /* Length of memory region */
  int pm_prot;      /* Memory protection options */
  int pm_flags;     /* Flags set by mmap(2) */
  VFSInode *pm_ino; /* Inode used for mapping */
  off_t pm_offset;  /* Offset into inode */
} ProcessMemoryRegion;

typedef struct
{
  ProcessFile *p_files[PROCESS_FILE_LIMIT];  /* File descriptors */
  int p_fdflags[PROCESS_FILE_LIMIT];         /* File descriptor flags */
  struct sigaction p_sigactions[NSIG];       /* Signal handler table */
  sigset_t p_sigblocked;                     /* Signal block mask */
  sigset_t p_sigpending;                     /* Signal pending mask */
  volatile ProcessTask *p_task;              /* Scheduler task */
  SortedArray *p_mregions;                   /* List of mmap'd regions */
  VFSInode *p_cwd;                           /* Working directory */
  char *p_cwdpath;                           /* Path to working directory */
  uint32_t p_break;                          /* Location of program break */
  uint32_t p_initbreak;                      /* Starting address of break */
  uint32_t p_maxbreak;                       /* Maximum address of break */
  uint32_t p_maxfds;                         /* Maximum file descriptors */
  struct rlimit p_addrspace;                 /* Limits of address space */
  struct rlimit p_coresize;                  /* Limits on core dump size */
  struct rlimit p_cputime;                   /* Limits on CPU time */
  struct rlimit p_filesize;                  /* Limits on file sizes */
  struct rlimit p_memlock;                   /* Limits on locked memory */
  struct rusage p_rusage;                    /* Resource usage */
  struct rusage p_cusage;                    /* Resourge usage of child
						processes */
  int p_pause;                               /* If process is paused */
  int p_sig;                                 /* Signal to be handled */
  siginfo_t p_siginfo;                       /* Signal info */
  size_t p_children;                         /* Number of child processes */
  int p_term;                                /* If process is terminated */
  int p_waitstat;                            /* Value to set wait status to */
  mode_t p_umask;                            /* File creation mask */
  uid_t p_uid;                               /* Real user id */
  uid_t p_euid;                              /* Effective user id */
  uid_t p_suid;                              /* Saved set-user-id */
  gid_t p_gid;                               /* Real group id */
  gid_t p_egid;                              /* Effective group id */
  gid_t p_sgid;                              /* Saved set-group-id */
  pid_t p_pgid;                              /* Process group id */
  pid_t p_sid;                               /* Session id */
  struct itimerval p_itimers[__NR_itimers];  /* Interval timers */
} Process;

__BEGIN_DECLS

extern Process process_table[PROCESS_LIMIT];
extern ProcessFile process_fd_table[PROCESS_SYS_FILE_LIMIT];

int process_exec (VFSInode *inode, uint32_t *entry, char *const *argv,
		  char *const *envp, DynamicLinkInfo *dlinfo);
int process_exec_sh (VFSInode *inode, uint32_t *entry, char *const *argv,
		     char *const *envp, DynamicLinkInfo *dlinfo);
int process_valid (pid_t pid);
void process_clear (pid_t pid, int partial);
void process_free (pid_t pid);
void process_region_free (void *elem, void *data);
int process_setup_std_streams (pid_t pid);
uint32_t process_set_break (uint32_t addr);
int process_mregion_cmp (const void *a, const void *b);
void process_add_rusage (struct rusage *usage, const Process *proc);
void process_remap_segments (void *base, SortedArray *mregions);
int process_alloc_fd (Process *proc, int fd);
int process_free_fd (Process *proc, int fd);
int process_terminated (pid_t pid);
int process_send_signal (pid_t pid, int sig);
void process_clear_sighandlers (pid_t pid);
void process_handle_signal (void);

__END_DECLS

#endif
