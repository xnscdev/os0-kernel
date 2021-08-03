/*************************************************************************
 * dirent.h -- This file is part of OS/0.                                *
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

#ifndef _BITS_DIRENT_H
#define _BITS_DIRENT_H

#ifndef _DIRENT_H
#error "<bits/dirent.h> should not be included directly"
#endif

#include <sys/types.h>

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     3
#define DT_BLK     4
#define DT_REG     5
#define DT_LNK     6
#define DT_SOCK    7

struct dirent
{
  ino_t d_ino;
  unsigned char d_namlen;
  unsigned char d_type;
  char d_name[256];
};

#define d_fileno d_ino

#endif
