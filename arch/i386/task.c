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

#include <i386/paging.h>
#include <i386/tss.h>
#include <libk/libk.h>
#include <sys/memory.h>
#include <sys/process.h>
#include <vm/heap.h>
#include <vm/paging.h>

void sys_exit_halt (void) __attribute__ ((noreturn));

volatile ProcessTask *task_current;
volatile ProcessTask *task_queue;
uint32_t task_stack_addr;
pid_t next_pid;

void
scheduler_init (void)
{
  __asm__ volatile ("cli");
  task_relocate_stack ((void *) TASK_STACK_ADDR, TASK_STACK_SIZE);

  task_current = kmalloc (sizeof (ProcessTask));
  task_current->t_pid = next_pid++;
  task_current->t_stack = TASK_STACK_ADDR;
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
  for (i = task->t_stack; i >= task->t_stack - TASK_STACK_SIZE; i -= PAGE_SIZE)
    {
      uint32_t paddr = get_paddr (task->t_pgdir, (void *) i);
      if (paddr != 0)
	mem_free ((void *) paddr, PAGE_SIZE);
    }

  page_dir_free (task->t_pgdir);
  kfree (task);
}

void
task_relocate_stack (void *addr, uint32_t size)
{
  uint32_t old_esp;
  uint32_t old_ebp;
  uint32_t new_esp;
  uint32_t new_ebp;
  uint32_t offset;
  uint32_t i;

  for (i = (uint32_t) addr; i >= (uint32_t) addr - size; i -= PAGE_SIZE)
    {
      void *paddr = mem_alloc (PAGE_SIZE, 0);
      map_page (curr_page_dir, (uint32_t) paddr, i,
		PAGE_FLAG_WRITE | PAGE_FLAG_USER);
#ifdef INVLPG_SUPPORT
      vm_page_inval (i);
#endif
    }
#ifndef INVLPG_SUPPORT
  vm_tlb_reset ();
#endif

  __asm__ volatile ("mov %%esp, %0" : "=r" (old_esp));
  __asm__ volatile ("mov %%ebp, %0" : "=r" (old_ebp));
  offset = (uint32_t) addr - task_stack_addr;
  new_esp = old_esp + offset;
  new_ebp = old_ebp + offset;
  memcpy ((void *) new_esp, (void *) old_esp,  task_stack_addr - old_esp);

  for (i = (uint32_t) addr; i >= (uint32_t) addr - size; i -= 4)
    {
      uint32_t value = *((uint32_t *) i);
      if (value > old_esp && value < task_stack_addr)
	{
	  value += offset;
	  *((uint32_t *) i) = value;
	}
    }
  __asm__ volatile ("mov %0, %%esp" :: "r" (new_esp));
  __asm__ volatile ("mov %0, %%ebp" :: "r" (new_ebp));
}

pid_t
task_getpid (void)
{
  return task_current->t_pid;
}

ProcessTask *
_task_fork (void)
{
  volatile ProcessTask *temp;
  ProcessTask *task;
  uint32_t *dir;
  uint32_t paddr;
  uint32_t i;

  dir = page_dir_clone (task_current->t_pgdir);
  if (dir == NULL)
    return NULL;

  /* Allocate and copy the stack */
  paddr = (uint32_t) mem_alloc (TASK_STACK_SIZE, 0);
  if (paddr == 0)
    {
      page_dir_free (dir);
      return NULL;
    }
  for (i = 0; i < TASK_STACK_SIZE; i += PAGE_SIZE)
    {
      map_page (dir, paddr + i, TASK_STACK_BOTTOM + i,
		PAGE_FLAG_WRITE | PAGE_FLAG_USER);
      map_page (curr_page_dir, paddr + i, PAGE_COPY_VADDR + i,
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
      mem_free ((void *) paddr, TASK_STACK_SIZE);
      page_dir_free (dir);
      return NULL;
    }
  task->t_pid = next_pid++;
  task->t_stack = TASK_STACK_ADDR;
  task->t_eip = 0;
  task->t_pgdir = dir;
  task->t_next = NULL;

  for (temp = task_queue; temp->t_next != NULL; temp->t_next++)
    ;
  task->t_prev = temp;
  temp->t_next = task;
  task_queue->t_prev = task;

  process_table[task->t_pid].p_task = task;
  memset (process_table[task->t_pid].p_files, 0,
	  sizeof (ProcessFile) * PROCESS_FILE_LIMIT);
  return task;
}
