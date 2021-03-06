/*************************************************************************
 * ext2-util.c -- This file is part of OS/0.                             *
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
#include <sys/ata.h>
#include <vm/heap.h>

#define EXT2_INDIR1_THRESH (blksize + EXT2_STORED_INODES)
#define EXT2_INDIR1_BLOCK0 (block - EXT2_STORED_INODES)

#define EXT2_INDIR2_THRESH (blksize * blksize + EXT2_INDIR1_THRESH)
#define EXT2_INDIR2_BLOCK0 (block - EXT2_INDIR1_THRESH)
#define EXT2_INDIR2_BLOCK1 (bptr1[realblock / blksize])
#define EXT2_INDIR2_BLOCK2 (bptr2[realblock % blksize])

#define EXT2_INDIR3_THRESH (blksize * blksize * blksize + EXT2_INDIR2_THRESH)
#define EXT2_INDIR3_BLOCK0 (block - EXT2_INDIR2_THRESH)
#define EXT2_INDIR3_BLOCK1 (bptr1[realblock / (blksize * blksize)])
#define EXT2_INDIR3_BLOCK2 (bptr2[realblock % (blksize * blksize) / blksize])
#define EXT2_INDIR3_BLOCK3 (bptr3[realblock % blksize])

static off64_t
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

      return (off64_t) esb->esb_bpg * index + i;
    }
  kfree (busage);
  return -ENOSPC;
}

static ino64_t
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

      return (ino64_t) esb->esb_ipg * index + i + 1;
    }
  kfree (iusage);
  return 0;
}

static int
ext2_unalloc_block (VFSSuperblock *sb, off64_t block)
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
ext2_clear_inode (VFSSuperblock *sb, ino64_t inode)
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
ext2_try_unalloc_pointer (VFSInode *inode, off64_t block, uint32_t *data)
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
ext2_extend_inode (VFSInode *inode, blkcnt_t origblocks, blkcnt_t newblocks)
{
  Ext2Superblock *esb = inode->vi_sb->sb_private;
  Ext2Inode *ei = inode->vi_private;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  blkcnt_t i;
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  int ret;

  for (i = origblocks; i < MIN (newblocks, EXT2_STORED_INODES); i++)
    {
      off64_t block =
	ext2_alloc_block (inode->vi_sb, inode->vi_ino / esb->esb_ipg);
      if (block < 0)
	return block;
      ei->ei_bptr0[i - 1] = block;
    }

  /* If the loop above was enough to allocate all needed blocks, finish */
  if (newblocks <= EXT2_STORED_INODES)
    {
      ext2_write_inode (inode);
      return 0;
    }

  /* Fill indirect block pointer */
  if (ei->ei_bptr1 == 0)
    {
      off64_t block =
	ext2_alloc_block (inode->vi_sb, inode->vi_ino / esb->esb_ipg);
      if (block < 0)
	return block;
      ei->ei_bptr1 = block;
    }
  bptr1 = kmalloc (blksize);
  if (unlikely (bptr1 == NULL))
    goto err;
  ret = ext2_read_blocks (bptr1, inode->vi_sb, ei->ei_bptr1, 1);
  if (ret != 0)
    goto err;
  for (i = origblocks; i < MIN (newblocks, blksize + EXT2_STORED_INODES); i++)
    {
      off64_t block =
	ext2_alloc_block (inode->vi_sb, inode->vi_ino / esb->esb_ipg);
      if (block < 0)
	{
	  ret = block;
	  goto err;
	}
      bptr1[i - EXT2_STORED_INODES] = block;
    }
  ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->ei_bptr1, 1);
  if (ret != 0)
    goto err;

  /* Fill doubly indirect block pointer */
  if (ei->ei_bptr2 == 0)
    {
      off64_t block =
	ext2_alloc_block (inode->vi_sb, inode->vi_ino / esb->esb_ipg);
      if (block < 0)
	return block;
      ei->ei_bptr2 = block;
    }
  bptr2 = kmalloc (blksize);
  if (unlikely (bptr2 == NULL))
    goto err;
  ret = ext2_read_blocks (bptr2, inode->vi_sb, ei->ei_bptr2, 1);
  if (ret != 0)
    goto err;
  for (i = 0; i < blksize; i++)
    {
      blkcnt_t j;
      ret = ext2_read_blocks (bptr1, inode->vi_sb, bptr2[i], 1);
      if (ret != 0)
	goto err;
      for (j = 0; j < blksize; j++)
	{
	  off64_t block;
	  if (origblocks >= i * blksize + j + EXT2_STORED_INODES)
	    continue;
	  if (newblocks < i * blksize + j + EXT2_STORED_INODES)
	    goto err; /* Done allocating, return */
	  block = ext2_alloc_block (inode->vi_sb, inode->vi_ino / esb->esb_ipg);
	  if (block < 0)
	    {
	      ret = block;
	      goto err;
	    }
	  bptr1[j] = block;
	}
      ret = ext2_write_blocks (bptr1, inode->vi_sb, bptr2[i], 1);
      if (ret != 0)
	goto err;
    }
  ret = ext2_write_blocks (bptr2, inode->vi_sb, ei->ei_bptr2, 1);
  if (ret != 0)
    goto err;

  /* Fill triply indirect block pointer */
  if (ei->ei_bptr3 == 0)
    {
      off64_t block =
	ext2_alloc_block (inode->vi_sb, inode->vi_ino / esb->esb_ipg);
      if (block < 0)
	return block;
      ei->ei_bptr3 = block;
    }
  bptr3 = kmalloc (blksize);
  if (unlikely (bptr3 == NULL))
    goto err;
  ret = ext2_read_blocks (bptr3, inode->vi_sb, ei->ei_bptr3, 1);
  if (ret != 0)
    goto err;
  for (i = 0; i < blksize; i++)
    {
      blkcnt_t j;
      ret = ext2_read_blocks (bptr2, inode->vi_sb, bptr3[i], 1);
      if (ret != 0)
	goto err;
      for (j = 0; j < blksize; j++)
	{
	  blkcnt_t k;
	  ret = ext2_read_blocks (bptr1, inode->vi_sb, bptr2[j], 1);
	  if (ret != 0)
	    goto err;
	  for (k = 0; k < blksize; k++)
	    {
	      off64_t block;
	      if (origblocks >=
		  i * blksize * blksize + j * blksize + k + EXT2_STORED_INODES)
		continue;
	      if (newblocks <
		  i * blksize * blksize + j * blksize + k + EXT2_STORED_INODES)
		goto err; /* Done allocating, return */
	      block =
		ext2_alloc_block (inode->vi_sb, inode->vi_ino / esb->esb_ipg);
	      if (block < 0)
		{
		  ret = block;
		  goto err;
		}
	      bptr1[k] = block;
	    }
	  ret = ext2_write_blocks (bptr1, inode->vi_sb, bptr2[j], 1);
	  if (ret != 0)
	    goto err;
	}
      ret = ext2_write_blocks (bptr2, inode->vi_sb, bptr3[i], 1);
      if (ret != 0)
	goto err;
    }
  ret = ext2_write_blocks (bptr3, inode->vi_sb, ei->ei_bptr3, 1);
  if (ret != 0)
    goto err;

  ext2_write_inode (inode);

 err:
  if (bptr1 != NULL)
    kfree (bptr1);
  if (bptr2 != NULL)
    kfree (bptr2);
  if (bptr3 != NULL)
    kfree (bptr3);
  return ret;
}

int
ext2_read_blocks (void *buffer, VFSSuperblock *sb, uint32_t block,
		  size_t nblocks)
{
  return sb->sb_dev->sd_read (sb->sb_dev, buffer, nblocks * sb->sb_blksize,
			      block * sb->sb_blksize);
}

int
ext2_write_blocks (const void *buffer, VFSSuperblock *sb, uint32_t block,
		   size_t nblocks)
{
  int ret = sb->sb_dev->sd_write (sb->sb_dev, buffer, nblocks * sb->sb_blksize,
				  block * sb->sb_blksize);
  if (ret == 0)
    ((Ext2Superblock *) sb->sb_private)->esb_wtime = time (NULL);
  return ret;
}

int
ext2_data_blocks (Ext2Inode *inode, VFSSuperblock *sb, off64_t block,
		  blkcnt_t nblocks, off64_t *result)
{
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  off64_t realblock;
  blksize_t blksize = sb->sb_blksize;
  int ret = 0;
  int i = 0;

  /* First 12 data blocks stored directly in inode */
  while (block < EXT2_STORED_INODES)
    {
      if (i == nblocks)
	return 0;
      result[i++] = inode->ei_bptr0[block++];
    }

  /* Read indirect block pointer */
  bptr1 = kmalloc (blksize);
  if (block < EXT2_INDIR1_THRESH)
    {
      if (ext2_read_blocks (bptr1, sb, inode->ei_bptr1, 1) != 0)
	goto err;
      while (block < EXT2_INDIR1_THRESH)
	{
	  if (i == nblocks)
	    goto end;
	  result[i++] = bptr1[EXT2_INDIR1_BLOCK0];
	  block++;
	}
    }

  /* Read doubly indirect block pointer */
  bptr2 = kmalloc (blksize);
  if (block < EXT2_INDIR2_THRESH)
    {
      realblock = EXT2_INDIR2_BLOCK0;
      if (ext2_read_blocks (bptr1, sb, inode->ei_bptr2, 1) != 0)
	goto err;
      if (ext2_read_blocks (bptr2, sb, EXT2_INDIR2_BLOCK1, 1) != 0)
	goto err;
      while (block < EXT2_INDIR2_THRESH)
	{
	  if (i == nblocks)
	    goto end;
	  if (realblock % blksize == 0)
	    {
	      if (ext2_read_blocks (bptr2, sb, EXT2_INDIR2_BLOCK1, 1) != 0)
		goto err;
	    }
	  result[i++] = EXT2_INDIR2_BLOCK2;
	  block++;
	  realblock++;
	}
    }

  /* Read triply indirect block pointer */
  bptr3 = kmalloc (blksize);
  if (block >= EXT2_INDIR3_THRESH)
    goto large;
  realblock = EXT2_INDIR3_BLOCK0;
  if (ext2_read_blocks (bptr1, sb, inode->ei_bptr3, 1) != 0)
    goto err;
  if (ext2_read_blocks (bptr2, sb, EXT2_INDIR3_BLOCK1, 1) != 0)
    goto err;
  if (ext2_read_blocks (bptr3, sb, EXT2_INDIR3_BLOCK2, 1) != 0)
    goto err;
  while (block < EXT2_INDIR3_THRESH)
    {
      if (i == nblocks)
	goto end;
      if (realblock % (blksize * blksize) == 0)
	{
	  if (ext2_read_blocks (bptr2, sb, EXT2_INDIR3_BLOCK1, 1) != 0)
	    goto err;
	}
      if (realblock % blksize == 0)
	{
	  if (ext2_read_blocks (bptr3, sb, EXT2_INDIR3_BLOCK2, 1) != 0)
	    goto err;
	}
      result[i++] = EXT2_INDIR3_BLOCK3;
      block++;
      realblock++;
    }

 large: /* File offset too large */
  ret = -EINVAL;
  goto end;

 err:
  ret = -EIO;
 end:
  kfree (bptr1);
  kfree (bptr2);
  kfree (bptr3);
  return ret;
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
ext2_unalloc_data_blocks (VFSInode *inode, off64_t start, blkcnt_t nblocks)
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
      off64_t block = start + i;
      off64_t realblock;

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
ext2_inode_offset (VFSSuperblock *sb, ino64_t inode)
{
  Ext2Superblock *esb = sb->sb_private;
  Ext2BGD *bgdt = (Ext2BGD *) (sb->sb_private + sizeof (Ext2Superblock));
  uint32_t inosize = esb->esb_versmaj > 0 ? esb->esb_inosize : 128;
  uint32_t inotbl = bgdt[(inode - 1) / esb->esb_ipg].eb_inotbl;
  uint32_t index = (inode - 1) % esb->esb_ipg;
  return inotbl * sb->sb_blksize + index * inosize;
}

Ext2Inode *
ext2_read_inode (VFSSuperblock *sb, ino64_t inode)
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
ext2_unref_inode (VFSSuperblock *sb, VFSInode *inode)
{
  Ext2Inode *ei = inode->vi_private;
  switch (ei->ei_nlink)
    {
    case 0:
      return 0;
    case 1:
      ei->ei_dtime = time (NULL);
      return ext2_clear_inode (sb, inode->vi_ino);
    default:
      ei->ei_nlink--;
      ext2_write_inode (inode);
      return 0;
    }
}

off64_t
ext2_alloc_block (VFSSuperblock *sb, int prefbg)
{
  int i;
  if (prefbg >= 0)
    {
      off64_t block = ext2_try_alloc_block (sb, prefbg);
      if (block == -ENOMEM)
	return -ENOMEM;
      else if (block >= 0)
	return block;
    }
  for (i = 0; i < ext2_bgdt_size (sb->sb_private); i++)
    {
      off64_t block;
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

ino64_t
ext2_create_inode (VFSSuperblock *sb, int prefbg)
{
  ino64_t inode;
  int i;
  if (prefbg >= 0)
    {
      inode = ext2_try_create_inode (sb, prefbg);
      if (inode > 0)
	return inode;
    }
  for (i = 0; i < ext2_bgdt_size (sb->sb_private); i++)
    {
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
  Ext2DirEntry *newent;
  VFSDirectory *d;
  VFSDirEntry *entry;
  void *data;
  size_t size;
  off64_t block;
  off64_t realblock;
  off64_t ret;

  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;

  size = strlen (name);
  if (size > EXT2_MAX_NAME_LEN)
    return -ENAMETOOLONG;
  data = kmalloc (dir->vi_sb->sb_blksize);
  if (unlikely (data == NULL))
    return -ENOMEM;

  d = ext2_alloc_dir (dir, dir->vi_sb);
  if (d == NULL)
    {
      kfree (data);
      return -ENOMEM;
    }

  while (1)
    {
      ret = ext2_readdir (&entry, d, dir->vi_sb);
      if (ret < 0)
	return ret;
      else if (ret == 1)
	break;
      if (strcmp (entry->d_name, name) == 0)
	{
	  kfree (d->vd_buffer);
	  kfree (d);
	  kfree (data);
	  vfs_destroy_dir_entry (entry);
	  return -EEXIST;
	}
      vfs_destroy_dir_entry (entry);
    }

  for (block = 0; (off64_t) block * dir->vi_sb->sb_blksize < dir->vi_size;
       block++)
    {
      int i = 0;
      ret =
	ext2_data_blocks (dir->vi_private, dir->vi_sb, block, 1, &realblock);
      if (ret < 0)
	{
	  kfree (data);
	  return ret;
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

	      /* Align to 4 bytes */
	      if (skip & 3)
		{
		  skip &= ~3;
		  skip += 4;
		}

	      /* Fill new entry data */
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

	      /* Write filename and update size */
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

  memset (data, 0, dir->vi_sb->sb_blksize);
  newent = data;
  newent->ed_inode = inode->vi_ino;
  newent->ed_size = dir->vi_sb->sb_blksize;
  newent->ed_namelenl = size & 0xff;
  if (esb->esb_reqft & EXT2_FT_REQ_DIRTYPE)
    {
      if (S_ISREG (inode->vi_mode))
	newent->ed_namelenh = EXT2_DIRTYPE_FILE;
      else if (S_ISDIR (inode->vi_mode))
	newent->ed_namelenh = EXT2_DIRTYPE_DIR;
      else if (S_ISCHR (inode->vi_mode))
	newent->ed_namelenh = EXT2_DIRTYPE_CHRDEV;
      else if (S_ISBLK (inode->vi_mode))
	newent->ed_namelenh = EXT2_DIRTYPE_BLKDEV;
      else if (S_ISFIFO (inode->vi_mode))
	newent->ed_namelenh = EXT2_DIRTYPE_FIFO;
      else if (S_ISSOCK (inode->vi_mode))
	newent->ed_namelenh = EXT2_DIRTYPE_SOCKET;
      else if (S_ISLNK (inode->vi_mode))
	newent->ed_namelenh = EXT2_DIRTYPE_LINK;
      else
	newent->ed_namelenh = EXT2_DIRTYPE_NONE;
    }
  else
    newent->ed_namelenh = (size >> 8) & 0xff;

  /* Directory inode size should always be multiple of block size */
  block = dir->vi_size / dir->vi_sb->sb_blksize;
  inode->vi_size += inode->vi_sb->sb_blksize;
  inode->vi_sectors += inode->vi_sb->sb_blksize / ATA_SECTSIZE;
  ret = ext2_extend_inode (dir, block, block + 1);
  if (ret != 0)
    {
      kfree (data);
      return ret;
    }
  ret = ext2_data_blocks (dir->vi_private, dir->vi_sb, block, 1, &realblock);
  if (ret < 0)
    {
      kfree (data);
      return ret;
    }
  ret = ext2_write_blocks (data, dir->vi_sb, realblock, 1);
  kfree (data);
  return ret;
}
