/*************************************************************************
 * tlb-flush.S -- This file is part of OS/0.                             *
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

#define _ASM

	.section .text
	.align 16
	.global flush_tlb_page
	.type flush_tlb_page, @function
flush_tlb_page:
	push	%ebp
	mov	%esp, %ebp
	mov	4(%esp), %eax
	/* TODO invlpg is only supported on i486 and up */
	invlpg	(%eax)
	pop	%ebp
	ret
