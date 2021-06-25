/*************************************************************************
 * mman.h -- This file is part of OS/0.                                  *
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

#ifndef _BITS_MMAN_H
#define _BITS_MMAN_H

#define PROT_READ  4
#define PROT_WRITE 2
#define PROT_EXEC  1
#define PROT_NONE  0
#define __PROT_MASK 7

#define MAP_SHARED        0x0001
#define MAP_PRIVATE       0x0002
#define MAP_ANONYMOUS     0x0004
#define MAP_ANON          MAP_ANONYMOUS
#define MAP_DENYWRITE     0x0008
#define MAP_EXECUTABLE    0x0010
#define MAP_FILE          0x0020
#define MAP_FIXED         0x0040
#define MAP_GROWSDOWN     0x0080
#define MAP_NORESERVE     0x0100
#define MAP_STACK         0x0200
#define MAP_UNINITIALIZED 0x0400

#define MAP_FAILED ((void *) -1)

#endif
