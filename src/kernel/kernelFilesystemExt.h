//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
// 
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the Free
//  Software Foundation; either version 2 of the License, or (at your option)
//  any later version.
// 
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
//  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
//  for more details.
//  
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  kernelFilesystemExt.h
//

#if !defined(_KERNELFILESYSTEMEXT_H)

#include "kernelDisk.h"
#include "kernelFilesystem.h"
#include "kernelLock.h"

/*
  The organisation of an ext2 file system on a floppy:

  offset # of blocks description
  -------- ----------- -----------
  0                  1 boot record
  -- block group 0 --
  (1024 bytes)       1 superblock
  2                  1 group descriptors
  3                  1 block bitmap
  4                  1 inode bitmap
  5                 23 inode table
  28              1412 data blocks

  The organisation of a 20MB ext2 file system:

  offset # of blocks description
  -------- ----------- -----------
  0                  1 boot record
  -- block group 0 --
  (1024 bytes)       1 superblock
  2                  1 group descriptors
  3                  1 block bitmap
  4                  1 inode bitmap
  5                214 inode table
  219             7974 data blocks
  -- block group 1 --
  8193               1 superblock backup
  8194               1 group descriptors backup
  8195               1 block bitmap
  8196               1 inode bitmap
  8197             214 inode table
  8408            7974 data blocks
  -- block group 2 --
  16385              1 block bitmap
  16386              1 inode bitmap
  16387            214 inode table
  16601           3879 data blocks
*/

// Definitions

// Superblock-related constants
#define EXT_SUPERBLOCK_SECTOR    2
#define EXT_SUPERBLOCK_SIZE      1024
#define EXT_SUPERBLOCK_DATASIZE  236
#define EXT_MAGICNUMBER_OFFSET   56
#define EXT_MAGICNUMBER          0xEF53

// EXT_ERRORS values for the 'errors' field in the superblock
#define EXT_ERRORS_CONTINUE      1  // Continue as if nothing happened
#define EXT_ERRORS_RO            2  // Remount read-only
#define EXT_ERRORS_PANIC         3  // Cause a kernel panic
#define EXT_ERRORS_DEFAULT       EXT2_ERRORS_CONTINUE

// EXT_OS: 32-bit identifier of the OS that created the file system for
// the 'creator_os' field in the superblock
#define EXT_OS_LINUX             0          // Linux
#define EXT_OS_HURD              1          // Hurd
#define EXT_OS_MASIX             2          // MASIX
#define EXT_OS_FREEBSD           3          // FreeBSD
#define EXT_OS_LITES4            4          // Lites
#define EXT_OS_VISOPSYS          0xA600D05  // Visopsys

// 32-bit revision level value for the 'rev_level' field in the superblock
#define EXT_GOOD_OLD_REV         0  // Original format
#define EXT_DYNAMIC_REV          1  // V2 format with dynamic inode sizes
// If the revision level (above) is EXT_GOOD_OLD_REV, here are a coupla
// fixed values
#define EXT_GOOD_OLD_FIRST_INODE 11
#define EXT_GOOD_OLD_INODE_SIZE  128

// Reserved inode numbers for the inode table
#define EXT_BAD_INO              1  // Bad blocks inode
#define EXT_ROOT_INO             2  // Root directory inode
#define EXT_ACL_IDX_INO          3  // ACL index inode
#define EXT_ACL_DATA_INO         4  // ACL data inode
#define EXT_BOOT_LOADER_INO      5  // Boot loader inode
#define EXT_UNDEL_DIR_INO        6  // Undelete directory inode

// File types for the file_type field in extDirectoryEntry
#define EXT_FT_UNKNOWN           0
#define EXT_FT_REG_FILE          1
#define EXT_FT_DIR               2
#define EXT_FT_CHRDEV            3
#define EXT_FT_BLKDEV            4
#define EXT_FT_FIFO              5
#define EXT_FT_SOCK              6
#define EXT_FT_SYMLINK           7
#define EXT_FT_MAX               8

// EXT_S_: 16-bit value used to indicate the format of the described file
// and the access rights for the i_mode field in extInode
//                 -- file format --
#define EXT_S_IFMT               0xF000  // Format mask
#define EXT_S_IFSOCK             0xC000  // Socket
#define EXT_S_IFLNK              0xA000  // Symbolic link
#define EXT_S_IFREG              0x8000  // Regular file
#define EXT_S_IFBLK              0x6000  // Block device
//                 -- access rights --
#define EXT_S_IFDIR              0x4000  // Directory
#define EXT_S_IFCHR              0x2000  // Character device
#define EXT_S_IFIFO              0x1000  // Fifo
#define EXT_S_ISUID              0x0800  // SUID
#define EXT_S_ISGID              0x0400  // SGID
#define EXT_S_ISVTX              0x0200  // Sticky bit
#define EXT_S_IRWXU              0x01C0  // User access rights mask
#define EXT_S_IRUSR              0x0100  // Read
#define EXT_S_IWUSR              0x0080  // Write
#define EXT_S_IXUSR              0x0040  // Execute
#define EXT_S_IRWXG              0x0038  // Group access rights mask
#define EXT_S_IRGRP              0x0020  // Read
#define EXT_S_IWGRP              0x0010  // Write
#define EXT_S_IXGRP              0x0008  // Execute
#define EXT_S_IRWXO              0x0007  // Others access rights mask
#define EXT_S_IROTH              0x0004  // Read
#define EXT_S_IWOTH              0x0002  // Write
#define EXT_S_IXOTH              0x0001  // Execute

// Values for the 'flags' field in extInode
#define EXT_SECRM_FL             0x00000001  // Secure deletion
#define EXT_UNRM_FL              0x00000002  // Record for undelete
#define EXT_COMPR_FL             0x00000004  // Compressed file
#define EXT_SYNC_FL              0x00000008  // Synchronous updates
#define EXT_IMMUTABLE_FL         0x00000010  // Immutable file
#define EXT_APPEND_FL            0x00000020  // Append only
#define EXT_NODUMP_FL            0x00000040  // Do not dump/delete file
#define EXT_NOATIME_FL           0x00000080  // Do not update .i_atime
#define EXT_DIRTY_FL             0x00000100  // Dirty (file is in use?)
#define EXT_COMPRBLK_FL          0x00000200  // Compressed blocks
#define EXT_NOCOMPR_FL           0x00000400  // Access raw compressed data
#define EXT_ECOMPR_FL            0x00000800  // Compression error
#define EXT_BTREE_FL             0x00001000  // B-tree format directory
#define EXT_INDEX_FL             0x00010000  // Hash indexed directory

// Structures

typedef volatile struct {
  unsigned block_bitmap;
  unsigned inode_bitmap;
  unsigned inode_table;
  unsigned short free_blocks_count;
  unsigned short free_inodes_count;
  unsigned short used_dirs_count;
  unsigned short pad;
  unsigned char reserved[12];

} extGroupDescriptor;

typedef volatile struct {
  unsigned short i_mode;
  unsigned short uid;
  unsigned size;
  unsigned atime;
  unsigned ctime;
  unsigned mtime;
  unsigned dtime;
  unsigned short gid;
  unsigned short links_count;
  unsigned blocks;
  unsigned flags;
  unsigned osd1;
  unsigned block[15];
  unsigned generation;
  unsigned file_acl;
  unsigned dir_acl;
  unsigned faddr;
  unsigned char osd2[12];

} extInode;

typedef volatile struct {
  extInode inode;
  // For keeping a list of free inode memory
  void *next;

} extInodeData;

typedef volatile struct {
  unsigned inode;
  unsigned short rec_len;
  unsigned char name_len;
  unsigned char file_type;
  char name[256];

} extDirectoryEntry;

typedef volatile struct {
  unsigned inodes_count;
  unsigned blocks_count;
  unsigned r_blocks_count;
  unsigned free_blocks_count;
  unsigned free_inodes_count;
  unsigned first_data_block;
  unsigned log_block_size;
  unsigned log_frag_size;
  unsigned blocks_per_group;
  unsigned frags_per_group;
  unsigned inodes_per_group;
  unsigned mtime;
  unsigned wtime;
  unsigned short mnt_count;
  unsigned short max_mnt_count;
  unsigned short magic;
  unsigned short state;
  unsigned short errors;
  unsigned short minor_rev_level;
  unsigned lastcheck;
  unsigned checkinterval;
  unsigned creator_os;
  unsigned rev_level;
  unsigned short def_resuid;
  unsigned short def_resgid;
  // EXT2_DYNAMIC_REV Specific
  unsigned first_ino;
  unsigned short inode_size;
  unsigned short block_group_nr;
  unsigned feature_compat;
  unsigned feature_incompat;
  unsigned feature_ro_compat;
  unsigned char uuid[16];
  unsigned char volume_name[16];
  unsigned char last_mounted[64];
  unsigned algo_bitmap;
  // Performance Hints
  unsigned char prealloc_blocks;
  unsigned char prealloc_dir_blocks;
  unsigned short alignment;
  // Journaling Support
  unsigned char journal_uuid[16];
  unsigned journal_inum;
  unsigned journal_dev;
  unsigned last_orphan;
  // End of superblock data
  unsigned blockSize;
  unsigned sectorsPerBlock;
  extGroupDescriptor *groups;
  unsigned numGroups;
  const kernelDisk *disk;

} extInternalData;

// Functions exported by kernelFileSystemExt.c, not defined elsewhere.

int kernelFilesystemExtDetect(const kernelDisk *);
int kernelFilesystemExtMount(kernelFilesystem *);
int kernelFilesystemExtUnmount(kernelFilesystem *);
unsigned kernelFilesystemExtGetFreeBytes(kernelFilesystem *);
int kernelFilesystemExtNewEntry(kernelFileEntry *);
int kernelFilesystemExtInactiveEntry(kernelFileEntry *);
int kernelFilesystemExtResolveLink(kernelFileEntry *);
int kernelFilesystemExtReadFile(kernelFileEntry *, unsigned, unsigned,
				unsigned char *);
int kernelFilesystemExtWriteFile(kernelFileEntry *, unsigned, unsigned,
				 unsigned char *);
int kernelFilesystemExtCreateFile(kernelFileEntry *);
int kernelFilesystemExtDeleteFile(kernelFileEntry *, int);
int kernelFilesystemExtFileMoved(kernelFileEntry *);
int kernelFilesystemExtReadDir(kernelFileEntry *);
int kernelFilesystemExtWriteDir(kernelFileEntry *);
int kernelFilesystemExtMakeDir(kernelFileEntry *);
int kernelFilesystemExtRemoveDir(kernelFileEntry *);
int kernelFilesystemExtTimestamp(kernelFileEntry *);

#define _KERNELFILESYSTEMEXT_H
#endif
