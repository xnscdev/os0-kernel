/*************************************************************************
 * task.h -- This file is part of OS/0.                                  *
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

#ifndef _SYS_TASK_H
#define _SYS_TASK_H

#include <sys/cdefs.h>
#include <sys/types.h>

#define TASK_SLICE 50
#define TASK_STACK_SIZE 16384

typedef struct _ProcessTask
{
  pid_t t_pid;
  uint32_t t_stack;
  uint32_t t_esp;
  uint32_t t_ebp;
  uint32_t t_eip;
  uint32_t *t_pgdir;
  struct _ProcessTask *t_prev;
  struct _ProcessTask *t_next;
} ProcessTask;

__BEGIN_DECLS

extern uint32_t task_stack_addr;

void scheduler_init (void);
void task_tick (void);
void task_load (uint32_t eip, uint32_t esp, uint32_t ebp, uint32_t *page_dir);
int task_fork (void);
ProcessTask *task_new (uint32_t eip, uint32_t *page_dir);
void task_free (ProcessTask *task);
void task_relocate_stack (void *addr, uint32_t size);
pid_t task_getpid (void);

__END_DECLS

#endif
