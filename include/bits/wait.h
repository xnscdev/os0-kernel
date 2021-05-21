/*************************************************************************
 * wait.h -- This file is part of OS/0.                                  *
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

#ifndef _BITS_WAIT_H
#define _BITS_WAIT_H

#ifndef _SYS_WAIT_H
#error  "<bits/wait.h> should not be included directly"
#endif

#define WNOHANG   1
#define WUNTRACED 2

#define WSTOPPED   2
#define WEXITED    4
#define WCONTINUED 8

#define WNOWAIT 0x01000000

#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)
#define WTERMSIG(status) ((status & 0x7f)
#define WSTOPSIG(status) WEXITSTATUS (status)
#define WIFEXITED(status) (WTERMSIG (status) == 0)
#define WIFSIGNALED(status) (((signed char) (WTERMSIG (status) + 1) >> 1) > 0)
#define WIFSTOPPED(status) (((status) & 0xff) == 0x7f)
#define WCOREDUMP(status) ((status) & 0x80)

typedef enum
{
  P_ALL,
  P_PID,
  P_PGID
} idtype_t;

#endif
