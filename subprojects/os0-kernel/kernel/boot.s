/*************************************************************************
 * boot.s -- This file is part of OS/0.                                  *
 * Copyright (C) 2020 XNSC                                               *
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

	.set ALIGN, 1 << 0
	.set MEMINFO, 1 << 1
	.set FLAGS, ALIGN | MEMINFO
	.set MAGIC, 0x1badb002
	.set CHECKSUM, -(MAGIC + FLAGS)

	.section .multiboot
	.align 4
	.long MAGIC
	.long FLAGS
	.long CHECKSUM

	.section .bss
	.align 16
stack_bottom:
	.skip 16384
stack_top:

	.section .text
	.global _start
	.type _start, @function
_start:
	mov	$stack_top, %esp

	push	%ebx
	call	main

	cli
1:
	hlt
	jmp	1b

	.size _start, . - _start
