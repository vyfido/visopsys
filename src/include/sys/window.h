// 
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  window.h
//

// This file describes things needed for interaction with the kernel's
// window manager and the Visopsys GUI.

#if !defined(_WINDOW_H)

#include <sys/image.h>
#include <sys/stream.h>

// Window events/masks
#define EVENT_MOUSE_DOWN   0x0001
#define EVENT_MOUSE_UP     0x0002
#define EVENT_MOUSE_MOVE   0x0004
#define EVENT_MOUSE_DRAG   0x0008
#define EVENT_MASK_MOUSE   0x000F
#define EVENT_KEY_DOWN     0x0010
#define EVENT_KEY_UP       0x0020
#define EVENT_MASK_KEY     0x00F0
#define EVENT_WINDOW_CLOSE 0x0100
#define EVENT_MASK_WINDOW  0x0F00

// The maximum numbers of window things
#define WINDOW_MAXWINDOWS 256
#define WINDOW_MAX_EVENTS 512
#define WINDOW_MAX_TITLE_LENGTH 80
#define WINDOW_MAX_COMPONENTS 256
#define WINDOW_MAX_LABEL_LENGTH 64

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

// An "object key".  Really a pointer to an object in kernel memory, but
// of course not usable by applications other than as a reference
typedef void * objectKey;

// A structure for containing various types of window events.
typedef struct {
  unsigned type;
  unsigned xPosition;
  unsigned yPosition;
  unsigned key;

} windowEvent;

// A structure for a queue of window events as a stream.
typedef struct {
  objectKey component;
  stream s;

} windowEventStream;

int windowRegisterEventHandler(objectKey, void (*)(objectKey, windowEvent *));
void windowGuiRun(void);
void windowGuiThread(void);
void windowGuiStop(void);
int windowNewInfoDialog(objectKey, char *, char *);
int windowNewErrorDialog(objectKey, char *, char *);
int windowNewQueryDialog(objectKey, char *, char *);

#define _WINDOW_H
#endif