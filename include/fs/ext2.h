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

#define EXT2_FT_COMPAT_DIR_PREALLOC     0x0001
#define EXT2_FT_COMPAT_IMAGIC_INODES    0x0002
#define EXT3_FT_COMPAT_HAS_JOURNAL      0x0004
#define EXT2_FT_COMPAT_EXT_XATTR        0x0008
#define EXT2_FT_COMPAT_RESIZE_INODE     0x0010
#define EXT2_FT_COMPAT_DIR_INDEX        0x0020
#define EXT2_FT_COMPAT_LAZY_BG          0x0040
#define EXT2_FT_COMPAT_EXCLUDE_BITMAP   0x0100
#define EXT4_FT_COMPAT_SPARSE_SUPER2    0x0200
#define EXT4_FT_COMPAT_FAST_COMMIT      0x0400
#define EXT4_FT_COMPAT_STABLE_INODES    0x0800

#define EXT2_FT_INCOMPAT_COMPRESSION    0x0001
#define EXT2_FT_INCOMPAT_FILETYPE       0x0002
#define EXT3_FT_INCOMPAT_RECOVER        0x0004
#define EXT3_FT_INCOMPAT_JOURNAL_DEV    0x0008
#define EXT2_FT_INCOMPAT_META_BG        0x0010
#define EXT3_FT_INCOMPAT_EXTENTS        0x0040
#define EXT4_FT_INCOMPAT_64BIT          0x0080
#define EXT4_FT_INCOMPAT_MMP            0x0100
#define EXT4_FT_INCOMPAT_FLEX_BG        0x0200
#define EXT4_FT_INCOMPAT_EA_INODE       0x0400
#define EXT4_FT_INCOMPAT_DIRDATA        0x1000
#define EXT4_FT_INCOMPAT_CSUM_SEED      0x2000
#define EXT4_FT_INCOMPAT_LARGEDIR       0x4000
#define EXT4_FT_INCOMPAT_INLINE_DATA    0x8000
#define EXT4_FT_INCOMPAT_ENCRYPT        0x10000
#define EXT4_FT_INCOMPAT_CASEFOLD       0x20000

#define EXT2_FT_RO_COMPAT_SPARSE_SUPER  0x0001
#define EXT2_FT_RO_COMPAT_LARGE_FILE    0x0002
#define EXT4_FT_RO_COMPAT_HUGE_FILE     0x0008
#define EXT4_FT_RO_COMPAT_GDT_CSUM      0x0010
#define EXT4_FT_RO_COMPAT_DIR_NLINK     0x0020
#define EXT4_FT_RO_COMPAT_EXTRA_ISIZE   0x0040
#define EXT4_FT_RO_COMPAT_HAS_SNAPSHOT  0x0080
#define EXT4_FT_RO_COMPAT_QUOTA         0x0100
#define EXT4_FT_RO_COMPAT_BIGALLOC      0x0200
#define EXT4_FT_RO_COMPAT_METADATA_CSUM 0x0400
#define EXT4_FT_RO_COMPAT_REPLICA       0x0800
#define EXT4_FT_RO_COMPAT_READONLY      0x1000
#define EXT4_FT_RO_COMPAT_PROJECT       0x2000
#define EXT4_FT_RO_COMPAT_SHARED_BLOCKS 0x4000
#define EXT4_FT_RO_COMPAT_VERITY        0x8000

/* Incompatible features that are unsupported */

#define EXT2_UNSUPPORTED_FEATURES (EXT2_FT_INCOMPAT_COMPRESSION		\
				   | EXT3_FT_INCOMPAT_RECOVER		\
				   | EXT3_FT_INCOMPAT_JOURNAL_DEV	\
				   | EXT2_FT_INCOMPAT_META_BG		\
				   | EXT3_FT_INCOMPAT_EXTENTS		\
				   | EXT4_FT_INCOMPAT_64BIT		\
				   | EXT4_FT_INCOMPAT_MMP		\
				   | EXT4_FT_INCOMPAT_FLEX_BG		\
				   | EXT4_FT_INCOMPAT_EA_INODE		\
				   | EXT4_FT_INCOMPAT_DIRDATA		\
				   | EXT4_FT_INCOMPAT_CSUM_SEED		\
				   | EXT4_FT_INCOMPAT_LARGEDIR		\
				   | EXT4_FT_INCOMPAT_INLINE_DATA	\
				   | EXT4_FT_INCOMPAT_ENCRYPT		\
				   | EXT4_FT_INCOMPAT_CASEFOLD)

typedef struct
{
  uint32_t s_inodes_count;
  uint32_t s_blocks_count;
  uint32_t s_r_blocks_count;
  uint32_t s_free_blocks_count;
  uint32_t s_free_inodes_count;
  uint32_t s_first_data_block;
  uint32_t s_log_block_size;
  uint32_t s_log_cluster_size;
  uint32_t s_blocks_per_group;
  uint32_t s_clusters_per_group;
  uint32_t s_inodes_per_group;
  uint32_t s_mtime;
  uint32_t s_wtime;
  uint16_t s_mnt_count;
  int16_t s_max_mnt_count;
  uint16_t s_magic;
  uint16_t s_state;
  uint16_t s_errors;
  uint16_t s_minor_rev_level;
  uint32_t s_lastcheck;
  uint32_t s_checkinterval;
  uint32_t s_creator_os;
  uint32_t s_rev_level;
  uint16_t s_def_resuid;
  uint16_t s_def_resgid;
  uint32_t s_first_ino;
  uint16_t s_inode_size;
  uint16_t s_block_group_nr;
  uint32_t s_feature_compat;
  uint32_t s_feature_incompat;
  uint32_t s_feature_ro_compat;
  unsigned char s_uuid[16];
  unsigned char s_volume_name[16];
  unsigned char s_last_mounted[64];
  uint32_t s_algorithm_usage_bitmap;
  unsigned char s_prealloc_blocks;
  unsigned char s_prealloc_dir_blocks;
  uint16_t s_resered_gdt_blocks;
  unsigned char s_journal_uuid[16];
  uint32_t s_journal_inum;
  uint32_t s_journal_dev;
  uint32_t s_last_orphan;
  uint32_t s_hash_seed[4];
  unsigned char s_def_hash_version;
  unsigned char s_jnl_backup_type;
  uint16_t s_desc_size;
  uint32_t s_default_mount_opts;
  uint32_t s_first_meta_bg;
  uint32_t s_mkfs_time;
  uint32_t s_jnl_blocks[17];
  uint32_t s_blocks_count_hi;
  uint32_t s_r_blocks_count_hi;
  uint32_t s_free_blocks_hi;
  uint16_t s_min_extra_isize;
  uint16_t s_want_extra_isize;
  uint32_t s_flags;
  uint16_t s_raid_stride;
  uint16_t s_mmp_update_interval;
  uint64_t s_mmp_block;
  uint32_t s_raid_stripe_width;
  unsigned char s_log_groups_per_flex;
  unsigned char s_checksum_type;
  unsigned char s_encryption_level;
  unsigned char s_reserved_pad;
  uint64_t s_kbytes_written;
  uint32_t s_snapshot_inum;
  uint32_t s_snapshot_id;
  uint64_t s_snapshot_r_blocks_count;
  uint32_t s_snapshot_list;
  uint32_t s_error_count;
  uint32_t s_first_error_time;
  uint32_t s_first_error_ino;
  uint64_t s_first_error_block;
  unsigned char s_first_error_func[32];
  uint32_t s_first_error_line;
  uint32_t s_last_error_time;
  uint32_t s_last_error_ino;
  uint32_t s_last_error_line;
  uint64_t s_last_error_block;
  unsigned char s_last_error_func[32];
  unsigned char s_mount_opts[64];
  uint32_t s_usr_quota_inum;
  uint32_t s_grp_quota_inum;
  uint32_t s_overhead_clusters;
  uint32_t s_backup_bgs[2];
  unsigned char s_encrypt_algos[4];
  unsigned char s_encrypt_pw_salt[16];
  uint32_t s_lpf_ino;
  uint32_t s_prj_quota_inum;
  uint32_t s_checksum_seed;
  unsigned char s_wtime_hi;
  unsigned char s_mtime_hi;
  unsigned char s_mkfs_time_hi;
  unsigned char s_lastcheck_hi;
  unsigned char s_first_error_time_hi;
  unsigned char s_last_error_time_hi;
  unsigned char s_first_error_errcode;
  unsigned char s_last_error_errcode;
  uint16_t s_encoding;
  uint16_t s_encoding_flags;
  uint32_t s_reserved[95];
  uint32_t s_checksum;
} Ext2Superblock;

typedef struct
{
  uint16_t i_mode;
  uint16_t i_uid;
  uint32_t i_size;
  uint32_t i_atime;
  uint32_t i_ctime;
  uint32_t i_mtime;
  uint32_t i_dtime;
  uint16_t i_gid;
  uint16_t i_links_count;
  uint32_t i_blocks;
  uint32_t i_flags;
  uint32_t i_osd1;
  uint32_t i_block[15];
  uint32_t i_generation;
  uint32_t i_file_acl;
  uint32_t i_size_high;
  uint32_t i_faddr;
  uint32_t i_osd2[3];
} Ext2Inode;

typedef struct
{
  uint16_t i_mode;
  uint16_t i_uid;
  uint32_t i_size;
  uint32_t i_atime;
  uint32_t i_ctime;
  uint32_t i_mtime;
  uint32_t i_dtime;
  uint16_t i_gid;
  uint16_t i_links_count;
  uint32_t i_blocks;
  uint32_t i_flags;
  uint32_t i_osd1;
  uint32_t i_block[15];
  uint32_t i_generation;
  uint32_t i_file_acl;
  uint32_t i_size_high;
  uint32_t i_faddr;
  uint32_t i_osd2[3];
  uint16_t i_extra_isize;
  uint16_t i_checksum_hi;
  uint32_t i_ctime_extra;
  uint32_t i_mtime_extra;
  uint32_t i_atime_extra;
  uint32_t i_crtime;
  uint32_t i_crtime_extra;
  uint32_t i_version_hi;
  uint32_t i_projid;
} Ext2LargeInode;

typedef struct
{
  uint32_t bg_block_bitmap;
  uint32_t bg_inode_bitmap;
  uint32_t bg_inode_table;
  uint16_t bg_free_blocks_count;
  uint16_t bg_free_inodes_count;
  uint16_t bg_used_dirs_count;
  uint16_t bg_flags;
  uint32_t bg_exclude_bitmap_lo;
  uint16_t bg_block_bitmap_csum_lo;
  uint16_t bg_inode_bitmap_csum_lo;
  uint16_t bg_itable_unused;
  uint16_t bg_checksum;
} Ext2GroupDesc;

typedef struct
{
  uint32_t ed_inode;
  uint16_t ed_size;
  uint8_t ed_namelenl;
  uint8_t ed_namelenh;
} Ext2DirEntry;

typedef struct
{
  Ext2Superblock f_super;
  int f_flags;
  int f_fragsize;
  unsigned int f_inode_blocks_per_group;
  mode_t f_umask;
  time_t f_now;
  unsigned int f_group_desc_count;
  unsigned long f_desc_blocks;
  Ext2GroupDesc *f_group_desc;
} Ext2Filesystem;

__BEGIN_DECLS

extern const VFSSuperblockOps ext2_sops;
extern const VFSInodeOps ext2_iops;
extern const VFSDirEntryOps ext2_dops;
extern const VFSFilesystem ext2_vfs;

void ext2_init (void);

int ext2_extend_inode (VFSInode *inode, blkcnt64_t origblocks,
		       blkcnt64_t newblocks);
int ext2_read_blocks (void *buffer, VFSSuperblock *sb, uint32_t block,
		      size_t nblocks);
int ext2_write_blocks (const void *buffer, VFSSuperblock *sb, uint32_t block,
		       size_t nblocks);
int ext2_data_blocks (Ext2Inode *inode, VFSSuperblock *sb, block_t block,
		      blkcnt64_t nblocks, block_t *result);
int ext2_unalloc_data_blocks (VFSInode *inode, block_t start,
			      blkcnt64_t nblocks);
uint32_t ext2_inode_offset (VFSSuperblock *sb, ino64_t inode);
Ext2Inode *ext2_read_inode (VFSSuperblock *sb, ino64_t inode);
int ext2_unref_inode (VFSSuperblock *sb, VFSInode *inode);
block_t ext2_alloc_block (VFSSuperblock *sb, int prefbg);
ino64_t ext2_create_inode (VFSSuperblock *sb, int prefbg);
int ext2_add_entry (VFSInode *dir, VFSInode *inode, const char *name);
uint32_t ext2_bgdt_size (Ext2Superblock *esb);

int ext2_mount (VFSMount *mp, int flags, void *data);
int ext2_unmount (VFSMount *mp, int flags);
VFSInode *ext2_alloc_inode (VFSSuperblock *sb);
void ext2_destroy_inode (VFSInode *inode);
VFSDirectory *ext2_alloc_dir (VFSInode *dir, VFSSuperblock *sb);
void ext2_destroy_dir (VFSDirectory *dir);
void ext2_fill_inode (VFSInode *inode);
void ext2_write_inode (VFSInode *inode);
void ext2_free (VFSSuperblock *sb);
void ext2_update (VFSSuperblock *sb);
int ext2_statfs (VFSSuperblock *sb, struct statfs64 *st);
int ext2_remount (VFSSuperblock *sb, int *flags, void *data);

int ext2_create (VFSInode *dir, const char *name, mode_t mode);
int ext2_lookup (VFSInode **inode, VFSInode *dir, VFSSuperblock *sb,
		 const char *name, int symcount);
int ext2_link (VFSInode *old, VFSInode *dir, const char *new);
int ext2_unlink (VFSInode *dir, const char *name);
int ext2_symlink (VFSInode *dir, const char *old, const char *new);
int ext2_read (VFSInode *inode, void *buffer, size_t len, off_t offset);
int ext2_write (VFSInode *inode, const void *buffer, size_t len, off_t offset);
int ext2_readdir (VFSDirEntry **entry, VFSDirectory *dir, VFSSuperblock *sb);
int ext2_chmod (VFSInode *inode, mode_t mode);
int ext2_chown (VFSInode *inode, uid_t uid, gid_t gid);
int ext2_mkdir (VFSInode *dir, const char *name, mode_t mode);
int ext2_rmdir (VFSInode *dir, const char *name);
int ext2_mknod (VFSInode *dir, const char *name, mode_t mode, dev_t rdev);
int ext2_rename (VFSInode *olddir, const char *oldname, VFSInode *newdir,
		 const char *newname);
int ext2_readlink (VFSInode *inode, char *buffer, size_t len);
int ext2_truncate (VFSInode *inode);
int ext2_getattr (VFSInode *inode, struct stat64 *st);
int ext2_setxattr (VFSInode *inode, const char *name, const void *value,
		   size_t len, int flags);
int ext2_getxattr (VFSInode *inode, const char *name, void *buffer,
		   size_t len);
int ext2_listxattr (VFSInode *inode, char *buffer, size_t len);
int ext2_removexattr (VFSInode *inode, const char *name);
int ext2_compare (VFSDirEntry *entry, const char *a, const char *b);
void ext2_iput (VFSDirEntry *entry, VFSInode *inode);

__END_DECLS

#endif
