// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
#include <sys/progress.h>
#include <sys/stream.h>

#ifndef _X_
#define _X_
#endif

// Window events/masks.  This first batch are "tier 2" events, produced by
// windows, widgets, etc. to indicate that some more abstract thing has
// happened.
#define EVENT_MASK_WINDOW        0xF000
#define EVENT_WINDOW_RESIZE      0x4000
#define EVENT_WINDOW_CLOSE       0x2000
#define EVENT_WINDOW_MINIMIZE    0x1000
#define EVENT_SELECTION          0x0400
// And these are "tier 1" events, produced by direct actions of the user.
#define EVENT_MASK_KEY           0x0300
#define EVENT_KEY_UP             0x0200
#define EVENT_KEY_DOWN           0x0100
#define EVENT_MASK_MOUSE         0x00FF
#define EVENT_MOUSE_DRAG         0x0080
#define EVENT_MOUSE_MOVE         0x0040
#define EVENT_MOUSE_RIGHTUP      0x0020
#define EVENT_MOUSE_RIGHTDOWN    0x0010
#define EVENT_MOUSE_MIDDLEUP     0x0008
#define EVENT_MOUSE_MIDDLEDOWN   0x0004
#define EVENT_MOUSE_LEFTUP       0x0002
#define EVENT_MOUSE_LEFTDOWN     0x0001

// The maximum numbers of window things
#define WINDOW_MAXWINDOWS        256
#define WINDOW_MAX_EVENTS        512
#define WINDOW_MAX_TITLE_LENGTH  80
#define WINDOW_MAX_COMPONENTS    256
#define WINDOW_MAX_LABEL_LENGTH  80
#define WINDOW_MAX_LISTITEMS     64
#define WINDOW_MAX_MENUS         16

// Flags for file browsing widgets/dialogs.
#define WINFILEBROWSE_CAN_CD     0x01
#define WINFILEBROWSE_CAN_DEL    0x02
#define WINFILEBROWSE_ALL        (WINFILEBROWSE_CAN_CD | WINFILEBROWSE_CAN_DEL)

// Some image file names for dialog boxes
#define INFOIMAGE_NAME           "/system/icons/infoicon.bmp"
#define ERRORIMAGE_NAME          "/system/icons/bangicon.bmp"
#define QUESTIMAGE_NAME          "/system/icons/questicon.bmp"
#define WAITIMAGE_NAME           "/system/mouse/mousebsy.bmp"

// An "object key".  Really a pointer to an object in kernel memory, but
// of course not usable by applications other than as a reference
typedef void * objectKey;

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
  int fixedWidth;
  int fixedHeight;
  int hasBorder;
  int stickyFocus;
  int useDefaultForeground;
  color foreground;
  int useDefaultBackground;
  color background;
  objectKey font;

} componentParameters;

// A structure for containing various types of window events.
typedef struct {
  unsigned type;
  int xPosition;
  int yPosition;
  unsigned key;

} windowEvent;

// A structure for a queue of window events as a stream.
typedef struct {
  objectKey component;
  volatile stream s;

} windowEventStream;

// Types of drawing operations
typedef enum {
  draw_pixel, draw_line, draw_rect, draw_oval, draw_image, draw_text
} drawOperation;

// Parameters for any drawing operations
typedef struct {
  drawOperation operation;
  drawMode mode;
  color foreground;
  color background;
  int xCoord1;
  int yCoord1;
  int xCoord2;
  int yCoord2;
  unsigned width;
  unsigned height;
  int thickness;
  int fill;
  objectKey font;
  void *data;

} windowDrawParameters;

// Types of scroll bars
typedef enum {
  scrollbar_vertical, scrollbar_horizontal
} scrollBarType;

// A structure for specifying display percentage and display position
// in scroll bar components
typedef struct {
  unsigned displayPercent;
  unsigned positionPercent;

} scrollBarState;

// Types of window list displays
typedef enum {
  windowlist_textonly, windowlist_icononly
} windowListType;

typedef struct {
  char text[WINDOW_MAX_LABEL_LENGTH];
  image iconImage;

} listItemParameters;

void windowCenterDialog(objectKey, objectKey);
int windowClearEventHandler(objectKey);
int windowClearEventHandlers(void);
int windowDestroyFileList(objectKey);
void windowGuiRun(void);
void windowGuiStop(void);
int windowGuiThread(void);
int windowGuiThreadPid(void);
objectKey windowNewBannerDialog(objectKey, const char *, const char *);
int windowNewChoiceDialog(objectKey, const char *, const char *, char *[],
			  int, int);
int windowNewColorDialog(objectKey, color *);
int windowNewErrorDialog(objectKey, const char *, const char *);
int windowNewFileDialog(objectKey, const char *, const char *, const char *,
			char *, unsigned);
objectKey windowNewFileList(objectKey, windowListType, int, int, const char *,
			    int, void *, componentParameters *);
int windowNewInfoDialog(objectKey, const char *, const char *);
int windowNewPasswordDialog(objectKey, const char *, const char *, int,
			    char *);
objectKey windowNewProgressDialog(objectKey, const char *, progress *);
int windowNewPromptDialog(objectKey, const char *, const char *, int, int,
			  char *);
int windowNewQueryDialog(objectKey, const char *, const char *);
int windowNewRadioDialog(objectKey, const char *, const char *, char *[],
			 int, int);
int windowProgressDialogDestroy(objectKey);
int windowRegisterEventHandler(objectKey, void (*)(objectKey, windowEvent *));
int windowUpdateFileList(objectKey, const char *);

#define _WINDOW_H
#endif
