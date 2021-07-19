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

#include <bits/mount.h>
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

static block_t
ext2_try_alloc_block (VFSSuperblock *sb, int index)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Superblock *esb = &fs->f_super;
  SpecDevice *dev = sb->sb_dev;
  Ext2GroupDesc *bgdt = fs->f_group_desc;
  unsigned char *busage;
  int ret;
  int i;
  if (bgdt[index].bg_free_blocks_count == 0)
    return -ENOSPC; /* No free block */
  busage = kmalloc (esb->s_blocks_per_group >> 3);
  if (unlikely (busage == NULL))
    return -ENOMEM;
  ret = dev->sd_read (dev, busage, esb->s_blocks_per_group >> 3,
		      bgdt[index].bg_block_bitmap * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (busage);
      return ret;
    }
  for (i = 0; i < esb->s_blocks_per_group; i++)
    {
      uint32_t byte = i >> 3;
      uint32_t offset = i % 8;
      if (busage[byte] & 1 << offset)
	continue;

      /* Mark the block as allocated */
      busage[byte] |= 1 << offset;
      ret = dev->sd_write (dev, busage, esb->s_blocks_per_group >> 3,
			   bgdt[index].bg_block_bitmap * sb->sb_blksize);
      kfree (busage);
      if (ret != 0)
	return ret;

      /* Subtract from free blocks in superblock and BGDT */
      esb->s_free_blocks_count--;
      bgdt[index].bg_free_blocks_count--;
      ext2_update (sb);

      return (block_t) esb->s_blocks_per_group * index + i;
    }
  kfree (busage);
  return -ENOSPC;
}

static ino64_t
ext2_try_create_inode (VFSSuperblock *sb, int index)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Superblock *esb = &fs->f_super;
  SpecDevice *dev = sb->sb_dev;
  Ext2GroupDesc *bgdt = fs->f_group_desc;
  unsigned char *iusage;
  int ret;
  int i;
  if (bgdt[index].bg_free_inodes_count == 0)
    return 0; /* No free inodes */
  iusage = kmalloc (esb->s_inodes_per_group >> 3);
  if (unlikely (iusage == NULL))
    return 0;
  ret = dev->sd_read (dev, iusage, esb->s_inodes_per_group >> 3,
		      bgdt[index].bg_inode_bitmap * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (iusage);
      return ret;
    }
  for (i = 0; i < esb->s_inodes_per_group; i++)
    {
      uint32_t byte = i >> 3;
      uint32_t offset = i % 8;
      if (iusage[byte] & 1 << offset)
	continue;

      /* Mark the inode as allocated */
      iusage[byte] |= 1 << offset;
      ret = dev->sd_write (dev, iusage, esb->s_inodes_per_group >> 3,
			   bgdt[index].bg_inode_bitmap * sb->sb_blksize);
      kfree (iusage);
      if (ret != 0)
	return ret;

      /* Subtract from free inodes in superblock and BGDT */
      esb->s_free_inodes_count--;
      bgdt[index].bg_free_inodes_count--;
      ext2_update (sb);

      return (ino64_t) esb->s_inodes_per_group * index + i + 1;
    }
  kfree (iusage);
  return 0;
}

static int
ext2_unalloc_block (VFSSuperblock *sb, block_t block)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Superblock *esb = &fs->f_super;
  SpecDevice *dev = sb->sb_dev;
  Ext2GroupDesc *bgdt = fs->f_group_desc;
  unsigned char *busage;
  uint32_t blkgrp = block / esb->s_blocks_per_group;
  uint32_t rem = block % esb->s_blocks_per_group;
  uint32_t byte = rem >> 3;
  uint32_t offset = rem % 8;
  int ret;
  busage = kmalloc (esb->s_blocks_per_group >> 3);
  if (unlikely (busage == NULL))
    return 0;
  ret = dev->sd_read (dev, busage, esb->s_blocks_per_group >> 3,
		      bgdt[blkgrp].bg_block_bitmap * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (busage);
      return ret;
    }

  /* Mark the block as unallocated */
  busage[byte] &= ~(1 << offset);
  ret = dev->sd_write (dev, busage, esb->s_blocks_per_group >> 3,
		       bgdt[blkgrp].bg_block_bitmap * sb->sb_blksize);
  kfree (busage);
  if (ret != 0)
    return ret;

  /* Add to free blocks in superblock and BGDT */
  esb->s_free_blocks_count++;
  bgdt[blkgrp].bg_free_blocks_count++;
  ext2_update (sb);
  return 0;
}

static int
ext2_clear_inode (VFSSuperblock *sb, ino64_t inode)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2Superblock *esb = &fs->f_super;
  SpecDevice *dev = sb->sb_dev;
  Ext2GroupDesc *bgdt = fs->f_group_desc;
  unsigned char *iusage;
  uint32_t blkgrp = inode / esb->s_inodes_per_group;
  uint32_t rem = inode % esb->s_inodes_per_group;
  uint32_t byte = rem >> 3;
  uint32_t offset = rem % 8;
  int ret;
  iusage = kmalloc (esb->s_inodes_per_group >> 3);
  if (unlikely (iusage == NULL))
    return 0;
  ret = dev->sd_read (dev, iusage, esb->s_inodes_per_group >> 3,
		      bgdt[blkgrp].bg_inode_bitmap * sb->sb_blksize);
  if (ret != 0)
    {
      kfree (iusage);
      return ret;
    }

  /* Mark the inode as unallocated */
  iusage[byte] &= ~(1 << offset);
  ret = dev->sd_write (dev, iusage, esb->s_inodes_per_group >> 3,
		       bgdt[blkgrp].bg_inode_bitmap * sb->sb_blksize);
  kfree (iusage);
  if (ret != 0)
    return ret;

  /* Add to free inodes in superblock and BGDT */
  esb->s_free_inodes_count++;
  bgdt[blkgrp].bg_free_inodes_count++;
  ext2_update (sb);
  return 0;
}

static int
ext2_try_unalloc_pointer (VFSInode *inode, block_t block, uint32_t *data)
{
  int i;
  for (i = 0; i < inode->vi_sb->sb_blksize >> 2; i++)
    {
      if (data[i] != 0)
	return 1;
    }
  return ext2_unalloc_block (inode->vi_sb, block);
}

static int
ext2_bg_super_test_root (unsigned int group, unsigned int x)
{
  while (1)
    {
      if (group < x)
	return 0;
      if (group == x)
	return 1;
      if (group % x)
	return 0;
      group /= x;
    }
}

static void
ext2_clear_block_uninit (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (group >= fs->f_group_desc_count
      || !ext2_has_group_desc_checksum (&fs->f_super)
      || !ext2_bg_test_flags (sb, group, EXT2_BG_BLOCK_UNINIT))
    return;
  ext2_bg_clear_flags (sb, group, EXT2_BG_BLOCK_UNINIT);
  ext2_group_desc_checksum_update (sb, group);
  fs->f_flags |= EXT2_FLAG_CHANGED | EXT2_FLAG_DIRTY | EXT2_FLAG_BB_DIRTY;
}

int
ext2_bg_has_super (Ext2Filesystem *fs, unsigned int group)
{
  if (group == 0)
    return 1;
  if (fs->f_super.s_feature_ro_compat & EXT2_FT_RO_COMPAT_SPARSE_SUPER)
    return group == fs->f_super.s_backup_bgs[0]
      || group == fs->f_super.s_backup_bgs[1];
  if (group <= 1)
    return 1;
  if (!(group & 1))
    return 0;
  if (ext2_bg_super_test_root (group, 3)
      || ext2_bg_super_test_root (group, 5)
      || ext2_bg_super_test_root (group, 7))
    return 1;
  return 0;
}

int
ext2_bg_test_flags (VFSSuperblock *sb, unsigned int group, uint16_t flags)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return gdp->bg_flags & flags;
}

void
ext2_bg_clear_flags (VFSSuperblock *sb, unsigned int group, uint16_t flags)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  gdp->bg_flags &= ~flags;
}

Ext2GroupDesc *
ext2_group_desc (VFSSuperblock *sb, Ext2GroupDesc *gdp, unsigned int group)
{
  static char *buffer;
  static size_t bufsize;
  Ext2Filesystem *fs = sb->sb_private;
  block_t block;
  int desc_size = EXT2_DESC_SIZE (fs->f_super);
  int desc_per_block = EXT2_DESC_PER_BLOCK (fs->f_super);
  int ret;

  if (group > fs->f_group_desc_count)
    return NULL;
  if (gdp != NULL)
    return (Ext2GroupDesc *) ((char *) gdp + group * desc_size);

  if (bufsize < sb->sb_blksize)
    {
      kfree (buffer);
      buffer = NULL;
    }
  if (buffer == NULL)
    {
      buffer = kmalloc (sb->sb_blksize);
      if (unlikely (buffer == NULL))
	return NULL;
      bufsize = sb->sb_blksize;
    }
  block = ext2_descriptor_block (sb, fs->f_super.s_first_data_block,
				 group / desc_per_block);
  ret = ext2_read_blocks (buffer, sb, block, 1);
  if (ret != 0)
    return NULL;
  return (Ext2GroupDesc *) (buffer + group % desc_per_block * desc_size);
}

Ext4GroupDesc *
ext4_group_desc (VFSSuperblock *sb, Ext2GroupDesc *gdp, unsigned int group)
{
  return (Ext4GroupDesc *) ext2_group_desc (sb, gdp, group);
}

void
ext2_super_bgd_loc (VFSSuperblock *sb, unsigned int group, block_t *super,
		    block_t *old_desc, block_t *new_desc, blkcnt64_t *used)
{
  Ext2Filesystem *fs = sb->sb_private;
  block_t group_block;
  block_t super_block = 0;
  block_t old_desc_block = 0;
  block_t new_desc_block = 0;
  unsigned int meta_bg;
  size_t meta_bg_size;
  blkcnt64_t nblocks = 0;
  block_t old_desc_blocks;;
  int has_super;

  group_block = ext2_group_first_block (fs, group);
  if (group_block == 0 && sb->sb_blksize == 1024)
    group_block = 1;

  if (fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_META_BG)
    old_desc_blocks = fs->f_super.s_first_meta_bg;
  else
    old_desc_blocks = fs->f_desc_blocks + fs->f_super.s_reserved_gdt_blocks;

  has_super = ext2_bg_has_super (fs, group);
  if (has_super)
    {
      super_block = group_block;
      nblocks++;
    }
  meta_bg_size = EXT2_DESC_PER_BLOCK (fs->f_super);
  meta_bg = group / meta_bg_size;

  if (!(fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_META_BG)
      || meta_bg < fs->f_super.s_first_meta_bg)
    {
      if (has_super)
	{
	  old_desc_block = group_block + 1;
	  nblocks += old_desc_blocks;
	}
    }
  else
    {
      if (group % meta_bg_size == 0 || group % meta_bg_size == 1
	  || group % meta_bg_size == meta_bg_size - 1)
	{
	  new_desc_block = group_block + (has_super ? 1 : 0);
	  nblocks++;
	}
    }

  if (super != NULL)
    *super = super_block;
  if (old_desc != NULL)
    *old_desc = old_desc_block;
  if (new_desc != NULL)
    *new_desc = new_desc_block;
  if (used != NULL)
    *used = nblocks;
}

block_t
ext2_block_bitmap_loc (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return (block_t) gdp->bg_block_bitmap |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (block_t) gdp->bg_block_bitmap_hi << 32 : 0);
}

block_t
ext2_inode_bitmap_loc (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return (block_t) gdp->bg_inode_bitmap |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (block_t) gdp->bg_inode_bitmap_hi << 32 : 0);
}

block_t
ext2_inode_table_loc (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return (block_t) gdp->bg_inode_table |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (block_t) gdp->bg_inode_table_hi << 32 : 0);
}

block_t
ext2_descriptor_block (VFSSuperblock *sb, block_t group_block, unsigned int i)
{
  int has_super = 0;
  int group_zero_adjust = 0;
  int bg;
  block_t block;
  Ext2Filesystem *fs = sb->sb_private;

  if (i == 0 && sb->sb_blksize == 1024 && EXT2_CLUSTER_RATIO (fs) > 1)
    group_zero_adjust = 1;

  if (!(fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_META_BG)
      || i < fs->f_super.s_first_meta_bg)
    return group_block + group_zero_adjust + i + 1;

  bg = EXT2_DESC_PER_BLOCK (fs->f_super) * i;
  if (ext2_bg_has_super (fs, bg))
    has_super = 1;
  block = ext2_group_first_block (fs, bg);

  if (group_block != fs->f_super.s_first_data_block
      && block + has_super + fs->f_super.s_blocks_per_group <
      ext2_blocks_count (&fs->f_super))
    {
      block += fs->f_super.s_blocks_per_group;
      if (ext2_bg_has_super (fs, bg + 1))
	has_super = 1;
      else
	has_super = 0;
    }
  return block + has_super + group_zero_adjust;
}

uint32_t
ext2_bg_free_blocks_count (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return gdp->bg_free_blocks_count |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (uint32_t) gdp->bg_free_blocks_count_hi << 16 : 0);
}

void
ext2_bg_free_blocks_count_set (VFSSuperblock *sb, unsigned int group,
			       uint32_t blocks)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  gdp->bg_free_blocks_count = blocks;
  if (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT)
    gdp->bg_free_blocks_count_hi = blocks >> 16;
}

uint32_t
ext2_bg_free_inodes_count (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return gdp->bg_free_inodes_count |
    (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (uint32_t) gdp->bg_free_inodes_count_hi << 16 : 0);
}

void
ext2_bg_free_inodes_count_set (VFSSuperblock *sb, unsigned int group,
			       uint32_t inodes)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  gdp->bg_free_inodes_count = inodes;
  if (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT)
    gdp->bg_free_inodes_count_hi = inodes >> 16;
}

void
ext2_block_alloc_stats (VFSSuperblock *sb, block_t block, int inuse)
{
  Ext2Filesystem *fs = sb->sb_private;
  unsigned int group = ext2_group_of_block (fs, block);
  if (block >= ext2_blocks_count (&fs->f_super))
    return;
  if (inuse > 0)
    ext2_mark_bitmap (fs->f_block_bitmap, block);
  else
    ext2_unmark_bitmap (fs->f_block_bitmap, block);
  ext2_bg_free_blocks_count_set (sb, group,
				 ext2_bg_free_blocks_count (sb, group) - inuse);
  ext2_bg_clear_flags (sb, group, EXT2_BG_BLOCK_UNINIT);
  ext2_group_desc_checksum_update (sb, group);
  ext2_free_blocks_count_add (&fs->f_super,
			      -inuse * (blkcnt64_t) EXT2_CLUSTER_RATIO (fs));
  fs->f_flags |= EXT2_FLAG_CHANGED | EXT2_FLAG_DIRTY | EXT2_FLAG_BB_DIRTY;
}

int
ext2_open_file (VFSSuperblock *sb, ino64_t inode, Ext2File *file)
{
  int ret = ext2_read_inode (sb, inode, &file->f_inode);
  if (ret != 0)
    return ret;
  file->f_ino = inode;
  file->f_sb = sb;
  file->f_flags = 0;
  file->f_buffer = kmalloc (sb->sb_blksize * 3);
  if (unlikely (file->f_buffer == NULL))
    return -ENOMEM;
  return 0;
}

int
ext2_read_inode (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode)
{
  Ext2Filesystem *fs = sb->sb_private;
  int len = EXT2_INODE_SIZE (fs->f_super);
  size_t bufsize = sizeof (Ext2Inode);
  Ext2LargeInode *iptr;
  char *ptr;
  block_t blockno;
  unsigned int group;
  unsigned long block;
  unsigned long offset;
  unsigned int i;
  int cache_slot;
  int csum_valid;
  int clen;
  int ret;

  if (unlikely (ino == 0 || ino > fs->f_super.s_inodes_count))
    return -EINVAL;

  /* Try lookup in inode cache */
  if (fs->f_icache == NULL)
    {
      ret = ext2_create_inode_cache (sb, 4);
      if (ret != 0)
	return ret;
    }
  for (i = 0; i < fs->f_icache->ic_cache_size; i++)
    {
      if (fs->f_icache->ic_cache[i].e_ino == ino)
	{
	  memcpy (inode, fs->f_icache->ic_cache[i].e_inode,
		  bufsize > len ? len : bufsize);
	  return 0;
	}
    }

  group = (ino - 1) / fs->f_super.s_inodes_per_group;
  if (group > fs->f_group_desc_count)
    return -EINVAL;
  offset = (ino - 1) % fs->f_super.s_inodes_per_group *
    EXT2_INODE_SIZE (fs->f_super);
  block = offset >> EXT2_BLOCK_SIZE_BITS (fs->f_super);
  blockno = ext2_inode_table_loc (sb, group);
  if (blockno == 0 || blockno < fs->f_super.s_first_data_block
      || blockno + fs->f_inode_blocks_per_group - 1 >=
      ext2_blocks_count (&fs->f_super))
    return -EINVAL;
  blockno += block;
  offset &= EXT2_BLOCK_SIZE (fs->f_super) - 1;

  cache_slot = (fs->f_icache->ic_cache_last + 1) % fs->f_icache->ic_cache_size;
  iptr = (Ext2LargeInode *) fs->f_icache->ic_cache[cache_slot].e_inode;
  ptr = (char *) iptr;
  while (len != 0)
    {
      clen = len;
      if (offset + len > sb->sb_blksize)
	clen = sb->sb_blksize - offset;
      if (blockno != fs->f_icache->ic_block)
	{
	  ret = ext2_read_blocks (fs->f_icache->ic_buffer, sb, blockno, 1);
	  if (ret != 0)
	    return ret;
	  fs->f_icache->ic_block = blockno;
	}
      memcpy (ptr, fs->f_icache->ic_buffer + (unsigned int) offset, clen);
      offset = 0;
      len -= clen;
      ptr += clen;
      blockno++;
    }
  len = EXT2_INODE_SIZE (fs->f_super);

  csum_valid = ext2_inode_checksum_valid (fs, ino, iptr);
  if (csum_valid)
    {
      fs->f_icache->ic_cache_last = cache_slot;
      fs->f_icache->ic_cache[cache_slot].e_ino = ino;
    }
  memcpy (inode, iptr, bufsize > len ? len : bufsize);
  return csum_valid ? 0 : -EINVAL;
}

int
ext2_update_inode (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode)
{
  Ext2Filesystem *fs = sb->sb_private;
  int len = EXT2_INODE_SIZE (fs->f_super);
  size_t bufsize = sizeof (Ext2Inode);
  Ext2LargeInode *winode;
  block_t blockno;
  unsigned long block;
  unsigned long offset;
  unsigned int group;
  char *ptr;
  unsigned int i;
  int clen;
  int ret;
  if (sb->sb_mntflags & MNT_RDONLY)
    return -EROFS;

  winode = kmalloc (len);
  if (unlikely (winode == NULL))
    return -ENOMEM;
  if (bufsize < len)
    {
      ret = ext2_read_inode (sb, ino, (Ext2Inode *) winode);
      if (ret != 0)
	{
	  kfree (winode);
	  return ret;
	}
    }

  /* Update inode cache if necessary */
  if (fs->f_icache != NULL)
    {
      for (i = 0; i < fs->f_icache->ic_cache_size; i++)
	{
	  if (fs->f_icache->ic_cache[i].e_ino == ino)
	    {
	      memcpy (fs->f_icache->ic_cache[i].e_inode, inode,
		      bufsize > len ? len : bufsize);
	      break;
	    }
	}
    }
  else
    {
      ret = ext2_create_inode_cache (sb, 4);
      if (ret != 0)
	{
	  kfree (winode);
	  return ret;
	}
    }
  memcpy (winode, inode, bufsize > len ? len : bufsize);

  /* Update inode checksum */
  ext2_inode_checksum_update (fs, ino, winode);

  group = (ino - 1) / fs->f_super.s_inodes_per_group;
  offset = (ino - 1) % fs->f_super.s_inodes_per_group *
    EXT2_INODE_SIZE (fs->f_super);
  block = offset >> EXT2_BLOCK_SIZE_BITS (fs->f_super);
  blockno = ext2_inode_table_loc (sb, group);
  if (blockno == 0 || blockno < fs->f_super.s_first_data_block
      || blockno + fs->f_inode_blocks_per_group - 1 >=
      ext2_blocks_count (&fs->f_super))
    {
      kfree (winode);
      return -EINVAL;
    }
  blockno += block;
  offset &= EXT2_BLOCK_SIZE (fs->f_super) - 1;

  ptr = (char *) winode;
  while (len)
    {
      clen = len;
      if (offset + len > sb->sb_blksize)
	clen = sb->sb_blksize - offset;
      if (fs->f_icache->ic_block != blockno)
	{
	  ret = ext2_read_blocks (fs->f_icache->ic_buffer, sb, blockno, 1);
	  if (ret != 0)
	    {
	      kfree (winode);
	      return ret;
	    }
	  fs->f_icache->ic_block = blockno;
	}
      memcpy (fs->f_icache->ic_buffer + (unsigned int) offset, ptr, clen);
      ret = ext2_write_blocks (fs->f_icache->ic_buffer, sb, blockno, 1);
      if (ret != 0)
	{
	  kfree (winode);
	  return ret;
	}
      offset = 0;
      ptr += clen;
      len -= clen;
      blockno++;
    }
  kfree (winode);
  return 0;
}

block_t
ext2_find_inode_goal (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
		      block_t block)
{
  Ext2Filesystem *fs = sb->sb_private;
  unsigned int group;
  unsigned char log_flex;
  Ext4Extent extent;
  Ext4ExtentHandle *handle = NULL;
  int ret;

  if (inode == NULL || ext2_is_inline_symlink (inode)
      || (inode->i_flags & EXT4_INLINE_DATA_FL))
    goto noblocks;
  if (inode->i_flags & EXT4_EXTENTS_FL)
    {
      ret = ext4_extent_open (sb, ino, inode, &handle);
      if (ret != 0)
	goto noblocks;
      ret = ext4_extent_goto (handle, 0, block);
      if (ret != 0)
	goto noblocks;
      ret = ext4_extent_get (handle, EXT2_EXTENT_CURRENT, &extent);
      if (ret != 0)
	goto noblocks;
      ext4_extent_free (handle);
      return extent.e_pblk + block - extent.e_lblk;
    }

  if (inode->i_block[0] != 0)
    return inode->i_block[0];

 noblocks:
  ext4_extent_free (handle);
  log_flex = fs->f_super.s_log_groups_per_flex;
  group = ext2_group_of_inode (fs, ino);
  if (log_flex != 0)
    group = group & ~((1 << log_flex) - 1);
  return ext2_group_first_block (fs, group);
}

int
ext2_create_inode_cache (VFSSuperblock *sb, unsigned int cache_size)
{
  Ext2Filesystem *fs = sb->sb_private;
  unsigned int i;
  if (fs->f_icache != NULL)
    return 0;

  fs->f_icache = kzalloc (sizeof (Ext2InodeCache));
  if (unlikely (fs->f_icache == NULL))
    return -ENOMEM;
  fs->f_icache->ic_buffer = kmalloc (sb->sb_blksize);
  if (unlikely (fs->f_icache->ic_buffer == NULL))
    goto err;

  fs->f_icache->ic_block = 0;
  fs->f_icache->ic_cache_last = -1;
  fs->f_icache->ic_cache_size = cache_size;
  fs->f_icache->ic_refcnt = 1;

  fs->f_icache->ic_cache = kmalloc (sizeof (Ext2InodeCacheEntry) * cache_size);
  if (unlikely (fs->f_icache->ic_cache == NULL))
    goto err;

  for (i = 0; i < cache_size; i++)
    {
      fs->f_icache->ic_cache[i].e_inode =
	kmalloc (EXT2_INODE_SIZE (fs->f_super));
      if (unlikely (fs->f_icache->ic_cache[i].e_inode == NULL))
	goto err;
    }
  ext2_flush_inode_cache (fs->f_icache);
  return 0;

 err:
  ext2_free_inode_cache (fs->f_icache);
  fs->f_icache = NULL;
  return -ENOMEM;
}

void
ext2_free_inode_cache (Ext2InodeCache *cache)
{
  unsigned int i;
  if (--cache->ic_refcnt > 0)
    return;
  if (cache->ic_buffer != NULL)
    kfree (cache->ic_buffer);
  for (i = 0; i < cache->ic_cache_size; i++)
    kfree (cache->ic_cache[i].e_inode);
  if (cache->ic_cache != NULL)
    kfree (cache->ic_cache);
  cache->ic_block = 0;
  kfree (cache);
}

int
ext2_flush_inode_cache (Ext2InodeCache *cache)
{
  unsigned int i;
  if (cache == NULL)
    return 0;
  for (i = 0; i < cache->ic_cache_size; i++)
    cache->ic_cache[i].e_ino = 0;
  cache->ic_block = 0;
  return 0;
}

int
ext2_file_flush (Ext2File *file)
{
  block_t ignore;
  int retflags;
  int ret;
  if (!(file->f_flags & (EXT2_FILE_BUFFER_VALID | EXT2_FILE_BUFFER_DIRTY)))
    return 0;
  if (file->f_physblock && (file->f_inode.i_flags & EXT4_EXTENTS_FL))
    {
      ret = ext2_bmap (file->f_sb, file->f_ino, &file->f_inode,
		       file->f_buffer + file->f_sb->sb_blksize, 0,
		       file->f_block, &retflags, &ignore);
      if (ret != 0)
	return ret;
      if (retflags & BMAP_RET_UNINIT)
	{
	  ret = ext2_bmap (file->f_sb, file->f_ino, &file->f_inode,
			   file->f_buffer + file->f_sb->sb_blksize, BMAP_SET,
			   file->f_block, 0, &file->f_physblock);
	  if (ret != 0)
	    return ret;
	}
    }

  if (file->f_physblock == 0)
    {
      ret = ext2_bmap (file->f_sb, file->f_ino, &file->f_inode,
		       file->f_buffer + file->f_sb->sb_blksize,
		       file->f_ino == 0 ? 0 : BMAP_ALLOC, file->f_block, 0,
		       &file->f_physblock);
      if (ret != 0)
	return ret;
    }

  ret = ext2_write_blocks (file->f_buffer, file->f_sb, file->f_physblock, 1);
  if (ret != 0)
    return ret;
  file->f_flags &= ~EXT2_FILE_BUFFER_DIRTY;
  return ret;
}

int
ext2_sync_file_buffer_pos (Ext2File *file)
{
  block_t block = file->f_pos / file->f_sb->sb_blksize;
  int ret;
  if (block != file->f_block)
    {
      ret = ext2_file_flush (file);
      if (ret != 0)
	return ret;
      file->f_flags &= ~EXT2_FILE_BUFFER_VALID;
    }
  file->f_block = block;
  return 0;
}

int
ext2_load_file_buffer (Ext2File *file, int nofill)
{
  int retflags;
  int ret;
  if (!(file->f_flags & EXT2_FILE_BUFFER_VALID))
    {
      ret = ext2_bmap (file->f_sb, file->f_ino, &file->f_inode,
		       file->f_buffer + file->f_sb->sb_blksize, 0,
		       file->f_block, &retflags, &file->f_physblock);
      if (ret != 0)
	return ret;
      if (!nofill)
	{
	  if (file->f_physblock && !(retflags & BMAP_RET_UNINIT))
	    {
	      ret = ext2_read_blocks (file->f_buffer, file->f_sb,
				      file->f_physblock, 1);
	      if (ret != 0)
		return ret;
	    }
	  else
	    memset (file->f_buffer, 0, file->f_sb->sb_blksize);
	}
      file->f_flags |= EXT2_FILE_BUFFER_VALID;
    }
  return 0;
}

int
ext2_iblk_add_blocks (VFSSuperblock *sb, Ext2Inode *inode, block_t nblocks)
{
  Ext2Filesystem *fs = sb->sb_private;
  blkcnt64_t b = inode->i_blocks;
  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
    b += (blkcnt64_t) inode->osd2.linux2.l_i_blocks_hi << 32;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
      || !(inode->i_flags & EXT4_HUGE_FILE_FL))
    nblocks *= sb->sb_blksize / 512;
  nblocks *= EXT2_CLUSTER_RATIO (fs);
  b += nblocks;
  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_HUGE_FILE)
    inode->osd2.linux2.l_i_blocks_hi = b >> 32;
  else if (b > 0xffffffff)
    return -EOVERFLOW;
  inode->i_blocks = b & 0xffffffff;
  return 0;
}

int
ext2_zero_blocks (VFSSuperblock *sb, block_t block, int num, block_t *result,
		  int *count)
{
  static void *buffer;
  static int stride_len;
  int c;
  int i;
  int ret;
  if (sb->sb_private == NULL)
    {
      if (buffer != NULL)
	{
	  kfree (buffer);
	  buffer = NULL;
	  stride_len = 0;
	}
      return 0;
    }
  if (num <= 0)
    return 0;

  if (num > stride_len && stride_len < EXT2_MAX_STRIDE_LENGTH (sb))
    {
      void *ptr;
      int new_stride = num;
      if (new_stride > EXT2_MAX_STRIDE_LENGTH (sb))
	new_stride = EXT2_MAX_STRIDE_LENGTH (sb);
      ptr = kmalloc (sb->sb_blksize * new_stride);
      if (unlikely (ptr == NULL))
	return -ENOMEM;
      kfree (buffer);
      buffer = ptr;
      stride_len = new_stride;
      memset (buffer, 0, sb->sb_blksize * stride_len);
    }

  i = 0;
  while (i < num)
    {
      if (block % stride_len)
	{
	  c = stride_len - block % stride_len;
	  if (c > num - i)
	    c = num - i;
	}
      else
	{
	  c = num - i;
	  if (c > stride_len)
	    c = stride_len;
	}
      ret = ext2_write_blocks (buffer, sb, block, c);
      if (ret != 0)
	{
	  if (count != NULL)
	    *count = c;
	  if (result != NULL)
	    *result = block;
	  return ret;
	}
      i += c;
      block += c;
    }
  return 0;
}

int
ext2_new_block (VFSSuperblock *sb, block_t goal, Ext2Bitmap *map,
		block_t *result, Ext2BlockAllocContext *ctx)
{
  Ext2Filesystem *fs = sb->sb_private;
  block_t b = 0;
  int ret;
  if (map == NULL)
    map = fs->f_block_bitmap;
  if (map == NULL)
    return -EINVAL;

  if (goal == 0 || goal >= ext2_blocks_count (&fs->f_super))
    goal &= ~EXT2_CLUSTER_MASK (fs);

  ret = ext2_find_first_zero_bitmap (map, goal,
				     ext2_blocks_count (&fs->f_super) - 1, &b);
  if (ret == -ENOENT && goal != fs->f_super.s_first_data_block)
    ret = ext2_find_first_zero_bitmap (map, fs->f_super.s_first_data_block,
				       goal - 1, &b);
  if (ret == -ENOENT)
    return -ENOENT;
  if (ret != 0)
    return ret;
  ext2_clear_block_uninit (sb, ext2_group_of_block (fs, b));
  *result = b;
  return 0;
}

int
ext2_alloc_block (VFSSuperblock *sb, block_t goal, char *blockbuf,
		  block_t *result, Ext2BlockAllocContext *ctx)
{
  Ext2Filesystem *fs = sb->sb_private;
  block_t block;
  int ret;
  if (fs->f_block_bitmap == NULL)
    {
      ret =
	ext2_read_bitmap (sb, EXT2_BITMAP_BLOCK, 0, fs->f_group_desc_count - 1);
      if (ret != 0)
	return ret;
      ret = ext2_new_block (sb, goal, NULL, &block, ctx);
      if (ret != 0)
	return ret;
    }
  if (blockbuf != NULL)
    {
      memset (blockbuf, 0, sb->sb_blksize);
      ret = ext2_write_blocks (blockbuf, sb, block, 1);
    }
  else
    ret = ext2_zero_blocks (sb, block, 1, NULL, NULL);
  if (ret != 0)
    return ret;
  ext2_block_alloc_stats (sb, block, 1);
  *result = block;
  return ret;
}

int
ext2_extend_inode (VFSInode *inode, blkcnt64_t origblocks, blkcnt64_t newblocks)
{
  Ext2Superblock *esb = &((Ext2Filesystem *) inode->vi_sb->sb_private)->f_super;
  Ext2Inode *ei = inode->vi_private;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  blkcnt64_t i;
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  int ret;

  for (i = origblocks; i < MIN (newblocks, EXT2_STORED_INODES); i++)
    {
      block_t block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
      if (block < 0)
	return block;
      ei->i_block[i - 1] = block;
    }

  /* If the loop above was enough to allocate all needed blocks, finish */
  if (newblocks <= EXT2_STORED_INODES)
    {
      ext2_write_inode (inode);
      return 0;
    }

  /* Fill indirect block pointer */
  if (ei->i_block[12] == 0)
    {
      block_t block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
      if (block < 0)
	return block;
      ei->i_block[12] = block;
    }
  bptr1 = kmalloc (blksize);
  if (unlikely (bptr1 == NULL))
    goto err;
  ret = ext2_read_blocks (bptr1, inode->vi_sb, ei->i_block[12], 1);
  if (ret != 0)
    goto err;
  for (i = origblocks; i < MIN (newblocks, blksize + EXT2_STORED_INODES); i++)
    {
      block_t block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
      if (block < 0)
	{
	  ret = block;
	  goto err;
	}
      bptr1[i - EXT2_STORED_INODES] = block;
    }
  ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->i_block[12], 1);
  if (ret != 0)
    goto err;

  /* Fill doubly indirect block pointer */
  if (ei->i_block[13] == 0)
    {
      block_t block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
      if (block < 0)
	return block;
      ei->i_block[13] = block;
    }
  bptr2 = kmalloc (blksize);
  if (unlikely (bptr2 == NULL))
    goto err;
  ret = ext2_read_blocks (bptr2, inode->vi_sb, ei->i_block[13], 1);
  if (ret != 0)
    goto err;
  for (i = 0; i < blksize; i++)
    {
      block_t j;
      ret = ext2_read_blocks (bptr1, inode->vi_sb, bptr2[i], 1);
      if (ret != 0)
	goto err;
      for (j = 0; j < blksize; j++)
	{
	  block_t block;
	  if (origblocks >= i * blksize + j + EXT2_STORED_INODES)
	    continue;
	  if (newblocks < i * blksize + j + EXT2_STORED_INODES)
	    goto err; /* Done allocating, return */
	  block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					esb->s_inodes_per_group);
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
  ret = ext2_write_blocks (bptr2, inode->vi_sb, ei->i_block[13], 1);
  if (ret != 0)
    goto err;

  /* Fill triply indirect block pointer */
  if (ei->i_block[14] == 0)
    {
      block_t block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
      if (block < 0)
	return block;
      ei->i_block[14] = block;
    }
  bptr3 = kmalloc (blksize);
  if (unlikely (bptr3 == NULL))
    goto err;
  ret = ext2_read_blocks (bptr3, inode->vi_sb, ei->i_block[14], 1);
  if (ret != 0)
    goto err;
  for (i = 0; i < blksize; i++)
    {
      block_t j;
      ret = ext2_read_blocks (bptr2, inode->vi_sb, bptr3[i], 1);
      if (ret != 0)
	goto err;
      for (j = 0; j < blksize; j++)
	{
	  block_t k;
	  ret = ext2_read_blocks (bptr1, inode->vi_sb, bptr2[j], 1);
	  if (ret != 0)
	    goto err;
	  for (k = 0; k < blksize; k++)
	    {
	      block_t block;
	      if (origblocks >=
		  i * blksize * blksize + j * blksize + k + EXT2_STORED_INODES)
		continue;
	      if (newblocks <
		  i * blksize * blksize + j * blksize + k + EXT2_STORED_INODES)
		goto err; /* Done allocating, return */
	      block = ext2_old_alloc_block (inode->vi_sb, inode->vi_ino /
					    esb->s_inodes_per_group);
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
  ret = ext2_write_blocks (bptr3, inode->vi_sb, ei->i_block[14], 1);
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
    ((Ext2Filesystem *) sb->sb_private)->f_super.s_wtime = time (NULL);
  return ret;
}

int
ext2_data_blocks (Ext2Inode *inode, VFSSuperblock *sb, block_t block,
		  blkcnt64_t nblocks, block_t *result)
{
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  block_t realblock;
  blksize_t blksize = sb->sb_blksize;
  int ret = 0;
  int i = 0;

  /* First 12 data blocks stored directly in inode */
  while (block < EXT2_STORED_INODES)
    {
      if (i == nblocks)
	return 0;
      result[i++] = inode->i_block[block++];
    }

  /* Read indirect block pointer */
  bptr1 = kmalloc (blksize);
  if (block < EXT2_INDIR1_THRESH)
    {
      if (ext2_read_blocks (bptr1, sb, inode->i_block[12], 1) != 0)
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
      if (ext2_read_blocks (bptr1, sb, inode->i_block[13], 1) != 0)
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
  if (ext2_read_blocks (bptr1, sb, inode->i_block[14], 1) != 0)
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
ext2_unalloc_data_blocks (VFSInode *inode, block_t start, blkcnt64_t nblocks)
{
  Ext2Inode *ei = inode->vi_private;
  blksize_t blksize = inode->vi_sb->sb_blksize;
  uint32_t *bptr1 = NULL;
  uint32_t *bptr2 = NULL;
  uint32_t *bptr3 = NULL;
  blkcnt64_t i;
  int ret = 0;

  for (i = 0; i < nblocks; i++)
    {
      block_t block = start + i;
      block_t realblock;

      /* First 12 data blocks stored directly in inode */
      if (block < EXT2_STORED_INODES)
        {
	  ret = ext2_unalloc_block (inode->vi_sb, ei->i_block[block]);
	  if (ret != 0)
	    goto err;
	  continue;
	}

      /* Read indirect block pointer */
      if (bptr1 == NULL)
	bptr1 = kmalloc (blksize);
      if (block < blksize + EXT2_STORED_INODES)
	{
	  if (ext2_read_blocks (bptr1, inode->vi_sb, ei->i_block[12], 1) != 0)
	    {
	      ret = -EIO;
	      goto err;
	    }
	  ret = ext2_unalloc_block (inode->vi_sb,
				    bptr1[block - EXT2_STORED_INODES]);
	  bptr1[block - EXT2_STORED_INODES] = 0;
	  if (ret != 0)
	    goto err;
	  ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->i_block[12], 1);
	  if (ret != 0)
	    goto err;
	  EXT2_TRY_UNALLOC (inode, ei->i_block[12], bptr1);
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
	  if (ext2_read_blocks (bptr1, inode->vi_sb, ei->i_block[13], 1) != 0)
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
	      ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->i_block[13], 1);
	      if (ret != 0)
		goto err;
	    }
	  EXT2_TRY_UNALLOC (inode, ei->i_block[12], bptr1);
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
	  if (ext2_read_blocks (bptr1, inode->vi_sb, ei->i_block[14], 1) != 0)
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
	      ret = ext2_write_blocks (bptr1, inode->vi_sb, ei->i_block[14], 1);
	      if (ret != 0)
		goto err;
	    }
	  EXT2_TRY_UNALLOC (inode, ei->i_block[14], bptr1);
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

int
ext2_unref_inode (VFSSuperblock *sb, VFSInode *inode)
{
  Ext2Inode *ei = inode->vi_private;
  switch (ei->i_links_count)
    {
    case 0:
      return 0;
    case 1:
      ei->i_dtime = time (NULL);
      return ext2_clear_inode (sb, inode->vi_ino);
    default:
      ei->i_links_count--;
      ext2_write_inode (inode);
      return 0;
    }
}

block_t
ext2_old_alloc_block (VFSSuperblock *sb, int prefbg)
{
  int i;
  if (prefbg >= 0)
    {
      block_t block = ext2_try_alloc_block (sb, prefbg);
      if (block == -ENOMEM)
	return -ENOMEM;
      else if (block >= 0)
	return block;
    }
  for (i = 0; i < ext2_bgdt_size (sb->sb_private); i++)
    {
      block_t block;
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
  Ext2Superblock *esb = &((Ext2Filesystem *) dir->vi_sb->sb_private)->f_super;
  Ext2DirEntry *newent;
  VFSDirectory *d;
  VFSDirEntry *entry;
  void *data;
  size_t size;
  block_t block;
  block_t realblock;
  block_t ret;

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

  for (block = 0; block * dir->vi_sb->sb_blksize < dir->vi_size;
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
	  if ((esb->s_feature_incompat & EXT2_FT_INCOMPAT_FILETYPE) == 0)
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
	      if (esb->s_feature_incompat & EXT2_FT_INCOMPAT_FILETYPE)
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
  if (esb->s_feature_incompat & EXT2_FT_INCOMPAT_FILETYPE)
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

int
ext2_superblock_checksum_valid (Ext2Filesystem *fs)
{
  uint32_t checksum;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;
  if (fs->f_super.s_checksum_type != EXT2_CRC32C_CHECKSUM)
    return 0;
  checksum =
    crc32 (0xffffffff, &fs->f_super, offsetof (Ext2Superblock, s_checksum));
  return checksum == fs->f_super.s_checksum;
}

uint16_t
ext2_bg_checksum (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  return gdp->bg_checksum;
}

void
ext2_bg_checksum_update (VFSSuperblock *sb, unsigned int group,
			 uint16_t checksum)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  gdp->bg_checksum = checksum;
}

uint16_t
ext2_group_desc_checksum (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext2GroupDesc *desc = ext2_group_desc (sb, fs->f_group_desc, group);
  size_t size = EXT2_DESC_SIZE (fs->f_super);
  size_t offset;
  uint16_t crc;
  if (fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM)
    {
      uint16_t old_crc = desc->bg_checksum;
      uint32_t c32;
      desc->bg_checksum = 0;
      c32 = crc32 (fs->f_checksum_seed, &group, sizeof (unsigned int));
      c32 = crc32 (c32, desc, size);
      desc->bg_checksum = old_crc;
      crc = c32 & 0xffff;
    }
  else
    {
      offset = offsetof (Ext2GroupDesc, bg_checksum);
      crc = crc16 (0xffff, fs->f_super.s_uuid, 16);
      crc = crc16 (crc, &group, sizeof (unsigned int));
      crc = crc16 (crc, desc, offset);
      offset += sizeof (desc->bg_checksum);
      if (offset < size)
	crc = crc16 (crc, (char *) desc + offset, size - offset);
    }
  return crc;
}

int
ext2_group_desc_checksum_valid (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (ext2_has_group_desc_checksum (&fs->f_super)
      && ext2_bg_checksum (sb, group) != ext2_group_desc_checksum (sb, group))
    return 0;
  return 1;
}

void
ext2_group_desc_checksum_update (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  if (!ext2_has_group_desc_checksum (&fs->f_super))
    return;
  ext2_bg_checksum_update (sb, group, ext2_group_desc_checksum (sb, group));
}

uint32_t
ext2_inode_checksum (Ext2Filesystem *fs, ino64_t ino, Ext2LargeInode *inode,
		     int has_hi)
{
  uint32_t gen;
  size_t size = EXT2_INODE_SIZE (fs->f_super);
  uint16_t old_lo;
  uint16_t old_hi = 0;
  uint32_t crc;

  /* Set checksum fields to zero */
  old_lo = inode->i_checksum_lo;
  inode->i_checksum_lo = 0;
  if (has_hi)
    {
      old_hi = inode->i_checksum_hi;
      inode->i_checksum_hi = 0;
    }

  /* Calculate checksum */
  gen = inode->i_generation;
  crc = crc32 (fs->f_checksum_seed, (unsigned char *) &ino, sizeof (ino64_t));
  crc = crc32 (crc, (unsigned char *) &gen, 4);
  crc = crc32 (crc, (unsigned char *) inode, size);

  /* Restore inode checksum fields */
  inode->i_checksum_lo = old_lo;
  if (has_hi)
    inode->i_checksum_hi = old_hi;
  return crc;
}

int
ext2_inode_checksum_valid (Ext2Filesystem *fs, ino64_t ino,
			   Ext2LargeInode *inode)
{
  uint32_t crc;
  uint32_t provided;
  unsigned int i;
  unsigned int has_hi;
  char *ptr;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;

  has_hi = EXT2_INODE_SIZE (fs->f_super) > EXT2_OLD_INODE_SIZE
    && inode->i_extra_isize >= EXT4_INODE_CSUM_HI_EXTRA_END;
  provided = inode->i_checksum_lo;
  crc = ext2_inode_checksum (fs, ino, inode, has_hi);

  if (has_hi)
    provided |= inode->i_checksum_hi << 16;
  else
    crc &= 0xffff;
  if (provided == crc)
    return 1;

  /* Check if inode is all zeros */
  for (ptr = (char *) inode, i = 0; i < sizeof (Ext2Inode); ptr++, i++)
    {
      if (*ptr != '\0')
	return 0;
    }
  return 1;
}

void
ext2_inode_checksum_update (Ext2Filesystem *fs, ino64_t ino,
			    Ext2LargeInode *inode)
{
  uint32_t crc;
  int has_hi;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return;

  has_hi = EXT2_INODE_SIZE (fs->f_super) > EXT2_OLD_INODE_SIZE
    && inode->i_extra_isize >= EXT4_INODE_CSUM_HI_EXTRA_END;
  crc = ext2_inode_checksum (fs, ino, inode, has_hi);

  inode->i_checksum_lo = crc & 0xffff;
  if (has_hi)
    inode->i_checksum_hi = crc >> 16;
}

uint32_t
ext2_block_bitmap_checksum (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t checksum = gdp->bg_block_bitmap_csum_lo;
  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_BLOCK_BITMAP_CSUM_HI_END)
    checksum |= (uint32_t) gdp->bg_block_bitmap_csum_hi << 16;
  return checksum;
}

int
ext2_block_bitmap_checksum_valid (VFSSuperblock *sb, unsigned int group,
				  char *bitmap, int size)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t provided;
  uint32_t crc;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;

  provided = gdp->bg_block_bitmap_csum_lo;
  crc = crc32 (fs->f_checksum_seed, bitmap, size);

  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_BLOCK_BITMAP_CSUM_HI_END)
    provided |= (uint32_t) gdp->bg_block_bitmap_csum_hi << 16;
  else
    crc &= 0xffff;
  return provided == crc;
}

void
ext2_block_bitmap_checksum_update (VFSSuperblock *sb, unsigned int group,
				   char *bitmap, int size)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t crc;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return;

  crc = crc32 (fs->f_checksum_seed, bitmap, size);
  gdp->bg_block_bitmap_csum_lo = crc & 0xffff;
  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_BLOCK_BITMAP_CSUM_HI_END)
    gdp->bg_block_bitmap_csum_hi = crc >> 16;
}

uint32_t
ext2_inode_bitmap_checksum (VFSSuperblock *sb, unsigned int group)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t checksum = gdp->bg_inode_bitmap_csum_lo;
  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_INODE_BITMAP_CSUM_HI_END)
    checksum |= (uint32_t) gdp->bg_inode_bitmap_csum_hi << 16;
  return checksum;
}

int
ext2_inode_bitmap_checksum_valid (VFSSuperblock *sb, unsigned int group,
				  char *bitmap, int size)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t provided;
  uint32_t crc;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return 1;

  provided = gdp->bg_inode_bitmap_csum_lo;
  crc = crc32 (fs->f_checksum_seed, bitmap, size);

  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_INODE_BITMAP_CSUM_HI_END)
    provided |= (uint32_t) gdp->bg_inode_bitmap_csum_hi << 16;
  else
    crc &= 0xffff;
  return provided == crc;
}

void
ext2_inode_bitmap_checksum_update (VFSSuperblock *sb, unsigned int group,
				   char *bitmap, int size)
{
  Ext2Filesystem *fs = sb->sb_private;
  Ext4GroupDesc *gdp = ext4_group_desc (sb, fs->f_group_desc, group);
  uint32_t crc;
  if (!(fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM))
    return;

  crc = crc32 (fs->f_checksum_seed, bitmap, size);
  gdp->bg_inode_bitmap_csum_lo = crc & 0xffff;
  if (EXT2_DESC_SIZE (fs->f_super) >= EXT4_BG_INODE_BITMAP_CSUM_HI_END)
    gdp->bg_inode_bitmap_csum_hi = crc >> 16;
}
