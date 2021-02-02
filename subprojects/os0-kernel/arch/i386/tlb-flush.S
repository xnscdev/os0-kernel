/*************************************************************************
 * tlb-flush.S -- This file is part of OS/0.                             *
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

#include <kconfig.h>

	.section .text
	.align 16
	.global vm_page_inval
	.type vm_page_inval, @function
vm_page_inval:
	push	%ebp
	mov	%esp, %ebp
#ifdef INVLPG_SUPPORT
	/* Avoid the expensive CR3 reload */
	mov	4(%esp), %eax
	invlpg	(%eax)
#else
	mov	%cr3, %eax
	mov	%eax, %cr3
#endif
	pop	%ebp
	ret
