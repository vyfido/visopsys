//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  kernelWindow.h
//
	
// This goes with the file kernelWindow.c

#if !defined(_KERNELWINDOW_H)

#include "kernelMouse.h"
#include "kernelGraphic.h"
#include "kernelText.h"
#include "kernelVariableList.h"
#include <sys/window.h>

// Definitions
#define DEFAULT_TITLEBAR_HEIGHT           19
#define DEFAULT_BORDER_THICKNESS          5
#define DEFAULT_SHADING_INCREMENT         15
#define DEFAULT_WINDOWMANAGER_CONFIG      "/system/windowmanager.conf"
#define DEFAULT_VARIABLEFONT_SMALL_FILE   "/system/fonts/arial-bold-10.bmp"
#define DEFAULT_VARIABLEFONT_SMALL_NAME   "arial-bold-10"
#define DEFAULT_VARIABLEFONT_MEDIUM_FILE  "/system/fonts/arial-bold-12.bmp"
#define DEFAULT_VARIABLEFONT_MEDIUM_NAME  "arial-bold-12"
#define DEFAULT_MOUSEPOINTER_DEFAULT      "/system/mouse.bmp"
#define DEFAULT_MOUSEPOINTER_BUSY         "/system/mousebsy.bmp"

#define WINFLAG_VISIBLE                   0x0001
#define WINFLAG_ENABLED                   0x0002
#define WINFLAG_MOVABLE                   0x0004
#define WINFLAG_RESIZABLE                 0x0008
#define WINFLAG_PACKED                    0x0010
#define WINFLAG_HASTITLEBAR               0x0020
#define WINFLAG_HASMINIMIZEBUTTON         0x0040
#define WINFLAG_HASCLOSEBUTTON            0x0080
#define WINFLAG_HASBORDER                 0x0100
#define WINFLAG_CANFOCUS                  0x0200
#define WINFLAG_HASFOCUS                  0x0400
#define WINFLAG_BACKGROUNDTILED           0x0800

#define WINNAME_TEMPCONSOLE               "temp console window"
#define WINNAME_ROOTWINDOW                "root window"

// How many tracers get displayed when a window is minimized or restored
#define WINDOW_MINREST_TRACERS            20
  
typedef enum {
  genericComponentType,
  borderComponentType,
  buttonComponentType,
  canvasComponentType,
  checkboxComponentType,
  containerComponentType,
  iconComponentType,
  imageComponentType,
  listComponentType,
  listItemComponentType,
  progressBarComponentType,
  radioButtonComponentType,
  scrollBarComponentType,
  textAreaComponentType,
  textLabelComponentType,
  titleBarComponentType,
  windowType

} kernelWindowObjectType;

// Types of borders
typedef enum {
  border_top, border_left, border_bottom, border_right
} borderType;

// The object that defines a GUI component inside a window
typedef volatile struct
{
  kernelWindowObjectType type;
  void *window;
  void *container;
  int xCoord;
  int yCoord;
  int level;
  int width;
  int height;
  unsigned flags;
  componentParameters parameters;
  windowEventStream events;
  void (*eventHandler)(void *, windowEvent *);
  void *data;

  // Routines for managing this component.  These are set by the
  // kernelWindowComponentNew routine, for things that are common to all
  // components.
  void (*drawBorder) (void *, int);
  void (*erase) (void *);
  int (*grey) (void *);

  // More routines for managing this component.  These are set by the
  // code which builds the instance of the particular component type
  int (*draw) (void *);
  int (*move) (void *, int, int);
  int (*resize) (void *, int, int);
  int (*focus) (void *, int);
  int (*getData) (void *, void *, int);
  int (*setData) (void *, void *, int);
  int (*getSelected) (void *);
  int (*setSelected) (void *, int);
  int (*mouseEvent) (void *, windowEvent *);
  int (*keyEvent) (void *, windowEvent *);
  int (*destroy) (void *);

} kernelWindowComponent;

typedef volatile struct
{
  borderType type;

} kernelWindowBorder;

typedef volatile struct
{
  const char label[WINDOW_MAX_LABEL_LENGTH];
  image buttonImage;
  int state;

} kernelWindowButton;

typedef volatile struct
{
  char *text;
  int selected;

} kernelWindowCheckbox;

typedef volatile struct
{
  const char name[WINDOW_MAX_LABEL_LENGTH];
  kernelWindowComponent *components[WINDOW_MAX_COMPONENTS];
  int numComponents;
  int doneLayout;

  // Functions
  int (*containerAdd) (kernelWindowComponent*, kernelWindowComponent *,
		       componentParameters *);
  int (*containerRemove) (kernelWindowComponent *, kernelWindowComponent *);
  int (*containerLayout) (kernelWindowComponent *);  

} kernelWindowContainer;

// An icon image component
typedef volatile struct
{
  image iconImage;
  char label[2][WINDOW_MAX_LABEL_LENGTH];
  int labelLines;
  int labelWidth;
  char command[MAX_PATH_NAME_LENGTH];

} kernelWindowIcon;

// An image as a window component
typedef volatile struct
{
  image imageData;
  drawMode mode;

} kernelWindowImage;

typedef kernelWindowImage kernelWindowCanvas;

typedef volatile struct
{
  int columns;
  int rows;
  int itemWidth;
  int selectMultiple;
  int selectedItem;
  int firstVisible;
  kernelWindowComponent *container;
  kernelWindowComponent *scrollBar;

} kernelWindowList;

typedef volatile struct
{
  const char *text;
  int selected;

} kernelWindowListItem;

typedef kernelWindowContainer kernelWindowMenu;

typedef kernelWindowContainer kernelWindowMenuBar;

typedef kernelWindowListItem kernelWindowMenuItem;

typedef kernelTextArea kernelWindowPasswordField;

typedef volatile struct
{
  int progressPercent;
  int sliderWidth;

} kernelWindowProgressBar;

typedef volatile struct
{
  char *text;
  int numItems;
  int selectedItem;

} kernelWindowRadioButton;

typedef volatile struct
{
  scrollBarType type;
  scrollBarState state;
  int sliderY;
  int sliderWidth;
  int sliderHeight;

} kernelWindowScrollBar;

typedef volatile struct
{
  kernelTextArea *area;
  int areaWidth;
  kernelWindowComponent *scrollBar;

} kernelWindowTextArea;

// A text area as a text field component
typedef kernelTextArea kernelWindowTextField;

typedef volatile struct
{
  char *text;
  int lines;

} kernelWindowTextLabel;

typedef volatile struct
{
  kernelWindowComponent *minimizeButton;
  kernelWindowComponent *closeButton;

} kernelWindowTitleBar;

// The object that defines a GUI window
typedef volatile struct
{
  kernelWindowObjectType type;
  int processId;
  char title[WINDOW_MAX_TITLE_LENGTH];
  int xCoord;
  int yCoord;
  int level;
  unsigned flags;
  kernelGraphicBuffer buffer;
  image backgroundImage;
  color background;
  windowEventStream events;
  kernelWindowComponent *titleBar;
  kernelWindowComponent *borders[4];
  kernelWindowComponent *sysContainer;
  kernelWindowComponent *mainContainer;
  kernelWindowComponent *focusComponent;
  
  void *parentWindow;
  void *dialogWindow;

  // Routines for managing this window
  int (*draw) (void *);
  int (*drawClip) (void *, int, int, int, int);

} kernelWindow;

// This is only used internally, to define a coordinate area
typedef struct 
{
  int leftX;
  int topY;
  int rightX;
  int bottomY;

} screenArea;

#define makeWindowScreenArea(windowP)                                 \
   &((screenArea) { windowP->xCoord, windowP->yCoord,                 \
                   (windowP->xCoord + (windowP->buffer.width - 1)),   \
                   (windowP->yCoord + (windowP->buffer.height - 1)) } )

#define makeComponentScreenArea(componentP)                             \
   &((screenArea) { (((kernelWindow *) componentP->window)->xCoord +    \
		     componentP->xCoord),                               \
		      (((kernelWindow *) componentP->window)->yCoord +  \
		       componentP->yCoord),                             \
		      (((kernelWindow *) componentP->window)->xCoord +  \
		       componentP->xCoord + (componentP->width - 1)),   \
		      (((kernelWindow *) componentP->window)->yCoord +  \
		       componentP->yCoord + (componentP->height - 1)) } )

static inline kernelWindow *getWindow(volatile void *object)
{
  if (((kernelWindow *) object)->type == windowType)
    return ((kernelWindow *) object);
  else
    return ((kernelWindow *) (((kernelWindowComponent *) object)->window));
}     

static inline int isPointInside(int xCoord, int yCoord,	screenArea *area)
{
  // Return 1 if point 1 is inside area 2
  
  if ((xCoord < area->leftX) || (xCoord > area->rightX) ||
      (yCoord < area->topY) || (yCoord > area->bottomY))
    return (0);
  else
    // Yup, it's inside
    return (1);
}

static inline int doLinesIntersect(int horizX1, int horizY, int horizX2,
				   int vertX, int vertY1, int vertY2)
{
  // True if the horizontal line intersects the vertical line
  
  if ((vertX < horizX1) || (vertX > horizX2) ||
      ((horizY < vertY1) || (horizY > vertY2)))
    return (0);
  else
    // Yup, they intersect
    return (1);
}

static inline int isComponentVisible(kernelWindowComponent *component)
{
  // True if the component and all its upstream containers are visible
  
  kernelWindowComponent *container = component->container;

  while (container)
    {
      if (!(container->flags & WINFLAG_VISIBLE))
	return (0);
      else
	container = container->container;
    }

  return (1);
}

// Functions exported by kernelWindow*.c functions
int kernelWindowInitialize(void);
int kernelWindowStart(void);
kernelWindow *kernelWindowMakeRoot(variableList *);
int kernelWindowShell(int);
void kernelWindowShellUpdateList(kernelWindow *[], int);
int kernelWindowLogin(const char *);
int kernelWindowLogout(void);
void kernelWindowRefresh(void);
kernelWindow *kernelWindowNew(int, const char *);
kernelWindow *kernelWindowNewDialog(kernelWindow *, const char *);
int kernelWindowDestroy(kernelWindow *);
int kernelWindowUpdateBuffer(kernelGraphicBuffer *, int, int, int, int);
int kernelWindowSetTitle(kernelWindow *, const char *);
int kernelWindowGetSize(kernelWindow *, int *, int *);
int kernelWindowSetSize(kernelWindow *, int, int);
int kernelWindowGetLocation(kernelWindow *, int *, int *);
int kernelWindowSetLocation(kernelWindow *, int, int);
int kernelWindowCenter(kernelWindow *);
void kernelWindowSnapIcons(kernelWindow *);
int kernelWindowSetHasBorder(kernelWindow *, int);
int kernelWindowSetHasTitleBar(kernelWindow *, int);
int kernelWindowSetMovable(kernelWindow *, int);
int kernelWindowSetResizable(kernelWindow *, int);
int kernelWindowSetHasMinimizeButton(kernelWindow *, int);
int kernelWindowSetHasCloseButton(kernelWindow *, int);
int kernelWindowSetColors(kernelWindow *, color *);
int kernelWindowSetVisible(kernelWindow *, int);
void kernelWindowSetMinimized(kernelWindow *, int);
int kernelWindowAddConsoleTextArea(kernelWindow *, componentParameters *);
void kernelWindowRedrawArea(int, int, int, int);
void kernelWindowProcessEvent(windowEvent *);
int kernelWindowRegisterEventHandler(objectKey,
				     void (*)(objectKey, windowEvent *));
int kernelWindowComponentEventGet(objectKey, windowEvent *);
int kernelWindowSetBackgroundImage(kernelWindow *, image *);
int kernelWindowTileBackground(const char *);
int kernelWindowCenterBackground(const char *);
int kernelWindowScreenShot(image *);
int kernelWindowSaveScreenShot(const char *);
int kernelWindowSetTextOutput(kernelWindowComponent *);
void kernelWindowDumpList(void);

// Functions for managing components
kernelWindowComponent *kernelWindowComponentNew(volatile void *,
						componentParameters *);
void kernelWindowComponentDestroy(kernelWindowComponent *);
int kernelWindowComponentSetVisible(kernelWindowComponent *, int);
int kernelWindowComponentSetEnabled(kernelWindowComponent *, int);
int kernelWindowComponentGetWidth(kernelWindowComponent *);
int kernelWindowComponentSetWidth(kernelWindowComponent *, int);
int kernelWindowComponentGetHeight(kernelWindowComponent *);
int kernelWindowComponentSetHeight(kernelWindowComponent *, int);
int kernelWindowComponentFocus(kernelWindowComponent *);
int kernelWindowComponentDraw(kernelWindowComponent *);
int kernelWindowComponentGetData(kernelWindowComponent *, void *, int);
int kernelWindowComponentSetData(kernelWindowComponent *, void *, int);
int kernelWindowComponentGetSelected(kernelWindowComponent *);
int kernelWindowComponentSetSelected(kernelWindowComponent *, int);

// Constructors exported by the different component types
kernelWindowComponent *kernelWindowNewBorder(volatile void *, borderType,
					     componentParameters *);
kernelWindowComponent *kernelWindowNewButton(volatile void *, const char *,
					     image *, componentParameters *);
kernelWindowComponent *kernelWindowNewCanvas(volatile void *, int, int,
					     componentParameters *);
kernelWindowComponent *kernelWindowNewCheckbox(volatile void *, const char *,
					       componentParameters *);
kernelWindowComponent *kernelWindowNewContainer(volatile void *, const char *,
						componentParameters *);
kernelWindowComponent *kernelWindowNewIcon(volatile void *, image *,
					   const char *,
					   componentParameters *);
kernelWindowComponent *kernelWindowNewImage(volatile void *, image *, drawMode,
					    componentParameters *);
kernelWindowComponent *kernelWindowNewList(volatile void *, int, int, int,
					   const char *[], int,
					   componentParameters *);
kernelWindowComponent *kernelWindowNewListItem(volatile void *, const char *,
					       componentParameters *);
kernelWindowComponent *kernelWindowNewMenu(volatile void *, const char *,
					   componentParameters *);
kernelWindowComponent *kernelWindowNewMenuBar(volatile void *,
					      componentParameters *);
kernelWindowComponent *kernelWindowNewMenuItem(volatile void *, const char *,
					       componentParameters *);
kernelWindowComponent *kernelWindowNewPasswordField(volatile void *, int,
						    componentParameters *);
kernelWindowComponent *kernelWindowNewProgressBar(volatile void *,
						  componentParameters *);
kernelWindowComponent *kernelWindowNewRadioButton(volatile void *, int, int,
						  const char **, int,
						  componentParameters *);
kernelWindowComponent *kernelWindowNewScrollBar(volatile void *, scrollBarType,
						int, int,
						componentParameters *);
kernelWindowComponent *kernelWindowNewTextArea(volatile void *, int, int, int,
					       componentParameters *);
kernelWindowComponent *kernelWindowNewTextField(volatile void *, int,
						componentParameters *);
kernelWindowComponent *kernelWindowNewTextLabel(volatile void *, const char *,
						componentParameters *);
kernelWindowComponent *kernelWindowNewTitleBar(volatile void *, int,
					       componentParameters *);

#define _KERNELWINDOW_H
#endif
