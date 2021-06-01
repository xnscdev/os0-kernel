/*************************************************************************
 * fcntl.h -- This file is part of OS/0.                                 *
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

#ifndef _SYS_FCNTL_H
#define _SYS_FCNTL_H

#define O_RDONLY  0x00000000
#define O_WRONLY  0x00000001
#define O_RDWR    0x00000002
#define O_ACCMODE 0x00000003

#define O_CREAT     0x00000100
#define O_EXCL      0x00000200
#define O_NOCTTY    0x00000400
#define O_TRUNC     0x00001000
#define O_APPEND    0x00002000
#define O_NONBLOCK  0x00004000
#define O_DSYNC     0x00010000
#define O_DIRECT    0x00040000
#define O_LARGEFILE 0x00100000
#define O_DIRECTORY 0x00200000
#define O_NOFOLLOW  0x00400000
#define O_NOATIME   0x01000000
#define O_CLOEXEC   0x02000000

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_GETLK  5
#define F_SETLK  6
#define F_SETLKW 7
#define F_GETOWN 8
#define F_SETOWN 9
#define F_GETSIG 10
#define F_SETSIG 11

#define F_DUPFD_CLOEXEC 20

#define FD_CLOEXEC 1

#define AT_FDCWD            0x0100
#define AT_SYMLINK_FOLLOW   0x0200
#define AT_SYMLINK_NOFOLLOW 0x0400
#define AT_EACCESS          0x0800
#define AT_REMOVEDIR        0x1000

#endif
