/*************************************************************************
 * features.S -- This file is part of OS/0.                              *
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

	.section .text
	.global cpu_enable_sse
	.type cpu_enable_sse, @function
cpu_enable_sse:
	mov	%cr0, %eax
	and	$0xfffb, %ax
	or	$2, %ax
	mov	%eax, %cr0
	mov	%cr4, %eax
	or	$0x600, %eax
	mov	%eax, %cr4
	ret

	.size cpu_enable_sse, . - cpu_enable_sse
