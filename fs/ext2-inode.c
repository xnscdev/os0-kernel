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
#include <sys/ata.h>
#include <sys/device.h>
#include <sys/sysmacros.h>
#include <vm/heap.h>
#include <errno.h>
#include <string.h>

static loff_t
ext2_try_alloc_block (VFSSuperblock *sb, int index)
{
  Ext2Superblock *esb = sb->sb_private;
  SpecDevice *dev = sb->sb_dev;
  Ext2BGD *bgdt = (Ext2BGD *) (sb->sb_private + sizeof (Ext2Superblock));
  unsigned char *busage;
  int ret;
  int i;
  if (bgdt[index].eb_bfree == 0)
    return -ENOSPC; /* No free block */
  busage = kmalloc (esb->esb_bpg >> 3);
  if (unlikely (busage == NULL))
    return -ENOMEM;
  ret = dev->sd_read (dev, busage, esb->esb_bpg >> 3,
		      bgdt[index].eb_busage * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (busage);
      return ret;
    }
  for (i = 0; i < esb->esb_bpg; i++)
    {
      uint32_t byte = i >> 3;
      uint32_t offset = i % 8;
      if (busage[byte] & 1 << offset)
	continue;

      /* Mark the block as allocated */
      busage[byte] |= 1 << offset;
      ret = dev->sd_write (dev, busage, esb->esb_bpg >> 3,
			   bgdt[index].eb_busage * sb->sb_blksize);
      kfree (busage);
      if (ret != 0)
	return ret;

      /* Subtract from free blocks in superblock and BGDT */
      esb->esb_fblocks--;
      bgdt[index].eb_bfree--;
      ext2_update (sb);

      return esb->esb_bpg * byte + i;
    }
  kfree (busage);
  return -ENOSPC;
}

static ino_t
ext2_try_create_inode (VFSSuperblock *sb, int index)
{
  Ext2Superblock *esb = sb->sb_private;
  SpecDevice *dev = sb->sb_dev;
  Ext2BGD *bgdt = (Ext2BGD *) (sb->sb_private + sizeof (Ext2Superblock));
  unsigned char *iusage;
  int ret;
  int i;
  if (bgdt[index].eb_ifree == 0)
    return 0; /* No free inodes */
  iusage = kmalloc (esb->esb_ipg >> 3);
  if (unlikely (iusage == NULL))
    return 0;
  ret = dev->sd_read (dev, iusage, esb->esb_ipg >> 3,
		      bgdt[index].eb_iusage * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (iusage);
      return ret;
    }
  for (i = 0; i < esb->esb_ipg; i++)
    {
      uint32_t byte = i >> 3;
      uint32_t offset = i % 8;
      if (iusage[byte] & 1 << offset)
	continue;

      /* Mark the inode as allocated */
      iusage[byte] |= 1 << offset;
      ret = dev->sd_write (dev, iusage, esb->esb_ipg >> 3,
			   bgdt[index].eb_iusage * sb->sb_blksize);
      kfree (iusage);
      if (ret != 0)
	return ret;

      /* Subtract from free inodes in superblock and BGDT */
      esb->esb_finodes--;
      bgdt[index].eb_ifree--;
      ext2_update (sb);

      return esb->esb_ipg * index + i + 1;
    }
  kfree (iusage);
  return 0;
}

static int
ext2_unalloc_block (VFSSuperblock *sb, loff_t block)
{
  Ext2Superblock *esb = sb->sb_private;
  SpecDevice *dev = sb->sb_dev;
  Ext2BGD *bgdt = (Ext2BGD *) (sb->sb_private + sizeof (Ext2Superblock));
  unsigned char *busage;
  uint32_t blkgrp = block / esb->esb_bpg;
  uint32_t rem = block % esb->esb_bpg;
  uint32_t byte = rem >> 3;
  uint32_t offset = rem % 8;
  int ret;
  busage = kmalloc (esb->esb_bpg >> 3);
  if (unlikely (busage == NULL))
    return 0;
  ret = dev->sd_read (dev, busage, esb->esb_bpg >> 3,
		      bgdt[blkgrp].eb_busage * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (busage);
      return ret;
    }

  /* Mark the block as unallocated */
  busage[byte] &= ~(1 << offset);
  ret = dev->sd_write (dev, busage, esb->esb_bpg >> 3,
		       bgdt[blkgrp].eb_busage * sb->sb_blksize);
  kfree (busage);
  if (ret != 0)
    return ret;

  /* Add to free blocks in superblock and BGDT */
  esb->esb_fblocks++;
  bgdt[blkgrp].eb_bfree++;
  ext2_update (sb);
  return 0;
}

static int
ext2_clear_inode (VFSSuperblock *sb, ino_t inode)
{
  Ext2Superblock *esb = sb->sb_private;
  SpecDevice *dev = sb->sb_dev;
  Ext2BGD *bgdt = (Ext2BGD *) (sb->sb_private + sizeof (Ext2Superblock));
  unsigned char *iusage;
  uint32_t blkgrp = inode / esb->esb_ipg;
  uint32_t rem = inode % esb->esb_ipg;
  uint32_t byte = rem >> 3;
  uint32_t offset = rem % 8;
  int ret;
  iusage = kmalloc (esb->esb_ipg >> 3);
  if (unlikely (iusage == NULL))
    return 0;
  ret = dev->sd_read (dev, iusage, esb->esb_ipg >> 3,
		      bgdt[blkgrp].eb_iusage * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (iusage);
      return ret;
    }

  /* Mark the inode as unallocated */
  iusage[byte] &= ~(1 << offset);
  ret = dev->sd_write (dev, iusage, esb->esb_ipg >> 3,
		       bgdt[blkgrp].eb_iusage * sb->sb_blksize);
  kfree (iusage);
  if (ret != 0)
    return ret;

  /* Add to free inodes in superblock and BGDT */
  esb->esb_finodes++;
  bgdt[blkgrp].eb_ifree++;
  ext2_update (sb);
  return 0;
}

static int
ext2_try_unalloc_pointer (VFSInode *inode, off_t block, uint32_t *data)
{
  int i;
  for (i = 0; i < inode->vi_sb->sb_blksize >> 2; i++)
    {
      if (data[i] != 0)
	return 1;
    }
  return ext2_unalloc_block (inode->vi_sb, block);
}

int
ext2_read_blocks (void *buffer, VFSSuperblock *sb, uint32_t block,
		  size_t nblocks)
{
  return sb->sb_dev->sd_read (sb->sb_dev, buffer, nblocks * sb->sb_blksize,
			      block * sb->sb_blksize);
}

int
ext2_write_blocks (void *buffer, VFSSuperblock *sb, uint32_t block,
		   size_t nblocks)
{
  int ret = sb->sb_dev->sd_write (sb->sb_dev, buffer, nblocks * sb->sb_blksize,
				  block * sb->sb_blksize);
  if (ret == 0)
    ((Ext2Superblock *) sb->sb_private)->esb_wtime = time (NULL);
  return ret;
}

off_t
ext2_data_block (Ext2Inode *inode, VFSSuperblock *sb, off_t block)
{
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  off_t realblock;

  /* First 12 data blocks stored directly in inode */
  if (block < EXT2_STORED_INODES)
    return inode->ei_bptr0[block];

  /* Read indirect block pointer */
  bptr1 = kmalloc (sb->sb_blksize);
  if (block < sb->sb_blksize + EXT2_STORED_INODES)
    {
      if (ext2_read_blocks (bptr1, sb, inode->ei_bptr1, 1) != 0)
        goto err;
      realblock = bptr1[block - EXT2_STORED_INODES];
      kfree (bptr1);
      return realblock;
    }

  /* Read doubly indirect block pointer */
  bptr2 = kmalloc (sb->sb_blksize);
  if (block < sb->sb_blksize * sb->sb_blksize + sb->sb_blksize +
      EXT2_STORED_INODES)
    {
      realblock = block - sb->sb_blksize - EXT2_STORED_INODES;
      if (ext2_read_blocks (bptr1, sb, inode->ei_bptr2, 1) != 0)
	goto err;
      if (ext2_read_blocks (bptr2, sb,
			    bptr1[realblock / sb->sb_blksize], 1) != 0)
	goto err;
      realblock = bptr2[realblock % sb->sb_blksize];
      kfree (bptr1);
      kfree (bptr2);
      return realblock;
    }

  /* Read triply indirect block pointer */
  bptr3 = kmalloc (sb->sb_blksize);
  if (block < sb->sb_blksize * sb->sb_blksize * sb->sb_blksize +
      sb->sb_blksize * sb->sb_blksize + sb->sb_blksize + EXT2_STORED_INODES)
    {
      realblock = block - sb->sb_blksize * sb->sb_blksize - sb->sb_blksize -
	EXT2_STORED_INODES;
      if (ext2_read_blocks (bptr1, sb, inode->ei_bptr3, 1) != 0)
	goto err;
      if (ext2_read_blocks (bptr2, sb, bptr1[realblock / (sb->sb_blksize *
							  sb->sb_blksize)],
			    1) != 0)
	goto err;
      if (ext2_read_blocks (bptr3, sb, bptr2[realblock % (sb->sb_blksize *
							  sb->sb_blksize) /
					     sb->sb_blksize], 1) != 0)
	goto err;
      realblock = bptr3[realblock % sb->sb_blksize];
      kfree (bptr1);
      kfree (bptr2);
      kfree (bptr3);
      return realblock;
    }

  /* Block offset too large */
  return -EINVAL;

  /* Read error */
 err:
  if (bptr1 != NULL)
    kfree (bptr1);
  if (bptr2 != NULL)
    kfree (bptr2);
  if (bptr3 != NULL)
    kfree (bptr3);
  return -EIO;
}

#define EXT2_TRY_UNALLOC(inode, param, data)				\
  do									\
    {									\
      ret = ext2_try_unalloc_pointer (inode, param, data);		\
      if (ret == 0)							\
        param = 0;							\
      else if (ret < 0)							\
	goto err;							\
    }									\
  while (0)

int
ext2_unalloc_data_blocks (VFSInode *inode, off_t start, blkcnt_t nblocks)
{
  Ext2Inode *ei = inode->vi_private;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  blkcnt_t i;
  int ret = 0;

  for (i = 0; i < nblocks; i++)
    {
      off_t block = start + i;
      off_t realblock;

      /* First 12 data blocks stored directly in inode */
      if (block < EXT2_STORED_INODES)
        {
	  ret = ext2_unalloc_block (inode->vi_sb, ei->ei_bptr0[block]);
	  if (ret != 0)
	    goto err;
	  continue;
	}

      /* Read indirect block pointer */
      if (bptr1 == NULL)
	bptr1 = kmalloc (blksize);
      if (block < blksize + EXT2_STORED_INODES)
	{
	  if (ext2_read_blocks (bptr1, inode->vi_sb, ei->ei_bptr1, 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  ret = ext2_unalloc_block (inode->vi_sb,
				    bptr1[block - EXT2_STORED_INODES]);
	  bptr1[block - EXT2_STORED_INODES] = 0;
	  if (ret != 0)
	    goto err;
	  ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->ei_bptr1, 1);
	  if (ret != 0)
	    goto err;
	  EXT2_TRY_UNALLOC (inode, ei->ei_bptr1, bptr1);
	  if (ret == 0)
	    ext2_write_inode (inode);
	  continue;
	}

      /* Read doubly indirect block pointer */
      if (bptr2 == NULL)
	bptr2 = kmalloc (blksize);
      if (block < blksize * blksize + blksize + EXT2_STORED_INODES)
	{
	  realblock = block - blksize - EXT2_STORED_INODES;
	  if (ext2_read_blocks (bptr1, inode->vi_sb, ei->ei_bptr2, 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  if (ext2_read_blocks (bptr2, inode->vi_sb,
				bptr1[realblock / blksize], 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  ret = ext2_unalloc_block (inode->vi_sb, bptr2[realblock % blksize]);
	  bptr2[realblock % blksize] = 0;
	  if (ret != 0)
	    goto err;
	  ret = ext2_write_blocks (bptr2, inode->vi_sb,
				   bptr1[realblock / blksize], 1);
	  if (ret != 0)
	    goto err;
	  EXT2_TRY_UNALLOC (inode, bptr1[realblock / blksize], bptr2);
	  if (ret == 0)
	    {
	      ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->ei_bptr2, 1);
	      if (ret != 0)
		goto err;
	    }
	  EXT2_TRY_UNALLOC (inode, ei->ei_bptr1, bptr1);
	  if (ret == 0)
	    ext2_write_inode (inode);
	  continue;
	}

      /* Read triply indirect block pointer */
      if (bptr3 == NULL)
	bptr3 = kmalloc (blksize);
      if (block < blksize * blksize * blksize + blksize * blksize + blksize +
	  EXT2_STORED_INODES)
	{
	  uint32_t blk;
	  realblock = block - blksize * blksize - blksize - EXT2_STORED_INODES;
	  if (ext2_read_blocks (bptr1, inode->vi_sb, ei->ei_bptr3, 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  if (ext2_read_blocks (bptr2, inode->vi_sb,
				bptr1[realblock / (blksize * blksize)], 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  blk = bptr2[realblock % (blksize * blksize) / blksize];
	  if (ext2_read_blocks (bptr3, inode->vi_sb, blk, 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  ret = ext2_unalloc_block (inode->vi_sb, bptr3[realblock % blksize]);
	  bptr3[realblock % blksize] = 0;
	  if (ret != 0)
	    goto err;
	  ret = ext2_write_blocks (bptr3, inode->vi_sb, blk, 1);
	  if (ret != 0)
	    goto err;
	  EXT2_TRY_UNALLOC (inode,
			    bptr2[realblock % (blksize * blksize) / blksize],
			    bptr3);
	  if (ret == 0)
	    {
	      ret =
		ext2_write_blocks (bptr2, inode->vi_sb,
				   bptr1[realblock / (blksize * blksize)], 1);
	      if (ret != 0)
		goto err;
	    }
	  EXT2_TRY_UNALLOC (inode, bptr1[realblock / (blksize * blksize)],
			    bptr2);
	  if (ret == 0)
	    {
	      ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->ei_bptr3, 1);
	      if (ret != 0)
		goto err;
	    }
	  EXT2_TRY_UNALLOC (inode, ei->ei_bptr3, bptr1);
	  if (ret == 0)
	    ext2_write_inode (inode);
	  continue;
	}

      /* Block offset too large */
      return -EINVAL;
    }

 err:
  if (bptr1 != NULL)
    kfree (bptr1);
  if (bptr2 != NULL)
    kfree (bptr2);
  if (bptr3 != NULL)
    kfree (bptr3);
  return ret;
}

#undef EXT2_TRY_UNALLOC

uint32_t
ext2_inode_offset (VFSSuperblock *sb, ino_t inode)
{
  Ext2Superblock *esb = sb->sb_private;
  Ext2BGD *bgdt = (Ext2BGD *) (sb->sb_private + sizeof (Ext2Superblock));
  uint32_t inosize = esb->esb_versmaj > 0 ? esb->esb_inosize : 128;
  uint32_t inotbl = bgdt[(inode - 1) / esb->esb_ipg].eb_inotbl;
  uint32_t index = (inode - 1) % esb->esb_ipg;
  return inotbl * sb->sb_blksize + index * inosize;
}

Ext2Inode *
ext2_read_inode (VFSSuperblock *sb, ino_t inode)
{
  Ext2Superblock *esb = sb->sb_private;
  Ext2BGD *bgdt = (Ext2BGD *) (sb->sb_private + sizeof (Ext2Superblock));
  uint32_t inosize = esb->esb_versmaj > 0 ? esb->esb_inosize : 128;
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
ext2_unref_inode (VFSSuperblock *sb, ino_t inode)
{
  VFSInode *vi = ext2_alloc_inode (sb);
  Ext2Inode *ei;
  int ret = 0;
  if (unlikely (vi == NULL))
    return -ENOMEM;
  vi->vi_ino = inode;
  ext2_fill_inode (vi);
  ei = vi->vi_private;
  if (ei == NULL)
    return -EIO;
  switch (ei->ei_nlink)
    {
    case 0:
      goto finish;
    case 1:
      ei->ei_dtime = time (NULL);
      ret = ext2_clear_inode (sb, inode);
    default:
      ei->ei_nlink--;
      ext2_write_inode (vi);
    }

 finish:
  ext2_destroy_inode (vi);
  return ret;
}

loff_t
ext2_alloc_block (VFSSuperblock *sb, int prefbg)
{
  int i;
  if (prefbg >= 0)
    {
      loff_t block = ext2_try_alloc_block (sb, prefbg);
      if (block == -ENOMEM)
	return -ENOMEM;
      else if (block >= 0)
	return block;
    }
  for (i = 0; i < ext2_bgdt_size (sb->sb_private); i++)
    {
      loff_t block;
      if (i == prefbg)
	continue; /* Already tried this block group */
      block = ext2_try_alloc_block (sb, i);
      if (block == -ENOMEM)
	return -ENOMEM;
      else if (block >= 0)
	return block;
    }
  return -ENOSPC;
}

ino_t
ext2_create_inode (VFSSuperblock *sb, int prefbg)
{
  int i;
  if (prefbg >= 0)
    {
      ino_t inode = ext2_try_create_inode (sb, prefbg);
      if (inode > 0)
	return inode;
    }
  for (i = 0; i < ext2_bgdt_size (sb->sb_private); i++)
    {
      ino_t inode;
      if (i == prefbg)
	continue; /* Already tried this block group */
      inode = ext2_try_create_inode (sb, i);
      if (inode > 0)
	return inode;
    }
  return 0;
}

int
ext2_add_entry (VFSInode *dir, VFSInode *inode, const char *name)
{
  Ext2Superblock *esb = dir->vi_sb->sb_private;
  VFSDirEntry *entries;
  VFSDirEntry *temp;
  void *data;
  size_t size;
  off_t block;
  off_t realblock;
  off_t ret;
  size = strlen (name);
  if (size > EXT2_MAX_NAME_LEN)
    return -ENAMETOOLONG;
  data = kmalloc (dir->vi_sb->sb_blksize);
  if (unlikely (data == NULL))
    return -ENOMEM;

  ret = ext2_readdir (&entries, dir->vi_sb, dir);
  if (ret != 0)
    return ret;
  temp = entries->d_next;
  while (1)
    {
      if (strcmp (entries->d_name, name) == 0)
        ret = -EEXIST;
      vfs_destroy_dir_entry (entries);
      entries = temp;
      if (entries == NULL)
	break;
      temp = entries->d_next;
    }
  if (ret != 0)
    return ret;

  for (block = 0; (loff_t) block * dir->vi_sb->sb_blksize < dir->vi_size;
       block++)
    {
      int i = 0;
      realblock = ext2_data_block (dir->vi_private, dir->vi_sb, block);
      if (ret < 0)
	{
	  kfree (data);
	  return realblock;
	}
      ret = ext2_read_blocks (data, dir->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (data);
	  return ret;
	}

      while (i < dir->vi_sb->sb_blksize)
	{
	  Ext2DirEntry *guess = (Ext2DirEntry *) (data + i);
	  size_t testsize = guess->ed_namelenl;
	  size_t extra;
	  if ((esb->esb_reqft & EXT2_FT_REQ_DIRTYPE) == 0)
	    testsize |= guess->ed_namelenh << 8;
	  extra = (guess->ed_size - sizeof (Ext2DirEntry) - testsize) & ~3;
	  if (guess->ed_inode == 0 || guess->ed_size == 0)
	    i += 4;
	  else if (extra >= size + sizeof (Ext2DirEntry))
	    {
	      size_t skip = testsize + sizeof (Ext2DirEntry);
	      Ext2DirEntry *new;
	      if (skip & 3)
		{
		  skip &= ~3;
		  skip += 4;
		}
	      new = (Ext2DirEntry *) (data + i + skip);
	      new->ed_inode = inode->vi_ino;
	      new->ed_size = guess->ed_size - skip;
	      new->ed_namelenl = size & 0xff;
	      if (esb->esb_reqft & EXT2_FT_REQ_DIRTYPE)
		{
		  if (S_ISREG (inode->vi_mode))
		    new->ed_namelenh = EXT2_DIRTYPE_FILE;
		  else if (S_ISDIR (inode->vi_mode))
		    new->ed_namelenh = EXT2_DIRTYPE_DIR;
		  else if (S_ISCHR (inode->vi_mode))
		    new->ed_namelenh = EXT2_DIRTYPE_CHRDEV;
		  else if (S_ISBLK (inode->vi_mode))
		    new->ed_namelenh = EXT2_DIRTYPE_BLKDEV;
		  else if (S_ISFIFO (inode->vi_mode))
		    new->ed_namelenh = EXT2_DIRTYPE_FIFO;
		  else if (S_ISSOCK (inode->vi_mode))
		    new->ed_namelenh = EXT2_DIRTYPE_SOCKET;
		  else if (S_ISLNK (inode->vi_mode))
		    new->ed_namelenh = EXT2_DIRTYPE_LINK;
		  else
		    new->ed_namelenh = EXT2_DIRTYPE_NONE;
		}
	      else
		new->ed_namelenh = size >> 8 & 0xff;
	      memcpy ((char *) new + sizeof (Ext2DirEntry), name, size);
	      guess->ed_size = skip;

	      ret = ext2_write_blocks (data, dir->vi_sb, realblock, 1);
	      kfree (data);
	      return ret;
	    }
	  else
	    i += guess->ed_size;
	}
    }

  /* TODO Allocate a new block and add a dir entry */
  kfree (data);
  return -ENOSPC;
}

int
ext2_create (VFSInode *dir, const char *name, mode_t mode)
{
  Ext2Superblock *esb = dir->vi_sb->sb_private;
  VFSInode *inode;
  Ext2Inode *ei;
  ino_t ino;
  time_t newtime = time (NULL);
  int ret;

  inode = ext2_alloc_inode (dir->vi_sb);
  if (unlikely (inode == NULL))
    return -ENOMEM;
  ino = ext2_create_inode (dir->vi_sb, dir->vi_ino / esb->esb_ipg);
  if (ino == 0)
    {
      kfree (inode);
      return -ENOSPC;
    }

  ei = kmalloc (sizeof (Ext2Inode));
  if (unlikely (ei == NULL))
    {
      kfree (inode);
      return -ENOMEM;
    }

  /* TODO Pre-allocate data blocks */
  ei->ei_mode = EXT2_TYPE_FILE | (mode & 07777);
  ei->ei_uid = 0;
  ei->ei_sizel = 0;
  ei->ei_atime = newtime;
  ei->ei_ctime = newtime;
  ei->ei_mtime = newtime;
  ei->ei_dtime = 0;
  ei->ei_gid = 0;
  ei->ei_nlink = 1;
  ei->ei_sectors = 0;
  ei->ei_flags = 0;
  ei->ei_oss1 = 0;
  memset (ei->ei_bptr0, 0, EXT2_STORED_INODES << 2);
  ei->ei_bptr1 = 0;
  ei->ei_bptr2 = 0;
  ei->ei_bptr3 = 0;
  ei->ei_gen = 0; /* ??? */
  ei->ei_acl = 0;
  ei->ei_sizeh = 0;
  ei->ei_fragbaddr = 0; /* ??? */
  memset (ei->ei_oss2, 0, 3);

  inode->vi_ino = ino;
  inode->vi_mode = mode | S_IFREG;
  inode->vi_uid = ei->ei_uid;
  inode->vi_gid = ei->ei_gid;
  inode->vi_flags = ei->ei_flags;
  inode->vi_nlink = ei->ei_nlink;
  inode->vi_size = ei->ei_sizel;
  if ((ei->ei_mode & 0xf000) == EXT2_TYPE_FILE && esb->esb_versmaj > 0
      && esb->esb_roft & EXT2_FT_RO_FILESIZE64)
    inode->vi_size |= (loff_t) ei->ei_sizeh << 32;
  inode->vi_atime.tv_sec = ei->ei_atime;
  inode->vi_atime.tv_nsec = 0;
  inode->vi_mtime.tv_sec = ei->ei_mtime;
  inode->vi_mtime.tv_nsec = 0;
  inode->vi_ctime.tv_sec = ei->ei_ctime;
  inode->vi_ctime.tv_nsec = 0;
  inode->vi_sectors = ei->ei_sectors;
  inode->vi_blocks = ei->ei_sectors * ATA_SECTSIZE / inode->vi_sb->sb_blksize;
  inode->vi_private = ei;

  ext2_write_inode (inode);
  ret = ext2_add_entry (dir, inode, name);
  vfs_destroy_inode (inode);
  return ret;
}

int
ext2_lookup (VFSDirEntry *entry, VFSSuperblock *sb, VFSPath *path)
{
  VFSDirEntry *dir = sb->sb_root;
  int ret;

  for (path = vfs_path_first (path); path != NULL; path = path->vp_next)
    {
      VFSDirEntry *entries;
      ret = ext2_readdir (&entries, sb, dir->d_inode);
      if (ret != 0)
	return ret;
      for (; entries != NULL; entries = entries->d_next)
	{
	  char *name = path->vp_long == NULL ? path->vp_short : path->vp_long;
	  vfs_destroy_dir_entry (entries->d_prev);
	  entries->d_prev = NULL;
	  if (strcmp (entries->d_name, name) == 0)
	    goto found;
	}
      vfs_destroy_dir_entry (entries);
      if (dir != sb->sb_root)
	vfs_destroy_dir_entry (dir);
      return -ENOENT;

    found:
      dir = entries;

      /* Clean up all unchecked entries */
      for (entries = entries->d_next; entries != NULL;
	   entries = entries->d_next)
        vfs_destroy_dir_entry (entries);
      dir->d_next = NULL;

      if (path->vp_next != NULL && !S_ISDIR (dir->d_inode->vi_mode))
	{
	  vfs_destroy_dir_entry (dir);
	  return -ENOTDIR;
	}
      if (strlen (dir->d_name) > EXT2_MAX_NAME_LEN)
	{
	  vfs_destroy_dir_entry (dir);
	  return -ENAMETOOLONG;
	}
    }

  memcpy (entry, dir, sizeof (VFSDirEntry));
  kfree (dir);
  return 0;
}

int
ext2_link (VFSInode *old, VFSInode *dir, const char *new)
{
  return old->vi_sb == dir->vi_sb ? ext2_add_entry (dir, old, new) : -EXDEV;
}

int
ext2_unlink (VFSInode *dir, const char *name)
{
  VFSSuperblock *sb = dir->vi_sb;
  Ext2Superblock *esb;
  Ext2Inode *ei;
  void *buffer;
  char *guessname;
  int blocks;
  int ret;
  int i;

  if (dir->vi_ino == 0)
    return -EINVAL;
  ei = ext2_read_inode (sb, dir->vi_ino);
  if (ei == NULL)
    return -EINVAL;

  if ((ei->ei_mode & 0xf000) != EXT2_TYPE_DIR)
    {
      kfree (ei);
      return -ENOTDIR;
    }

  esb = sb->sb_private;
  blocks = (dir->vi_size + sb->sb_blksize - 1) / sb->sb_blksize;
  buffer = kmalloc (sb->sb_blksize);

  for (i = 0; i < blocks; i++)
    {
      int bytes = 0;
      loff_t block = ext2_data_block (ei, sb, i);
      Ext2DirEntry *last = NULL;
      if (ext2_read_blocks (buffer, sb, block, 1) != 0)
	{
	  kfree (buffer);
	  kfree (ei);
	  return -EIO;
	}
      while (bytes < sb->sb_blksize)
	{
	  Ext2DirEntry *guess = (Ext2DirEntry *) (buffer + bytes);
	  uint16_t namelen;
	  if (guess->ed_inode == 0 || guess->ed_size == 0)
	    {
	      bytes += 4;
	      continue;
	    }

	  namelen = guess->ed_namelenl;
	  if ((esb->esb_reqft & EXT2_FT_REQ_DIRTYPE) == 0)
	    namelen |= guess->ed_namelenh << 8;

	  guessname = (char *) guess + sizeof (Ext2DirEntry);
	  if (strlen (name) == namelen
	      && strncmp (guessname, name, namelen) == 0)
	    {
	      ino_t temp = guess->ed_inode;
	      kfree (ei);
	      if (last != NULL)
	        last->ed_size += guess->ed_size;
	      memset (guess, 0, sizeof (Ext2DirEntry));
	      ret = ext2_write_blocks (buffer, sb, block, 1);
	      kfree (buffer);
	      if (ret != 0)
		return ret;
	      return ext2_unref_inode (sb, temp);
	    }

	  bytes += guess->ed_size;
	  last = guess;
	}
    }

  kfree (buffer);
  kfree (ei);
  return -ENOENT;
}

int
ext2_symlink (VFSInode *dir, const char *old, const char *new)
{
  return -ENOSYS;
}

int
ext2_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  char *temp;
  uint32_t realblock;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  off_t start_block = offset / blksize;
  off_t mid_block = start_block + (offset % blksize != 0);
  off_t end_block = (offset + len) / blksize;
  size_t blocks = end_block - mid_block;
  size_t start_diff = mid_block * blksize - offset;
  size_t end_diff = offset + len - end_block * blksize;
  size_t i;
  int ret;

  if (S_ISDIR (inode->vi_mode))
    return -EISDIR;
  if (offset > inode->vi_size)
    return -EINVAL;

  if (mid_block > end_block)
    {
      /* Completely contained in a single block */
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, start_block);
      if (realblock < 0)
	return realblock;
      temp = kmalloc (blksize);
      if (unlikely (temp == NULL))
	return -ENOMEM;
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (buffer, temp + blksize - start_diff, len);
      kfree (temp);
      return 0;
    }

  for (i = 0; i < blocks; i++)
    {
      realblock = ext2_data_block (inode->vi_private, inode->vi_sb, i);
      if (realblock < 0)
	return realblock;
      ret = ext2_read_blocks (buffer + start_diff + i * blksize, inode->vi_sb,
			      realblock, 1);
      if (ret != 0)
        return ret;
    }

  /* Read unaligned starting bytes */
  if (start_diff != 0)
    {
      temp = kmalloc (blksize);
      if (unlikely (temp == NULL))
	return -ENOMEM;
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, start_block);
      if (realblock < 0)
	{
	  kfree (temp);
	  return realblock;
	}
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (buffer, temp + blksize - start_diff, start_diff);
    }

  /* Read unaligned ending bytes */
  if (end_diff != 0)
    {
      if (temp == NULL)
	{
	  temp = kmalloc (blksize);
	  if (unlikely (temp == NULL))
	    return -ENOMEM;
	}
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, end_block);
      if (realblock < 0)
	{
	  kfree (temp);
	  return realblock;
	}
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (buffer + start_diff + blocks * blksize, temp, end_diff);
    }

  kfree (temp);
  return 0;
}

int
ext2_write (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  char *temp;
  uint32_t realblock;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  off_t start_block = offset / blksize;
  off_t mid_block = start_block + (offset % blksize != 0);
  off_t end_block = (offset + len) / blksize;
  size_t blocks = end_block - mid_block;
  size_t start_diff = mid_block * blksize - offset;
  size_t end_diff = offset + len - end_block * blksize;
  size_t i;
  int ret;

  if (S_ISDIR (inode->vi_mode))
    return -EISDIR;
  if (offset > inode->vi_size)
    return -EINVAL; /* TODO Truncate file to (offset + len) bytes */

  if (mid_block > end_block)
    {
      /* Completely contained in a single block */
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, start_block);
      if (realblock < 0)
	return realblock;
      temp = kmalloc (blksize);
      if (unlikely (temp == NULL))
	return -ENOMEM;
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (temp + blksize - start_diff, buffer, len);
      ret = ext2_write_blocks (temp, inode->vi_sb, realblock, 1);
      kfree (temp);
      return ret;
    }

  for (i = 0; i < blocks; i++)
    {
      realblock = ext2_data_block (inode->vi_private, inode->vi_sb, i);
      if (realblock < 0)
	return realblock;
      ret = ext2_write_blocks (buffer + start_diff + i * blksize, inode->vi_sb,
			       realblock, 1);
      if (ret != 0)
        return ret;
    }

  /* Write unaligned starting bytes */
  if (start_diff != 0)
    {
      temp = kmalloc (blksize);
      if (unlikely (temp == NULL))
	return -ENOMEM;
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, start_block);
      if (realblock < 0)
	{
	  kfree (temp);
	  return realblock;
	}
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (temp + blksize - start_diff, buffer, start_diff);
      ret = ext2_write_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
    }

  /* Write unaligned ending bytes */
  if (end_diff != 0)
    {
      if (temp == NULL)
	{
	  temp = kmalloc (blksize);
	  if (unlikely (temp == NULL))
	    return -ENOMEM;
	}
      realblock =
	ext2_data_block (inode->vi_private, inode->vi_sb, end_block);
      if (realblock < 0)
	{
	  kfree (temp);
	  return realblock;
	}
      ret = ext2_read_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
      memcpy (temp, buffer + start_diff + blocks * blksize, end_diff);
      ret = ext2_write_blocks (temp, inode->vi_sb, realblock, 1);
      if (ret != 0)
	{
	  kfree (temp);
	  return ret;
	}
    }

  kfree (temp);
  return 0;
}

int
ext2_readdir (VFSDirEntry **entries, VFSSuperblock *sb, VFSInode *dir)
{
  Ext2Superblock *esb;
  Ext2Inode *ei;
  VFSDirEntry *result = NULL;
  VFSDirEntry *temp;
  void *buffer;
  int blocks;
  int i;

  if (dir->vi_ino == 0)
    return -EINVAL;
  ei = ext2_read_inode (sb, dir->vi_ino);
  if (ei == NULL)
    return -EINVAL;

  if ((ei->ei_mode & 0xf000) != EXT2_TYPE_DIR)
    {
      kfree (ei);
      return -ENOTDIR;
    }

  esb = sb->sb_private;
  blocks = (dir->vi_size + sb->sb_blksize - 1) / sb->sb_blksize;
  buffer = kmalloc (sb->sb_blksize);

  for (i = 0; i < blocks; i++)
    {
      int bytes = 0;
      if (ext2_read_blocks (buffer, sb, ext2_data_block (ei, sb, i), 1) != 0)
	{
	  kfree (buffer);
	  kfree (ei);
	  return -EIO;
	}
      while (bytes < sb->sb_blksize)
	{
	  Ext2DirEntry *guess = (Ext2DirEntry *) (buffer + bytes);
	  uint16_t namelen;
	  if (guess->ed_inode == 0 || guess->ed_size == 0)
	    {
	      bytes += 4;
	      continue;
	    }

	  namelen = guess->ed_namelenl;
	  if ((esb->esb_reqft & EXT2_FT_REQ_DIRTYPE) == 0)
	    namelen |= guess->ed_namelenh << 8;

	  temp = kmalloc (sizeof (VFSDirEntry));
	  temp->d_flags = 0;
	  temp->d_inode = ext2_alloc_inode (sb);
	  temp->d_inode->vi_ino = guess->ed_inode;
	  ext2_fill_inode (temp->d_inode);
	  temp->d_mounted = 0;
	  temp->d_name = kmalloc (namelen + 1);
	  strncpy (temp->d_name, buffer + bytes + 8, namelen);
	  temp->d_name[namelen] = '\0';
	  temp->d_ops = &ext2_dops;
	  temp->d_prev = result;
	  if (result != NULL)
	    result->d_next = temp;
	  temp->d_next = NULL;
	  result = temp;

	  bytes += guess->ed_size;
	}
    }

  kfree (buffer);
  kfree (ei);
  for (; result->d_prev != NULL; result = result->d_prev)
    ;
  *entries = result;
  return 0;
}

int
ext2_chmod (VFSInode *inode, mode_t mode)
{
  return -ENOSYS;
}

int
ext2_chown (VFSInode *inode, uid_t uid, gid_t gid)
{
  return -ENOSYS;
}

int
ext2_mkdir (VFSInode *dir, const char *name, mode_t mode)
{
  return -ENOSYS;
}

int
ext2_rmdir (VFSInode *dir, const char *name)
{
  return -ENOSYS;
}

int
ext2_mknod (VFSInode *dir, const char *name, mode_t mode, dev_t rdev)
{
  return -ENOSYS;
}

int
ext2_rename (VFSInode *olddir, const char *oldname, VFSInode *newdir,
	     const char *newname)
{
  return -ENOSYS;
}

int
ext2_readlink (VFSDirEntry *entry, char *buffer, size_t len)
{
  int i;
  loff_t size = entry->d_inode->vi_size;
  Ext2Inode *inode = entry->d_inode->vi_private;

  if (size <= 60)
    {
      /* Path stored directly in block pointers */
      for (i = 0; i < len && i < size && i < 48; i++)
	buffer[i] = ((char *) inode->ei_bptr0)[i];
      for (; i < len && i < size && i < 52; i++)
	buffer[i] = ((char *) &inode->ei_bptr1)[i - 48];
      for (; i < len && i < size && i < 56; i++)
	buffer[i] = ((char *) &inode->ei_bptr2)[i - 52];
      for (; i < len && i < size && i < 60; i++)
	buffer[i] = ((char *) &inode->ei_bptr3)[i - 56];
    }
  else
    {
      uint32_t block = 0;
      VFSSuperblock *sb = entry->d_inode->vi_sb;
      uint32_t realblock;
      void *currblock;

      realblock = ext2_data_block (inode, sb, block);
      if (realblock < 0)
        return -EIO;

      currblock = kmalloc (sb->sb_blksize);
      if (unlikely (currblock == NULL))
	return -ENOMEM;

      while (1)
	{
	  if (ext2_read_blocks (currblock, sb, realblock, 1) != 0)
	    {
	      kfree (currblock);
	      return -EIO;
	    }
	  if (i + sb->sb_blksize > entry->d_inode->vi_size)
	    {
	      memcpy (buffer + i, currblock, entry->d_inode->vi_size - i);
	      kfree (currblock);
	      return i;
	    }
	  if (i + sb->sb_blksize > len)
	    {
	      memcpy (buffer + i, currblock, len - i);
	      kfree (currblock);
	      return i;
	    }

	  memcpy (buffer + i, currblock, sb->sb_blksize);
	  i += sb->sb_blksize;
	  realblock = ext2_data_block (inode, sb, ++block);
	  if (realblock < 0)
	    {
	      kfree (currblock);
	      return -EIO;
	    }
	}
    }
  return 0;
}

int
ext2_truncate (VFSInode *inode)
{
  Ext2Superblock *esb = inode->vi_sb->sb_private;
  Ext2Inode *ei = inode->vi_private;
  uint64_t origsize = ei->ei_sizel;
  uint64_t newsize = inode->vi_size & 0xffffffff;
  blksize_t blksize = inode->vi_sb->sb_blksize;

  if (S_ISDIR (inode->vi_mode))
    return -EISDIR;

  if (esb->esb_versmaj > 0 && S_ISREG (inode->vi_mode)
      && esb->esb_roft & EXT2_FT_RO_FILESIZE64)
    {
      origsize |= (uint64_t) ei->ei_sizeh << 32;
      ei->ei_sizeh = inode->vi_size >> 32;
      newsize |= inode->vi_size >> 32 << 32;
    }
  inode->vi_size = newsize;
  inode->vi_sectors = (newsize + blksize - 1) / blksize * ATA_SECTSIZE;
  ext2_write_inode (inode);

  if (origsize == newsize)
    return 0;
  else if (origsize > newsize)
    {
      off_t start = (newsize + blksize - 1) / blksize;
      blkcnt_t nblocks = (origsize + blksize - 1) / blksize - start;
      if (nblocks != 0)
	return ext2_unalloc_data_blocks (inode, start, nblocks);
      else
	return 0;
    }
  else
    return -ENOTSUP; /* TODO File enlargement support */
  return 0;
}

int
ext2_getattr (VFSMount *mp, VFSDirEntry *entry, struct stat *st)
{
  VFSInode *inode = entry->d_inode;
  if (inode == NULL)
    return -EINVAL;
  st->st_dev = makedev (inode->vi_sb->sb_dev->sd_major,
			inode->vi_sb->sb_dev->sd_minor);
  st->st_ino = inode->vi_ino;
  st->st_mode = inode->vi_mode;
  st->st_nlink = inode->vi_nlink;
  st->st_uid = inode->vi_uid;
  st->st_gid = inode->vi_gid;
  st->st_rdev = inode->vi_rdev;
  st->st_size = inode->vi_size;
  st->st_atime = inode->vi_atime.tv_sec;
  st->st_mtime = inode->vi_mtime.tv_sec;
  st->st_ctime = inode->vi_ctime.tv_sec;
  st->st_blksize = mp->vfs_sb.sb_blksize;
  st->st_blocks = inode->vi_blocks;
  return 0;
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
