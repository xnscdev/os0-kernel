/*************************************************************************
 * compile.h -- This file is part of OS/0.                               *
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

#ifndef _LIBK_COMPILE_H
#define _LIBK_COMPILE_H

#include <sys/memory.h>

/* Macros for marking data in low memory */

#define __low_text __attribute__ ((section (".multiboot.text")))
#define __low_data __attribute__ ((section (".multiboot.data")))
#define __low_rodata __attribute__ ((section (".multiboot.rodata")))

#define LOW_STRING(str) (__extension__ ({		\
	static const __low_rodata char __c[] = (str);	\
	(const char *) &__c;				\
      }))

#define LOW_ACCESS(x) (*((__typeof__ (x) *) ((char *) &(x) + RELOC_VADDR)))

/* Branch prediction macros */

#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)

#endif
