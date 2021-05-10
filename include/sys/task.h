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

#ifndef _ASM
#include <sys/cdefs.h>
#include <sys/types.h>
#endif

#define TASK_STACK_ADDR    0xff404000
#define TASK_STACK_SIZE    16384
#define TASK_STACK_BOTTOM  (TASK_STACK_ADDR - TASK_STACK_SIZE)
#define TASK_EXIT_PAGE     0xff406000

#ifndef _ASM

typedef struct _ProcessTask
{
  pid_t t_pid;
  uint32_t t_stack;
  uint32_t t_esp;
  uint32_t t_eip;
  uint32_t *t_pgdir;
  volatile struct _ProcessTask *t_prev;
  volatile struct _ProcessTask *t_next;
} ProcessTask;

__BEGIN_DECLS

extern uint32_t task_stack_addr;

void scheduler_init (void);
void task_tick (void);
int task_fork (void);
void task_yield (void);
int task_new (uint32_t eip);
void task_exec (uint32_t eip, char *const *argv, char *const *envp)
  __attribute__ ((noreturn));
void task_free (ProcessTask *task);
void task_relocate_stack (void *addr, uint32_t size);
pid_t task_getpid (void);

__END_DECLS

#endif

#endif
