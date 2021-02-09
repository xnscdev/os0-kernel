/*************************************************************************
 * vfs.h -- This file is part of OS/0.                                   *
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

#ifndef _FS_VFS_H
#define _FS_VFS_H

#include <libk/libk.h>
#include <sys/mount.h>
#include <stddef.h>

typedef struct _VFSMount VFSMount;
typedef struct _VFSInode VFSInode;
typedef struct _VFSDirEntry VFSDirEntry;

typedef struct
{
  int (*vfs_mount) (VFSMount *, const char *, u32, void *);
  int (*vfs_unmount) (VFSMount *, u32);
  int (*vfs_statfs) (VFSMount *, struct statfs *);
} VFSMountOps;

typedef struct
{
  int (*vfs_create) (VFSInode *, VFSDirEntry *, mode_t);
  VFSDirEntry *(*vfs_lookup) (VFSInode *, VFSDirEntry *);
  int (*vfs_link) (VFSDirEntry *, VFSInode *, VFSDirEntry *);
  int (*vfs_unlink) (VFSInode *, VFSDirEntry *);
  int (*vfs_symlink) (VFSInode *, VFSDirEntry *, const char *);
  int (*vfs_mkdir) (VFSInode *, VFSDirEntry *, mode_t);
  int (*vfs_rmdir) (VFSInode *, VFSDirEntry *);
  int (*vfs_mknod) (VFSInode *, VFSDirEntry *, mode_t, dev_t);
  int (*vfs_rename) (VFSInode *, VFSDirEntry *, VFSInode *, VFSDirEntry *);
  int (*vfs_readlink) (VFSDirEntry *, char *, size_t);
  void (*vfs_truncate) (VFSInode *);
  int (*vfs_permission) (VFSInode *, mode_t);
  int (*vfs_getattr) (VFSMount *, VFSDirEntry *, struct stat *);
  int (*vfs_setxattr) (VFSDirEntry *, const char *, const void *, size_t, int);
  int (*vfs_getxattr) (VFSDirEntry *, const char *, void *, size_t);
  int (*vfs_listxattr) (VFSDirEntry *, char *, size_t);
  int (*vfs_removexattr) (VFSDirEntry *, const char *);
} VFSInodeOps;

typedef struct
{
  int (*d_compare) (VFSDirEntry *, const char *, const char *);
  void (*d_iput) (VFSDirEntry *, VFSInode *);
} VFSDirEntryOps;

struct _VFSMount
{
  char vfs_name[16];
  u32 vfs_flags;
  VFSMount *vfs_parent;
  VFSMountOps *vfs_ops;
  VFSDirEntry *vfs_mountpoint;
  VFSDirEntry *vfs_root;
};

struct _VFSInode
{
  uid_t vi_uid;
  gid_t vi_gid;
  u32 vi_flags;
  u32 vi_ino;
  u32 vi_nlink;
  dev_t vi_rdev;
  loff_t vi_size;
  struct timespec vi_atime;
  struct timespec vi_mtime;
  struct timespec vi_ctime;
  blkcnt_t vi_blocks;
  VFSInodeOps *vi_ops;
};

struct _VFSDirEntry
{
  u32 d_flags;
  VFSInode *d_inode;
  int d_mounted;
  VFSDirEntry *d_parent;
  char *d_name;
  VFSDirEntryOps *d_ops;
};

#endif
