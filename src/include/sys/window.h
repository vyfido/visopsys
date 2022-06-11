//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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

#ifndef _WINDOW_H
#define _WINDOW_H

#include <sys/apidefs.h>
#include <sys/charset.h>
#include <sys/compress.h>
#include <sys/file.h>
#include <sys/graphic.h>
#include <sys/image.h>
#include <sys/keyboard.h>
#include <sys/loader.h>
#include <sys/lock.h>
#include <sys/mouse.h>
#include <sys/paths.h>
#include <sys/progress.h>
#include <sys/stream.h>

#ifndef _X_
#define _X_
#endif

// The maximum numbers of window things
#define WINDOW_MAX_WINDOWS					256
#define WINDOW_MAX_EVENTS					512
#define WINDOW_MAX_TITLE_LENGTH				80
#define WINDOW_MAX_LABEL_LENGTH				80
#define WINDOW_MAX_LABEL_LINES				4

// Window events/masks.  This first batch are "tier 2" events, produced by
// the system, windows, widgets, etc. to indicate that some more abstract
// thing has happened.
#define WINDOW_EVENT_SHELL					0x01000000
#define WINDOW_EVENT_MASK_WINDOW			0x00F00000
#define WINDOW_EVENT_WINDOW_REFRESH			0x00800000
#define WINDOW_EVENT_WINDOW_RESIZE			0x00400000
#define WINDOW_EVENT_WINDOW_CLOSE			0x00200000
#define WINDOW_EVENT_WINDOW_MINIMIZE		0x00100000
#define WINDOW_EVENT_SELECTION				0x00020000
#define WINDOW_EVENT_CURSOR_MOVE			0x00010000
// And these are "tier 1" events, produced by direct input from the user.
#define WINDOW_EVENT_MASK_KEY				0x0000C000
#define WINDOW_EVENT_KEY_UP					0x00008000
#define WINDOW_EVENT_KEY_DOWN				0x00004000
#define WINDOW_EVENT_MASK_MOUSE				0x00003FFF
#define WINDOW_EVENT_MOUSE_ENTER			0x00002000
#define WINDOW_EVENT_MOUSE_EXIT				0x00001000
#define WINDOW_EVENT_MOUSE_DRAG				0x00000800
#define WINDOW_EVENT_MOUSE_MOVE				0x00000400
#define WINDOW_EVENT_MOUSE_RIGHTUP			0x00000200
#define WINDOW_EVENT_MOUSE_RIGHTDOWN		0x00000100
#define WINDOW_EVENT_MOUSE_RIGHT \
	(WINDOW_EVENT_MOUSE_RIGHTUP | WINDOW_EVENT_MOUSE_RIGHTDOWN)
#define WINDOW_EVENT_MOUSE_MIDDLEUP			0x00000080
#define WINDOW_EVENT_MOUSE_MIDDLEDOWN		0x00000040
#define WINDOW_EVENT_MOUSE_MIDDLE \
	(WINDOW_EVENT_MOUSE_MIDDLEUP | WINDOW_EVENT_MOUSE_MIDDLEDOWN)
#define WINDOW_EVENT_MOUSE_LEFTUP			0x00000020
#define WINDOW_EVENT_MOUSE_LEFTDOWN			0x00000010
#define WINDOW_EVENT_MOUSE_LEFT \
	(WINDOW_EVENT_MOUSE_LEFTUP | WINDOW_EVENT_MOUSE_LEFTDOWN)
#define WINDOW_EVENT_MOUSE_DOWN \
	(WINDOW_EVENT_MOUSE_LEFTDOWN | WINDOW_EVENT_MOUSE_MIDDLEDOWN | \
	WINDOW_EVENT_MOUSE_RIGHTDOWN)
#define WINDOW_EVENT_MOUSE_UP \
	(WINDOW_EVENT_MOUSE_LEFTUP | WINDOW_EVENT_MOUSE_MIDDLEUP | \
	WINDOW_EVENT_MOUSE_RIGHTUP)
#define WINDOW_EVENT_MOUSE_SCROLLUP			0x00000008
#define WINDOW_EVENT_MOUSE_SCROLLDOWN		0x00000004
#define WINDOW_EVENT_MOUSE_SCROLLVERT \
	(WINDOW_EVENT_MOUSE_SCROLLUP | WINDOW_EVENT_MOUSE_SCROLLDOWN)
#define WINDOW_EVENT_MOUSE_SCROLLLEFT		0x00000002
#define WINDOW_EVENT_MOUSE_SCROLLRIGHT		0x00000001
#define WINDOW_EVENT_MOUSE_SCROLLHORIZ \
	(WINDOW_EVENT_MOUSE_SCROLLLEFT | WINDOW_EVENT_MOUSE_SCROLLRIGHT)
#define WINDOW_EVENT_MOUSE_SCROLL \
	(WINDOW_EVENT_MOUSE_SCROLLVERT | WINDOW_EVENT_MOUSE_SCROLLHORIZ)

// Window shell events
#define WINDOW_SHELL_EVENT_WINDOWLIST		0x0020
#define WINDOW_SHELL_EVENT_REFRESH			0x0010
#define WINDOW_SHELL_EVENT_CHANGEBACKGRND	0x0008
#define WINDOW_SHELL_EVENT_RAISEWINMENU		0x0004
#define WINDOW_SHELL_EVENT_NEWBARCOMP		0x0002
#define WINDOW_SHELL_EVENT_DESTROYBARCOMP	0x0001

// Flags for windows
#define WINDOW_FLAG_ICONIFIED				0x0400
#define WINDOW_FLAG_VISIBLE					0x0200
#define WINDOW_FLAG_MOVABLE					0x0100
#define WINDOW_FLAG_RESIZABLE				0x00C0
#define WINDOW_FLAG_RESIZABLEX				0x0080
#define WINDOW_FLAG_RESIZABLEY				0x0040
#define WINDOW_FLAG_HASBORDER				0x0020
#define WINDOW_FLAG_CANFOCUS				0x0010
#define WINDOW_FLAG_HASFOCUS				0x0008
#define WINDOW_FLAG_ROOTWINDOW				0x0004
#define WINDOW_FLAG_BACKGROUNDTILED			0x0002
#define WINDOW_FLAG_DEBUGLAYOUT				0x0001

// Flags for window component parameters
#define COMP_PARAMS_FLAG_CANDRAG			0x0200
#define COMP_PARAMS_FLAG_NOSCROLLBARS		0x0100
#define COMP_PARAMS_FLAG_CLICKABLECURSOR	0x0080
#define COMP_PARAMS_FLAG_CUSTOMFOREGROUND	0x0040
#define COMP_PARAMS_FLAG_CUSTOMBACKGROUND	0x0020
#define COMP_PARAMS_FLAG_STICKYFOCUS		0x0010
#define COMP_PARAMS_FLAG_HASBORDER			0x0008
#define COMP_PARAMS_FLAG_CANFOCUS			0x0004
#define COMP_PARAMS_FLAG_FIXEDHEIGHT		0x0002
#define COMP_PARAMS_FLAG_FIXEDWIDTH			0x0001

// Flags for file browsing widgets/dialogs
#define WINDOW_FILEBROWSE_CAN_CD			0x01
#define WINDOW_FILEBROWSE_CAN_DEL			0x02
#define WINDOW_FILEBROWSE_ALL \
	(WINDOW_FILEBROWSE_CAN_CD | WINDOW_FILEBROWSE_CAN_DEL)

// Some icon file names for dialog boxes
#define WINDOW_INFOIMAGE_NAME				PATH_SYSTEM_ICONS "/info.ico"
#define WINDOW_ERRORIMAGE_NAME				PATH_SYSTEM_ICONS "/error.ico"
#define WINDOW_QUESTIMAGE_NAME				PATH_SYSTEM_ICONS "/question.ico"
#define WINDOW_WAITIMAGE_NAME				PATH_SYSTEM_MOUSE "/busy.ico"

// Window keyboard widget parameters
#define WINDOW_KEYBOARD_KEYROWS				6
#define WINDOW_KEYBOARD_ROW0_P0_KEYS		13
#define WINDOW_KEYBOARD_ROW0_P1_KEYS		3
#define WINDOW_KEYBOARD_ROW0_KEYS \
	(WINDOW_KEYBOARD_ROW0_P0_KEYS + WINDOW_KEYBOARD_ROW0_P1_KEYS)
#define WINDOW_KEYBOARD_ROW1_P0_KEYS		14
#define WINDOW_KEYBOARD_ROW1_P1_KEYS		3
#define WINDOW_KEYBOARD_ROW1_KEYS \
	(WINDOW_KEYBOARD_ROW1_P0_KEYS + WINDOW_KEYBOARD_ROW1_P1_KEYS)
#define WINDOW_KEYBOARD_ROW2_P0_KEYS		14
#define WINDOW_KEYBOARD_ROW2_P1_KEYS		3
#define WINDOW_KEYBOARD_ROW2_KEYS \
	(WINDOW_KEYBOARD_ROW2_P0_KEYS + WINDOW_KEYBOARD_ROW2_P1_KEYS)
#define WINDOW_KEYBOARD_ROW3_P0_KEYS		14
#define WINDOW_KEYBOARD_ROW3_P1_KEYS		0
#define WINDOW_KEYBOARD_ROW3_KEYS \
	(WINDOW_KEYBOARD_ROW3_P0_KEYS + WINDOW_KEYBOARD_ROW3_P1_KEYS)
#define WINDOW_KEYBOARD_ROW4_P0_KEYS		13
#define WINDOW_KEYBOARD_ROW4_P1_KEYS		1
#define WINDOW_KEYBOARD_ROW4_KEYS \
	(WINDOW_KEYBOARD_ROW4_P0_KEYS + WINDOW_KEYBOARD_ROW4_P1_KEYS)
#define WINDOW_KEYBOARD_ROW5_P0_KEYS		8
#define WINDOW_KEYBOARD_ROW5_P1_KEYS		3
#define WINDOW_KEYBOARD_ROW5_KEYS \
	(WINDOW_KEYBOARD_ROW5_P0_KEYS + WINDOW_KEYBOARD_ROW5_P1_KEYS)
#define WINDOW_KEYBOARD_MAX_ROWKEYS			17
#define WINDOW_KEYBOARD_GAP					5

// Info about a GUI window
typedef struct {
	int processId;
	char title[WINDOW_MAX_TITLE_LENGTH];
	int xCoord;
	int yCoord;
	unsigned flags;
	int width;
	int height;
	objectKey parentWindow;
	objectKey dialogWindow;

} windowInfo;

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
	unsigned flags;						// Attributes - See COMP_PARAMS_FLAG_*
	componentXOrientation orientationX;	// left, center, right
	componentYOrientation orientationY;	// top, middle, bottom
	color foreground;					// Foreground drawing color
	color background;					// Background frawing color
	objectKey font;						// Font for text

} componentParameters;

// A structure for window shell events
typedef struct {
	unsigned type;
	objectKey component;
	int processId;

} windowShellEvent;

// A structure for containing various types of window events
typedef struct {
	unsigned type;
	union {
		windowShellEvent shell;

		struct {
			int x;
			int y;
		} coord;

		struct {
			keyScan scan;
			unsigned unicode;
		} key;
	};

} windowEvent;

// A type for a queue of window events as a stream
typedef stream windowEventStream;

// Types of drawing operations
typedef enum {
	draw_pixel, draw_line, draw_rect, draw_oval, draw_image, draw_text,
	draw_buffer

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

typedef struct _windowTreeItem {
	char text[WINDOW_MAX_LABEL_LENGTH + 1];
	char displayText[WINDOW_MAX_LABEL_LENGTH + 1];
	objectKey key;
	struct _windowTreeItem *firstChild;
	struct _windowTreeItem *next;
	int expanded;
	int subItem;

} windowTreeItem;

typedef struct _windowArchiveList {
	objectKey key;
	archiveMemberInfo *members;
	int numMembers;
	void (*selectionCallback)(int);

	// Externally-callable service functions
	int (*eventHandler)(struct _windowArchiveList *, windowEvent *);
	int (*update)(struct _windowArchiveList *, archiveMemberInfo *, int);
	int (*destroy)(struct _windowArchiveList *);

} windowArchiveList;

typedef struct _windowFileList {
	objectKey key;
	char cwd[MAX_PATH_LENGTH + 1];
	void *fileEntries;
	int numFileEntries;
	int browseFlags;
	int iconThreadPid;
	spinLock lock;
	void *data;

	void (*selectionCallback)(struct _windowFileList *, file *, char *,
		loaderFileClass *);

	// Externally-callable service functions
	int (*update)(struct _windowFileList *);
	int (*eventHandler)(struct _windowFileList *, windowEvent *);
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

typedef struct {
	int xCoord;
	int yCoord;
	int width;
	int height;
	keyScan scan;
	const char *string1;
	const char *string2;

} windowKey;

typedef struct _windowKeyboard {
	objectKey canvas;
	keyMap map;
	char charsetName[CHARSET_NAME_LEN + 1];
	unsigned shiftState;
	unsigned toggleState;
	int width;
	int height;
	color foreground;
	color background;
	objectKey font;
	int fontWidth;
	int fontHeight;
	objectKey smallFont;
	int smallFontHeight;
	windowKey *leftShift;
	windowKey *rightShift;
	windowKey *leftControl;
	windowKey *rightControl;
	windowKey *pressedKey;
	struct {
		int numKeys;
		windowKey keys[WINDOW_KEYBOARD_MAX_ROWKEYS];

	} rows[WINDOW_KEYBOARD_KEYROWS];

	// Externally-callable service functions
	int (*eventHandler)(struct _windowKeyboard *, windowEvent *);
	int (*setMap)(struct _windowKeyboard *, keyMap *);
	int (*setCharset)(struct _windowKeyboard *, const char *);

	// If set, this is called when keys are pressed/released
	int (*callback)(int, keyScan);

} windowKeyboard;

typedef enum {
	pixedmode_draw, pixedmode_pick, pixedmode_select

} pixelEditorMode;

typedef struct _windowPixelEditor {
	objectKey canvas;
	int width;
	int height;
	graphicBuffer buffer;
	image *img;
	int minPixelSize;
	int maxPixelSize;
	int pixelSize;
	int horizPixels;
	int vertPixels;
	int startHoriz;
	int startVert;
	scrollBarState horiz;
	scrollBarState vert;
	pixelEditorMode mode;
	windowDrawParameters drawing;
	color foreground;
	color background;
	int changed;

	// Externally-callable service functions
	int (*resize)(struct _windowPixelEditor *);
	int (*eventHandler)(struct _windowPixelEditor *, windowEvent *);
	int (*zoom)(struct _windowPixelEditor *, int);
	int (*scrollHoriz)(struct _windowPixelEditor *, int);
	int (*scrollVert)(struct _windowPixelEditor *, int);
	int (*destroy)(struct _windowPixelEditor *);

} windowPixelEditor;

// Functions exported by libwindow
void windowCenterDialog(objectKey, objectKey);
int windowClearEventHandler(objectKey);
int windowClearEventHandlers(void);
void windowGuiRun(void);
void windowGuiStop(void);
int windowGuiThread(void);
int windowGuiThreadPid(void);
windowArchiveList *windowNewArchiveList(objectKey, windowListType, int, int,
	archiveMemberInfo *, int, void (*)(int), componentParameters *);
objectKey windowNewBannerDialog(objectKey, const char *, const char *);
int windowNewChoiceDialog(objectKey, const char *, const char *, char *[],
	int, int);
int windowNewColorDialog(objectKey, color *);
int windowNewErrorDialog(objectKey, const char *, const char *);
int windowNewFileDialog(objectKey, const char *, const char *, const char *,
	char *, unsigned, fileType, int);
windowFileList *windowNewFileList(objectKey, windowListType, int, int,
	const char *, int, void (*)(windowFileList *, file *, char *,
	loaderFileClass *), componentParameters *);
int windowNewInfoDialog(objectKey, const char *, const char *);
windowKeyboard *windowNewKeyboard(objectKey, int, int, void *,
	componentParameters *);
int windowNewLanguageDialog(objectKey, char *);
int windowNewNumberDialog(objectKey, const char *, const char *, int, int,
	int, int *);
int windowNewPasswordDialog(objectKey, const char *, const char *, int,
	char *);
windowPixelEditor *windowNewPixelEditor(objectKey, int, int, image *,
	componentParameters *);
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

#endif

