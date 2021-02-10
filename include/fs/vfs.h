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

#include <sys/cdefs.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define VFS_FS_TABLE_SIZE 8

typedef struct _VFSMount VFSMount;
typedef struct _VFSSuperblock VFSSuperblock;
typedef struct _VFSInode VFSInode;
typedef struct _VFSDirEntry VFSDirEntry;

typedef struct
{
  VFSInode *(*sb_alloc_inode) (VFSSuperblock *);
  void (*sb_destroy_inode) (VFSInode *);
  void (*sb_fill_inode) (VFSInode *);
  void (*sb_write_inode) (VFSInode *);
  void (*sb_delete_inode) (VFSInode *);
  void (*sb_free) (VFSSuperblock *);
  void (*sb_update) (VFSSuperblock *);
  int (*sb_statfs) (VFSSuperblock *, struct statfs *);
  int (*sb_remount) (VFSSuperblock *, int *, void *);
} VFSSuperblockOps;

typedef struct
{
  int (*vfs_create) (VFSInode *, VFSDirEntry *, mode_t);
  VFSDirEntry *(*vfs_lookup) (const char *path);
  int (*vfs_link) (VFSDirEntry *, VFSInode *, VFSDirEntry *);
  int (*vfs_unlink) (VFSInode *, VFSDirEntry *);
  int (*vfs_symlink) (VFSInode *, VFSDirEntry *, const char *);
  int (*vfs_mkdir) (VFSInode *, VFSDirEntry *, mode_t);
  int (*vfs_rmdir) (VFSInode *, VFSDirEntry *);
  int (*vfs_mknod) (VFSInode *, VFSDirEntry *, mode_t, dev_t);
  int (*vfs_rename) (VFSInode *, VFSDirEntry *, VFSInode *, VFSDirEntry *);
  int (*vfs_readlink) (VFSDirEntry *, char *, size_t);
  int (*vfs_truncate) (VFSInode *);
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
  int vfs_flags;
  int (*vfs_mount) (VFSMount *, uint32_t, void *);
  int (*vfs_unmount) (VFSMount *, uint32_t);
  const VFSSuperblockOps *vfs_sops;
  const VFSInodeOps *vfs_iops;
  const VFSDirEntryOps *vfs_dops;
} VFSFilesystem;

struct _VFSSuperblock
{
  dev_t sb_dev;
  blksize_t sb_blksize;
  size_t sb_maxsize;
  VFSSuperblockOps *sb_ops;
  int sb_flags;
  unsigned long sb_magic;
  VFSDirEntry *sb_root;
  char sb_name[16];
};

struct _VFSInode
{
  uid_t vi_uid;
  gid_t vi_gid;
  int vi_flags;
  ino_t vi_ino;
  nlink_t vi_nlink;
  dev_t vi_rdev;
  loff_t vi_size;
  struct timespec vi_atime;
  struct timespec vi_mtime;
  struct timespec vi_ctime;
  blkcnt_t vi_blocks;
  VFSInodeOps *vi_ops;
  void *vi_private;
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

struct _VFSMount
{
  VFSFilesystem *vfs_fstype;
  VFSSuperblock vfs_sb;
  VFSMount *vfs_parent;
  const char *vfs_mntpoint;
  void *vfs_private;
};

__BEGIN_DECLS

extern VFSFilesystem fs_table[VFS_FS_TABLE_SIZE];

void vfs_init (void);

int vfs_register (const VFSFilesystem *fs);
int vfs_mount (const char *type, const char *dir, int flags, void *data);

__END_DECLS

#endif
