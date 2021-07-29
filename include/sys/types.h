/*************************************************************************
 * types.h -- This file is part of OS/0.                                 *
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

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define howmany(x, y) ((x) % (y) == 0 ? (x) / (y) : (x) / (y) + 1)

/* POSIX types */

typedef int32_t blksize_t;
typedef int64_t clock_t;
typedef int32_t clockid_t;
typedef uint16_t dev_t;
typedef uint16_t gid_t;
typedef uint16_t id_t;
typedef int32_t key_t;
typedef uint32_t mode_t;
typedef uint32_t nlink_t;
typedef int16_t pid_t;
typedef int32_t ssize_t;
typedef uint32_t suseconds_t;
typedef int64_t time_t;
typedef int32_t timer_t;
typedef uint16_t uid_t;
typedef uint32_t useconds_t;

typedef int32_t blkcnt32_t;
typedef uint32_t fsblkcnt32_t;
typedef uint32_t fsfilcnt32_t;
typedef uint32_t ino32_t;
typedef int32_t off32_t;

typedef int64_t blkcnt64_t;
typedef uint64_t fsblkcnt64_t;
typedef uint64_t fsfilcnt64_t;
typedef uint64_t ino64_t;
typedef int64_t off64_t;
typedef off64_t loff_t;

#if defined _FILE_OFFSET_BITS && _FILE_OFFSET_BITS == 64
typedef blkcnt64_t blkcnt_t;
typedef fsblkcnt64_t fsblkcnt_t;
typedef fsfilcnt64_t fsfilcnt_t;
typedef ino64_t ino_t;
typedef off64_t off_t;
#else
typedef blkcnt32_t blkcnt_t;
typedef fsblkcnt32_t fsblkcnt_t;
typedef fsfilcnt32_t fsfilcnt_t;
typedef ino32_t ino_t;
typedef off32_t off_t;
#endif

#endif
