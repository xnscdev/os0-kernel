/*************************************************************************
 * task.c -- This file is part of OS/0.                                  *
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

#include <kconfig.h>

#include <fs/vfs.h>
#include <i386/tss.h>
#include <libk/libk.h>
#include <sys/process.h>
#include <sys/rtld.h>
#include <sys/syscall.h>
#include <vm/heap.h>
#include <vm/paging.h>

void sys_exit_halt (void) __attribute__ ((noreturn));
void signal_trampoline (void) __attribute__ ((aligned (PAGE_SIZE)));

volatile ProcessTask *task_current;
volatile ProcessTask *task_queue;
volatile int task_switch_enabled;

void
scheduler_init (void)
{
  int i;
  __asm__ volatile ("cli");
  task_current = kmalloc (sizeof (ProcessTask));
  assert (task_current != NULL);
  task_current->t_pid = 0;
  task_current->t_ppid = 0;
  task_current->t_esp = 0;
  task_current->t_eip = 0;
  task_current->t_pgdir = curr_page_dir;
  task_current->t_fini = NULL;
  task_current->t_pgcopied = 0;
  task_current->t_priority = PRIO_MIN;
  task_current->t_prev = task_current;
  task_current->t_next = NULL;

  task_queue = task_current;
  process_table[0].p_task = task_current;
  for (i = 0; i < NSIG; i++)
    process_table[0].p_sigactions[i].sa_handler = SIG_DFL;
  process_table[0].p_umask = S_IWGRP | S_IWOTH;
  process_table[0].p_maxbreak = PROCESS_BREAK_LIMIT;
  process_table[0].p_maxfds = PROCESS_FILE_LIMIT;
  process_table[0].p_addrspace.rlim_cur = RLIM_INFINITY;
  process_table[0].p_addrspace.rlim_max = LONG_MAX;
  process_table[0].p_coresize.rlim_cur = 0;
  process_table[0].p_coresize.rlim_max = 0;
  process_table[0].p_cputime.rlim_cur = RLIM_INFINITY;
  process_table[0].p_cputime.rlim_max = LONG_MAX;
  process_table[0].p_filesize.rlim_cur = RLIM_INFINITY;
  process_table[0].p_filesize.rlim_max = LONG_MAX;
  process_table[0].p_memlock.rlim_cur = RLIM_INFINITY;
  process_table[0].p_memlock.rlim_max = LONG_MAX;

  task_switch_enabled = 1;
  __asm__ volatile ("sti");
}

void
task_timer_tick (void)
{
  uint32_t esp;
  struct timeval *tp;
  Process *proc;
  int i;
  if (task_current == NULL)
    return;
  proc = &process_table[task_getpid ()];

  /* Update process execution time */
  __asm__ volatile ("mov %%esp, %0" : "=r" (esp));
  /* If the current stack pointer is in the user mode interrupt stack, the
     process is a user mode process so the user time should be updated,
     otherwise update system time */
  if (esp > SYSCALL_STACK_ADDR)
    tp = &proc->p_rusage.ru_utime;
  else
    tp = &proc->p_rusage.ru_stime;
  tp->tv_usec += 1000; /* Timer tick happens every millisecond */
  if (tp->tv_usec >= 1000000)
    {
      tp->tv_sec++;
      tp->tv_usec -= 1000000;
    }

  /* Update real-time interval timer for all processes */
  for (i = 0; i < PROCESS_LIMIT; i++)
    {
      if (process_table[i].p_task == NULL)
	continue;
      tp = &process_table[i].p_itimers[ITIMER_REAL].it_value;
      if (tp->tv_sec != 0 || tp->tv_usec != 0)
	{
	  if (tp->tv_usec >= 1000)
	    tp->tv_usec -= 1000;
	  else if (tp->tv_sec > 0)
	    {
	      tp->tv_sec--;
	      tp->tv_usec = 999000 - tp->tv_usec;
	    }

	  if (tp->tv_sec == 0 && tp->tv_usec == 0)
	    {
	      /* Reset the timer and send a signal */
	      tp->tv_sec =
		process_table[i].p_itimers[ITIMER_REAL].it_interval.tv_sec;
	      tp->tv_usec =
		process_table[i].p_itimers[ITIMER_REAL].it_interval.tv_usec;
	      process_table[i].p_siginfo.si_code = SI_TIMER;
	      process_send_signal (i, SIGALRM);
	    }
	}
    }
}

void
task_free (ProcessTask *task)
{
  uint32_t i;
  /* Unlink from task queue */
  task->t_prev->t_next = task->t_next;
  if (task->t_next == NULL)
    task_queue->t_prev = task->t_prev;
  else
    task->t_next->t_prev = task->t_prev;

  /* Free task stack */
  for (i = TASK_STACK_BOTTOM; i < TASK_STACK_ADDR; i += PAGE_SIZE)
    {
      uint32_t paddr = get_paddr (task->t_pgdir, (void *) i);
      if (paddr != 0)
	free_page (paddr);
    }

  if (task->t_pgcopied)
    page_dir_free (task->t_pgdir);
  kfree (task);
}

pid_t
task_getpid (void)
{
  return task_current->t_pid;
}

pid_t
task_getppid (void)
{
  return task_current->t_ppid;
}

int
task_setup_exec (char *const *argv, char *const *envp)
{
  void *data = (void *) EXEC_DATA_VADDR + sizeof (void *) * 2;
  char *ptr;
  size_t len = 0;
  size_t narg;
  size_t nenv;
  size_t i;

  for (narg = 0; argv[narg] != NULL; narg++)
    len += strlen (argv[narg]) + 1;
  for (nenv = 0; envp[nenv] != NULL; nenv++)
    len += strlen (envp[nenv]) + 1;

  if (len + sizeof (void *) * (narg + nenv + 4) >= ARG_MAX)
    return -E2BIG;

  /* Copy argv data and pointers */
  ((void **) EXEC_DATA_VADDR)[0] = (void **) EXEC_DATA_VADDR + 2;
  ptr = data + sizeof (void *) * (narg + 1);
  for (i = 0; i < narg; i++)
    {
      ((char **) data)[i] = ptr;
      ptr = stpcpy (ptr, argv[i]) + 1;
    }
  ((char **) data)[i] = NULL;

  /* Copy envp data and pointers */
  data = (void *) (((uintptr_t) ptr - 1) | (PAGE_SIZE - 1)) + 1;
  ((void **) EXEC_DATA_VADDR)[1] = data;
  ptr = data + sizeof (void *) * (nenv + 1);
  for (i = 0; i < nenv; i++)
    {
      ((char **) data)[i] = ptr;
      ptr = stpcpy (ptr, envp[i]) + 1;
    }
  ((char **) data)[i] = NULL;
  return 0;
}

ProcessTask *
_task_fork (int copy_pgdir)
{
  volatile ProcessTask *temp;
  ProcessTask *task;
  SortedArray *mregions;
  Process *proc;
  Process *parent;
  char *cwdpath;
  uint32_t *dir;
  uint32_t paddr;
  uint32_t i;
  pid_t tpid;
  pid_t pid;

  for (pid = 1; pid < PROCESS_LIMIT; pid++)
    {
      if (process_table[pid].p_task == NULL)
	goto found;
    }
  return NULL;

 found:
  if (copy_pgdir)
    {
      dir = page_dir_clone (task_current->t_pgdir);
      if (dir == NULL)
	return NULL;
    }
  else
    dir = task_current->t_pgdir;

  /* Allocate and copy the stack */
  for (i = 0; i < TASK_STACK_SIZE; i += PAGE_SIZE)
    {
      paddr = alloc_page ();
      if (unlikely (paddr == 0))
	{
	  int j;
	  for (j = 0; j < i; j += PAGE_SIZE)
	    {
	      paddr = get_paddr (curr_page_dir, (void *) (PAGE_COPY_VADDR + i));
	      if (paddr != 0)
		free_page (paddr);
	    }
	  page_dir_free (dir);
	  return NULL;
	}
      map_page (dir, paddr, TASK_STACK_BOTTOM + i,
		PAGE_FLAG_WRITE | PAGE_FLAG_USER);
      map_page (curr_page_dir, paddr, PAGE_COPY_VADDR + i,
		PAGE_FLAG_WRITE | PAGE_FLAG_USER);
      vm_page_inval (PAGE_COPY_VADDR + i);
    }
  vm_tlb_reset_386 ();
  memcpy ((void *) PAGE_COPY_VADDR, (void *) TASK_STACK_BOTTOM,
	  TASK_STACK_SIZE);

  /* Map the exit busy-wait page as user mode so process can exit */
  map_page (dir, get_paddr (curr_page_dir, sys_exit_halt), TASK_EXIT_PAGE,
	    PAGE_FLAG_USER);

  /* Allow user mode code to execute the functions in this page */
  map_page (dir, get_paddr (curr_page_dir, signal_trampoline),
	    (uint32_t) signal_trampoline, PAGE_FLAG_USER);

  task = kmalloc (sizeof (ProcessTask));
  if (task == NULL)
    goto err;
  task->t_pid = pid;
  task->t_ppid = task_getpid ();
  task->t_eip = 0;
  task->t_pgdir = dir;
  task->t_fini = NULL;
  task->t_pgcopied = copy_pgdir;
  task->t_priority = 0;
  task->t_next = NULL;

  proc = &process_table[pid];
  parent = &process_table[task_getpid ()];

  /* Copy memory region array */
  mregions = sorted_array_new (PROCESS_MMAP_LIMIT, process_mregion_cmp);
  if (parent->p_mregions != NULL)
    {
      for (i = 0; i < parent->p_mregions->sa_size; i++)
	{
	  ProcessMemoryRegion *region = kmalloc (sizeof (ProcessMemoryRegion));
	  ProcessMemoryRegion *pmem;
	  if (unlikely (region == NULL))
	    goto err;
	  pmem = parent->p_mregions->sa_elems[i];
	  region->pm_base = pmem->pm_base;
	  region->pm_len = pmem->pm_len;
	  region->pm_prot = pmem->pm_prot;
	  region->pm_flags = pmem->pm_flags;
	  region->pm_ino = pmem->pm_ino;
	  vfs_ref_inode (region->pm_ino);
	  sorted_array_insert (mregions, region);
	}
    }

  cwdpath = strdup (parent->p_cwdpath);
  if (unlikely (cwdpath == NULL))
    {
      kfree (task);
      goto err;
    }

  /* Copy file descriptors */
  for (i = 0; i < PROCESS_FILE_LIMIT; i++)
    {
      proc->p_files[i] = parent->p_files[i];
      if (proc->p_files[i] != NULL)
	proc->p_files[i]->pf_refcnt++;
    }

  /* Add one to child count of each parent process */
  tpid = parent->p_task->t_pid;
  while (tpid != 0)
    {
      process_table[tpid].p_children++;
      tpid = process_table[tpid].p_task->t_ppid;
    }
  process_table[0].p_children++;

  /* Inherit parent working directory, real/effective/saved UID/GID,
     process group ID, session ID, resource limits, and blocked signal mask */
  proc->p_cwd = parent->p_cwd;
  proc->p_cwdpath = cwdpath;
  vfs_ref_inode (proc->p_cwd);
  proc->p_uid = parent->p_uid;
  proc->p_euid = parent->p_euid;
  proc->p_suid = parent->p_suid;
  proc->p_gid = parent->p_gid;
  proc->p_egid = parent->p_egid;
  proc->p_sgid = parent->p_sgid;
  proc->p_pgid = parent->p_pgid;
  proc->p_sid = parent->p_sid;
  proc->p_maxbreak = parent->p_maxbreak;
  proc->p_maxfds = parent->p_maxfds;
  memcpy (&proc->p_addrspace, &parent->p_addrspace, sizeof (struct rlimit));
  memcpy (&proc->p_coresize, &parent->p_coresize, sizeof (struct rlimit));
  memcpy (&proc->p_cputime, &parent->p_cputime, sizeof (struct rlimit));
  memcpy (&proc->p_filesize, &parent->p_filesize, sizeof (struct rlimit));
  memcpy (&proc->p_memlock, &parent->p_memlock, sizeof (struct rlimit));
  memcpy (&proc->p_sigblocked, &parent->p_sigblocked, sizeof (sigset_t));
  memcpy (&proc->p_sigpending, &parent->p_sigpending, sizeof (sigset_t));

  for (temp = task_queue; temp->t_next != NULL; temp = temp->t_next)
    ;
  task->t_prev = temp;
  temp->t_next = task;
  task_queue->t_prev = task;
  proc->p_mregions = mregions;
  proc->p_task = task;
  return task;

 err:
  for (i = 0; i < TASK_STACK_SIZE; i += PAGE_SIZE)
    {
      uint32_t paddr =
	get_paddr (curr_page_dir, (void *) (PAGE_COPY_VADDR + i));
      if (paddr != 0)
	free_page (paddr);
    }
  if (copy_pgdir)
    page_dir_free (dir);
  sorted_array_destroy (mregions, process_region_free, dir);
  return NULL;
}

void
_task_set_fini_funcs (void (*func) (void))
{
  task_current->t_fini = func;
}
