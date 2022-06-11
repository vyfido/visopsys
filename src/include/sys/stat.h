//
//  Visopsys
//  Copyright (C) 1998-2015 J. Andrew McLaughlin
//
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  stat.h
//

// This file is the Visopsys implementation of the standard <sys/stat.h>
// file found in Unix.

#if !defined(_STAT_H)

// Contains the time_t definition
#include <time.h>
// Contains the rest of the definitions
#include <sys/types.h>

struct stat {
	dev_t st_dev;			// device
	ino_t st_ino;			// inode
	mode_t st_mode;			// protection
	nlink_t st_nlink;		// number of hard links
	uid_t st_uid;			// user ID of owner
	gid_t st_gid;			// group ID of owner
	dev_t st_rdev;			// device type (if inode device)
	off_t st_size;			// total size, in bytes
	blksize_t st_blksize;	// blocksize for filesystem I/O
	blkcnt_t st_blocks;		// number of blocks allocated
	time_t st_atime;		// time of last access
	time_t st_mtime;		// time of last modification
	time_t st_ctime;		// time of last change
};

#define _STAT_H
#endif

