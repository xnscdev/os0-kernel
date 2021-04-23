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

#include <i386/pic.h>
#include <sys/cmos.h>

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
	cli
	mov	$PIC_EOI, %al
	outb	%al, $PIC_SLAVE_COMMAND
	outb	%al, $PIC_MASTER_COMMAND

	mov	task_current, %edi

	/* Exit if scheduler is uninitialized */
	test	%edi, %edi
	jz	3f

	/* Save task ESP and EIP */
	mov	%esp, 8(%edi)
	call	read_ip
	test	%eax, %eax
	jz	3f /* If EAX is zero we have just switched tasks */
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
	/* Switch stack */
	mov	%ebx, %esp
	/* XXX EDI might be an invalid address in the new page table */
	mov	12(%edi), %ecx
	xor	%eax, %eax /* Zero EAX to exit on new task switch */
	jmp	*%ecx

3:
	sti
	popa
	iret

	.size irq8, . - irq8
