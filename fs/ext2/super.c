/*************************************************************************
 * super.c -- This file is part of OS/0.                                 *
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
#include <fs/devfs.h>
#include <fs/ext2.h>
#include <libk/libk.h>
#include <sys/ata.h>
#include <sys/process.h>
#include <sys/sysmacros.h>
#include <vm/heap.h>
#include <errno.h>

const VFSSuperblockOps ext2_sops = {
  .sb_alloc_inode = ext2_alloc_inode,
  .sb_destroy_inode = ext2_destroy_inode,
  .sb_alloc_dir = ext2_alloc_dir,
  .sb_destroy_dir = ext2_destroy_dir,
  .sb_fill_inode = ext2_fill_inode,
  .sb_write_inode = ext2_write_inode,
  .sb_free = ext2_free,
  .sb_update = ext2_update,
  .sb_statfs = ext2_statfs,
  .sb_remount = ext2_remount
};

const VFSInodeOps ext2_iops = {
  .vfs_create = ext2_create,
  .vfs_lookup = ext2_lookup,
  .vfs_link = ext2_link,
  .vfs_unlink = ext2_unlink,
  .vfs_symlink = ext2_symlink,
  .vfs_read = ext2_read,
  .vfs_write = ext2_write,
  .vfs_readdir = ext2_readdir,
  .vfs_chmod = ext2_chmod,
  .vfs_chown = ext2_chown,
  .vfs_mkdir = ext2_mkdir,
  .vfs_rmdir = ext2_rmdir,
  .vfs_mknod = ext2_mknod,
  .vfs_rename = ext2_rename,
  .vfs_readlink = ext2_readlink,
  .vfs_truncate = ext2_truncate,
  .vfs_getattr = ext2_getattr,
  .vfs_setxattr = ext2_setxattr,
  .vfs_getxattr = ext2_getxattr,
  .vfs_listxattr = ext2_listxattr,
  .vfs_removexattr = ext2_removexattr
};

const VFSDirEntryOps ext2_dops = {
  .d_compare = ext2_compare,
  .d_iput = ext2_iput
};

const VFSFilesystem ext2_vfs = {
  .vfs_name = EXT2_FS_NAME,
  .vfs_flags = 0,
  .vfs_mount = ext2_mount,
  .vfs_unmount = ext2_unmount,
  .vfs_sops = &ext2_sops,
  .vfs_iops = &ext2_iops,
  .vfs_dops = &ext2_dops
};

static int
ext2_openfs (SpecDevice *dev, VFSSuperblock *sb, Ext2Filesystem *fs)
{
  unsigned long first_meta_bg;
  unsigned long i;
  block_t group_block;
  block_t block;
  char *dest;
  size_t inosize;
  uint64_t ngroups;
  int group_zero_adjust = 0;
  int ret;
  fs->f_umask = S_IWGRP | S_IWOTH;

  /* Read and verify superblock */
  if (dev->sd_read (dev, &fs->f_super, sizeof (Ext2Superblock), 1024) < 0)
    return -EIO;
  if (!ext2_superblock_checksum_valid (fs)
      || fs->f_super.s_magic != EXT2_MAGIC
      || fs->f_super.s_rev_level > 1)
    return -EINVAL;
  sb->sb_magic = fs->f_super.s_magic;

  /* Check for unsupported features */
  if ((fs->f_super.s_feature_incompat & ~EXT2_INCOMPAT_SUPPORTED)
      || (!(sb->sb_mntflags & MS_RDONLY)
	  && (fs->f_super.s_feature_ro_compat & ~EXT2_RO_COMPAT_SUPPORTED)))
    return -ENOTSUP;
  if (fs->f_super.s_feature_incompat & EXT3_FT_INCOMPAT_JOURNAL_DEV)
    return -ENOTSUP; /* Journals are currently not supported */

  /* Check for valid block and cluster sizes */
  if (fs->f_super.s_log_block_size >
      EXT2_MAX_BLOCK_LOG_SIZE - EXT2_MIN_BLOCK_LOG_SIZE)
    return -EINVAL;
  if ((fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_BIGALLOC)
      && fs->f_super.s_log_block_size != fs->f_super.s_log_cluster_size)
    return -EINVAL;
  sb->sb_blksize = EXT2_BLOCK_SIZE (fs->f_super);

  /* Determine inode size */
  inosize = EXT2_INODE_SIZE (fs->f_super);
  if (inosize < EXT2_OLD_INODE_SIZE || inosize > sb->sb_blksize
      || (inosize & (inosize - 1)))
    return -EINVAL;

  if ((fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT)
      && fs->f_super.s_desc_size < EXT2_MIN_DESC_SIZE_64)
    return -EINVAL;

  fs->f_cluster_ratio_bits =
    fs->f_super.s_log_cluster_size - fs->f_super.s_log_block_size;
  if (fs->f_super.s_blocks_per_group !=
      fs->f_super.s_clusters_per_group << fs->f_cluster_ratio_bits)
    return -EINVAL;
  fs->f_inode_blocks_per_group = (fs->f_super.s_inodes_per_group * inosize +
				  sb->sb_blksize - 1) / sb->sb_blksize;

  /* Don't read group descriptors for journal devices */
  if (fs->f_super.s_feature_incompat & EXT3_FT_INCOMPAT_JOURNAL_DEV)
    {
      fs->f_group_desc_count = 0;
      return 0;
    }

  if (fs->f_super.s_inodes_per_group == 0)
    return -EINVAL;

  /* Initialize checksum seed */
  if (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_CSUM_SEED)
    fs->f_checksum_seed = fs->f_super.s_checksum_seed;
  else if ((fs->f_super.s_feature_ro_compat & EXT4_FT_RO_COMPAT_METADATA_CSUM)
	   && (fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_EA_INODE))
    fs->f_checksum_seed = crc32 (0xffffffff, fs->f_super.s_uuid, 16);

  /* Check for valid block count info */
  if (fs->f_super.s_blocks_per_group == 0
      || (fs->f_super.s_blocks_per_group >
	  EXT2_MAX_BLOCKS_PER_GROUP (fs->f_super))
      || (fs->f_inode_blocks_per_group >=
	  EXT2_MAX_INODES_PER_GROUP (fs->f_super))
      || EXT2_DESC_PER_BLOCK (fs->f_super) == 0
      || fs->f_super.s_first_data_block >= ext2_blocks_count (&fs->f_super))
    return -EINVAL;

  /* Determine number of block groups */
  ngroups = div64_ceil (ext2_blocks_count (&fs->f_super) -
			fs->f_super.s_first_data_block,
			fs->f_super.s_blocks_per_group);
  if (ngroups >> 32)
    return -EINVAL;
  fs->f_group_desc_count = ngroups;
  if (ngroups * fs->f_super.s_inodes_per_group != fs->f_super.s_inodes_count)
    return -EINVAL;
  fs->f_desc_blocks =
    div32_ceil (fs->f_group_desc_count, EXT2_DESC_PER_BLOCK (fs->f_super));

  /* Read block group descriptors */
  fs->f_group_desc = kmalloc (fs->f_desc_blocks * sb->sb_blksize);
  if (unlikely (fs->f_group_desc == NULL))
    return -ENOMEM;

  group_block = fs->f_super.s_first_data_block;
  if (group_block == 0 && sb->sb_blksize == 1024)
    group_zero_adjust = 1;
  dest = (char *) fs->f_group_desc;

  if (fs->f_super.s_feature_incompat & EXT2_FT_INCOMPAT_META_BG)
    {
      first_meta_bg = fs->f_super.s_first_meta_bg;
      if (first_meta_bg > fs->f_desc_blocks)
	first_meta_bg = fs->f_desc_blocks;
    }
  else
    first_meta_bg = fs->f_desc_blocks;
  if (first_meta_bg > 0)
    {
      ret = dev->sd_read (dev, dest, first_meta_bg * sb->sb_blksize,
			  (group_block + group_zero_adjust + 1) *
			  sb->sb_blksize);
      if (ret < 0)
	{
	  kfree (fs->f_group_desc);
	  return ret;
	}
      dest += sb->sb_blksize * first_meta_bg;
    }

  for (i = first_meta_bg; i < fs->f_desc_blocks; i++)
    block = ext2_descriptor_block (sb, group_block, i);
  for (i = first_meta_bg; i < fs->f_desc_blocks; i++)
    {
      block = ext2_descriptor_block (sb, group_block, i);
      ret = ext2_read_blocks (dest, sb, block, 1);
      if (ret != 0)
	{
	  kfree (fs->f_group_desc);
	  return ret;
	}
      dest += sb->sb_blksize;
    }

  fs->f_stride = fs->f_super.s_raid_stride;
  if ((fs->f_super.s_feature_incompat & EXT4_FT_INCOMPAT_MMP)
      && !(sb->sb_mntflags & MS_RDONLY))
    {
      ret = ext4_mmp_start (sb);
      if (ret != 0)
	{
	  ext4_mmp_stop (sb);
	  kfree (fs->f_group_desc);
	  return ret;
	}
    }
  return 0;
}

uint32_t
ext2_bgdt_size (Ext2Superblock *esb)
{
  uint32_t a = (esb->s_blocks_count + esb->s_blocks_per_group - 1) /
    esb->s_blocks_per_group;
  uint32_t b = (esb->s_inodes_count + esb->s_inodes_per_group - 1) /
    esb->s_inodes_per_group;
  return a > b ? a : b;
}

int
ext2_mount (VFSMount *mp, int flags, void *data)
{
  VFSInode *inode;
  SpecDevice *dev;
  Ext2Filesystem *fs;
  int ret;
  if (data == NULL)
    return -EINVAL;

  mp->vfs_sb.sb_fstype = mp->vfs_fstype;
  mp->vfs_sb.sb_ops = &ext2_sops;

  ret = vfs_open_file (&inode, data, 1);
  if (ret != 0)
    return ret;

  /* Make sure we are mounting a valid block device */
  if (strcmp (inode->vi_sb->sb_fstype->vfs_name, devfs_vfs.vfs_name) != 0
      || !S_ISBLK (inode->vi_mode))
    return -EINVAL;
  dev = device_lookup (major (inode->vi_rdev), minor (inode->vi_rdev));
  assert (dev != NULL);

  /* Fill filesystem data */
  fs = kzalloc (sizeof (Ext2Filesystem));
  if (unlikely (fs == NULL))
    return -ENOMEM;
  mp->vfs_sb.sb_dev = dev;
  mp->vfs_sb.sb_private = fs;
  ret = ext2_openfs (dev, &mp->vfs_sb, fs);
  if (ret != 0)
    {
      kfree (fs);
      return ret;
    }

  if (!(mp->vfs_sb.sb_mntflags & MS_RDONLY))
    {
      /* Update mount time and count */
      fs->f_super.s_mnt_count++;
      fs->f_super.s_mtime = time (NULL);
      fs->f_super.s_state &= ~EXT2_STATE_VALID;
      fs->f_flags |= EXT2_FLAG_CHANGED | EXT2_FLAG_DIRTY;
      ext2_flush (&mp->vfs_sb, 0);
    }

  mp->vfs_sb.sb_root = vfs_alloc_inode (&mp->vfs_sb);
  if (unlikely (mp->vfs_sb.sb_root == NULL))
    {
      kfree (fs->f_group_desc);
      kfree (fs);
      return -ENOMEM;
    }
  mp->vfs_sb.sb_root->vi_ino = EXT2_ROOT_INODE;
  ext2_fill_inode (mp->vfs_sb.sb_root);
  return 0;
}

int
ext2_unmount (VFSMount *mp, int flags)
{
  return ext2_flush (&mp->vfs_sb, 0);
}

VFSInode *
ext2_alloc_inode (VFSSuperblock *sb)
{
  VFSInode *inode = kzalloc (sizeof (VFSInode));
  if (unlikely (inode == NULL))
    return NULL;
  inode->vi_ops = &ext2_iops;
  inode->vi_sb = sb;
  return inode;
}

void
ext2_destroy_inode (VFSInode *inode)
{
  kfree (inode->vi_private);
  kfree (inode);
}

VFSDirectory *
ext2_alloc_dir (VFSInode *dir, VFSSuperblock *sb)
{
  VFSDirectory *d = kzalloc (sizeof (VFSDirectory));
  block_t realblock;

  if (unlikely (d == NULL))
    return NULL;
  d->vd_inode = dir;
  d->vd_buffer = kmalloc (sb->sb_blksize);
  if (unlikely (d->vd_buffer == NULL))
    goto err;

  if (ext2_data_blocks (dir->vi_private, sb, 0, 1, &realblock) < 0)
    goto err;
  if (ext2_read_blocks (d->vd_buffer, sb, realblock, 1) < 0)
    goto err;
  return d;

 err:
  ext2_destroy_dir (d);
  return NULL;
}

void
ext2_destroy_dir (VFSDirectory *dir)
{
  if (dir == NULL)
    return;
  kfree (dir->vd_buffer);
  kfree (dir);
}

int
ext2_fill_inode (VFSInode *inode)
{
  Ext2Filesystem *fs = inode->vi_sb->sb_private;
  Ext2Superblock *esb = &fs->f_super;
  Ext2File *file = kmalloc (sizeof (Ext2File));
  int ret;
  if (unlikely (file == NULL))
    return -ENOMEM;
  ret = ext2_open_file (inode->vi_sb, inode->vi_ino, file);
  if (ret != 0)
    return ret;
  inode->vi_uid = file->f_inode.i_uid;
  inode->vi_gid = file->f_inode.i_gid;
  inode->vi_nlink = file->f_inode.i_links_count;
  inode->vi_size = file->f_inode.i_size;
  if (S_ISREG (file->f_inode.i_mode) && esb->s_rev_level > 0
      && (esb->s_feature_ro_compat & EXT2_FT_RO_COMPAT_LARGE_FILE))
    inode->vi_size |= (off64_t) file->f_inode.i_size_high << 32;
  inode->vi_atime.tv_sec = file->f_inode.i_atime;
  inode->vi_atime.tv_nsec = 0;
  inode->vi_mtime.tv_sec = file->f_inode.i_mtime;
  inode->vi_mtime.tv_nsec = 0;
  inode->vi_ctime.tv_sec = file->f_inode.i_ctime;
  inode->vi_ctime.tv_nsec = 0;
  inode->vi_sectors = file->f_inode.i_blocks;
  inode->vi_blocks =
    file->f_inode.i_blocks * ATA_SECTSIZE / inode->vi_sb->sb_blksize;

  /* Set mode and device numbers if applicable */
  inode->vi_mode = file->f_inode.i_mode & S_IFMT;
  if (S_ISBLK (inode->vi_mode) || S_ISCHR (inode->vi_mode))
    inode->vi_rdev = *((dev_t *) file->f_inode.i_block);
  inode->vi_mode |= file->f_inode.i_mode & 07777;
  inode->vi_private = file;
  return 0;
}

int
ext2_write_inode (VFSInode *inode)
{
  Ext2Filesystem *fs = inode->vi_sb->sb_private;
  Ext2Inode *ei = inode->vi_private;

  /* Update disk inode structure */
  ei->i_uid = inode->vi_uid;
  ei->i_size = inode->vi_size & 0xffffffff;
  ei->i_atime = inode->vi_atime.tv_sec;
  ei->i_ctime = inode->vi_ctime.tv_sec;
  ei->i_mtime = inode->vi_mtime.tv_sec;
  ei->i_gid = inode->vi_gid;
  ei->i_links_count = inode->vi_nlink;
  ei->i_blocks = inode->vi_sectors;
  if (S_ISREG (inode->vi_mode) && fs->f_super.s_rev_level > 0
      && fs->f_super.s_feature_ro_compat & EXT2_FT_RO_COMPAT_LARGE_FILE)
    ei->i_size_high = inode->vi_size >> 32;

  /* Write new inode to disk */
  return ext2_update_inode (inode->vi_sb, inode->vi_ino, ei);
}

void
ext2_free (VFSSuperblock *sb)
{
  vfs_unref_inode (sb->sb_root);
  kfree (sb->sb_private);
}

void
ext2_update (VFSSuperblock *sb)
{
  ext2_flush (sb, 0);
}

int
ext2_statfs (VFSSuperblock *sb, struct statfs64 *st)
{
  Ext2Superblock *esb = (Ext2Superblock *) sb->sb_private;
  uid_t euid = process_table[task_getpid ()].p_euid;
  st->f_type = EXT2_MAGIC;
  st->f_bsize = sb->sb_blksize;
  st->f_blocks = esb->s_blocks_count;
  st->f_bfree = esb->s_free_blocks_count;
  st->f_bavail = esb->s_free_blocks_count -
    (euid != 0 && euid != esb->s_def_resuid) * esb->s_r_blocks_count;
  st->f_files = esb->s_inodes_count;
  st->f_ffree = esb->s_free_inodes_count;
  st->f_fsid.f_val[0] = sb->sb_dev->sd_major;
  st->f_fsid.f_val[1] = sb->sb_dev->sd_minor;
  st->f_namelen = EXT2_MAX_NAME_LEN;
  st->f_flags = sb->sb_mntflags;
  return 0;
}

int
ext2_remount (VFSSuperblock *sb, int *flags, void *data)
{
  return -ENOSYS;
}

int
ext2_compare (VFSDirEntry *entry, const char *a, const char *b)
{
  return strcmp (a, b);
}

void
ext2_iput (VFSDirEntry *entry, VFSInode *inode)
{
}

void
ext2_init (void)
{
  if (vfs_register (&ext2_vfs) != 0)
    panic ("Failed to register " EXT2_FS_NAME " filesystem");
}
