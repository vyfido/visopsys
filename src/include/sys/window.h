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
//  window.h
//

// This file describes things needed for interaction with the kernel's
// window manager

#if !defined(_WINDOW_H)

#include <sys/image.h>

// These describe the X orientation and Y orientation of a component,
// respectively, within its grid cell

typedef enum {
  orient_left, orient_center, orient_right
}  componentXOrientation;
typedef enum {
  orient_top, orient_middle, orient_bottom
}  componentYOrientation;

// This structure is needed to describe parameters consistent to all
// window components

typedef struct {
  int gridX;
  int gridY;
  int gridWidth;
  int gridHeight;
  int padLeft;
  int padRight;
  int padTop;
  int padBottom;
  componentXOrientation orientationX;
  componentYOrientation orientationY;
  int hasBorder;
  int useDefaultForeground;
  color foreground;
  int useDefaultBackground;
  color background;

} componentParameters;

#define _WINDOW_H
#endif
