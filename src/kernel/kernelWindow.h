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
//  kernelWindow.h
//
	
// This goes with the file kernelWindow.c

#if !defined(_KERNELWINDOW_H)

#include "kernelMouse.h"
#include "kernelGraphic.h"
#include "kernelText.h"
#include "kernelVariableList.h"
#include <string.h>
#include <sys/window.h>

// Definitions

#define WINDOW_TITLEBAR_HEIGHT              19
#define WINDOW_TITLEBAR_MINWIDTH            (WINDOW_TITLEBAR_HEIGHT * 4)
#define WINDOW_BORDER_THICKNESS             3
#define WINDOW_SHADING_INCREMENT            15
#define WINDOW_RADIOBUTTON_SIZE             10
#define WINDOW_CHECKBOX_SIZE                10
#define WINDOW_MIN_WIDTH                    (WINDOW_TITLEBAR_MINWIDTH + \
					     (WINDOW_BORDER_THICKNESS * 2))
#define WINDOW_MIN_HEIGHT                   (WINDOW_TITLEBAR_HEIGHT + \
                                             (WINDOW_BORDER_THICKNESS * 2))
#define WINDOW_MINREST_TRACERS              20
#define WINDOW_DEFAULT_CONFIG               "/system/config/window.conf"
#define WINDOW_DEFAULT_DESKTOP_CONFIG       "/system/config/desktop.conf"
#define WINDOW_DEFAULT_VARFONT_SMALL_FILE   "/system/fonts/arial-bold-10.bmp"
#define WINDOW_DEFAULT_VARFONT_SMALL_NAME   "arial-bold-10"
#define WINDOW_DEFAULT_VARFONT_MEDIUM_FILE  "/system/fonts/arial-bold-12.bmp"
#define WINDOW_DEFAULT_VARFONT_MEDIUM_NAME  "arial-bold-12"

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

typedef struct {
  struct {
    int minWidth;
    int minHeight;
    int minRestTracers;
  } window;

  struct {
    int height;
    int minWidth;
  } titleBar;

  struct {
    int thickness;
    int shadingIncrement;
  } border;

  struct {
    int size;
  } radioButton;

  struct {
    int size;
  } checkbox;

  struct {
    kernelAsciiFont *defaultFont;
    struct {
      struct {
	char file[MAX_PATH_NAME_LENGTH];
	char name[MAX_NAME_LENGTH];
	kernelAsciiFont *font;
      } small;
      struct {
	char file[MAX_PATH_NAME_LENGTH];
	char name[MAX_NAME_LENGTH];
	kernelAsciiFont *font;
      } medium;
    } varWidth;
  } font;

} kernelWindowVariables;

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
  componentParameters params;
  windowEventStream events;
  void (*eventHandler)(volatile struct _kernelWindowComponent *,
		       windowEvent *);
  int doneLayout;
  void *data;

  // Routines for managing this component.  These are set by the
  // kernelWindowComponentNew routine, for things that are common to all
  // components.
  int (*drawBorder) (volatile struct _kernelWindowComponent *, int);
  int (*erase) (volatile struct _kernelWindowComponent *);
  int (*grey) (volatile struct _kernelWindowComponent *);

  // Routines that should be implemented by components that 'contain'
  // or instantiate other components
  int (*add) (volatile struct _kernelWindowComponent *,
	      volatile struct _kernelWindowComponent *);
  int (*remove) (volatile struct _kernelWindowComponent *,
		 volatile struct _kernelWindowComponent *);
  int (*numComps) (volatile struct _kernelWindowComponent *);
  int (*flatten) (volatile struct _kernelWindowComponent *,
		  volatile struct _kernelWindowComponent **, int *, unsigned);
  int (*layout) (volatile struct _kernelWindowComponent *);
  volatile struct _kernelWindowComponent *
  (*eventComp) (volatile struct _kernelWindowComponent *, int, int);
  int (*setBuffer) (volatile struct _kernelWindowComponent *,
		    kernelGraphicBuffer *);

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

  // Functions
  void (*drawGrid) (kernelWindowComponent *);

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
  image image;
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

} kernelWindowMenu;

typedef volatile struct {
  kernelWindowComponent *visibleMenu;
  kernelWindowComponent *container;

} kernelWindowMenuBar;

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
  kernelMousePointer *pointer;

  volatile struct _kernelWindow *parentWindow;
  volatile struct _kernelWindow *dialogWindow;

  // Routines for managing this window
  int (*draw) (volatile struct _kernelWindow *);
  int (*drawClip) (volatile struct _kernelWindow *, int, int, int, int);
  int (*update) (volatile struct _kernelWindow *, int, int, int, int);
  int (*focusNextComponent) (volatile struct _kernelWindow *);
  int (*changeComponentFocus) (volatile struct _kernelWindow *,
			       kernelWindowComponent *);

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

static inline int doAreasIntersect(screenArea *firstArea,
				   screenArea *secondArea)
{
  // Return 1 if area 1 and area 2 intersect.

  if (isPointInside(firstArea->leftX, firstArea->topY, secondArea) ||
      isPointInside(firstArea->rightX, firstArea->topY, secondArea) ||
      isPointInside(firstArea->leftX, firstArea->bottomY, secondArea) ||
      isPointInside(firstArea->rightX, firstArea->bottomY, secondArea) ||
      isPointInside(secondArea->leftX, secondArea->topY, firstArea) ||
      isPointInside(secondArea->rightX, secondArea->topY, firstArea) ||
      isPointInside(secondArea->leftX, secondArea->bottomY, firstArea) ||
      isPointInside(secondArea->rightX, secondArea->bottomY, firstArea))
    return (1);

  else if (doLinesIntersect(firstArea->leftX, firstArea->topY,
			    firstArea->rightX,
			    secondArea->leftX, secondArea->topY,
			    secondArea->bottomY) ||
	   doLinesIntersect(secondArea->leftX, secondArea->topY,
			    secondArea->rightX,
			    firstArea->leftX, firstArea->topY,
			    firstArea->bottomY))
    return (1);

  else
    // Nope, not intersecting
    return (0);
}

static inline void removeFromContainer(kernelWindowComponent *component)
{
  // Remove the component from its parent container
  if (component->container && component->container->remove)
    component->container->remove(component->container, component);
  component->container = NULL;
}

#ifdef DEBUG
static inline const char *componentTypeString(kernelWindowObjectType type)
{
  // Return a string representation of a window object type.
 switch (type)
   {
   case genericComponentType:
     return ("genericComponentType");
   case borderComponentType:
     return ("borderComponentType");
   case buttonComponentType:
     return ("buttonComponentType");
   case canvasComponentType:
     return ("canvasComponentType");
   case checkboxComponentType:
     return ("checkboxComponentType");
   case containerComponentType:
     return ("containerComponentType");
   case iconComponentType:
     return ("iconComponentType");
   case imageComponentType:
     return ("imageComponentType");
   case listComponentType:
     return ("listComponentType");
   case listItemComponentType:
     return ("listItemComponentType");
   case menuComponentType:
     return ("menuComponentType");
   case menuBarComponentType:
     return ("menuBarComponentType");
   case progressBarComponentType:
     return ("progressBarComponentType");
   case radioButtonComponentType:
     return ("radioButtonComponentType");
   case scrollBarComponentType:
     return ("scrollBarComponentType");
   case sliderComponentType:
     return ("sliderComponentType");
   case sysContainerComponentType:
     return ("sysContainerComponentType");
   case textAreaComponentType:
     return ("textAreaComponentType");
   case textLabelComponentType:
     return ("textLabelComponentType");
   case titleBarComponentType:
     return ("titleBarComponentType");
   case windowType:
     return ("windowType");
   default:
     return ("unknown");
   }
}
#endif

// Functions exported by kernelWindow*.c functions
int kernelWindowInitialize(void);
int kernelWindowStart(void);
kernelWindow *kernelWindowMakeRoot(void);
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
int kernelWindowSwitchPointer(objectKey, const char *);
void kernelWindowMoveConsoleTextArea(kernelWindow *, kernelWindow *);

// Functions for managing components.  This first batch is from
// kernelWindowComponent.c
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
