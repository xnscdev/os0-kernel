/*************************************************************************
 * memcmp.S -- This file is part of OS/0.                                *
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
	.global memcmp
	.type memcmp, @function
memcmp:
	push	%esi
	mov	%edi, %edx
	mov	8(%esp), %esi
	mov	12(%esp), %edi
	mov	16(%esp), %ecx
	cld
	xor	%eax, %eax
	repe
	cmpsb
	jz	1f

	sbb	%eax, %eax
	or	$1, %al

1:
	pop	%esi
	mov	%edx, %edi
	ret

	.size memcmp, . - memcmp
