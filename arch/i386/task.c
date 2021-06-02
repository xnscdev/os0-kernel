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
#include <vm/heap.h>
#include <vm/paging.h>

void sys_exit_halt (void) __attribute__ ((noreturn));

volatile ProcessTask *task_current;
volatile ProcessTask *task_queue;

void
scheduler_init (void)
{
  __asm__ volatile ("cli");
  task_current = kmalloc (sizeof (ProcessTask));
  assert (task_current != NULL);
  task_current->t_pid = 0;
  task_current->t_ppid = 0;
  task_current->t_esp = 0;
  task_current->t_eip = 0;
  task_current->t_pgdir = curr_page_dir;
  task_current->t_prev = task_current;
  task_current->t_next = NULL;

  task_queue = task_current;
  process_table[0].p_task = task_current;
  __asm__ volatile ("sti");
}

void
task_timer_tick (void)
{
  uint32_t esp;
  struct timeval *tp;
  if (task_current == NULL)
    return;
  __asm__ volatile ("mov %%esp, %0" : "=r" (esp));
  if (esp > SYSCALL_STACK_ADDR)
    tp = &process_table[task_getpid ()].p_rusage.ru_utime;
  else
    tp = &process_table[task_getpid ()].p_rusage.ru_stime;
  tp->tv_usec += 1000000;
  if (tp->tv_usec >= 1000000)
    {
      tp->tv_sec++;
      tp->tv_usec -= 1000000;
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

ProcessTask *
_task_fork (void)
{
  volatile ProcessTask *temp;
  ProcessTask *task;
  Process *proc;
  Process *parent;
  uint32_t *dir;
  uint32_t paddr;
  uint32_t i;
  pid_t pid;

  for (pid = 1; pid < PROCESS_LIMIT; pid++)
    {
      if (process_table[pid].p_task == NULL)
	goto found;
    }
  return NULL;

 found:
  dir = page_dir_clone (task_current->t_pgdir);
  if (dir == NULL)
    return NULL;

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
#ifdef INVLPG_SUPPORT
      vm_page_inval (paddr + i);
#endif
    }
#ifndef INVLPG_SUPPORT
  vm_tlb_reset ();
#endif
  memcpy ((void *) PAGE_COPY_VADDR, (void *) TASK_STACK_BOTTOM,
	  TASK_STACK_SIZE);

  /* This is done so user mode processes don't page fault when calling 
     sys_exit() because it is mapped to a supervisor page */
  map_page (dir, get_paddr (curr_page_dir, sys_exit_halt), TASK_EXIT_PAGE,
	    PAGE_FLAG_USER);

  task = kmalloc (sizeof (ProcessTask));
  if (task == NULL)
    {
      for (i = 0; i < TASK_STACK_SIZE; i += PAGE_SIZE)
	{
	  uint32_t paddr =
	    get_paddr (curr_page_dir, (void *) (PAGE_COPY_VADDR + i));
	  if (paddr != 0)
	    free_page (paddr);
	}
      page_dir_free (dir);
      return NULL;
    }
  task->t_pid = pid;
  task->t_ppid = task_getpid ();
  task->t_eip = 0;
  task->t_pgdir = dir;
  task->t_next = NULL;

  for (temp = task_queue; temp->t_next != NULL; temp = temp->t_next)
    ;
  task->t_prev = temp;
  temp->t_next = task;
  task_queue->t_prev = task;

  proc = &process_table[pid];
  parent = &process_table[task_getpid ()];
  proc->p_task = task;
  memset (proc->p_files, 0, sizeof (ProcessFile) * PROCESS_FILE_LIMIT);

  /* Inherit parent working directory, real/effective UID/GID */
  proc->p_cwd = parent->p_cwd;
  vfs_ref_inode (proc->p_cwd);
  proc->p_uid = parent->p_uid;
  proc->p_euid = parent->p_euid;
  proc->p_gid = parent->p_gid;
  proc->p_egid = parent->p_egid;
  return task;
}
