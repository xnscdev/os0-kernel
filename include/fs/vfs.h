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
#include <sys/device.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <time.h>

#define VFS_FS_TABLE_SIZE 8
#define VFS_MOUNT_TABLE_SIZE 16

#define FS_TYPE_UNKNOWN 0
#define FS_TYPE_EXT2    1

#define VFS_PATH_SHORT_MAX 16

#define VI_FLAG_NONBLOCK 0x01000000

#define SYMLINK_MODE (S_IFLNK | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

typedef struct _VFSMount VFSMount;
typedef struct _VFSSuperblock VFSSuperblock;
typedef struct _VFSInode VFSInode;

typedef int (*VFSDirEntryFillFunc) (const char *, size_t, ino64_t,
				    unsigned char, off64_t, void *);

typedef struct
{
  VFSInode *(*sb_alloc_inode) (VFSSuperblock *);
  void (*sb_destroy_inode) (VFSInode *);
  int (*sb_fill_inode) (VFSInode *);
  int (*sb_write_inode) (VFSInode *);
  void (*sb_free) (VFSSuperblock *);
  void (*sb_update) (VFSSuperblock *);
  int (*sb_statfs) (VFSSuperblock *, struct statfs64 *);
  int (*sb_remount) (VFSSuperblock *, int *, void *);
} VFSSuperblockOps;

typedef struct
{
  int (*vfs_create) (VFSInode *, const char *, mode_t);
  int (*vfs_lookup) (VFSInode **, VFSInode *, VFSSuperblock *, const char *,
		     int);
  int (*vfs_link) (VFSInode *, VFSInode *, const char *);
  int (*vfs_unlink) (VFSInode *, const char *);
  int (*vfs_symlink) (VFSInode *, const char *, const char *);
  int (*vfs_read) (VFSInode *, void *, size_t, off_t);
  int (*vfs_write) (VFSInode *, const void *, size_t, off_t);
  int (*vfs_readdir) (VFSInode *, VFSDirEntryFillFunc, void *);
  int (*vfs_chmod) (VFSInode *, mode_t);
  int (*vfs_chown) (VFSInode *, uid_t, gid_t);
  int (*vfs_mkdir) (VFSInode *, const char *, mode_t);
  int (*vfs_rmdir) (VFSInode *, const char *);
  int (*vfs_mknod) (VFSInode *, const char *, mode_t, dev_t);
  int (*vfs_rename) (VFSInode *, const char *, VFSInode *, const char *);
  int (*vfs_readlink) (VFSInode *, char *, size_t);
  int (*vfs_truncate) (VFSInode *);
  int (*vfs_getattr) (VFSInode *, struct stat64 *);
  int (*vfs_setxattr) (VFSInode *, const char *, const void *, size_t, int);
  int (*vfs_getxattr) (VFSInode *, const char *, void *, size_t);
  int (*vfs_listxattr) (VFSInode *, char *, size_t);
  int (*vfs_removexattr) (VFSInode *, const char *);
} VFSInodeOps;

typedef struct
{
  char vfs_name[16];
  int vfs_flags;
  int (*vfs_mount) (VFSMount *, int, void *);
  int (*vfs_unmount) (VFSMount *, int);
  const VFSSuperblockOps *vfs_sops;
  const VFSInodeOps *vfs_iops;
} VFSFilesystem;

struct _VFSSuperblock
{
  SpecDevice *sb_dev;
  blksize_t sb_blksize;
  size_t sb_maxsize;
  const VFSSuperblockOps *sb_ops;
  int sb_flags;
  unsigned long sb_mntflags;
  unsigned int sb_mntslot;
  unsigned long sb_magic;
  VFSInode *sb_root;
  const VFSFilesystem *sb_fstype;
  void *sb_private;
};

struct _VFSInode
{
  uid_t vi_uid;
  gid_t vi_gid;
  ino64_t vi_ino;
  nlink_t vi_nlink;
  dev_t vi_rdev;
  off64_t vi_size;
  size_t vi_sectors;
  mode_t vi_mode;
  struct timespec vi_atime;
  struct timespec vi_mtime;
  struct timespec vi_ctime;
  blkcnt64_t vi_blocks;
  unsigned int vi_flags;
  const VFSInodeOps *vi_ops;
  VFSSuperblock *vi_sb;
  unsigned long vi_refcnt;
  void *vi_private;
};

struct _VFSMount
{
  const VFSFilesystem *vfs_fstype;
  VFSSuperblock vfs_sb;
  VFSInode *vfs_mntpoint;
  char *vfs_mntpath;
  void *vfs_private;
};

typedef uint64_t block_t;

__BEGIN_DECLS

extern VFSFilesystem fs_table[VFS_FS_TABLE_SIZE];
extern VFSMount mount_table[VFS_MOUNT_TABLE_SIZE];
extern VFSInode *vfs_root_inode;

void vfs_init (void);

int vfs_register (const VFSFilesystem *fs);
int vfs_guess_type (SpecDevice *dev);
int vfs_mount (const char *type, const char *dir, int flags, void *data);

VFSInode *vfs_alloc_inode (VFSSuperblock *sb);
void vfs_ref_inode (VFSInode *inode);
void vfs_unref_inode (VFSInode *inode);
int vfs_fill_inode (VFSInode *inode);
int vfs_write_inode (VFSInode *inode);
void vfs_free_sb (VFSSuperblock *sb);
void vfs_update_sb (VFSSuperblock *sb);
int vfs_statfs (VFSSuperblock *sb, struct statfs64 *st);
int vfs_remount (VFSSuperblock *sb, int *flags, void *data);

int vfs_perm_check_read (VFSInode *inode, int real);
int vfs_perm_check_write (VFSInode *inode, int real);
int vfs_perm_check_exec (VFSInode *inode, int real);
int vfs_can_seek (VFSInode *inode);

int vfs_create (VFSInode *dir, const char *name, mode_t mode);
int vfs_lookup (VFSInode **inode, VFSInode *dir, VFSSuperblock *sb,
		const char *name, int symcount);
int vfs_link (VFSInode *old, VFSInode *dir, const char *new);
int vfs_unlink (VFSInode *dir, const char *name);
int vfs_symlink (VFSInode *dir, const char *old, const char *new);
int vfs_read (VFSInode *inode, void *buffer, size_t len, off_t offset);
int vfs_write (VFSInode *inode, const void *buffer, size_t len, off_t offset);
int vfs_readdir (VFSInode *inode, VFSDirEntryFillFunc func, void *private);
int vfs_chmod (VFSInode *inode, mode_t mode);
int vfs_chown (VFSInode *inode, uid_t uid, gid_t gid);
int vfs_mkdir (VFSInode *dir, const char *name, mode_t mode);
int vfs_rmdir (VFSInode *dir, const char *name);
int vfs_mknod (VFSInode *dir, const char *name, mode_t mode, dev_t rdev);
int vfs_rename (VFSInode *olddir, const char *oldname, VFSInode *newdir,
		const char *newname);
int vfs_readlink (VFSInode *inode, char *buffer, size_t len);
int vfs_truncate (VFSInode *inode);
int vfs_getattr (VFSInode *inode, struct stat64 *st);
int vfs_setxattr (VFSInode *inode, const char *name, const void *value,
		  size_t len, int flags);
int vfs_getxattr (VFSInode *inode, const char *name, void *buffer,
		  size_t len);
int vfs_listxattr (VFSInode *inode, char *buffer, size_t len);
int vfs_removexattr (VFSInode *inode, const char *name);

int vfs_open_file (VFSInode **inode, const char *path, int follow_symlinks);
char *vfs_path_resolve (const char *path);
void vfs_unmount_all (void);

__END_DECLS

#endif
