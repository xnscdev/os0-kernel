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
boot_page_tbl0:
	.set paddr, 0
	.rept 1024
	.long paddr | 0x003
	.set paddr, paddr + PAGE_SIZE
	.endr
boot_page_tbl1:
	.set paddr, 1024 * PAGE_SIZE
	.rept 1024
	.long paddr | 0x003
	.set paddr, paddr + PAGE_SIZE
	.endr

boot_page_dir:
	.long boot_page_tbl0 + 0x003
	.long boot_page_tbl1 + 0x003
	.skip 3064
	.long boot_page_tbl0 + 0x003
	.long boot_page_tbl1 + 0x003
	.skip PAGE_SIZE - 3080

	.section .bootstrap_stack, "aw", @nobits
stack_bottom:
	.skip 16384
stack_top:

	.section .multiboot.text, "ax", @progbits
	.global _start
	.type _start, @function
_start:
	cli
	mov	$boot_page_dir, %ecx
	mov	%ecx, %cr3

	mov	%cr0, %ecx
	or	$0x80010000, %ecx
	mov	%ecx, %cr0

	lea	higher_start, %ecx
	jmp	*%ecx

	.section .text
higher_start:
	movl	$0, boot_page_dir
	movl	$0, boot_page_dir+4
	mov	%cr3, %ecx
	mov	%ecx, %cr3

	mov	$stack_top, %esp

	call	paging_init
/*
	call	gdt_init
	call	idt_init

	sti
*/

	add	$RELOC_VADDR, %ebx
	push	%ebx
	call	kmain
1:
	hlt
	jmp	1b
