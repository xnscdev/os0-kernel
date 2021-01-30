/*************************************************************************
 * io.h -- This file is part of OS/0.                                    *
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

#ifndef _SYS_IO_H
#define _SYS_IO_H

#include <sys/cdefs.h>
#include <stdint.h>

static __inline uint8_t
inb (uint16_t port)
{
  uint8_t value;
  __asm__ volatile ("inb %w1, %0" : "=a" (value) : "Nd" (port));
  return value;
}

static __inline uint8_t
inb_p (uint16_t port)
{
  uint8_t value;
  __asm__ volatile ("inb %w1, %0\noutb %%al, $0x80" : "=a" (value) :
		    "Nd" (port));
  return value;
}

static __inline uint16_t
inw (uint16_t port)
{
  uint16_t value;
  __asm__ volatile ("inw %w1, %0" : "=a" (value) : "Nd" (port));
  return value;
}

static __inline uint16_t
inw_p (uint16_t port)
{
  uint16_t value;
  __asm__ volatile ("inw %w1, %0\noutb %%al, $0x80" : "=a" (value) :
		    "Nd" (port));
  return value;
}

static __inline uint32_t
inl (uint16_t port)
{
  uint32_t value;
  __asm__ volatile ("inl %w1, %0" : "=a" (value) : "Nd" (port));
  return value;
}

static __inline uint32_t
inl_p (uint16_t port)
{
  uint32_t value;
  __asm__ volatile ("inl %w1, %0\noutb %%al, $0x80" : "=a" (value) :
		    "Nd" (port));
  return value;
}

static __inline void
outb (uint8_t value, uint16_t port)
{
  __asm__ volatile ("outb %b0, %w1" :: "a" (value), "Nd" (port));
}

static __inline void
outb_p (uint8_t value, uint16_t port)
{
  __asm__ volatile ("outb %b0, %w1\noutb %%al, $0x80" :: "a" (value),
		    "Nd" (port));
}

static __inline void
outw (uint16_t value, uint16_t port)
{
  __asm__ volatile ("outw %w0, %w1" :: "a" (value), "Nd" (port));
}

static __inline void
outw_p (uint16_t value, uint16_t port)
{
  __asm__ volatile ("outw %w0, %w1\noutb %%al, $0x80" :: "a" (value),
		    "Nd" (port));
}

static __inline void
outl (uint32_t value, uint16_t port)
{
  __asm__ volatile ("outl %0, %w1" :: "a" (value), "Nd" (port));
}

static __inline void
outl_p (uint32_t value, uint16_t port)
{
  __asm__ volatile ("outl %0, %w1\noutb %%al, $0x80" :: "a" (value),
		    "Nd" (port));
}

static __inline void
insb (uint16_t port, void *addr, uint32_t count)
{
  __asm__ volatile ("cld; rep; insb" : "=D" (addr), "=c" (count) :
		    "d" (port), "0" (addr), "1" (count));
}

static __inline void
insw (uint16_t port, void *addr, uint32_t count)
{
  __asm__ volatile ("cld; rep; insw" : "=D" (addr), "=c" (count) :
		    "d" (port), "0" (addr), "1" (count));
}

static __inline void
insl (uint16_t port, void *addr, uint32_t count)
{
  __asm__ volatile ("cld; rep; insl" : "=D" (addr), "=c" (count) :
		    "d" (port), "0" (addr), "1" (count));
}

static __inline void
outsb (uint16_t port, void *addr, uint32_t count)
{
  __asm__ volatile ("cld; rep; outsb" : "=S" (addr), "=c" (count) :
		    "d" (port), "0" (addr), "1" (count));
}

static __inline void
outsw (uint16_t port, void *addr, uint32_t count)
{
  __asm__ volatile ("cld; rep; outsw" : "=S" (addr), "=c" (count) :
		    "d" (port), "0" (addr), "1" (count));
}

static __inline void
outsl (uint16_t port, void *addr, uint32_t count)
{
  __asm__ volatile ("cld; rep; outsl" : "=S" (addr), "=c" (count) :
		    "d" (port), "0" (addr), "1" (count));
}

#endif
