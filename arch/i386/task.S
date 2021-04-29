/*************************************************************************
 * task.S -- This file is part of OS/0.                                  *
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

#include <i386/paging.h>
#include <i386/pic.h>
#include <sys/cmos.h>
#include <sys/memory.h>
#include <sys/task.h>
#include <errno.h>

	.section .text
	.align 16
	.global irq8
	.type irq8, @function
irq8:
	pusha

	/* Read RTC C status to allow next interrupt */
	mov	$CMOS_RTC_CSTAT, %al
	outb	%al, $CMOS_PORT_REGISTER
	inb	$CMOS_PORT_DATA, %al

	/* Acknowledge PIC */
	mov	$PIC_EOI, %al
	outb	%al, $PIC_SLAVE_COMMAND
	outb	%al, $PIC_MASTER_COMMAND

	mov	task_current, %edi

	/* Exit if scheduler is uninitialized */
	test	%edi, %edi
	jz	.finish

	/* Save task ESP and EIP */
	mov	%esp, 8(%edi)
	call	read_ip
.entry:
	test	%eax, %eax
	jz	.finish /* If EAX is zero we have just switched tasks */
	mov	%eax, 12(%edi)

	/* Switch to next task */
	mov	24(%edi), %edi
	test	%edi, %edi
	jnz	1f
	mov	task_queue, %edi

1:
	mov	%edi, task_current
	mov	8(%edi), %ebx
	mov	16(%edi), %esi

	/* Update TSS stack */
	push	%ebx
	call	tss_update_stack
	add	$4, %esp

	/* Switch page directory */
	push	%esi
	push	curr_page_dir
	call	get_paddr
	add	$8, %esp
	mov	%esi, curr_page_dir
	mov	%cr3, %ecx
	cmp	%ecx, %eax
	je	2f
	mov	%eax, %cr3

2:
	/* Switch stack and jump to new task EIP */
	mov	%ebx, %esp
	mov	task_current, %edi
	mov	12(%edi), %ecx
	xor	%eax, %eax /* Zero EAX to exit on new task switch */
	jmp	*%ecx

.finish:
	/* TODO Free all processes in exit queue */
	popa
	iret

	.size irq8, . - irq8

	.global task_fork
	.type task_fork, @function
task_fork:
	push	%ebp
	mov	%esp, %ebp
	sub	$12, %esp

	/* -4(%ebp): parent task
	   -8(%ebp): child task
	   -12(%ebp): saved EFLAGS */

	mov	task_current, %eax
	mov	%eax, -4(%ebp)

	/* Create new task */
	call	_task_fork
	test	%eax, %eax
	jnz	2f

	mov	$-ENOMEM, %eax
1:
	add	$12, %esp
	pop	%ebp
	ret

2:
	mov	%eax, -8(%ebp)

	/* Store EFLAGS */
	pushf
	pop	%eax
	mov	%eax, -12(%ebp)

	/* Move stack pointer to new task stack */
	mov	%esp, %ecx
	sub	$(TASK_STACK_BOTTOM - PAGE_COPY_VADDR), %esp

	/* Setup fake interrupt stack frame */
	pushl	-12(%ebp)
	mov	%cs, %ax
	cwtl
	push	%eax
	pushl	$.child
	pusha

	/* Set stack and instruction pointer of new task */
	mov	%ecx, %esp
	sub	$44, %ecx
	mov	-8(%ebp), %eax
	mov	%ecx, 8(%eax)
	mov	$.entry, %ecx
	mov	%ecx, 12(%eax)

	/* Return PID of new task as parent */
	mov	(%eax), %eax
	and	$0xffff, %eax
	jmp	1b

.child:
	/* Child task returns 0 from fork */
	xor	%eax, %eax
	jmp	1b

	.size task_fork, . - task_fork

	.global _task_stack_setup
	.type _task_stack_setup, @function
_task_stack_setup:
	/* Move stack pointer to new task stack */
	push	%ebp
	mov	%esp, %ebp
	mov	$(PAGE_COPY_VADDR + TASK_STACK_SIZE), %esp

	/* Setup fake interrupt stack frame */
	pushf
	mov	%cs, %ax
	cwtl
	push	%eax
	pushl	12(%ebp)

	/* Update stack pointer */
	mov	8(%ebp), %edx
	mov	8(%edx), %eax
	sub	$44, %eax
	mov	%eax, 8(%edx)
	mov	$.entry, %eax
	mov	%eax, 12(%edx)

	leave
	ret

	.size _task_stack_setup, . - _task_stack_setup
