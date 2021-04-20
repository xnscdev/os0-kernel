/*************************************************************************
 * user.S -- This file is part of OS/0.                                  *
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

#define _ASM

#include <sys/memory.h>

	.section .text
	.align 16
	.global user_mode_exec
	.type user_mode_exec, @function
user_mode_exec:
	cli
	mov	$0x23, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%esp, %eax
	pushl	$0x23
	push	%eax
	pushf
	pop	%eax
	or	$0x200, %eax
	push	%eax
	pushl	$0x1b
	pushl	4(%esp)
	iret

	.size user_mode_exec, . - user_mode_exec
