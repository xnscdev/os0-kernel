/*************************************************************************
 * stack.h -- This file is part of OS/0.                                 *
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

#ifndef _LIBK_STACK_H
#define _LIBK_STACK_H

#include <libk/types.h>
#include <sys/cdefs.h>
#include <stddef.h>

typedef struct
{
  void **s_base;
  void **s_ptr;
  size_t s_size;
  size_t s_max;
} Stack;

__BEGIN_DECLS

Stack *stack_new (uint32_t max);
Stack *stack_place (void *addr, uint32_t max);
int stack_push (Stack *stack, void *elem);
void *stack_pop (Stack *stack);
void stack_destroy (Stack *stack, ReleasePredicate func, void *data);

__END_DECLS

#endif
