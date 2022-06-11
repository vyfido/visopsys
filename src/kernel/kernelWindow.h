//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
#define WINDOW_TITLEBAR_HEIGHT              19
#define WINDOW_TITLEBAR_MINWIDTH            (WINDOW_TITLEBAR_HEIGHT * 4)
#define WINDOW_BORDER_THICKNESS             3
#define WINDOW_SHADING_INCREMENT            15
#define WINDOW_MIN_WIDTH                    (WINDOW_TITLEBAR_MINWIDTH + \
					     (WINDOW_BORDER_THICKNESS * 2))
#define WINDOW_MIN_HEIGHT                   (WINDOW_TITLEBAR_HEIGHT + \
                                             (WINDOW_BORDER_THICKNESS * 2))
#define WINDOW_MANAGER_DEFAULT_CONFIG       "/system/config/windowmanager.conf"
#define WINDOW_DEFAULT_VARFONT_SMALL_FILE   "/system/fonts/arial-bold-10.bmp"
#define WINDOW_DEFAULT_VARFONT_SMALL_NAME   "arial-bold-10"
#define WINDOW_DEFAULT_VARFONT_MEDIUM_FILE  "/system/fonts/arial-bold-12.bmp"
#define WINDOW_DEFAULT_VARFONT_MEDIUM_NAME  "arial-bold-12"
#define WINDOW_DEFAULT_MOUSEPOINTER_DEFAULT "/system/mouse.bmp"
#define WINDOW_DEFAULT_MOUSEPOINTER_BUSY    "/system/mouse/mousebsy.bmp"

#define WINFLAG_VISIBLE                     0x0200
#define WINFLAG_ENABLED                     0x0100
#define WINFLAG_MOVABLE                     0x0080
#define WINFLAG_RESIZABLE                   0x0060
#define WINFLAG_RESIZABLEX                  0x0040
#define WINFLAG_RESIZABLEY                  0x0020
#define WINFLAG_HASBORDER                   0x0010
#define WINFLAG_CANFOCUS                    0x0008
#define WINFLAG_HASFOCUS                    0x0004
#define WINFLAG_BACKGROUNDTILED             0x0002
#define WINFLAG_DEBUGLAYOUT                 0x0001

#define WINNAME_TEMPCONSOLE                 "temp console window"
#define WINNAME_ROOTWINDOW                  "root window"

// How many tracers get displayed when a window is minimized or restored
#define WINDOW_MINREST_TRACERS              20
  
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
  menuComponentType,
  menuBarComponentType,
  progressBarComponentType,
  radioButtonComponentType,
  scrollBarComponentType,
  sliderComponentType,
  sysContainerComponentType,
  textAreaComponentType,
  textLabelComponentType,
  titleBarComponentType,
  windowType

} kernelWindowObjectType;

// Forward declarations, where necessary
struct _kernelWindow;

// The object that defines a GUI component inside a window
typedef volatile struct _kernelWindowComponent {
  kernelWindowObjectType type;
  kernelWindowObjectType subType;
  volatile struct _kernelWindow *window;
  volatile struct _kernelWindowComponent *container;
  volatile struct _kernelWindowComponent *contextMenu;
  kernelGraphicBuffer *buffer;
  int xCoord;
  int yCoord;
  int level;
  int width;
  int height;
  int minWidth;
  int minHeight;
  unsigned flags;
  componentParameters parameters;
  windowEventStream events;
  void (*eventHandler)(volatile struct _kernelWindowComponent *,
		       windowEvent *);
  void *data;

  // Routines for managing this component.  These are set by the
  // kernelWindowComponentNew routine, for things that are common to all
  // components.
  int (*drawBorder) (volatile struct _kernelWindowComponent *, int);
  int (*erase) (volatile struct _kernelWindowComponent *);
  int (*grey) (volatile struct _kernelWindowComponent *);

  // More routines for managing this component.  These are set by the
  // code which builds the instance of the particular component type
  int (*draw) (volatile struct _kernelWindowComponent *);
  int (*update) (volatile struct _kernelWindowComponent *);
  int (*move) (volatile struct _kernelWindowComponent *, int, int);
  int (*resize) (volatile struct _kernelWindowComponent *, int, int);
  int (*focus) (volatile struct _kernelWindowComponent *, int);
  int (*getData) (volatile struct _kernelWindowComponent *, void *, int);
  int (*setData) (volatile struct _kernelWindowComponent *, void *, int);
  int (*getSelected) (volatile struct _kernelWindowComponent *, int *);
  int (*setSelected) (volatile struct _kernelWindowComponent *, int);
  int (*mouseEvent) (volatile struct _kernelWindowComponent *, windowEvent *);
  int (*keyEvent) (volatile struct _kernelWindowComponent *, windowEvent *);
  int (*destroy) (volatile struct _kernelWindowComponent *);

} kernelWindowComponent;

typedef volatile struct {
  borderType type;

} kernelWindowBorder;

typedef volatile struct {
  const char label[WINDOW_MAX_LABEL_LENGTH];
  image buttonImage;
  int state;

} kernelWindowButton;

typedef volatile struct {
  char *text;
  int selected;

} kernelWindowCheckbox;

typedef volatile struct {
  const char name[WINDOW_MAX_LABEL_LENGTH];
  kernelWindowComponent **components;
  int maxComponents;
  int numComponents;
  int numColumns;
  int numRows;
  int doneLayout;

  // Functions
  int (*containerAdd) (kernelWindowComponent *, kernelWindowComponent *);
  int (*containerRemove) (kernelWindowComponent *, kernelWindowComponent *);
  int (*containerLayout) (kernelWindowComponent *);
  int (*containerSetBuffer) (kernelWindowComponent *, kernelGraphicBuffer *);
  void (*containerDrawGrid) (kernelWindowComponent *);

} kernelWindowContainer;

// An icon image component
typedef volatile struct {
  int selected;
  image iconImage;
  char label[2][WINDOW_MAX_LABEL_LENGTH];
  int labelLines;
  int labelWidth;
  char command[MAX_PATH_NAME_LENGTH];

} kernelWindowIcon;

// An image as a window component
typedef volatile struct {
  image imageData;
  drawMode mode;

} kernelWindowImage;

typedef kernelWindowImage kernelWindowCanvas;

typedef volatile struct {
  windowListType type;
  int columns;
  int rows;
  int itemWidth;
  int itemHeight;
  int selectMultiple;
  int multiColumn;
  int selectedItem;
  int firstVisibleRow;
  int itemRows;
  kernelWindowComponent *container;
  kernelWindowComponent *scrollBar;

} kernelWindowList;

typedef volatile struct {
  windowListType type;
  listItemParameters params;
  kernelWindowComponent *icon;
  int selected;

} kernelWindowListItem;

typedef volatile struct {
  kernelGraphicBuffer buffer;
  kernelWindowComponent *container;
  int menuBarSelected;

} kernelWindowMenu;

typedef kernelWindowContainer kernelWindowMenuBar;

typedef kernelWindowListItem kernelWindowMenuItem;

typedef kernelTextArea kernelWindowPasswordField;

typedef volatile struct {
  int progressPercent;
  int sliderWidth;

} kernelWindowProgressBar;

typedef volatile struct {
  char *text;
  int numItems;
  int selectedItem;

} kernelWindowRadioButton;

typedef volatile struct {
  scrollBarType type;
  scrollBarState state;
  int sliderY;
  int sliderHeight;

} kernelWindowScrollBar;

typedef kernelWindowScrollBar kernelWindowSlider;

typedef volatile struct {
  kernelTextArea *area;
  int areaWidth;
  kernelWindowComponent *scrollBar;

} kernelWindowTextArea;

// A text area as a text field component
typedef kernelTextArea kernelWindowTextField;

typedef volatile struct {
  char *text;
  int lines;

} kernelWindowTextLabel;

typedef volatile struct {
  kernelWindowComponent *minimizeButton;
  kernelWindowComponent *closeButton;

} kernelWindowTitleBar;

// The object that defines a GUI window
typedef volatile struct _kernelWindow {
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
  kernelWindowComponent *menuBar;
  kernelWindowComponent *contextMenu;
  kernelWindowComponent *sysContainer;
  kernelWindowComponent *mainContainer;
  kernelWindowComponent *focusComponent;
  kernelWindowComponent *oldFocusComponent;

  volatile struct _kernelWindow *parentWindow;
  volatile struct _kernelWindow *dialogWindow;

  // Routines for managing this window
  int (*draw) (volatile struct _kernelWindow *);
  int (*drawClip) (volatile struct _kernelWindow *, int, int, int, int);
  int (*update) (volatile struct _kernelWindow *, int, int, int, int);

} kernelWindow;

// This is only used internally, to define a coordinate area
typedef struct {
  int leftX;
  int topY;
  int rightX;
  int bottomY;

} screenArea;

#define makeWindowScreenArea(windowP)                                 \
   &((screenArea) { windowP->xCoord, windowP->yCoord,                 \
                   (windowP->xCoord + (windowP->buffer.width - 1)),   \
                   (windowP->yCoord + (windowP->buffer.height - 1)) } )

static inline screenArea *
makeComponentScreenArea(kernelWindowComponent *component)
{
  return (&((screenArea) {
    (component->window->xCoord + component->xCoord),
      (component->window->yCoord + component->yCoord),
      (component->window->xCoord + component->xCoord + (component->width - 1)),
      (component->window->yCoord + component->yCoord + (component->height - 1))
      }));
}

static inline kernelWindow *getWindow(objectKey object)
{
  if (((kernelWindow *) object)->type == windowType)
    return ((kernelWindow *) object);
  else
    return (((kernelWindowComponent *) object)->window);
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
int kernelWindowShell(int, int);
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
int kernelWindowSnapIcons(objectKey);
int kernelWindowSetHasBorder(kernelWindow *, int);
int kernelWindowSetHasTitleBar(kernelWindow *, int);
int kernelWindowSetMovable(kernelWindow *, int);
int kernelWindowSetResizable(kernelWindow *, int);
int kernelWindowRemoveMinimizeButton(kernelWindow *);
int kernelWindowRemoveCloseButton(kernelWindow *);
int kernelWindowSetColors(kernelWindow *, color *);
int kernelWindowSetVisible(kernelWindow *, int);
void kernelWindowSetMinimized(kernelWindow *, int);
int kernelWindowAddConsoleTextArea(kernelWindow *);
void kernelWindowRedrawArea(int, int, int, int);
void kernelWindowDrawAll(void);
void kernelWindowResetColors(void);
void kernelWindowProcessEvent(windowEvent *);
int kernelWindowRegisterEventHandler(kernelWindowComponent *,
				     void (*)(kernelWindowComponent *,
					      windowEvent *));
int kernelWindowComponentEventGet(objectKey, windowEvent *);
int kernelWindowSetBackgroundImage(kernelWindow *, image *);
int kernelWindowTileBackground(const char *);
int kernelWindowCenterBackground(const char *);
int kernelWindowScreenShot(image *);
int kernelWindowSaveScreenShot(const char *);
int kernelWindowSetTextOutput(kernelWindowComponent *);
int kernelWindowLayout(kernelWindow *);
void kernelWindowDebugLayout(kernelWindow *);
int kernelWindowContextAdd(objectKey, windowMenuContents *);
int kernelWindowContextSet(objectKey, kernelWindowComponent *);

// Functions for managing components
kernelWindowComponent *kernelWindowComponentNew(objectKey,
						componentParameters *);
void kernelWindowComponentDestroy(kernelWindowComponent *);
int kernelWindowComponentSetVisible(kernelWindowComponent *, int);
int kernelWindowComponentSetEnabled(kernelWindowComponent *, int);
int kernelWindowComponentGetWidth(kernelWindowComponent *);
int kernelWindowComponentSetWidth(kernelWindowComponent *, int);
int kernelWindowComponentGetHeight(kernelWindowComponent *);
int kernelWindowComponentSetHeight(kernelWindowComponent *, int);
int kernelWindowComponentFocus(kernelWindowComponent *);
int kernelWindowComponentUnfocus(kernelWindowComponent *);
int kernelWindowComponentDraw(kernelWindowComponent *);
int kernelWindowComponentGetData(kernelWindowComponent *, void *, int);
int kernelWindowComponentSetData(kernelWindowComponent *, void *, int);
int kernelWindowComponentGetSelected(kernelWindowComponent *, int *);
int kernelWindowComponentSetSelected(kernelWindowComponent *, int);

// Constructors exported by the different component types
kernelWindowComponent *kernelWindowNewBorder(objectKey, borderType,
					     componentParameters *);
kernelWindowComponent *kernelWindowNewButton(objectKey, const char *,
					     image *, componentParameters *);
kernelWindowComponent *kernelWindowNewCanvas(objectKey, int, int,
					     componentParameters *);
kernelWindowComponent *kernelWindowNewCheckbox(objectKey, const char *,
					       componentParameters *);
kernelWindowComponent *kernelWindowNewContainer(objectKey, const char *,
						componentParameters *);
kernelWindowComponent *kernelWindowNewIcon(objectKey, image *,
					   const char *,
					   componentParameters *);
kernelWindowComponent *kernelWindowNewImage(objectKey, image *, drawMode,
					    componentParameters *);
kernelWindowComponent *kernelWindowNewList(objectKey, windowListType,
					   int, int, int, listItemParameters *,
					   int, componentParameters *);
kernelWindowComponent *kernelWindowNewListItem(objectKey, windowListType,
					       listItemParameters *,
					       componentParameters *);
kernelWindowComponent *kernelWindowNewMenu(objectKey, const char *,
					   windowMenuContents *,
					   componentParameters *);
kernelWindowComponent *kernelWindowNewMenuBar(kernelWindow *,
					      componentParameters *);
kernelWindowComponent *kernelWindowNewMenuItem(kernelWindowComponent *,
					       const char *,
					       componentParameters *);
kernelWindowComponent *kernelWindowNewPasswordField(objectKey, int,
						    componentParameters *);
kernelWindowComponent *kernelWindowNewProgressBar(objectKey,
						  componentParameters *);
kernelWindowComponent *kernelWindowNewRadioButton(objectKey, int, int,
						  const char **, int,
						  componentParameters *);
kernelWindowComponent *kernelWindowNewScrollBar(objectKey, scrollBarType,
						int, int,
						componentParameters *);
kernelWindowComponent *kernelWindowNewSlider(objectKey, scrollBarType,
					     int, int, componentParameters *);
kernelWindowComponent *kernelWindowNewSysContainer(kernelWindow *,
						   componentParameters *);
kernelWindowComponent *kernelWindowNewTextArea(objectKey, int, int, int,
					       componentParameters *);
kernelWindowComponent *kernelWindowNewTextField(objectKey, int,
						componentParameters *);
kernelWindowComponent *kernelWindowNewTextLabel(objectKey, const char *,
						componentParameters *);
kernelWindowComponent *kernelWindowNewTitleBar(kernelWindow *,
					       componentParameters *);

#define _KERNELWINDOW_H
#endif
