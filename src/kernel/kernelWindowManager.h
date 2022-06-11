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
//  kernelWindowManager.h
//
	
// This goes with the file kernelWindowManager.c

#if !defined(_KERNELWINDOWMANAGER_H)

#include "kernelMouseFunctions.h"
#include "kernelGraphicFunctions.h"
#include "kernelText.h"
#include <sys/window.h>

// Definitions
#define MAX_TITLE_LENGTH 80
#define MAX_COMPONENTS 256
#define MAX_LABEL_LENGTH 64
#define DEFAULT_TITLEBAR_HEIGHT 17
#define DEFAULT_BORDER_THICKNESS 5
#define DEFAULT_SHADING_INCREMENT 15
#define DEFAULT_ROOTCOLOR_RED 0x28
#define DEFAULT_ROOTCOLOR_GREEN 0x5D
#define DEFAULT_ROOTCOLOR_BLUE 0xAB
#define MAX_WINDOWS 256
#define MAX_MOUSE_UPDATES 256
#define DEFAULT_GREY 200
#define DEFAULT_WINDOWMANAGER_CONFIG "/system/windowmanager.conf"
#define DEFAULT_VARIABLEFONT_SMALL_FILE "/system/arial-bold-12.bmp"
#define DEFAULT_VARIABLEFONT_SMALL_NAME "arial-bold-12"

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
  void *window;
  int xCoord;
  int yCoord;
  unsigned width;
  unsigned height;
  int visible;
  componentParameters parameters;
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
  int (*mouseEvent) (void *, kernelMouseStatus *);
  int (*destroy) (void *);

} kernelWindowComponent;

// The object that defines a GUI window
typedef volatile struct
{
  int processId;
  char title[MAX_TITLE_LENGTH];
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
  kernelMouseStatus mouseEvent;
  image backgroundImage;
  int backgroundTiled;
  int numberComponents;
  kernelWindowComponent *componentList[MAX_COMPONENTS];

  // Routines for managing this window
  int (*draw) (void *);
  int (*drawClip) (void *, int, int, int, int);

} kernelWindow;

typedef volatile struct
{
  char label[MAX_LABEL_LENGTH];
  image buttonImage;
  int (*callBackFunction) (void *);

} kernelWindowButtonComponent;

// An icon image component
typedef volatile struct
{
  image iconImage;
  char label[MAX_LABEL_LENGTH];
  char command[128];

} kernelWindowIconComponent;

// A text area as a window component
typedef kernelTextArea kernelWindowTextAreaComponent;

typedef kernelWindowTextAreaComponent kernelTextFieldComponent;

typedef volatile struct
{
  const char *text;
  kernelAsciiFont *font;

} kernelWindowTextLabelComponent;

// An image as a window component
typedef image kernelWindowImageComponent;

typedef volatile struct
{
  kernelWindowComponent *closeButton;

} kernelWindowTitleBarComponent;

// Functions exported by kernelWindowManager.c
int kernelWindowManagerInitialize(void);
int kernelWindowManagerStart(void);
int kernelWindowManagerLogin(int);
int kernelWindowManagerLogout(void);
kernelWindow *kernelWindowManagerNewWindow(int, const char *, int, int,
					   unsigned, unsigned);
int kernelWindowManagerDestroyWindow(kernelWindow *);
int kernelWindowManagerUpdateBuffer(kernelGraphicBuffer *, int, int,
				    unsigned, unsigned);
int kernelWindowSetTitle(kernelWindow *, const char *);
int kernelWindowGetSize(kernelWindow *, unsigned *, unsigned *);
int kernelWindowSetSize(kernelWindow *, unsigned, unsigned);
int kernelWindowAutoSize(kernelWindow *);
int kernelWindowGetLocation(kernelWindow *, int *, int *);
int kernelWindowSetLocation(kernelWindow *, int, int);
int kernelWindowSetHasBorder(kernelWindow *, int);
int kernelWindowSetHasTitleBar(kernelWindow *, int);
int kernelWindowSetMovable(kernelWindow *, int);
int kernelWindowSetHasCloseButton(kernelWindow *, int);
int kernelWindowLayout(kernelWindow *);
int kernelWindowSetVisible(kernelWindow *, int);
kernelWindowComponent *kernelWindowNewComponent(void);
int kernelWindowAddComponent(kernelWindow *, kernelWindowComponent *,
			     componentParameters *);
int kernelWindowAddClientComponent(kernelWindow *, kernelWindowComponent *,
				   componentParameters *);
unsigned kernelWindowComponentGetWidth(kernelWindowComponent *);
unsigned kernelWindowComponentGetHeight(kernelWindowComponent *);
void kernelWindowManagerRedrawArea(int, int, unsigned, unsigned);
void kernelWindowManagerProcessMouseEvent(kernelMouseStatus *);
int kernelWindowManagerTileBackground(const char *);
int kernelWindowManagerCenterBackground(const char *);
int kernelWindowManagerScreenShot(image *);
int kernelWindowManagerSaveScreenShot(const char *);
int kernelWindowManagerSetTextOutput(kernelWindowComponent *);
int kernelWindowManagerShutdown(void);


// Constructors exported by the different component types
kernelWindowComponent *kernelWindowNewButtonComponent(kernelWindow *,
				      unsigned, unsigned, const char *,
				      image *, int (*) (void *));
kernelWindowComponent *kernelWindowNewIconComponent(kernelWindow *,
				    image *, const char *, const char *);
kernelWindowComponent *kernelWindowNewImageComponent(kernelWindow *,
						     image *);
kernelWindowComponent *kernelWindowNewTextAreaComponent(kernelWindow *,
						int, int, kernelAsciiFont *);
kernelWindowComponent *kernelWindowNewTextFieldComponent(kernelWindow *, int,
							 kernelAsciiFont *);
kernelWindowComponent *kernelWindowNewTextLabelComponent(kernelWindow *,
					 kernelAsciiFont *, const char *);
kernelWindowComponent *kernelWindowNewTitleBarComponent(kernelWindow *,
							unsigned, unsigned);

#define _KERNELWINDOWMANAGER_H
#endif
