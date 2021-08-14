/*************************************************************************
 * param.h -- This file is part of OS/0.                                 *
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

#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H

#include <sys/syslimits.h>
#include <sys/types.h>
#include <endian.h>
#include <limits.h>

#ifndef HZ
#define HZ 1000
#endif

#ifndef EXEC_PAGESIZE
#define EXEC_PAGESIZE 4096
#endif

#define NBBY CHAR_BIT

#if !defined MAXPATHLEN && defined PATH_MAX
#define MAXPATHLEN PATH_MAX
#endif

#if !defined MAXSYMLINKS && defined SYMLOOP_MAX
#define MAXSYMLINKS SYMLOOP_MAX
#endif

#if !defined MAXHOSTNAMELEN && defined HOST_NAME_MAX
#define MAXHOSTNAMELEN HOST_NAME_MAX
#endif

#if !defined NGROUPS && defined NGROUPS_MAX
#define NGROUPS NGROUPS_MAX
#endif

#if !defined CANBSIZ && defined MAX_CANON
#define CANBSIZ MAX_CANON
#endif

#if !defined NOFILE && defined OPEN_MAX
#define NOFILE OPEN_MAX
#endif

#ifndef NOGROUP
#define NOGROUP 65535
#endif

#ifndef NODEV
#define NODEV ((dev_t) -1)
#endif

#ifndef DEV_BSIZE
#define DEV_BSIZE 512
#endif

#define setbit(a, i) ((a)[(i) / NBBY] |= 1 << ((i) % NBBY))
#define clrbit(a, i) ((a)[(i) / NBBY] &= ~(1 << ((i) % NBBY)))
#define isset(a, i)  ((a)[(i) / NBBY] & (1 << ((i) % NBBY)))
#define isclr(a, i)  (((a)[(i) / NBBY] & (1 << ((i) % NBBY))) == 0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#endif
