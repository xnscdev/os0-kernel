/*************************************************************************
 * boot.S -- This file is part of OS/0.                                  *
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
#include <sys/multiboot.h>
#include <sys/task.h>

	.set ALIGN, 1 << 0
	.set MEMINFO, 1 << 1
	.set FLAGS, ALIGN | MEMINFO
	.set MAGIC, MULTIBOOT_MAGIC
	.set CHECKSUM, -(MAGIC + FLAGS)

	.section .multiboot.data, "a", @progbits
	.align 4
	.long MAGIC
	.long FLAGS
	.long CHECKSUM

	.align PAGE_SIZE
	.type boot_page_tbl0, @object
	.size boot_page_tbl0, PAGE_SIZE * 4
boot_page_tbl0:
	.set paddr, 0
	.rept 1024
	.long paddr | 0x003
	.set paddr, paddr + PAGE_SIZE
	.endr

	.type boot_page_tbl1, @object
	.size boot_page_tbl1, PAGE_SIZE * 4
boot_page_tbl1:
	.set paddr, 1024 * PAGE_SIZE
	.rept 1024
	.long paddr | 0x003
	.set paddr, paddr + PAGE_SIZE
	.endr

	.global boot_page_dir
	.type boot_page_dir, @object
	.size boot_page_dir, PAGE_SIZE * 4
boot_page_dir:
	.long boot_page_tbl0 + 0x003
	.long boot_page_tbl1 + 0x003
	.skip 3064
	.long boot_page_tbl0 + 0x003
	.long boot_page_tbl1 + 0x003
	.skip PAGE_SIZE - 3080

__die_not_multiboot_msg:
	.asciz "Bad multiboot magic number"

	.section .bootstrap_stack, "aw", @nobits
	.align PAGE_SIZE
stack_bottom:
	.skip 16384
stack_top:

	.section .multiboot.text, "ax", @progbits
	.global _start
	.type _start, @function
_start:
	/* Disable interrupts */
	cli

	/* Make sure the kernel was booted with multiboot */
	cmp	$0x2badb002, %eax
	je	1f

	pushl	$__die_not_multiboot_msg
	call	low_abort

1:
	/* Initialize ACPI. This must be done before initializing the paging
	   environment, since we need to read raw low memory and the pointers
	   in ACPI structures may not point to a mapped address once entering
	   paging mode. */
	call	acpi_init

	/* Setup initial paging environment */
	mov	$boot_page_dir, %ecx
	mov	%ecx, %cr3
	mov	%cr0, %ecx
	or	$0x80010000, %ecx
	mov	%ecx, %cr0

	/* Jump to higher half start */
	lea	higher_start, %ecx
	jmp	*%ecx

	.size _start, . - _start

	.section .text
higher_start:
	mov	$stack_top, %esp

	/* Initialize main page tables */
	pushl	$stack_bottom
	call	paging_init

	/* Set new stack pointer */
	mov	$SYSCALL_STACK_ADDR, %esp

	call	gdt_init
	call	idt_init
	call	rtc_init

	sti

	pushl	$stack_bottom
	add	$RELOC_VADDR, %ebx
	push	%ebx
	call	kmain
1:
	hlt
	jmp	1b

	.size higher_start, . - higher_start
