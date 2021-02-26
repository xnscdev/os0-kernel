/*************************************************************************
 * ext2.h -- This file is part of OS/0.                                  *
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

#ifndef _FS_EXT2_H
#define _FS_EXT2_H

#include <fs/vfs.h>
#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

#define EXT2_MAGIC         0xef53
#define EXT2_FS_NAME       "ext2"
#define EXT2_STORED_INODES 12
#define EXT2_ROOT_INODE    2
#define EXT2_MAX_NAME_LEN  255

#define EXT2_TYPE_FIFO   0x1000
#define EXT2_TYPE_CHRDEV 0x2000
#define EXT2_TYPE_DIR    0x4000
#define EXT2_TYPE_BLKDEV 0x6000
#define EXT2_TYPE_FILE   0x8000
#define EXT2_TYPE_LINK   0xa000
#define EXT2_TYPE_SOCKET 0xc000

#define EXT2_DIRTYPE_NONE   0
#define EXT2_DIRTYPE_FILE   1
#define EXT2_DIRTYPE_DIR    2
#define EXT2_DIRTYPE_CHRDEV 3
#define EXT2_DIRTYPE_BLKDEV 4
#define EXT2_DIRTYPE_FIFO   5
#define EXT2_DIRTYPE_SOCKET 6
#define EXT2_DIRTYPE_LINK   7

#define EXT2_FT_OPT_PREALLOC  0x01
#define EXT2_FT_OPT_AFSINODES 0x02
#define EXT2_FT_OPT_JOURNALED 0x04
#define EXT2_FT_OPT_XATTR     0x08
#define EXT2_FT_OPT_FSRESIZE  0x10
#define EXT2_FT_OPT_DIRHASH   0x20

#define EXT2_FT_REQ_COMPRESS  0x01
#define EXT2_FT_REQ_DIRTYPE   0x02
#define EXT2_FT_REQ_RPLAYJRNL 0x04
#define EXT2_FT_REQ_JRNLDEV   0x08

#define EXT2_FT_RO_SPARSE     0x01
#define EXT2_FT_RO_FILESIZE64 0x02
#define EXT2_FT_RO_BTREEDIR   0x04

typedef struct
{
  uint32_t esb_inodes;
  uint32_t esb_blocks;
  uint32_t esb_rblocks;
  uint32_t esb_finodes;
  uint32_t esb_fblocks;
  uint32_t esb_sbid;
  uint32_t esb_blksize;
  uint32_t esb_fragsize;
  uint32_t esb_bpg;
  uint32_t esb_fpg;
  uint32_t esb_ipg;
  uint32_t esb_mtime;
  uint32_t esb_wtime;
  uint16_t esb_fsck;
  uint16_t esb_fmnt;
  uint16_t esb_magic;
  uint16_t esb_state;
  uint16_t esb_errh;
  uint16_t esb_versmin;
  uint32_t esb_chktime;
  uint32_t esb_chkint;
  uint32_t esb_vendor;
  uint32_t esb_versmaj;
  uint16_t esb_ruid;
  uint16_t esb_rgid;
  uint32_t esb_ino1;
  uint16_t esb_inosize;
  uint16_t esb_bakgid;
  uint32_t esb_optft;
  uint32_t esb_reqft;
  uint32_t esb_roft;
  unsigned char esb_fsid[16];
  unsigned char esb_volname[16];
  unsigned char esb_mntpoint[64];
  uint32_t esb_compr;
  uint8_t esb_fblkpalloc;
  uint8_t esb_dblkpalloc;
  uint16_t esb_reserved;
  unsigned char esb_jrnlid[16];
  uint32_t esb_jrnlino;
  uint32_t esb_jrnldev;
  uint32_t esb_orphlst;
  unsigned char esb_unused[788];
} __attribute__ ((packed)) Ext2Superblock;

typedef struct
{
  uint32_t eb_busage;
  uint32_t eb_iusage;
  uint32_t eb_inotbl;
  uint16_t eb_bfree;
  uint16_t eb_ifree;
  uint16_t eb_dirs;
  unsigned char eb_unused[14];
} __attribute__ ((packed)) Ext2BGD;

typedef struct
{
  uint16_t ei_mode;
  uint16_t ei_uid;
  uint32_t ei_sizel;
  uint32_t ei_atime;
  uint32_t ei_ctime;
  uint32_t ei_mtime;
  uint32_t ei_dtime;
  uint16_t ei_gid;
  uint16_t ei_nlink;
  uint32_t ei_sectors;
  uint32_t ei_flags;
  uint32_t ei_oss1;
  uint32_t ei_bptr0[EXT2_STORED_INODES];
  uint32_t ei_bptr1;
  uint32_t ei_bptr2;
  uint32_t ei_bptr3;
  uint32_t ei_gen;
  uint32_t ei_acl;
  uint32_t ei_sizeh;
  uint32_t ei_fragbaddr;
  uint32_t ei_oss2[3];
} __attribute__ ((packed)) Ext2Inode;

typedef struct
{
  uint32_t ed_inode;
  uint16_t ed_size;
  uint8_t ed_namelenl;
  uint8_t ed_namelenh;
} __attribute__ ((packed)) Ext2DirEntry;

__BEGIN_DECLS

extern const VFSSuperblockOps ext2_sops;
extern const VFSInodeOps ext2_iops;
extern const VFSDirEntryOps ext2_dops;
extern const VFSFilesystem ext2_vfs;

void ext2_init (void);

int ext2_read_blocks (void *buffer, VFSSuperblock *sb, uint32_t block,
		      size_t nblocks);
int ext2_write_blocks (void *buffer, VFSSuperblock *sb, uint32_t block,
		       size_t nblocks);
off_t ext2_data_block (Ext2Inode *inode, VFSSuperblock *sb, off_t block);
uint32_t ext2_inode_offset (VFSSuperblock *sb, ino_t inode);
Ext2Inode *ext2_read_inode (VFSSuperblock *sb, ino_t inode);
int ext2_unref_inode (VFSSuperblock *sb, ino_t inode);
loff_t ext2_alloc_block (VFSSuperblock *sb, int prefbg);
ino_t ext2_create_inode (VFSSuperblock *sb, int prefbg);
uint32_t ext2_bgdt_size (Ext2Superblock *esb);

int ext2_mount (VFSMount *mp, int flags, void *data);
int ext2_unmount (VFSMount *mp, int flags);
VFSInode *ext2_alloc_inode (VFSSuperblock *sb);
void ext2_destroy_inode (VFSInode *inode);
void ext2_fill_inode (VFSInode *inode);
void ext2_write_inode (VFSInode *inode);
void ext2_free (VFSSuperblock *sb);
void ext2_update (VFSSuperblock *sb);
int ext2_statvfs (VFSSuperblock *sb, struct statvfs *st);
int ext2_remount (VFSSuperblock *sb, int *flags, void *data);
int ext2_create (VFSInode *dir, const char *name, mode_t mode);
int ext2_lookup (VFSDirEntry *entry, VFSSuperblock *sb, VFSPath *path);
int ext2_link (VFSInode *old, VFSInode *dir, const char *new);
int ext2_unlink (VFSInode *dir, const char *name);
int ext2_symlink (VFSInode *dir, const char *old, const char *new);
int ext2_read (VFSInode *inode, void *buffer, size_t len, off_t offset);
int ext2_write (VFSInode *inode, void *buffer, size_t len, off_t offset);
int ext2_readdir (VFSDirEntry **entries, VFSSuperblock *sb, VFSInode *dir);
int ext2_chmod (VFSInode *inode, mode_t mode);
int ext2_chown (VFSInode *inode, uid_t uid, gid_t gid);
int ext2_mkdir (VFSInode *dir, const char *name, mode_t mode);
int ext2_rmdir (VFSInode *dir, const char *name);
int ext2_mknod (VFSInode *dir, const char *name, mode_t mode, dev_t rdev);
int ext2_rename (VFSInode *olddir, const char *oldname, VFSInode *newdir,
		 const char *newname);
int ext2_readlink (VFSDirEntry *entry, char *buffer, size_t len);
int ext2_truncate (VFSInode *inode);
int ext2_getattr (VFSMount *mp, VFSDirEntry *entry, struct stat *st);
int ext2_setxattr (VFSDirEntry *entry, const char *name, const void *value,
		   size_t len, int flags);
int ext2_getxattr (VFSDirEntry *entry, const char *name, void *buffer,
		   size_t len);
int ext2_listxattr (VFSDirEntry *entry, char *buffer, size_t len);
int ext2_removexattr (VFSDirEntry *entry, const char *name);
int ext2_compare (VFSDirEntry *entry, const char *a, const char *b);
void ext2_iput (VFSDirEntry *entry, VFSInode *inode);

__END_DECLS

#endif
