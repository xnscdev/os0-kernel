/*************************************************************************
 * random.S -- This file is part of OS/0.                               *
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

#define RANDOM_ATTEMPTS 100

	.section .text
	.align 16
	.global cpu_random
	.type cpu_random, @function
cpu_random:
	mov	$RANDOM_ATTEMPTS, %ecx
1:
	rdrand	%eax
	jc	2f
	loop	1b

	mov	$-1, %eax
	ret

2:
	mov	%eax, 4(%esp)
	xor	%eax, %eax
	ret

	.size cpu_random, . - cpu_random
