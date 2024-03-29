/*************************************************************************
 * pcspk.c -- This file is part of OS/0.                                 *
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

#include <sys/io.h>
#include <sys/timer.h>

void
speaker_init (void)
{
  timer_set_freq (TIMER_PORT_CHANNEL2, PCSPK_FREQ);
}

void
speaker_beep (void)
{
  unsigned char c = inb (PCSPK_PORT);
  if (c != (c | 3))
    outb (c | 3, PCSPK_PORT);
  msleep (PCSPK_DELAY);
  outb (inb (PCSPK_PORT) & 0xfc, PCSPK_PORT);
}
