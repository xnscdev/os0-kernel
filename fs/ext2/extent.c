/*************************************************************************
 * util.c -- This file is part of OS/0.                                  *
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
#include <libk/libk.h>
#include <vm/heap.h>

int
ext4_extent_open (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
		  Ext4ExtentHandle **handle)
{
  return -ENOTSUP;
}

int
ext4_extent_goto (Ext4ExtentHandle *handle, int leaflvl, block_t block)
{
  return -ENOTSUP;
}

int
ext4_extent_get (Ext4ExtentHandle *handle, int flags, Ext4Extent *extent)
{
  return -ENOTSUP;
}

int
ext4_extent_set_bmap (Ext4ExtentHandle *handle, block_t logical,
		      block_t physical, int flags)
{
  return -ENOTSUP;
}

void
ext4_extent_free (Ext4ExtentHandle *handle)
{
}
