/*************************************************************************
 * paging.S -- This file is part of OS/0.                                *
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

	.section .text
	.align 16
	.global paging_loaddir
	.type paging_loaddir, @function
paging_loaddir:
	push	%ebp
	mov	%esp, %ebp
	mov	8(%esp), %eax
	mov	%eax, %cr3
	pop	%ebp
	ret

	.size paging_loaddir, . - paging_loaddir

	.global paging_enable
	.type paging_enable, @function
paging_enable:
	push	%ebp
	mov	%esp, %ebp
	mov	%cr0, %eax
	or	$0x80000000, %eax
	mov	%eax, %cr0
	pop	%ebp
	ret

	.size paging_enable, . - paging_enable
