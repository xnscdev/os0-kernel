/*************************************************************************
 * isr.S -- This file is part of OS/0.                                   *
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

#define EXC_POP(x) EXC_POP_ ## x
#define EXC_POP_0
#define EXC_POP_1 add $4, %esp

	.section .text
	.align 16
#define EXC(x, p)			\
	.global exc ## x;		\
	.type exc ## x, @function;	\
exc ## x:				\
	call	exc ## x ## _handler;	\
	EXC_POP(p);			\
	iret;				\
	.size exc ## x, . - exc ## x
#define IRQ_SKIP_8
#define IRQ(x)				\
	.global irq ## x;		\
	.type irq ## x, @function;	\
irq ## x:				\
	pusha;				\
	call	irq ## x ## _handler;	\
	popa;				\
	iret;				\
	.size irq ## x, . - irq ## x
#include "irq.inc"
#undef EXC
#undef IRQ
#undef IRQ_SKIP_8

	.global irq8
	.type irq8, @function
irq8:
	pusha
	call	irq8_handler
	popa
	iret

	.size irq8, . - irq8
