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

/* Copyright (C) 1993, 1994, 1995, 1996 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

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

/* Superblock feature flags */

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

#define EXT2_INCOMPAT_SUPPORTED (EXT2_FT_INCOMPAT_FILETYPE	\
				 | EXT3_FT_INCOMPAT_JOURNAL_DEV \
				 | EXT2_FT_INCOMPAT_META_BG	\
				 | EXT3_FT_INCOMPAT_RECOVER	\
				 | EXT3_FT_INCOMPAT_EXTENTS	\
				 | EXT4_FT_INCOMPAT_FLEX_BG	\
				 | EXT4_FT_INCOMPAT_EA_INODE	\
				 | EXT4_FT_INCOMPAT_MMP		\
				 | EXT4_FT_INCOMPAT_64BIT	\
				 | EXT4_FT_INCOMPAT_INLINE_DATA \
				 | EXT4_FT_INCOMPAT_ENCRYPT	\
				 | EXT4_FT_INCOMPAT_CASEFOLD	\
				 | EXT4_FT_INCOMPAT_CSUM_SEED	\
				 | EXT4_FT_INCOMPAT_LARGEDIR)

#define EXT2_RO_COMPAT_SUPPORTED (EXT2_FT_RO_COMPAT_SPARSE_SUPER	\
				  | EXT4_FT_RO_COMPAT_HUGE_FILE		\
				  | EXT2_FT_RO_COMPAT_LARGE_FILE	\
				  | EXT4_FT_RO_COMPAT_DIR_NLINK		\
				  | EXT4_FT_RO_COMPAT_EXTRA_ISIZE	\
				  | EXT4_FT_RO_COMPAT_GDT_CSUM		\
				  | EXT4_FT_RO_COMPAT_BIGALLOC		\
				  | EXT4_FT_RO_COMPAT_QUOTA		\
				  | EXT4_FT_RO_COMPAT_METADATA_CSUM	\
				  | EXT4_FT_RO_COMPAT_READONLY		\
				  | EXT4_FT_RO_COMPAT_PROJECT		\
				  | EXT4_FT_RO_COMPAT_SHARED_BLOCKS	\
				  | EXT4_FT_RO_COMPAT_VERITY)

/* Inode flags */

#define EXT2_SECRM_FL            0x00000001
#define EXT2_UNRM_FL             0x00000002
#define EXT2_COMPR_FL            0x00000004
#define EXT2_SYNC_FL             0x00000008
#define EXT2_IMMUTABLE_FL        0x00000010
#define EXT2_APPEND_FL           0x00000020
#define EXT2_NODUMP_FL           0x00000040
#define EXT4_NOATIME_FL          0x00000080
#define EXT2_DIRTY_FL            0x00000100
#define EXT2_COMPRBLK_FL         0x00000200
#define EXT2_NOCOMPR_FL          0x00000400
#define EXT4_ENCRYPT_FL          0x00000800
#define EXT2_BTREE_FL            0x00001000
#define EXT2_INDEX_FL            0x00001000
#define EXT2_IMAGIC_FL           0x00002000
#define EXT3_JOURNAL_DATA_FL     0x00004000
#define EXT2_NOTAIL_FL           0x00008000
#define EXT2_DIRSYNC_FL          0x00010000
#define EXT2_TOPDIR_FL           0x00020000
#define EXT4_HUGE_FILE_FL        0x00040000
#define EXT4_EXTENTS_FL          0x00080000
#define EXT4_VERITY_FL           0x00100000
#define EXT4_EA_INODE_FL         0x00200000
#define EXT4_SNAPFILE_FL         0x01000000
#define EXT4_SNAPFILE_DELETED_FL 0x04000000
#define EXT4_SNAPFILE_SHRUNK_FL  0x08000000
#define EXT4_INLINE_DATA_FL      0x10000000
#define EXT4_PROJINHERIT_FL      0x20000000
#define EXT4_CASEFOLD_FL         0x40000000
#define EXT2_RESERVED_FL         0x80000000

/* Helper macros */

#define EXT2_FILE_BUFFER_VALID 0x2000
#define EXT2_FILE_BUFFER_DIRTY 0x4000

#define EXT2_OLD_REV     0
#define EXT2_DYNAMIC_REV 1

#define EXT2_OLD_INODE_SIZE  128
#define EXT2_OLD_FIRST_INODE 11

#define EXT2_MIN_BLOCK_LOG_SIZE 10
#define EXT2_MAX_BLOCK_LOG_SIZE 16
#define EXT2_MIN_BLOCK_SIZE (1 << EXT2_MIN_BLOCK_LOG_SIZE)
#define EXT2_MAX_BLOCK_SIZE (1 << EXT2_MAX_BLOCK_LOG_SIZE)

#define EXT2_MIN_DESC_SIZE    32
#define EXT2_MIN_DESC_SIZE_64 64
#define EXT2_MAX_DESC_SIZE    EXT2_MIN_BLOCK_SIZE

#define EXT4_INODE_CSUM_HI_EXTRA_END					\
  (offsetof (Ext2LargeInode, i_checksum_hi) + 2 - EXT2_OLD_INODE_SIZE)
#define EXT4_BG_BLOCK_BITMAP_CSUM_HI_END			\
  (offsetof (Ext4GroupDesc, bg_block_bitmap_csum_hi) + 2)
#define EXT4_BG_INODE_BITMAP_CSUM_HI_END			\
  (offsetof (Ext4GroupDesc, bg_inode_bitmap_csum_hi) + 2)

#define EXT2_FLAG_CHANGED  0x01
#define EXT2_FLAG_DIRTY    0x02
#define EXT2_FLAG_VALID    0x04
#define EXT2_FLAG_IB_DIRTY 0x08
#define EXT2_FLAG_BB_DIRTY 0x10
#define EXT2_FLAG_64BIT    0x20

#define BMAP_ALLOC  0x0001
#define BMAP_SET    0x0002
#define BMAP_UNINIT 0x0004
#define BMAP_ZERO   0x0008

#define BMAP_RET_UNINIT 0x0001

#define BLOCK_ALLOC_UNKNOWN  0
#define BLOCK_ALLOC_DATA     1
#define BLOCK_ALLOC_METADATA 2

#define EXT2_BITMAP_BLOCK 0x0001
#define EXT2_BITMAP_INODE 0x0002

#define EXT2_BMAP_MAGIC_BLOCK   0x0001
#define EXT2_BMAP_MAGIC_INODE   0x0002
#define EXT2_BMAP_MAGIC_BLOCK64 0x0004
#define EXT2_BMAP_MAGIC_INODE64 0x0008
#define EXT2_BMAP_MAGIC_VALID(m) (m == EXT2_BMAP_MAGIC_BLOCK		\
				  || m == EXT2_BMAP_MAGIC_INODE		\
				  || m == EXT2_BMAP_MAGIC_BLOCK64	\
				  || m == EXT2_BMAP_MAGIC_INODE64)

#define EXT2_BG_INODE_UNINIT 0x0001
#define EXT2_BG_BLOCK_UNINIT 0x0002
#define EXT2_BG_BLOCK_ZEROED 0x0004

#define EXT2_EXTENT_FLAGS_LEAF         0x0001
#define EXT2_EXTENT_FLAGS_UNINIT       0x0002
#define EXT2_EXTENT_FLAGS_SECOND_VISIT 0x0004

#define EXT2_EXTENT_CURRENT   0x00
#define EXT2_EXTENT_ROOT      0x01
#define EXT2_EXTENT_LAST_LEAF 0x02
#define EXT2_EXTENT_FIRST_SIB 0x03
#define EXT2_EXTENT_LAST_SIB  0x04
#define EXT2_EXTENT_NEXT_SIB  0x05
#define EXT2_EXTENT_PREV_SIB  0x06
#define EXT2_EXTENT_NEXT_LEAF 0x07
#define EXT2_EXTENT_PREV_LEAF 0x08
#define EXT2_EXTENT_NEXT      0x09
#define EXT2_EXTENT_PREV      0x0a
#define EXT2_EXTENT_UP        0x0b
#define EXT2_EXTENT_DOWN      0x0c
#define EXT2_EXTENT_DOWN_LAST 0x0d
#define EXT2_EXTENT_MOVE_MASK 0x0f

#define EXT2_EXTENT_INSERT_AFTER      1
#define EXT2_EXTENT_INSERT_NOSPLIT    2
#define EXT2_EXTENT_DELETE_KEEP_EMPTY 1
#define EXT2_EXTENT_SET_BMAP_UNINIT   1

#define EXT2_NDIR_BLOCKS 12
#define EXT2_IND_BLOCK   EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK  (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK  (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS    (EXT2_TIND_BLOCK + 1)

#define EXT2_CRC32C_CHECKSUM 1

#define EXT2_BLOCK_SIZE(s) (EXT2_MIN_BLOCK_SIZE << s.s_log_block_size)
#define EXT2_BLOCK_SIZE_BITS(s) (s.s_log_block_size + 10)
#define EXT2_CLUSTER_SIZE(s) (EXT2_MIN_BLOCK_SIZE << s.s_log_cluster_size)
#define EXT2_INODE_SIZE(s)						\
  (s.s_rev_level == EXT2_OLD_REV ? EXT2_OLD_INODE_SIZE : s.s_inode_size)
#define EXT2_DESC_SIZE(s)				\
  (s.s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?	\
   s.s_desc_size : EXT2_MIN_DESC_SIZE)
#define EXT2_INODES_PER_BLOCK(s) (EXT2_BLOCK_SIZE (s) / EXT2_INODE_SIZE (s))
#define EXT2_DESC_PER_BLOCK(s) (EXT2_BLOCK_SIZE (s) / EXT2_DESC_SIZE (s))
#define EXT2_MAX_BLOCKS_PER_GROUP(s)				\
  (65528 * (EXT2_CLUSTER_SIZE (s) / EXT2_BLOCK_SIZE (s)))
#define EXT2_MAX_INODES_PER_GROUP(s) (65536 - EXT2_INODES_PER_BLOCK (s))
#define EXT2_CLUSTER_RATIO(fs) (1 << fs->f_cluster_ratio_bits)
#define EXT2_CLUSTER_MASK(fs) (EXT2_CLUSTER_RATIO (fs) - 1)
#define EXT2_GROUPS_TO_BLOCKS(s, g) ((block_t) s.s_blocks_per_group * (g))
#define EXT2_GROUPS_TO_CLUSTERS(s, g) ((block_t) s.s_clusters_per_group * (g))
#define EXT2_B2C(fs, block) ((block) >> (fs)->f_cluster_ratio_bits)
#define EXT2_I_SIZE(i) ((i).i_size | (uint64_t) (i).i_size_high << 32)
#define EXT2_MAX_STRIDE_LENGTH(sb) (4194304 / (int) sb->sb_blksize)

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
  uint16_t s_reserved_gdt_blocks;
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
  union
  {
    struct
    {
      uint32_t l_i_version;
    } linux1;
    struct
    {
      uint32_t h_i_translator;
    } hurd1;
  } osd1;
  uint32_t i_block[EXT2_N_BLOCKS];
  uint32_t i_generation;
  uint32_t i_file_acl;
  uint32_t i_size_high;
  uint32_t i_faddr;
  union
  {
    struct
    {
      uint16_t l_i_blocks_hi;
      uint16_t l_i_file_acl_hi;
      uint16_t l_i_uid_high;
      uint16_t l_i_gid_high;
      uint16_t l_i_checksum_lo;
#define i_checksum_lo osd2.linux2.l_i_checksum_lo
      uint16_t l_i_reserved;
    } linux2;
    struct
    {
      unsigned char h_i_frag;
      unsigned char h_i_fsize;
      uint16_t h_i_mode_high;
      uint16_t h_i_uid_high;
      uint16_t h_i_gid_high;
      uint32_t h_i_author;
    } hurd2;
  } osd2;
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
  union
  {
    struct
    {
      uint32_t l_i_version;
    } linux1;
    struct
    {
      uint32_t h_i_translator;
    } hurd1;
  } osd1;
  uint32_t i_block[15];
  uint32_t i_generation;
  uint32_t i_file_acl;
  uint32_t i_size_high;
  uint32_t i_faddr;
  union
  {
    struct
    {
      uint16_t l_i_blocks_hi;
      uint16_t l_i_file_acl_hi;
      uint16_t l_i_uid_high;
      uint16_t l_i_gid_high;
      uint16_t l_i_checksum_lo;
      uint16_t l_i_reserved;
    } linux2;
    struct
    {
      unsigned char h_i_frag;
      unsigned char h_i_fsize;
      uint16_t h_i_mode_high;
      uint16_t h_i_uid_high;
      uint16_t h_i_gid_high;
      uint32_t h_i_author;
    } hurd2;
  } osd2;
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
  uint32_t bg_block_bitmap_hi;
  uint32_t bg_inode_bitmap_hi;
  uint32_t bg_inode_table_hi;
  uint16_t bg_free_blocks_count_hi;
  uint16_t bg_free_inodes_count_hi;
  uint16_t bg_used_dirs_count_hi;
  uint16_t bg_itable_unused_hi;
  uint32_t bg_exclude_bitmap_hi;
  uint16_t bg_block_bitmap_csum_hi;
  uint16_t bg_inode_bitmap_csum_hi;
  uint32_t bg_reserved;
} Ext4GroupDesc;

typedef struct
{
  uint32_t ed_inode;
  uint16_t ed_size;
  uint8_t ed_namelenl;
  uint8_t ed_namelenh;
} Ext2DirEntry;

typedef struct
{
  uint32_t mmp_magic;
  uint32_t mmp_seq;
  uint64_t mmp_time;
  unsigned char mmp_nodename[64];
  unsigned char mmp_bdevname[32];
  uint16_t mmp_check_interval;
  uint16_t mmp_pad1;
  uint32_t mmp_pad2[226];
  uint32_t mmp_checksum;
} Ext4MMPBlock;

typedef struct
{
  Ext2Inode f_inode;
  ino64_t f_ino;
  VFSSuperblock *f_sb;
  uint64_t f_pos;
  block_t f_block;
  block_t f_physblock;
  int f_flags;
  char *f_buffer;
} Ext2File;

typedef struct
{
  ino64_t e_ino;
  Ext2Inode *e_inode;
} Ext2InodeCacheEntry;

typedef struct
{
  void *ic_buffer;
  block_t ic_block;
  int ic_cache_last;
  unsigned int ic_cache_size;
  int ic_refcnt;
  Ext2InodeCacheEntry *ic_cache;
} Ext2InodeCache;

typedef enum
{
  EXT2_BMAP64_BITARRAY,
  EXT2_BMAP64_RBTREE,
  EXT2_BMAP64_AUTODIR
} Ext2BitmapType;

typedef struct
{
  int b_magic;
} Ext2Bitmap;

typedef struct
{
  int b_magic;
  uint32_t b_start;
  uint32_t b_end;
  uint32_t b_real_end;
  char *b_bitmap;
} Ext2Bitmap32;

typedef struct _Ext2Bitmap64 Ext2Bitmap64;

typedef struct
{
  Ext2BitmapType b_type;
  int (*b_new_bmap) (VFSSuperblock *, Ext2Bitmap64 *);
  void (*b_free_bmap) (Ext2Bitmap64 *);
  void (*b_mark_bmap) (Ext2Bitmap64 *, uint64_t);
  void (*b_unmark_bmap) (Ext2Bitmap64 *, uint64_t);
  int (*b_test_bmap) (Ext2Bitmap64 *, uint64_t);
  void (*b_mark_bmap_extent) (Ext2Bitmap64 *, uint64_t, unsigned int);
  void (*b_unmark_bmap_extent) (Ext2Bitmap64 *, uint64_t, unsigned int);
  void (*b_set_bmap_range) (Ext2Bitmap64 *, uint64_t, size_t, void *);
  void (*b_get_bmap_range) (Ext2Bitmap64 *, uint64_t, size_t, void *);
  int (*b_find_first_zero) (Ext2Bitmap64 *, uint64_t, uint64_t, uint64_t *);
  int (*b_find_first_set) (Ext2Bitmap64 *, uint64_t, uint64_t, uint64_t *);
} Ext2BitmapOps;

struct _Ext2Bitmap64
{
  int b_magic;
  Ext2BitmapOps *b_ops;
  int b_flags;
  uint64_t b_start;
  uint64_t b_end;
  uint64_t b_real_end;
  int b_cluster_bits;
  void *b_private;
};

#define EXT2_BITMAP_IS_32(b) (b->b_magic == EXT2_BMAP_MAGIC_BLOCK	\
			      || b->b_magic == EXT2_BMAP_MAGIC_INODE)
#define EXT2_BITMAP_IS_64(b) (b->b_magic == EXT2_BMAP_MAGIC_BLOCK64	\
			      || b->b_magic == EXT2_BMAP_MAGIC_INODE64)

typedef struct
{
  Ext2Superblock f_super;
  int f_flags;
  int f_fragsize;
  unsigned int f_group_desc_count;
  unsigned long f_desc_blocks;
  Ext2GroupDesc *f_group_desc;
  unsigned int f_inode_blocks_per_group;
  Ext2Bitmap *f_block_bitmap;
  Ext2Bitmap *f_inode_bitmap;
  uint32_t f_stride;
  mode_t f_umask;
  time_t f_now;
  int f_cluster_ratio_bits;
  uint16_t f_default_bitmap_type;
  Ext2InodeCache *f_icache;
  void *f_mmp_buffer;
  int f_mmp_fd;
  time_t f_mmp_last_written;
  uint32_t f_checksum_seed;
} Ext2Filesystem;

typedef struct
{
  char *p_buffer;
  int p_entries;
  int p_max_entries;
  int p_left;
  int p_visit_num;
  int p_flags;
  block_t p_end_block;
  void *p_curr;
} Ext4ExtentPath;

typedef struct
{
  Ext2Filesystem *eh_fs;
  ino64_t eh_ino;
  Ext2Inode *eh_inode;
  Ext2Inode eh_inode_buf;
  int eh_type;
  int eh_level;
  int eh_max_depth;
  int eh_max_paths;
  Ext4ExtentPath *path;
} Ext4ExtentHandle;

typedef struct
{
  block_t e_pblk;
  block_t e_lblk;
  uint32_t e_len;
  uint32_t e_flags;
} Ext4Extent;

typedef struct
{
  ino64_t bc_ino;
  Ext2Inode *bc_inode;
  block_t bc_block;
  int bc_flags;
} Ext2BlockAllocContext;

__BEGIN_DECLS

extern const VFSSuperblockOps ext2_sops;
extern const VFSInodeOps ext2_iops;
extern const VFSDirEntryOps ext2_dops;
extern const VFSFilesystem ext2_vfs;
extern Ext2BitmapOps ext2_bitarray_ops;

static inline blkcnt64_t
ext2_blocks_count (const Ext2Superblock *s)
{
  return (blkcnt64_t) s->s_blocks_count |
    (s->s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (blkcnt64_t) s->s_blocks_count_hi << 32 : 0);
}

static inline void
ext2_blocks_count_set (Ext2Superblock *s, blkcnt64_t blocks)
{
  s->s_blocks_count = blocks;
  if (s->s_feature_incompat & EXT4_FT_INCOMPAT_64BIT)
    s->s_blocks_count_hi = blocks >> 32;
}

static inline void
ext2_blocks_count_add (Ext2Superblock *s, blkcnt64_t blocks)
{
  blkcnt64_t temp = ext2_blocks_count (s) + blocks;
  ext2_blocks_count_set (s, temp);
}

static inline blkcnt64_t
ext2_r_blocks_count (const Ext2Superblock *s)
{
  return (blkcnt64_t) s->s_r_blocks_count |
    (s->s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (blkcnt64_t) s->s_r_blocks_count_hi << 32 : 0);
}

static inline void
ext2_r_blocks_count_set (Ext2Superblock *s, blkcnt64_t blocks)
{
  s->s_r_blocks_count = blocks;
  if (s->s_feature_incompat & EXT4_FT_INCOMPAT_64BIT)
    s->s_r_blocks_count_hi = blocks >> 32;
}

static inline void
ext2_r_blocks_count_add (Ext2Superblock *s, blkcnt64_t blocks)
{
  blkcnt64_t temp = ext2_r_blocks_count (s) + blocks;
  ext2_r_blocks_count_set (s, temp);
}

static inline blkcnt64_t
ext2_free_blocks_count (const Ext2Superblock *s)
{
  return (blkcnt64_t) s->s_free_blocks_count |
    (s->s_feature_incompat & EXT4_FT_INCOMPAT_64BIT ?
     (blkcnt64_t) s->s_free_blocks_hi << 32 : 0);
}

static inline void
ext2_free_blocks_count_set (Ext2Superblock *s, blkcnt64_t blocks)
{
  s->s_free_blocks_count = blocks;
  if (s->s_feature_incompat & EXT4_FT_INCOMPAT_64BIT)
    s->s_free_blocks_hi = blocks >> 32;
}

static inline void
ext2_free_blocks_count_add (Ext2Superblock *s, blkcnt64_t blocks)
{
  blkcnt64_t temp = ext2_free_blocks_count (s) + blocks;
  ext2_free_blocks_count_set (s, temp);
}

static inline int
ext2_has_group_desc_checksum (const Ext2Superblock *s)
{
  return s->s_feature_ro_compat &
    (EXT4_FT_RO_COMPAT_GDT_CSUM | EXT4_FT_RO_COMPAT_METADATA_CSUM) ? 1 : 0;
}

static inline block_t
ext2_group_first_block (const Ext2Filesystem *fs, unsigned int group)
{
  return fs->f_super.s_first_data_block +
    EXT2_GROUPS_TO_BLOCKS (fs->f_super, group);
}

static inline block_t
ext2_group_last_block (const Ext2Filesystem *fs, unsigned int group)
{
  return group == fs->f_group_desc_count - 1 ?
    ext2_blocks_count (&fs->f_super) - 1 : ext2_group_first_block (fs, group) +
    fs->f_super.s_blocks_per_group - 1;
}

static inline blkcnt64_t
ext2_group_blocks_count (const Ext2Filesystem *fs, unsigned int group)
{
  blkcnt64_t nblocks;
  if (group == fs->f_group_desc_count - 1)
    {
      nblocks = (ext2_blocks_count (&fs->f_super) -
		 fs->f_super.s_first_data_block) %
	fs->f_super.s_blocks_per_group;
      if (nblocks == 0)
	nblocks = fs->f_super.s_blocks_per_group;
    }
  else
    nblocks = fs->f_super.s_blocks_per_group;
  return nblocks;
}

static inline unsigned int
ext2_group_of_block (const Ext2Filesystem *fs, block_t block)
{
  return (block - fs->f_super.s_first_data_block) /
    fs->f_super.s_blocks_per_group;
}

static inline unsigned int
ext2_group_of_inode (const Ext2Filesystem *fs, ino64_t inode)
{
  return (inode - 1) / fs->f_super.s_inodes_per_group;
}

static inline int
ext2_is_inline_symlink (const Ext2Inode *inode)
{
  return S_ISLNK (inode->i_mode)
    && EXT2_I_SIZE (*inode) < sizeof (inode->i_block);
}

void ext2_init (void);

int ext2_bg_has_super (Ext2Filesystem *fs, unsigned int group);
int ext2_bg_test_flags (VFSSuperblock *sb, unsigned int group, uint16_t flags);
void ext2_bg_clear_flags (VFSSuperblock *sb, unsigned int group,
			  uint16_t flags);
int ext4_mmp_start (VFSSuperblock *sb);
int ext4_mmp_stop (VFSSuperblock *sb);
Ext2GroupDesc *ext2_group_desc (VFSSuperblock *sb, Ext2GroupDesc *gdp,
				unsigned int group);
Ext4GroupDesc *ext4_group_desc (VFSSuperblock *sb, Ext2GroupDesc *gdp,
				unsigned int group);
void ext2_super_bgd_loc (VFSSuperblock *sb, unsigned int group, block_t *super,
			 block_t *old_desc, block_t *new_desc,
			 blkcnt64_t *used);
block_t ext2_block_bitmap_loc (VFSSuperblock *sb, unsigned int group);
block_t ext2_inode_bitmap_loc (VFSSuperblock *sb, unsigned int group);
block_t ext2_inode_table_loc (VFSSuperblock *sb, unsigned int group);
block_t ext2_descriptor_block (VFSSuperblock *sb, block_t group_block,
			       unsigned int i);
uint32_t ext2_bg_free_blocks_count (VFSSuperblock *sb, unsigned int group);
void ext2_bg_free_blocks_count_set (VFSSuperblock *sb, unsigned int group,
				    uint32_t blocks);
uint32_t ext2_bg_free_inodes_count (VFSSuperblock *sb, unsigned int group);
void ext2_bg_free_inodes_count_set (VFSSuperblock *sb, unsigned int group,
				    uint32_t inodes);
int ext2_read_bitmap (VFSSuperblock *sb, int flags, unsigned int start,
		      unsigned int end);
void ext2_mark_bitmap (Ext2Bitmap *bmap, uint64_t arg);
void ext2_unmark_bitmap (Ext2Bitmap *bmap, uint64_t arg);
void ext2_mark_block_bitmap_range (Ext2Bitmap *bmap, block_t block,
				   blkcnt64_t num);
int ext2_set_bitmap_range (Ext2Bitmap *bmap, uint64_t start, unsigned int num,
			   void *data);
int ext2_find_first_zero_bitmap (Ext2Bitmap *bmap, block_t start, block_t end,
				 block_t *result);
void ext2_block_alloc_stats (VFSSuperblock *sb, block_t block, int inuse);
int ext2_open_file (VFSSuperblock *sb, ino64_t inode, Ext2File *file);
int ext2_read_inode (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode);
int ext2_update_inode (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode);
block_t ext2_find_inode_goal (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
			      block_t block);
int ext2_create_inode_cache (VFSSuperblock *sb, unsigned int cache_size);
void ext2_free_inode_cache (Ext2InodeCache *cache);
int ext2_flush_inode_cache (Ext2InodeCache *cache);
int ext2_file_flush (Ext2File *file);
int ext2_sync_file_buffer_pos (Ext2File *file);
int ext2_load_file_buffer (Ext2File *file, int nofill);
int ext2_bmap (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
	       char *blockbuf, int flags, block_t block, int *retflags,
	       block_t *physblock);
int ext4_extent_open (VFSSuperblock *sb, ino64_t ino, Ext2Inode *inode,
		      Ext4ExtentHandle **handle);
int ext4_extent_goto (Ext4ExtentHandle *handle, int leaflvl, block_t block);
int ext4_extent_get (Ext4ExtentHandle *handle, int flags, Ext4Extent *extent);
int ext4_extent_set_bmap (Ext4ExtentHandle *handle, block_t logical,
			  block_t physical, int flags);
void ext4_extent_free (Ext4ExtentHandle *handle);
int ext2_iblk_add_blocks (VFSSuperblock *sb, Ext2Inode *inode, block_t nblocks);
int ext2_zero_blocks (VFSSuperblock *sb, block_t block, int num,
		      block_t *result, int *count);
int ext2_alloc_block (VFSSuperblock *sb, block_t goal, char *blockbuf,
		      block_t *result, Ext2BlockAllocContext *ctx);
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
int ext2_unref_inode (VFSSuperblock *sb, VFSInode *inode);
block_t ext2_old_alloc_block (VFSSuperblock *sb, int prefbg);
ino64_t ext2_create_inode (VFSSuperblock *sb, int prefbg);
int ext2_add_entry (VFSInode *dir, VFSInode *inode, const char *name);
uint32_t ext2_bgdt_size (Ext2Superblock *esb);
int ext2_superblock_checksum_valid (Ext2Filesystem *fs);
uint16_t ext2_bg_checksum (VFSSuperblock *sb, unsigned int group);
void ext2_bg_checksum_update (VFSSuperblock *sb, unsigned int group,
			      uint16_t checksum);
uint16_t ext2_group_desc_checksum (VFSSuperblock *sb, unsigned int group);
int ext2_group_desc_checksum_valid (VFSSuperblock *sb, unsigned int group);
void ext2_group_desc_checksum_update (VFSSuperblock *sb, unsigned int group);
uint32_t ext2_inode_checksum (Ext2Filesystem *fs, ino64_t ino,
			      Ext2LargeInode *inode, int has_hi);
int ext2_inode_checksum_valid (Ext2Filesystem *fs, ino64_t ino,
			       Ext2LargeInode *inode);
void ext2_inode_checksum_update (Ext2Filesystem *fs, ino64_t ino,
				 Ext2LargeInode *inode);
uint32_t ext2_block_bitmap_checksum (VFSSuperblock *sb, unsigned int group);
int ext2_block_bitmap_checksum_valid (VFSSuperblock *sb, unsigned int group,
				      char *bitmap, int size);
void ext2_block_bitmap_checksum_update (VFSSuperblock *sb, unsigned int group,
					char *bitmap, int size);
uint32_t ext2_inode_bitmap_checksum (VFSSuperblock *sb, unsigned int group);
int ext2_inode_bitmap_checksum_valid (VFSSuperblock *sb, unsigned int group,
				      char *bitmap, int size);
void ext2_inode_bitmap_checksum_update (VFSSuperblock *sb, unsigned int group,
					char *bitmap, int size);

int ext2_mount (VFSMount *mp, int flags, void *data);
int ext2_unmount (VFSMount *mp, int flags);
VFSInode *ext2_alloc_inode (VFSSuperblock *sb);
void ext2_destroy_inode (VFSInode *inode);
VFSDirectory *ext2_alloc_dir (VFSInode *dir, VFSSuperblock *sb);
void ext2_destroy_dir (VFSDirectory *dir);
int ext2_fill_inode (VFSInode *inode);
int ext2_write_inode (VFSInode *inode);
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
