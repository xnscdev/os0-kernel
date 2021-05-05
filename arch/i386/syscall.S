/*************************************************************************
 * syscall.S -- This file is part of OS/0.                               *
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
#include <sys/task.h>

	.section .text
	.align 16
	.global syscall
	.type syscall, @function
syscall:
	push	%ebp
	push	%edi
	push	%esi
	push	%edx
	push	%ecx
	push	%ebx

	mov	%eax, %ebx
	mov	syscall_table(,%ebx,4), %ecx
	call	*%ecx

	cmp	$1, %ebx
	jne	1f

	/* exit(2) should idle after running */
	movl	$TASK_EXIT_PAGE, 24(%esp)

1:
	pop	%ebx
	pop	%ecx
	pop	%edx
	pop	%esi
	pop	%edi
	pop	%ebp
	iret

	.size syscall, . - syscall

	.align PAGE_SIZE
	.global sys_exit_halt
	.type sys_exit_halt, @function
sys_exit_halt:
	jmp	sys_exit_halt

	.size sys_exit_halt, . - sys_exit_halt
