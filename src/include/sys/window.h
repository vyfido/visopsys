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
//  window.h
//

// This file describes things needed for interaction with the kernel's
// window manager and the Visopsys GUI.

#if !defined(_WINDOW_H)

#include <sys/file.h>
#include <sys/graphic.h>
#include <sys/image.h>
#include <sys/keyboard.h>
#include <sys/loader.h>
#include <sys/paths.h>
#include <sys/progress.h>
#include <sys/stream.h>

#ifndef _X_
#define _X_
#endif

// Window events/masks.  This first batch are "tier 2" events, produced by
// the system, windows, widgets, etc. to indicate that some more abstract
// thing has happened.
#define EVENT_MASK_WINDOW					0x0F000000
#define EVENT_WINDOW_REFRESH				0x08000000
#define EVENT_WINDOW_RESIZE					0x04000000
#define EVENT_WINDOW_CLOSE					0x02000000
#define EVENT_WINDOW_MINIMIZE				0x01000000
#define EVENT_SELECTION						0x00200000
#define EVENT_CURSOR_MOVE					0x00100000
// And these are "tier 1" events, produced by direct input from the user.
#define EVENT_MASK_KEY						0x000F0000
#define EVENT_KEY_UP						0x00020000
#define EVENT_KEY_DOWN						0x00010000
#define EVENT_MASK_MOUSE					0x0000FFFF
#define EVENT_MOUSE_ENTER					0x00002000
#define EVENT_MOUSE_EXIT					0x00001000
#define EVENT_MOUSE_DRAG					0x00000800
#define EVENT_MOUSE_MOVE					0x00000400
#define EVENT_MOUSE_RIGHTUP					0x00000200
#define EVENT_MOUSE_RIGHTDOWN				0x00000100
#define EVENT_MOUSE_RIGHT \
	(EVENT_MOUSE_RIGHTUP | EVENT_MOUSE_RIGHTDOWN)
#define EVENT_MOUSE_MIDDLEUP				0x00000080
#define EVENT_MOUSE_MIDDLEDOWN				0x00000040
#define EVENT_MOUSE_MIDDLE \
	(EVENT_MOUSE_MIDDLEUP | EVENT_MOUSE_MIDDLEDOWN)
#define EVENT_MOUSE_LEFTUP					0x00000020
#define EVENT_MOUSE_LEFTDOWN				0x00000010
#define EVENT_MOUSE_LEFT \
	(EVENT_MOUSE_LEFTUP | EVENT_MOUSE_LEFTDOWN)
#define EVENT_MOUSE_DOWN \
	(EVENT_MOUSE_LEFTDOWN | EVENT_MOUSE_MIDDLEDOWN | EVENT_MOUSE_RIGHTDOWN)
#define EVENT_MOUSE_UP \
	(EVENT_MOUSE_LEFTUP | EVENT_MOUSE_MIDDLEUP | EVENT_MOUSE_RIGHTUP)
#define EVENT_MOUSE_SCROLLUP				0x00000008
#define EVENT_MOUSE_SCROLLDOWN				0x00000004
#define EVENT_MOUSE_SCROLLVERT \
	(EVENT_MOUSE_SCROLLUP | EVENT_MOUSE_SCROLLDOWN)
#define EVENT_MOUSE_SCROLLLEFT				0x00000002
#define EVENT_MOUSE_SCROLLRIGHT				0x00000001
#define EVENT_MOUSE_SCROLLHORIZ \
	(EVENT_MOUSE_SCROLLLEFT | EVENT_MOUSE_SCROLLRIGHT)
#define EVENT_MOUSE_SCROLL \
	(EVENT_MOUSE_SCROLLVERT | EVENT_MOUSE_SCROLLHORIZ)

// The maximum numbers of window things
#define WINDOW_MAXWINDOWS					256
#define WINDOW_MAX_EVENTS					512
#define WINDOW_MAX_EVENTHANDLERS			256
#define WINDOW_MAX_TITLE_LENGTH				80
#define WINDOW_MAX_LABEL_LENGTH				80
#define WINDOW_MAX_LABEL_LINES				4

// Flags for window components
#define WINDOW_COMPFLAG_NOSCROLLBARS		0x0100
#define WINDOW_COMPFLAG_CLICKABLECURSOR		0x0080
#define WINDOW_COMPFLAG_CUSTOMBACKGROUND	0x0040
#define WINDOW_COMPFLAG_CUSTOMFOREGROUND	0x0020
#define WINDOW_COMPFLAG_STICKYFOCUS			0x0010
#define WINDOW_COMPFLAG_HASBORDER			0x0008
#define WINDOW_COMPFLAG_CANFOCUS			0x0004
#define WINDOW_COMPFLAG_FIXEDHEIGHT			0x0002
#define WINDOW_COMPFLAG_FIXEDWIDTH			0x0001

// Flags for file browsing widgets/dialogs.
#define WINFILEBROWSE_CAN_CD				0x01
#define WINFILEBROWSE_CAN_DEL				0x02
#define WINFILEBROWSE_ALL \
	(WINFILEBROWSE_CAN_CD | WINFILEBROWSE_CAN_DEL)

// Some image file names for dialog boxes
#define INFOIMAGE_NAME						PATH_SYSTEM_ICONS "/infoicon.ico"
#define ERRORIMAGE_NAME						PATH_SYSTEM_ICONS "/bangicon.ico"
#define QUESTIMAGE_NAME						PATH_SYSTEM_ICONS "/questicon.ico"
#define WAITIMAGE_NAME						PATH_SYSTEM_MOUSE "/busy.bmp"

// An "object key".  Really a pointer to an object in kernel memory, but
// of course not usable by applications other than as a reference
typedef volatile void * objectKey;

// These describe the X orientation and Y orientation of a component,
// respectively, within its grid cell

typedef enum {
	orient_left, orient_center, orient_right

} componentXOrientation;

typedef enum {
	orient_top, orient_middle, orient_bottom

} componentYOrientation;

// This structure is needed to describe parameters consistent to all
// window components
typedef struct {
	int gridX;							// Grid X coordinate
	int gridY;							// Grid Y coordinate
	int gridWidth;						// Grid span width
	int gridHeight;						// Grid span height
	int padLeft;						//
	int padRight;						// Pixels of empty space (padding)
	int padTop;							// around each side of the component
	int padBottom;						//
	int flags;							// Attributes - See WINDOW_COMPFLAG_*
	componentXOrientation orientationX;	// left, center, right
	componentYOrientation orientationY;	// top, middle, bottom
	color foreground;					// Foreground drawing color
	color background;					// Background frawing color
	objectKey font;						// Font for text

} componentParameters;

// A structure for containing various types of window events.
typedef struct {
	unsigned type;
	int xPosition;
	int yPosition;
	keyScan key;
	unsigned ascii;

} __attribute__((packed)) windowEvent;

// A structure for a queue of window events as a stream.
typedef stream windowEventStream;

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
	int buffer;
	objectKey font;
	void *data;

} windowDrawParameters;

// Types of scroll bars
typedef enum {
	scrollbar_vertical, scrollbar_horizontal

} scrollBarType;

// Types of dividers
typedef enum {
	divider_vertical, divider_horizontal

} dividerType;

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
	char text[WINDOW_MAX_LABEL_LENGTH + 1];
	image iconImage;

} listItemParameters;

typedef struct _windowFileList {
	objectKey key;
	char cwd[MAX_PATH_LENGTH];
	void *fileEntries;
	int numFileEntries;
	int browseFlags;
	void (*selectionCallback)(file *, char *, loaderFileClass *);

	// Externally-callable service routines
	int (*eventHandler)(struct _windowFileList *, windowEvent *);
	int (*update)(struct _windowFileList *);
	int (*destroy)(struct _windowFileList *);

} windowFileList;

typedef struct {
	char text[WINDOW_MAX_LABEL_LENGTH + 1];
	objectKey key;

} windowMenuItem;

typedef struct {
	int numItems;
	windowMenuItem items[];

} windowMenuContents;

void windowCenterDialog(objectKey, objectKey);
int windowClearEventHandler(objectKey);
int windowClearEventHandlers(void);
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
	char *, unsigned, int);
windowFileList *windowNewFileList(objectKey, windowListType, int, int,
	const char *, int, void *, componentParameters *);
int windowNewInfoDialog(objectKey, const char *, const char *);
int windowNewLanguageDialog(objectKey, char *);
int windowNewNumberDialog(objectKey, const char *, const char *, int, int,
	int, int *);
int windowNewPasswordDialog(objectKey, const char *, const char *, int,
	char *);
objectKey windowNewProgressDialog(objectKey, const char *, progress *);
int windowNewPromptDialog(objectKey, const char *, const char *, int, int,
	char *);
int windowNewQueryDialog(objectKey, const char *, const char *);
int windowNewRadioDialog(objectKey, const char *, const char *, char *[],
	int, int);
objectKey windowNewThumbImage(objectKey, const char *, unsigned, unsigned, int,
	componentParameters *);
int windowProgressDialogDestroy(objectKey);
int windowRegisterEventHandler(objectKey, void (*)(objectKey, windowEvent *));
int windowThumbImageUpdate(objectKey, const char *, unsigned, unsigned, int,
	color *);

#define _WINDOW_H
#endif

