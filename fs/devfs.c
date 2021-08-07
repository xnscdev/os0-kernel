/*************************************************************************
 * devfs.c -- This file is part of OS/0.                                 *
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
#include <libk/libk.h>
#include <sys/ata.h>
#include <sys/process.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <vm/heap.h>
#include <errno.h>

const VFSSuperblockOps devfs_sops = {
  .sb_alloc_inode = devfs_alloc_inode,
  .sb_destroy_inode = (void (*) (VFSInode *)) kfree,
  .sb_free = devfs_free
};

const VFSInodeOps devfs_iops = {
  .vfs_lookup = devfs_lookup,
  .vfs_read = devfs_read,
  .vfs_write = devfs_write,
  .vfs_readdir = devfs_readdir,
  .vfs_readlink = devfs_readlink,
  .vfs_getattr = devfs_getattr
};

const VFSFilesystem devfs_vfs = {
  .vfs_name = DEVFS_FS_NAME,
  .vfs_flags = 0,
  .vfs_mount = devfs_mount,
  .vfs_unmount = devfs_unmount,
  .vfs_sops = &devfs_sops,
  .vfs_iops = &devfs_iops
};

const struct
{
  const char *name;
  char *link;
  int fd;
} devfs_links[] = {
  {"stdin", "fd/0", STDIN_FILENO},
  {"stdout", "fd/1", STDOUT_FILENO},
  {"stderr", "fd/2", STDERR_FILENO},
  {NULL, 0}
};

int
devfs_mount (VFSMount *mp, int flags, void *data)
{
  mp->vfs_sb.sb_fstype = mp->vfs_fstype;
  mp->vfs_sb.sb_ops = &devfs_sops;
  mp->vfs_sb.sb_root = devfs_alloc_inode (&mp->vfs_sb);
  if (unlikely (mp->vfs_sb.sb_root == NULL))
    return -ENOMEM;
  mp->vfs_sb.sb_root->vi_ino = DEVFS_ROOT_INODE;
  mp->vfs_sb.sb_root->vi_mode =
    S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
  mp->vfs_sb.sb_root->vi_nlink = 3;
  mp->vfs_sb.sb_root->vi_refcnt = 1;
  return 0;
}

int
devfs_unmount (VFSMount *mp, int flags)
{
  return 0;
}

VFSInode *
devfs_alloc_inode (VFSSuperblock *sb)
{
  VFSInode *inode = kzalloc (sizeof (VFSInode));
  time_t t;
  if (unlikely (inode == NULL))
    return NULL;
  inode->vi_ops = &devfs_iops;
  inode->vi_sb = sb;

  /* Set reasonable defaults */
  t = time (NULL);
  inode->vi_uid = 0;
  inode->vi_gid = 0;
  inode->vi_size = 0;
  inode->vi_sectors = 0;
  inode->vi_atime.tv_sec = t;
  inode->vi_atime.tv_nsec = 0;
  inode->vi_mtime.tv_sec = t;
  inode->vi_mtime.tv_nsec = 0;
  inode->vi_ctime.tv_sec = t;
  inode->vi_ctime.tv_nsec = 0;
  inode->vi_blocks = 0;
  return inode;
}

void
devfs_free (VFSSuperblock *sb)
{
  vfs_unref_inode (sb->sb_root);
}

int
devfs_lookup (VFSInode **inode, VFSInode *dir, VFSSuperblock *sb,
	      const char *name, int symcount)
{
  int i;
  if (!S_ISDIR (dir->vi_mode))
    return -ENOTDIR;
  if (dir->vi_ino == DEVFS_ROOT_INODE)
    {
      if (strcmp (name, "fd") == 0)
	{
	  *inode = vfs_alloc_inode (sb);
	  if (unlikely (*inode == NULL))
	    return -ENOMEM;
	  (*inode)->vi_ino = DEVFS_FDS_INODE;
	  (*inode)->vi_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	  (*inode)->vi_nlink = 2;
	  (*inode)->vi_private = NULL;
	  return 0;
	}
      for (i = 0; devfs_links[i].name != NULL; i++)
	{
	  if (strcmp (name, devfs_links[i].name) == 0)
	    {
	      *inode = vfs_alloc_inode (sb);
	      if (unlikely (*inode == NULL))
		return -ENOMEM;
	      (*inode)->vi_ino = DEVFS_LINK_INODE (i);
	      (*inode)->vi_mode = SYMLINK_MODE;
	      (*inode)->vi_nlink = 1;
	      (*inode)->vi_private = devfs_links[i].link;
	      return 0;
	    }
	}
      for (i = 0; i < DEVICE_TABLE_SIZE; i++)
	{
	  if (strcmp (device_table[i].sd_name, name) == 0)
	    {
	      *inode = vfs_alloc_inode (sb);
	      if (unlikely (*inode == NULL))
		return -ENOMEM;
	      if (device_table[i].sd_type == DEVICE_TYPE_BLOCK)
		(*inode)->vi_mode = S_IFBLK;
	      else
		(*inode)->vi_mode = S_IFCHR;
	      (*inode)->vi_mode |= S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	      (*inode)->vi_nlink = 1;
	      (*inode)->vi_rdev =
	        makedev (device_table[i].sd_major, device_table[i].sd_minor);
	      (*inode)->vi_ino = DEVFS_DEVICE_INODE (device_table[i].sd_major,
						     device_table[i].sd_minor);
	      (*inode)->vi_private = &device_table[i];
	      return 0;
	    }
	}
      return -ENOENT;
    }
  else if (dir->vi_ino == DEVFS_FDS_INODE)
    {
      int fd = 0;
      /* Convert string to file descriptor */
      while (isdigit (*name))
	{
	  fd *= 10;
	  fd += *name - '0';
	  name++;
	}
      if (*name != '\0')
	return -ENOENT;
      *inode = inode_from_fd (fd);
      if (unlikely (*inode == NULL))
	return -ENOENT;
      vfs_ref_inode (*inode);
      return 0;
    }
  return -ENOENT;
}

int
devfs_read (VFSInode *inode, void *buffer, size_t len, off_t offset)
{
  SpecDevice *dev;
  if (!S_ISCHR (inode->vi_mode) && !S_ISBLK (inode->vi_mode))
    return -ENOTSUP;
  dev = inode->vi_private;
  return dev->sd_read (dev, buffer, len, offset);
}

int
devfs_write (VFSInode *inode, const void *buffer, size_t len, off_t offset)
{
  SpecDevice *dev;
  if (!S_ISCHR (inode->vi_mode) && !S_ISBLK (inode->vi_mode))
    return -ENOTSUP;
  dev = inode->vi_private;
  return dev->sd_write (dev, buffer, len, offset);
}

int
devfs_readdir (VFSInode *inode, VFSDirEntryFillFunc func, void *private)
{
  Process *proc = &process_table[task_getpid ()];
  int ret;
  int i;
  switch (inode->vi_ino)
    {
    case DEVFS_ROOT_INODE:
      for (i = 0; i < DEVICE_TABLE_SIZE; i++)
	{
	  SpecDevice *dev = &device_table[i];
	  if (*dev->sd_name != '\0')
	    {
	      ret = func (dev->sd_name, strlen (dev->sd_name),
			  DEVFS_DEVICE_INODE (dev->sd_major, dev->sd_minor),
			  dev->sd_type == DEVICE_TYPE_BLOCK ? DT_BLK : DT_CHR,
			  i, private);
	      if (ret > 0)
		return 0;
	      if (ret < 0)
		return ret;
	    }						
	}
      for (i = 0; devfs_links[i].name != NULL; i++)
	{
	  ret = func (devfs_links[i].name, strlen (devfs_links[i].name),
		      DEVFS_LINK_INODE (i), DT_LNK, i + DEVICE_TABLE_SIZE,
		      private);
	  if (ret > 0)
	    return 0;
	  if (ret < 0)
	    return ret;
	}
      return func ("fd", 2, 1, DT_DIR, DEVICE_TABLE_SIZE, private);
    case DEVFS_FDS_INODE:
      for (i = 0; i < PROCESS_FILE_LIMIT; i++)
	{
	  if (proc->p_files[i] != NULL)
	    {
	      char buffer[4];
	      itoa (i, buffer, 10);
	      ret = func (buffer, strlen (buffer), DEVFS_FD_INODE (i), DT_BLK,
			  i, private);
	      if (ret > 0)
		return 0;
	      if (ret < 0)
		return ret;
	    }
	}
      break;
    default:
      return -ENOENT;
    }
  return 0;
}

int
devfs_readlink (VFSInode *inode, char *buffer, size_t len)
{
  strncpy (buffer, inode->vi_private, len);
  return 0;
}

int
devfs_getattr (VFSInode *inode, struct stat64 *st)
{
  st->st_dev = ((uint32_t) inode->vi_sb->sb_fstype -
		(uint32_t) fs_table) / sizeof (VFSFilesystem);
  st->st_blksize = DEVFS_BLKSIZE;
  return 0;
}

void
devfs_init (void)
{
  if (vfs_register (&devfs_vfs) != 0)
    panic ("Failed to register " DEVFS_FS_NAME " filesystem");
}
