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

#include <libk/time.h>
#include <sys/cdefs.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdint.h>

#define VFS_FS_TABLE_SIZE 8

typedef struct _VFSMount VFSMount;
typedef struct _VFSInode VFSInode;
typedef struct _VFSDirEntry VFSDirEntry;

typedef struct
{
  int (*vfs_mount) (VFSMount *, const char *, uint32_t, void *);
  int (*vfs_unmount) (VFSMount *, uint32_t);
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

typedef struct
{
  char vfs_name[16];
  uint32_t vfs_flags;
  int (*vfs_init) (void);
  int (*vfs_destroy) (void);
  VFSMountOps *vfs_mops;
  VFSInodeOps *vfs_iops;
  VFSDirEntryOps *vfs_dops;
} VFSFilesystem;

struct _VFSMount
{
  char vfs_name[16];
  uint32_t vfs_flags;
  VFSMount *vfs_parent;
  VFSMountOps *vfs_ops;
  VFSDirEntry *vfs_mountpoint;
  VFSDirEntry *vfs_root;
};

struct _VFSInode
{
  uid_t vi_uid;
  gid_t vi_gid;
  uint32_t vi_flags;
  uint32_t vi_ino;
  uint32_t vi_nlink;
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
  uint32_t d_flags;
  VFSInode *d_inode;
  int d_mounted;
  VFSDirEntry *d_parent;
  char *d_name;
  VFSDirEntryOps *d_ops;
};

__BEGIN_DECLS

extern VFSFilesystem fs_table[VFS_FS_TABLE_SIZE];

void fs_init (void);

int fs_register (const VFSFilesystem *fs);

__END_DECLS

#endif
