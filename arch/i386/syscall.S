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

#include <bits/signal.h>
#include <sys/errno.h>
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
	test	%ecx, %ecx
	jnz	1f

	/* No corresponding system call function */
	call	task_getpid
	pushl	$SIGSYS
	push	%eax
	call	process_send_signal
	add	$8, %esp
	mov	$-ENOSYS, %eax
	jmp	2f

1:
	call	*%ecx

	cmp	$1, %ebx
	jne	2f

	/* exit(2) was called, first call the finalizer functions of all
	   loaded shared objects, if there were any. Finally, change the
	   return address to point to a busy-wait loop until the next
	   task switch occurs */
	mov	task_current, %ecx
	mov	16(%ecx), %edx
	test	%edx, %edx
	jz	2f
	call	*%edx

	movl	$TASK_EXIT_PAGE, 24(%esp)

2:
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
	/* This does the same thing as `sti' (sets IF) without causing
	   a general protection fault in user mode */
	pushf
	orl	$0x200, (%esp)
	popf
1:
	jmp	1b /* Loop until next task switch */

	.size sys_exit_halt, . - sys_exit_halt
