/*************************************************************************
 * devfs.h -- This file is part of OS/0.                                 *
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

#ifndef _FS_DEVFS_H
#define _FS_DEVFS_H

#include <fs/vfs.h>
#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

#define DEVFS_FS_NAME    "devfs"
#define DEVFS_ROOT_INODE 0
#define DEVFS_FD_INODE   1

__BEGIN_DECLS

extern const VFSSuperblockOps devfs_sops;
extern const VFSInodeOps devfs_iops;
extern const VFSDirEntryOps devfs_dops;
extern const VFSFilesystem devfs_vfs;

void devfs_init (void);

int devfs_mount (VFSMount *mp, int flags, void *data);
int devfs_unmount (VFSMount *mp, int flags);
VFSInode *devfs_alloc_inode (VFSSuperblock *sb);
void devfs_destroy_inode (VFSInode *inode);
void devfs_free (VFSSuperblock *sb);

int devfs_lookup (VFSInode **inode, VFSInode *dir, VFSSuperblock *sb,
		  const char *name, int follow_symlinks);
int devfs_read (VFSInode *inode, void *buffer, size_t len, off_t offset);
VFSDirEntry *devfs_readdir (VFSDirectory *dir, VFSSuperblock *sb);
int devfs_readlink (VFSInode *inode, char *buffer, size_t len);
int devfs_getattr (VFSInode *inode, struct stat *st);
int devfs_compare (VFSDirEntry *entry, const char *a, const char *b);

__END_DECLS

#endif
