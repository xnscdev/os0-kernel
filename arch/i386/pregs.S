/*************************************************************************
 * pregs.S -- This file is part of OS/0.                                 *
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

	.section .rodata
.fmt0:
	.asciz "\12EAX 0x%x  ECX 0x%x  EDX 0x%x"
.fmt1:
	.asciz "\12EBX 0x%x  ESI 0x%x  EDI 0x%x"

	.section .text
	.align 16
	.global print_registers
	.type print_registers, @function
print_registers:
	push	%ebp
	mov	%esp, %ebp

	push	%edx
	push	%ecx
	push	%eax
	push	$.fmt0
	call	printk
	add	$16, %esp

	push	%edi
	push	%esi
	push	%ebx
	push	$.fmt1
	call	printk
	add	$16, %esp

	pop	%ebp
	ret

	.size print_registers, . - print_registers
