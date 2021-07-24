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

#ifndef _SYS_TIMER_H
#define _SYS_TIMER_H

#include <sys/cdefs.h>
#include <sys/time.h>
#include <stdint.h>

__BEGIN_DECLS

void timer_set_freq (uint32_t freq);
uint32_t timer_get_freq (void);
uint32_t timer_get_rem_ms (void);

void msleep (uint32_t ms);
long timer_poll (void);

__END_DECLS

#endif
