/*************************************************************************
 * ext2.c -- This file is part of OS/0.                                  *
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
ext2_init_bgdt (VFSSuperblock *sb, SpecDevice *dev)
{
  Ext2Superblock *esb = (Ext2Superblock *) sb->sb_private;
  size_t size = ext2_bgdt_size (esb);
  void *ptr;
  int ret;
  ptr = kmalloc (sizeof (Ext2Superblock) + sizeof (Ext2BGD) * size);
  if (unlikely (ptr == NULL))
    return -ENOMEM;
  memcpy (ptr, esb, sizeof (Ext2Superblock));
  sb->sb_private = ptr;
  ret = dev->sd_read (dev, sb->sb_private + sizeof (Ext2Superblock),
		      sizeof (Ext2BGD) * size,
		      sb->sb_blksize >= 4096 ? sb->sb_blksize : 2048);
  if (ret != 0)
    {
      kfree (esb);
      return ret;
    }
  return 0;
}

static int
ext2_init_disk (VFSMount *mp, int flags, const char *path)
{
  VFSInode *inode;
  SpecDevice *dev;
  Ext2Superblock *esb;
  int ret = vfs_open_file (&inode, path, 1);
  if (ret != 0)
    return ret;

  /* Make sure we are mounting a valid block device */
  if (strcmp (inode->vi_sb->sb_fstype->vfs_name, devfs_vfs.vfs_name) != 0
      || !S_ISBLK (inode->vi_mode))
    return -EINVAL;
  dev = device_lookup (major (inode->vi_rdev), minor (inode->vi_rdev));
  assert (dev != NULL);

  esb = kmalloc (sizeof (Ext2Superblock));
  if (unlikely (esb == NULL))
    return -ENOMEM;

  ret = dev->sd_read (dev, esb, sizeof (Ext2Superblock), 1024);
  if (ret != 0)
    {
      kfree (esb);
      return ret;
    }

  /* Check ext2 magic number */
  if (esb->s_magic != EXT2_MAGIC)
    {
      printk ("fs-ext2: bad magic number 0x%x in ext2 filesystem",
	      esb->s_magic);
      kfree (esb);
      return -EINVAL;
    }

  /* Fill VFS superblock */
  mp->vfs_sb.sb_dev = dev;
  mp->vfs_sb.sb_blksize = 1 << (esb->s_log_block_size + 10);
  mp->vfs_sb.sb_mntflags = flags;
  mp->vfs_sb.sb_magic = esb->s_magic;

  esb->s_mnt_count++;
  esb->s_mtime = time (NULL);

  /* Initialize block group descriptor table */
  mp->vfs_sb.sb_private = esb;
  ret = ext2_init_bgdt (&mp->vfs_sb, dev);
  if (ret != 0)
    {
      kfree (mp->vfs_sb.sb_private);
      return ret;
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
  int ret;
  if (data == NULL)
    return -EINVAL;

  mp->vfs_sb.sb_fstype = mp->vfs_fstype;
  mp->vfs_sb.sb_ops = &ext2_sops;

  ret = ext2_init_disk (mp, flags, data);
  if (ret != 0)
    return ret;

  mp->vfs_sb.sb_root = vfs_alloc_inode (&mp->vfs_sb);
  if (unlikely (mp->vfs_sb.sb_root == NULL))
    return -ENOMEM;
  mp->vfs_sb.sb_root->vi_ino = EXT2_ROOT_INODE;
  ext2_fill_inode (mp->vfs_sb.sb_root);
  return 0;
}

int
ext2_unmount (VFSMount *mp, int flags)
{
  return 0;
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
  off64_t realblock;

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

void
ext2_fill_inode (VFSInode *inode)
{
  Ext2Superblock *esb = (Ext2Superblock *) inode->vi_sb->sb_private;
  Ext2Inode *ei = ext2_read_inode (inode->vi_sb, inode->vi_ino);
  if (ei == NULL)
    return;
  inode->vi_uid = ei->i_uid;
  inode->vi_gid = ei->i_gid;
  inode->vi_nlink = ei->i_links_count;
  inode->vi_size = ei->i_size;
  if ((ei->i_mode & 0xf000) == EXT2_TYPE_FILE && esb->s_rev_level > 0
      && esb->s_feature_ro_compat & EXT2_FT_RO_COMPAT_LARGE_FILE)
    inode->vi_size |= (off64_t) ei->i_size_high << 32;
  inode->vi_atime.tv_sec = ei->i_atime;
  inode->vi_atime.tv_nsec = 0;
  inode->vi_mtime.tv_sec = ei->i_mtime;
  inode->vi_mtime.tv_nsec = 0;
  inode->vi_ctime.tv_sec = ei->i_ctime;
  inode->vi_ctime.tv_nsec = 0;
  inode->vi_sectors = ei->i_blocks;
  inode->vi_blocks = ei->i_blocks * ATA_SECTSIZE / inode->vi_sb->sb_blksize;

  /* Set mode and device numbers if applicable */
  switch (ei->i_mode & 0xf000)
    {
    case EXT2_TYPE_FIFO:
      inode->vi_mode = S_IFIFO;
      inode->vi_rdev = 0;
      break;
    case EXT2_TYPE_CHRDEV:
      inode->vi_mode = S_IFCHR;
      inode->vi_rdev = *((dev_t *) ei->i_block);
      break;
    case EXT2_TYPE_DIR:
      inode->vi_mode = S_IFDIR;
      inode->vi_rdev = 0;
      break;
    case EXT2_TYPE_BLKDEV:
      inode->vi_mode = S_IFBLK;
      inode->vi_rdev = *((dev_t *) ei->i_block);
      break;
    case EXT2_TYPE_FILE:
      inode->vi_mode = S_IFREG;
      inode->vi_rdev = 0;
      break;
    case EXT2_TYPE_LINK:
      inode->vi_mode = S_IFLNK;
      inode->vi_rdev = 0;
      break;
    case EXT2_TYPE_SOCKET:
      inode->vi_mode = S_IFSOCK;
      inode->vi_rdev = 0;
      break;
    default:
      /* Should not happen */
      inode->vi_mode = 0;
      inode->vi_rdev = 0;
    }
  inode->vi_mode |= ei->i_mode & 07777;
  inode->vi_private = ei;
}

void
ext2_write_inode (VFSInode *inode)
{
  Ext2Superblock *esb = inode->vi_sb->sb_private;
  Ext2Inode *ei = inode->vi_private;
  SpecDevice *dev = inode->vi_sb->sb_dev;
  uint32_t offset = ext2_inode_offset (inode->vi_sb, inode->vi_ino);
  ei->i_uid = inode->vi_uid;
  ei->i_size = inode->vi_size & 0xffffffff;
  ei->i_atime = inode->vi_atime.tv_sec;
  ei->i_ctime = inode->vi_ctime.tv_sec;
  ei->i_mtime = inode->vi_mtime.tv_sec;
  ei->i_gid = inode->vi_gid;
  ei->i_links_count = inode->vi_nlink;
  ei->i_blocks = inode->vi_sectors;
  if (S_ISREG (inode->vi_mode) && esb->s_rev_level > 0
      && esb->s_feature_ro_compat & EXT2_FT_RO_COMPAT_LARGE_FILE)
    ei->i_size_high = inode->vi_size >> 32;
  dev->sd_write (dev, ei, sizeof (Ext2Inode), offset);
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
  Ext2Superblock *esb = sb->sb_private;
  SpecDevice *dev = sb->sb_dev;

  /* Update superblock */
  if (dev->sd_write (dev, esb, sizeof (Ext2Superblock), 1024) != 0)
    return;

  /* Update block group descriptor table */
  dev->sd_write (dev, sb->sb_private + sizeof (Ext2Superblock),
		 sizeof (Ext2BGD) * ext2_bgdt_size (esb),
		 sb->sb_blksize >= 4096 ? sb->sb_blksize : 2048);
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
