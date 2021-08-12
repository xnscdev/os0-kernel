/*************************************************************************
 * timer.h -- This file is part of OS/0.                                 *
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

#ifndef _I386_TIMER_H
#define _I386_TIMER_H

#include <sys/cdefs.h>
#include <stdint.h>

#define TIMER_FREQ 1193182

#define TIMER_PORT_CHANNEL0 0x40
#define TIMER_PORT_CHANNEL1 0x41
#define TIMER_PORT_CHANNEL2 0x42
#define TIMER_PORT_COMMAND  0x43

#define TIMER_CHANNEL_COUNT 3
#define TIMER_CHANNEL(n)    ((n) - TIMER_PORT_CHANNEL0)

#define PCSPK_PORT  0x61
#define PCSPK_FREQ  500
#define PCSPK_DELAY 100

__BEGIN_DECLS

void timer_tick (void);

__END_DECLS

#endif
