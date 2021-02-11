/*************************************************************************
 * ext2-inode.c -- This file is part of OS/0.                            *
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
#include <libk/compile.h>
#include <sys/device.h>
#include <vm/heap.h>
#include <errno.h>
#include <string.h>

static int
ext2_read_blocks (void *buffer, VFSSuperblock *sb, uint32_t block,
		  size_t nblocks)
{
  return sb->sb_dev->sd_read (sb->sb_dev, buffer, nblocks * sb->sb_blksize,
			      block * sb->sb_blksize);
}

static Ext2Inode *
ext2_read_inode (VFSSuperblock *sb, ino_t inode)
{
  Ext2Superblock *esb = (Ext2Superblock *) sb->sb_private;
  Ext2BGD *bgdt = (Ext2BGD *) (sb->sb_private + sizeof (Ext2Superblock));
  uint32_t inosize = 1 << (esb->esb_inosize + 10);
  uint32_t inotbl = bgdt[(inode - 1) / esb->esb_ipg].eb_inotbl;
  uint32_t index = (inode - 1) % esb->esb_ipg;
  uint32_t block = inotbl + index * inosize / sb->sb_blksize;
  uint32_t offset = index % (sb->sb_blksize / inosize);
  void *buffer;
  Ext2Inode *result;

  if (inode == 0)
    return NULL;

  buffer = kmalloc (sb->sb_blksize);
  if (unlikely (buffer == NULL))
    return NULL;
  if (ext2_read_blocks (buffer, sb, block, 1) != 0)
    {
      kfree (buffer);
      return NULL;
    }

  result = kmalloc (sizeof (Ext2Inode));
  if (unlikely (result == NULL))
    {
      kfree (buffer);
      return NULL;
    }
  memcpy (result, buffer + offset * inosize, sizeof (Ext2Inode));
  kfree (buffer);
  return result;
}

int
ext2_create (VFSInode *dir, VFSDirEntry *entry, mode_t mode)
{
  return -ENOSYS;
}

int
ext2_lookup (VFSDirEntry *entry, VFSSuperblock *sb, VFSPath *path)
{
  return -ENOSYS;
}

int
ext2_link (VFSDirEntry *old, VFSInode *dir, VFSDirEntry *new)
{
  return -ENOSYS;
}

int
ext2_unlink (VFSInode *dir, VFSDirEntry *entry)
{
  return -ENOSYS;
}

int
ext2_symlink (VFSInode *dir, VFSDirEntry *entry, const char *name)
{
  return -ENOSYS;
}

int
ext2_readdir (VFSDirEntry **entries, VFSSuperblock *sb, VFSInode *dir)
{
  return -ENOSYS;
}

int
ext2_mkdir (VFSInode *dir, VFSDirEntry *entry, mode_t mode)
{
  return -ENOSYS;
}

int
ext2_rmdir (VFSInode *dir, VFSDirEntry *entry)
{
  return -ENOSYS;
}

int
ext2_mknod (VFSInode *dir, VFSDirEntry *entry, mode_t mode, dev_t rdev)
{
  return -ENOSYS;
}

int
ext2_rename (VFSInode *olddir, VFSDirEntry *oldentry, VFSInode *newdir,
	     VFSDirEntry *newentry)
{
  return -ENOSYS;
}

int
ext2_readlink (VFSDirEntry *entry, char *buffer, size_t len)
{
  return -ENOSYS;
}

int
ext2_truncate (VFSInode *inode)
{
  return -ENOSYS;
}

int
ext2_permission (VFSInode *inode, mode_t mask)
{
  return -ENOSYS;
}

int
ext2_getattr (VFSMount *mp, VFSDirEntry *entry, struct stat *st)
{
  return -ENOSYS;
}

int
ext2_setxattr (VFSDirEntry *entry, const char *name, const void *value,
	       size_t len, int flags)
{
  return -ENOSYS;
}

int
ext2_getxattr (VFSDirEntry *entry, const char *name, void *buffer, size_t len)
{
  return -ENOSYS;
}

int
ext2_listxattr (VFSDirEntry *entry, char *buffer, size_t len)
{
  return -ENOSYS;
}

int
ext2_removexattr (VFSDirEntry *entry, const char *name)
{
  return -ENOSYS;
}
