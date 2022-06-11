//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  msdos.h
//

// This is the header for the handling of MS-DOS disk labels

#if !defined(_MSDOS_H)

typedef struct {
  unsigned char driveActive;
  unsigned char startHead;
  unsigned char startCylSect;
  unsigned char startCyl;
  unsigned char tag;
  unsigned char endHead;
  unsigned char endCylSect;
  unsigned char endCyl;
  unsigned startLogical;
  unsigned sizeLogical;

} __attribute__((packed)) msdosEntry;

typedef struct {
  msdosEntry entries[4];

} __attribute__((packed)) msdosTable;

// Function to get the disk label structure
diskLabel *getLabelMsdos(void);

#define _MSDOS_H
#endif
