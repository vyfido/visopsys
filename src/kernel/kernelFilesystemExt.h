//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
#include <sys/ext.h>

// Structures

typedef struct {
  extInode inode;
  // For keeping a list of free inode memory
  void *next;

} extInodeData;

typedef volatile struct {
  extSuperblock superblock;
  unsigned blockSize;
  unsigned sectorsPerBlock;
  extGroupDescriptor *groups;
  unsigned numGroups;
  const kernelDisk *disk;

} extInternalData;

#define _KERNELFILESYSTEMEXT_H
#endif
