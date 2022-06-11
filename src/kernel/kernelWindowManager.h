//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  kernelWindowManager.h
//
	
// This goes with the file kernelWindowManager.c

#if !defined(_KERNELWINDOWMANAGER_H)

#include "kernelMouse.h"
#include "kernelGraphic.h"
#include "kernelText.h"
#include <sys/window.h>

// Definitions
#define DEFAULT_TITLEBAR_HEIGHT 17
#define DEFAULT_BORDER_THICKNESS 5
#define DEFAULT_SHADING_INCREMENT 15
#define DEFAULT_ROOTCOLOR_RED 0x28
#define DEFAULT_ROOTCOLOR_GREEN 0x5D
#define DEFAULT_ROOTCOLOR_BLUE 0xAB
#define DEFAULT_GREY 200
#define DEFAULT_WINDOWMANAGER_CONFIG "/system/windowmanager.conf"
#define DEFAULT_WINDOWMANAGER_SPLASH "/system/visopsys.bmp"
#define DEFAULT_VARIABLEFONT_SMALL_FILE "/system/arial-bold-10.bmp"
#define DEFAULT_VARIABLEFONT_SMALL_NAME "arial-bold-10"
#define DEFAULT_VARIABLEFONT_MEDIUM_FILE "/system/arial-bold-12.bmp"
#define DEFAULT_VARIABLEFONT_MEDIUM_NAME "arial-bold-12"
#define DEFAULT_MOUSEPOINTER_DEFAULT "/system/mouse.bmp"
#define DEFAULT_MOUSEPOINTER_BUSY "/system/mousebsy.bmp"

typedef enum
{
  windowGenericComponent,
  windowButtonComponent,
  windowIconComponent,
  windowImageComponent,
  windowTextAreaComponent,
  windowTextFieldComponent,
  windowTextLabelComponent,
  windowTitleBarComponent

} kernelWindowComponentType;

// The object that defines a GUI component inside a window
typedef volatile struct
{
  kernelWindowComponentType type;
  int processId;
  void *window;
  int xCoord;
  int yCoord;
  int level;
  unsigned width;
  unsigned height;
  int visible;
  componentParameters parameters;
  windowEventStream events;
  void (*eventHandler)(void *, windowEvent *);
  void *data;

  // Routines for managing this component.  These are set by the
  // kernelWindowNewComponent routine, for things that are common to all
  // components.
  void (*drawBorder) (void *);

  // More routines for managing this component.  These are set by the
  // code which builds the instance of the particular component type
  int (*draw) (void *);
  int (*erase) (void *);
  int (*move) (void *, int, int);
  int (*resize) (void *, unsigned, unsigned);
  int (*mouseEvent) (void *, windowEvent *);
  int (*keyEvent) (void *, windowEvent *);
  int (*destroy) (void *);

} kernelWindowComponent;

// The object that defines a GUI window
typedef volatile struct
{
  int processId;
  char title[WINDOW_MAX_TITLE_LENGTH];
  int xCoord;
  int yCoord;
  int level;
  int hasFocus;
  int visible;
  int movable;
  int hasTitleBar;
  int hasCloseButton;
  int hasBorder;
  kernelGraphicBuffer buffer;
  image backgroundImage;
  color background;
  int backgroundTiled;
  windowEventStream events;
  int numberComponents;
  kernelWindowComponent *componentList[WINDOW_MAX_COMPONENTS];
  kernelWindowComponent *focusComponent;
  
  void *parentWindow;
  void *dialogWindow;

  // Routines for managing this window
  int (*draw) (void *);
  int (*drawClip) (void *, int, int, int, int);

} kernelWindow;

typedef volatile struct
{
  const char *text;
  kernelAsciiFont *font;

} kernelWindowTextLabel;

typedef volatile struct
{
  kernelWindowComponent *label;
  image buttonImage;

} kernelWindowButton;

// An icon image component
typedef volatile struct
{
  image iconImage;
  char label[2][WINDOW_MAX_LABEL_LENGTH];
  int labelLines;
  int labelWidth;
  char command[128];

} kernelWindowIcon;

// A text area as a window component
typedef kernelTextArea kernelWindowTextArea;

typedef kernelWindowTextArea kernelTextField;

// An image as a window component
typedef image kernelWindowImage;

typedef volatile struct
{
  kernelWindowComponent *closeButton;

} kernelWindowTitleBar;

// Functions exported by kernelWindowManager.c
int kernelWindowManagerInitialize(void);
int kernelWindowManagerStart(void);
int kernelWindowManagerLogin(const char *, const char *);
const char *kernelWindowManagerGetUser(void);
int kernelWindowManagerLogout(void);
kernelWindow *kernelWindowManagerNewWindow(int, const char *, int, int,
					   unsigned, unsigned);
kernelWindow *kernelWindowManagerNewDialog(kernelWindow *, const char *,
					   int, int, unsigned, unsigned);
int kernelWindowManagerDestroyWindow(kernelWindow *);
int kernelWindowManagerUpdateBuffer(kernelGraphicBuffer *, int, int,
				    unsigned, unsigned);
int kernelWindowSetTitle(kernelWindow *, const char *);
int kernelWindowGetSize(kernelWindow *, unsigned *, unsigned *);
int kernelWindowSetSize(kernelWindow *, unsigned, unsigned);
int kernelWindowAutoSize(kernelWindow *);
int kernelWindowGetLocation(kernelWindow *, int *, int *);
int kernelWindowSetLocation(kernelWindow *, int, int);
int kernelWindowCenter(kernelWindow *);
int kernelWindowSetHasBorder(kernelWindow *, int);
int kernelWindowSetHasTitleBar(kernelWindow *, int);
int kernelWindowSetMovable(kernelWindow *, int);
int kernelWindowSetHasCloseButton(kernelWindow *, int);
int kernelWindowLayout(kernelWindow *);
int kernelWindowSetVisible(kernelWindow *, int);
kernelWindowComponent *kernelWindowNewComponent(void);
void kernelWindowDestroyComponent(kernelWindowComponent *);
int kernelWindowAddComponent(kernelWindow *, kernelWindowComponent *,
			     componentParameters *);
int kernelWindowAddClientComponent(kernelWindow *, kernelWindowComponent *,
				   componentParameters *);
int kernelWindowAddConsoleTextArea(kernelWindow *, componentParameters *);
unsigned kernelWindowComponentGetWidth(kernelWindowComponent *);
unsigned kernelWindowComponentGetHeight(kernelWindowComponent *);
void kernelWindowFocusComponent(kernelWindow *, kernelWindowComponent *);
void kernelWindowManagerRedrawArea(int, int, unsigned, unsigned);
void kernelWindowManagerProcessEvent(windowEvent *);
int kernelWindowRegisterEventHandler(objectKey,
				     void (*)(objectKey, windowEvent *));
int kernelWindowComponentEventGet(objectKey, windowEvent *);
int kernelWindowManagerTileBackground(const char *);
int kernelWindowManagerCenterBackground(const char *);
int kernelWindowManagerScreenShot(image *);
int kernelWindowManagerSaveScreenShot(const char *);
int kernelWindowManagerSetTextOutput(kernelWindowComponent *);
int kernelWindowManagerShutdown(void);


// Constructors exported by the different component types
kernelWindowComponent *kernelWindowNewButton(kernelWindow *, unsigned,
			     unsigned, kernelWindowComponent *, image *);
kernelWindowComponent *kernelWindowNewIcon(kernelWindow *, image *,
					   const char *, const char *);
kernelWindowComponent *kernelWindowNewImage(kernelWindow *, image *);
kernelWindowComponent *kernelWindowNewTextArea(kernelWindow *, int, int,
					       kernelAsciiFont *);
kernelWindowComponent *kernelWindowNewTextField(kernelWindow *, int,
						kernelAsciiFont *);
kernelWindowComponent *kernelWindowNewTextLabel(kernelWindow *,
					kernelAsciiFont *, const char *);
kernelWindowComponent *kernelWindowNewTitleBar(kernelWindow *, unsigned,
					       unsigned);

#define _KERNELWINDOWMANAGER_H
#endif
