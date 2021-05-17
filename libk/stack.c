/*************************************************************************
 * stack.c -- This file is part of OS/0.                                 *
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

#include <libk/libk.h>
#include <vm/heap.h>

Stack *
stack_new (uint32_t max)
{
  Stack *stack = kmalloc (sizeof (Stack));
  if (unlikely (stack == NULL))
    return NULL;
  stack->s_base = kmalloc (sizeof (void *) * max);
  if (unlikely (stack->s_base == NULL))
    {
      kfree (stack);
      return NULL;
    }
  stack->s_ptr = stack->s_base;
  stack->s_size = 0;
  stack->s_max = max;
  return stack;
}

Stack *
stack_place (void *addr, uint32_t max)
{
  Stack *stack = kmalloc (sizeof (Stack));
  if (unlikely (stack == NULL))
    return NULL;
  stack->s_base = addr;
  stack->s_ptr = addr;
  stack->s_size = 0;
  stack->s_max = max;
  return stack;
}

int
stack_push (Stack *stack, void *elem)
{
  if (stack == NULL || stack->s_size == stack->s_max)
    return -1;
  *stack->s_ptr++ = elem;
  stack->s_size++;
  return 0;
}

void *
stack_pop (Stack *stack)
{
  if (stack == NULL || stack->s_size == 0)
    return NULL;
  stack->s_size--;
  return *--stack->s_ptr;
}

void
stack_destroy (Stack *stack, ReleasePredicate func, void *data)
{
  void *elem;
  if (stack == NULL)
    return;
  if (func != NULL)
    {
      while (1)
	{
	  elem = stack_pop (stack);
	  if (elem == NULL)
	    break;
	  func (elem, data);
	}
    }
  kfree (stack->s_base);
  kfree (stack);
}
