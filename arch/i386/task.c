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
#include <sys/memory.h>
#include <sys/task.h>
#include <vm/heap.h>
#include <vm/paging.h>
#include <string.h>

uint32_t read_ip (void);

volatile ProcessTask *task_current;
volatile ProcessTask *task_queue;
uint32_t task_stack_addr;

static pid_t next_pid;

void
scheduler_init (void)
{
  __asm__ volatile ("cli");
  task_relocate_stack ((void *) STACK_VADDR, STACK_LEN);

  task_current = kmalloc (sizeof (ProcessTask));
  task_current->t_pid = next_pid++;
  task_current->t_esp = 0;
  task_current->t_ebp = 0;
  task_current->t_eip = 0;
  task_current->t_pgdir = curr_page_dir;
  task_current->t_next = NULL;

  task_queue = task_current;
  __asm__ volatile ("sti");
}

void
task_tick (void)
{
  uint32_t esp;
  uint32_t ebp;
  uint32_t eip;

  if (task_current == NULL)
    return;

  __asm__ volatile ("mov %%esp, %0" : "=r" (esp));
  __asm__ volatile ("mov %%ebp, %0" : "=r" (ebp));
  eip = read_ip ();
  if (eip == 0)
    return;

  task_current->t_eip = eip;
  task_current->t_esp = esp;
  task_current->t_ebp = ebp;

  task_current = task_current->t_next;
  if (task_current == NULL)
    task_current = task_queue;

  esp = task_current->t_esp;
  ebp = task_current->t_ebp;
  __asm__ volatile ("cli;"
		    "mov %0, %%ecx;"
		    "mov %1, %%esp;"
		    "mov %2, %%ebp;"
		    "mov %3, %%cr3;"
		    "xor %%eax, %%eax;"
		    "sti;"
		    "jmp *%%ecx" :: "r" (eip), "r" (esp), "r" (ebp),
		    "r" (curr_page_dir));
}

int
task_fork (void)
{
  volatile ProcessTask *parent;
  volatile ProcessTask *temp;
  ProcessTask *task;
  uint32_t *dir;
  uint32_t eip;

  __asm__ volatile ("cli");
  parent = task_current;
  dir = page_dir_clone (curr_page_dir);

  task = kmalloc (sizeof (ProcessTask));
  task->t_pid = next_pid++;
  task->t_esp = 0;
  task->t_ebp = 0;
  task->t_eip = 0;
  task->t_pgdir = dir;
  task->t_next = NULL;

  for (temp = task_queue; temp->t_next != NULL; temp->t_next++)
    ;
  temp->t_next = task;

  eip = read_ip ();
  if (task_current == parent)
    {
      uint32_t esp;
      uint32_t ebp;
      __asm__ volatile ("mov %%esp, %0" : "=r" (esp));
      __asm__ volatile ("mov %%ebp, %0" : "=r" (ebp));
      task->t_esp = esp;
      task->t_ebp = ebp;
      task->t_eip = eip;

      __asm__ volatile ("sti");
      return task->t_pid;
    }
  else
    return 0;
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
      map_page ((uint32_t) paddr, i, PAGE_FLAG_WRITE | PAGE_FLAG_USER);
#ifdef INVLPG_SUPPORT
      vm_page_inval ((void *) i);
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
  return 0;
}
