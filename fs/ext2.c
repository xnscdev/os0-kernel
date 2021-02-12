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

#include <fs/ext2.h>
#include <libk/libk.h>
#include <vm/heap.h>
#include <errno.h>

const VFSSuperblockOps ext2_sops = {
  .sb_alloc_inode = ext2_alloc_inode,
  .sb_destroy_inode = ext2_destroy_inode,
  .sb_fill_inode = ext2_fill_inode,
  .sb_write_inode = ext2_write_inode,
  .sb_delete_inode = ext2_delete_inode,
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
  .vfs_readdir = ext2_readdir,
  .vfs_mkdir = ext2_mkdir,
  .vfs_rmdir = ext2_rmdir,
  .vfs_mknod = ext2_mknod,
  .vfs_rename = ext2_rename,
  .vfs_readlink = ext2_readlink,
  .vfs_truncate = ext2_truncate,
  .vfs_permission = ext2_permission,
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
  .vfs_name = "ext2",
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
  uint32_t a = (esb->esb_blocks + esb->esb_bpg - 1) / esb->esb_bpg;
  uint32_t b = (esb->esb_inodes + esb->esb_ipg - 1) / esb->esb_ipg;
  uint32_t size = a > b ? a : b;
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
ext2_init_disk (VFSMount *mp, int flags, const char *devname)
{
  SpecDevice *dev = device_lookup (devname);
  Ext2Superblock *esb;
  int ret;

  if (dev == NULL)
    return -ENOENT;
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
  if (esb->esb_magic != EXT2_MAGIC)
    {
      kfree (esb);
      return -EINVAL;
    }

  /* Fill VFS superblock */
  mp->vfs_sb.sb_dev = device_lookup_devid (dev->sd_major, dev->sd_minor);
  mp->vfs_sb.sb_blksize = 1 << (esb->esb_blksize + 10);
  mp->vfs_sb.sb_flags = flags;
  mp->vfs_sb.sb_magic = esb->esb_magic;

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

int
ext2_mount (VFSMount *mp, int flags, void *data)
{
  VFSDirEntry *root;
  VFSPath *devpath;
  VFSPath *temp;
  int ret;

  if (data == NULL)
    return -EINVAL;
  ret = vfs_namei (&devpath, (char *) data);
  if (ret != 0)
    return ret;

  mp->vfs_sb.sb_fstype = mp->vfs_fstype;
  mp->vfs_sb.sb_ops = &ext2_sops;

  /* Set root dir entry and inode */
  root = kmalloc (sizeof (VFSDirEntry));
  if (unlikely (root == NULL))
    {
      vfs_path_free (devpath);
      return -ENOMEM;
    }
  root->d_flags = 0;
  root->d_mounted = 1;
  root->d_name = strdup ("/");
  if (unlikely (root->d_name == NULL))
    {
      vfs_path_free (devpath);
      kfree (root);
      return -ENOMEM;
    }
  root->d_inode = EXT2_ROOT_INODE;
  mp->vfs_sb.sb_root = root;

  temp = vfs_path_first (devpath);
  if (temp == NULL || strcmp (temp->vp_short, "dev") != 0
      || temp != devpath->vp_prev || *devpath->vp_short == '\0')
    return -EINVAL;
  ret = ext2_init_disk (mp, flags, devpath->vp_short);
  if (ret != 0)
    {
      vfs_path_free (devpath);
      return ret;
    }
  vfs_path_free (devpath);
  return 0;
}

int
ext2_unmount (VFSMount *mp, int flags)
{
  kfree (mp->vfs_sb.sb_root->d_name);
  kfree (mp->vfs_sb.sb_root);
  kfree (mp->vfs_sb.sb_private);
  return 0;
}

VFSInode *
ext2_alloc_inode (VFSSuperblock *sb)
{
  VFSInode *inode = kzalloc (sizeof (VFSInode));
  inode->vi_ops = &ext2_iops;
  return inode;
}

void
ext2_destroy_inode (VFSInode *inode)
{
  kfree (inode);
}

void
ext2_fill_inode (VFSInode *inode)
{
}

void
ext2_write_inode (VFSInode *inode)
{
}

void
ext2_delete_inode (VFSInode *inode)
{
}

void
ext2_free (VFSSuperblock *sb)
{
}

void
ext2_update (VFSSuperblock *sb)
{
}

int
ext2_statfs (VFSSuperblock *sb, struct statfs *st)
{
  Ext2Superblock *esb = (Ext2Superblock *) sb->sb_private;
  st->f_type = EXT2_MAGIC;
  st->f_bsize = sb->sb_blksize;
  st->f_blocks = esb->esb_blocks;
  st->f_bfree = esb->esb_fblocks;
  /* TODO Consider reserved blocks */
  st->f_bavail = esb->esb_fblocks;
  st->f_files = esb->esb_inodes;
  st->f_ffree = esb->esb_finodes;
  st->f_fsid.f_val[0] = sb->sb_dev->sd_major;
  st->f_fsid.f_val[1] = sb->sb_dev->sd_minor;
  st->f_namelen = 255; /* ext2 name length limit */
  st->f_flags = sb->sb_flags;
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
