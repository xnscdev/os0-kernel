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

#include <bits/signal.h>
#include <bits/syscall.h>
#include <i386/paging.h>
#include <i386/pic.h>
#include <sys/cmos.h>
#include <sys/memory.h>
#include <sys/task.h>
#include <errno.h>

	.section .data
	.align PAGE_SIZE
	.global init_argv
	.type init_argv, @object
init_argv:
	.long .argv
.argv:
	.long .arg0
	.long 0
.arg0:
	.asciz "/sbin/init"

	.global init_envp
	.type init_envp, @object
init_envp:
	.long .envp
.envp:
	.long .env0
	.long 0
.env0:
	.asciz "TERM=xterm"

	.section .bss
	.type signal_return_addr, @object
	.size signal_return_addr, 16
signal_return_addr:
	.skip 4

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
	jz	.next_start

	/* Save task ESP and EIP */
	mov	%esp, 4(%edi)
	call	read_ip
.entry:
	test	%eax, %eax
	jz	.next_start /* If EAX is zero we have just switched tasks */
	mov	%eax, 8(%edi)

1:
	/* Switch to next task */
	mov	20(%edi), %edi
	test	%edi, %edi
	jnz	2f
	mov	task_queue, %edi

2:
	/* If this process is terminated, skip it */
	movzw	(%edi), %eax
	push	%eax
	call	process_terminated
	add	$4, %esp
	test	%eax, %eax
	jnz	1b

	mov	%edi, task_current
	mov	4(%edi), %ebx
	mov	12(%edi), %esi

	/* Switch page directory */
	push	%esi
	push	curr_page_dir
	call	get_paddr
	add	$8, %esp
	mov	%esi, curr_page_dir
	mov	%cr3, %ecx
	cmp	%ecx, %eax
	je	3f
	mov	%eax, %cr3

3:
	/* Switch stack and jump to new task EIP */
	mov	%ebx, %esp
	mov	task_current, %edi
	mov	8(%edi), %ecx
	xor	%eax, %eax /* Zero EAX to exit on new task switch */
	jmp	*%ecx

.next_start:
	/* Free exited process, if any */
	mov	exit_task, %eax
	test	%eax, %eax
	jz	4f

	push	%eax
	call	process_free
	add	$4, %esp
	movl	$0, exit_task

4:
	call	process_handle_signal
	mov	process_signal, %ecx
	test	%ecx, %ecx
	jz	.done

	/* Signal should be handled, execute signal handler */
	mov	32(%esp), %eax
	mov	%eax, signal_return_addr
	movl	$signal_trampoline, 32(%esp)

.done:
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
	cmp	$TASK_STACK_BOTTOM, %esp
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
	mov	%ecx, 4(%eax)
	movl	$.entry, 8(%eax)

	/* Return PID of new task as parent */
	mov	(%eax), %eax
	and	$0xffff, %eax
	jmp	1b

.child:
	/* Child task returns 0 from fork */
	xor	%eax, %eax
	jmp	1b

	.size task_fork, . - task_fork

	.global task_yield
	.type task_yield, @function
task_yield:
	pushf
	mov	%cs, %ax
	cwtl
	push	%eax
	call	irq8
	ret

	.size task_yield, . - task_yield

	.global task_exec
	.type task_exec, @function
task_exec:
	mov	4(%esp), %ecx
	mov	8(%esp), %esi
	mov	12(%esp), %edi
	mov	16(%esp), %esi
	cli
	pushf
	mov	%cs, %ax
	cwtl
	push	%eax
	push	$1f
	jmp	irq8

1:
	/* Clear signal handlers */
	call	process_clear_sighandlers

	cmp	$SYSCALL_STACK_ADDR, %esp
	jna	2f
	jmp	*%ecx

2:
	/* Switch to user mode and reenable interrupts */
	mov	$0x23, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%esp, %eax
	pushl	$0x23
	push	%eax
	pushf
	orl	$0x200, (%esp)
	pushl	$0x1b
	push	%ecx
	iret

	.align PAGE_SIZE
	.global signal_trampoline
	.type signal_trampoline, @function
signal_trampoline:
	/* TODO Support alternate signal stacks */
	pushl	signal_return_addr
	pusha
	pushf

	pushl	$0
	pushl	$TASK_SIGINFO_PAGE
	pushl	process_signal
	mov	process_signal_handler, %ecx
	call	*%ecx
	add	$12, %esp

	/* Reset handled signal */
	call	process_signal_handled

	popf
	popa
	ret

	.size signal_trampoline, . - signal_trampoline

	.global rtld_setup_dynamic_linker
	.type rtld_setup_dynamic_linker, @function
rtld_setup_dynamic_linker:
	/* ESI is the address of the dynamic link info structure upon entry
	   to this routine */

	/* Call dynamic linker */
	pushl	20(%esi) /* Dynamic structure */
	pushl	4(%esi)  /* Address of program ELF header */
	call	*12(%esi)
	add	$8, %esp

	test	%eax, %eax
	jz	1f

	/* Dynamic linker failed to execute, abort the program */
	pushl	$SIGABRT
	mov	$SYS_getpid, %eax
	int	$0x80
	mov	%eax, %ebx
	mov	$SYS_kill, %eax
	mov	$SIGABRT, %ecx
	int	$0x80

1:
	/* Dynamic linker finished executing, jump to real executable start */
	jmp	*8(%esi)

	.size rtld_setup_dynamic_linker, . - rtld_setup_dynamic_linker
