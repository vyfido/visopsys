//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  disk.h
//

// This file contains definitions and structures for using and manipulating
// disk in Visopsys.

#if !defined(_DISK_H)

#define MAX_DESCRIPTION_LENGTH 80

typedef enum { floppy, idecdrom, scsicdrom, idedisk, scsidisk } diskType;
typedef enum { fixed, removable } mediaType;

typedef struct
{
  int number;
  int physicalDevice;
  char description[MAX_DESCRIPTION_LENGTH];
  diskType type;
  mediaType fixedRemovable;

  unsigned startHead;
  unsigned startCylinder;
  unsigned startSector;
  unsigned startLogicalSector;

  unsigned heads;
  unsigned cylinders;
  unsigned sectors;
  unsigned logicalSectors;
  unsigned sectorSize;

} disk;

#define _DISK_H
#endif
