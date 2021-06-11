/*************************************************************************
 * signal.c -- This file is part of OS/0.                                *
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

#include <libk/libk.h>

int
sigemptyset (sigset_t *set)
{
  memset (set, 0, sizeof (sigset_t));
  return 0;
}

int
sigfillset (sigset_t *set)
{
  memset (set, 0xff, sizeof (sigset_t));
  return 0;
}

int
sigaddset (sigset_t *set, int sig)
{
  set->sig[sig / CHAR_BIT] |= 1 << (7 - sig % CHAR_BIT);
  return 0;
}

int
sigdelset (sigset_t *set, int sig)
{
  set->sig[sig / CHAR_BIT] &= ~(1 << (7 - sig % CHAR_BIT));
  return 0;
}

int
sigismember (const sigset_t *set, int sig)
{
  return set->sig[sig / CHAR_BIT] & 1 << (7 - sig % CHAR_BIT) ? 1 : 0;
}
