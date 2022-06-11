/*
 * dir.h - Exports for directory handling. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002 Anton Altaparmakov
 * Copyright (c) 2005 Yura Pakhuchiy
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_DIR_H
#define _NTFS_DIR_H

#include "types.h"

#define PATH_SEP '/'

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

/* The little endian Unicode string $I30 as a global constant. */
extern ntfschar I30[5];

extern u64 ntfs_inode_lookup_by_name(ntfs_inode *dir_ni,
		const ntfschar *uname, const int uname_len);

extern ntfs_inode *ntfs_pathname_to_inode(ntfs_volume *vol, ntfs_inode *parent,
		const char *pathname);

extern ntfs_inode *ntfs_create(ntfs_inode *dir_ni, ntfschar *name, u8 name_len,
		const unsigned type);
extern int ntfs_delete(ntfs_inode *ni, ntfs_inode *dir_ni, ntfschar *name,
		u8 name_len);
extern int ntfs_link(ntfs_inode *ni, ntfs_inode *dir_ni, ntfschar *name,
		u8 name_len);

/*
 * File types (adapted from include <linux/fs.h>)
 */
#define NTFS_DT_UNKNOWN		0
#define NTFS_DT_FIFO		1
#define NTFS_DT_CHR		2
#define NTFS_DT_DIR		4
#define NTFS_DT_BLK		6
#define NTFS_DT_REG		8
#define NTFS_DT_LNK		10
#define NTFS_DT_SOCK		12
#define NTFS_DT_WHT		14

/*
 * This is the "ntfs_filldir" function type, used by ntfs_readdir() to let
 * the caller specify what kind of dirent layout it wants to have.
 * This allows the caller to read directories into their application or
 * to have different dirent layouts depending on the binary type.
 */
typedef int (*ntfs_filldir_t)(void *dirent, const ntfschar *name,
		const int name_len, const int name_type, const s64 pos,
		const MFT_REF mref, const unsigned dt_type);

extern int ntfs_readdir(ntfs_inode *dir_ni, s64 *pos,
		void *dirent, ntfs_filldir_t filldir);

#endif /* defined _NTFS_DIR_H */

