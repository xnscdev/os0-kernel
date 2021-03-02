/*************************************************************************
 * fsguess.c -- This file is part of OS/0.                               *
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

#include <fs/ext2.h>

static int
vfs_guess_ext2 (SpecDevice *dev)
{
  uint16_t magic[1];
  if (dev->sd_read (dev, magic, 2, 0x438) != 0)
    return 0;
  return magic == EXT2_MAGIC;
}

int
vfs_guess_type (SpecDevice *dev)
{
  if (vfs_guess_ext2 (dev))
    return FS_TYPE_EXT2;
  return FS_TYPE_UNKNOWN;
}
