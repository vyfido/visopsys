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
//  kernelWindow.c
//

// This is the code that does all of the generic stuff for setting up
// GUI windows.

#include "kernelWindow.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelFileStream.h"
#include "kernelFilesystem.h"
#include "kernelLoader.h"
#include "kernelLock.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelUser.h"
#include "kernelWindowEventStream.h"
#include <string.h>
#include <stdio.h>
#include <values.h>

static void windowThread(void) __attribute__((noreturn));

static int initialized = 0;
static int screenWidth = 0;
static int screenHeight = 0;

static kernelAsciiFont *defaultFont = NULL;
static kernelWindow *rootWindow = NULL;
static kernelWindowComponent *consoleTextArea = NULL;
static kernelWindow *consoleWindow = NULL;

// Keeps the data for all the windows
static kernelWindow *windowList[WINDOW_MAXWINDOWS];
static volatile int numberWindows = 0;
static lock windowListLock;

static kernelWindow *focusWindow = NULL;
static int winThreadPid = 0;

static kernelWindowComponent *draggingComponent = NULL;
static kernelWindowComponent *activeMenu = NULL;
static windowEventStream mouseEvents;
static windowEventStream keyEvents;

extern color kernelDefaultForeground;
extern color kernelDefaultBackground;
extern color kernelDefaultDesktop;


static inline int isAreaInside(screenArea *firstArea, screenArea *secondArea)
{
  // Return 1 if area 1 is 'inside' area 2

  if ((firstArea->leftX < secondArea->leftX) ||
      (firstArea->topY < secondArea->topY) ||
      (firstArea->rightX > secondArea->rightX) ||
      (firstArea->bottomY > secondArea->bottomY))
    return (0);
  else
    // Yup, it's covered.
    return (1);
}


static int doAreasIntersect(screenArea *firstArea, screenArea *secondArea)
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


static inline void getIntersectingArea(screenArea *firstArea,
				       screenArea *secondArea,
				       screenArea *overlap)
{
  // This will fill in the 'overlap' area with the coordinates shared by
  // the first two

  if (firstArea->leftX < secondArea->leftX)
    overlap->leftX = secondArea->leftX;
  else
    overlap->leftX = firstArea->leftX;
  
  if (firstArea->topY < secondArea->topY)
    overlap->topY = secondArea->topY;
  else
    overlap->topY = firstArea->topY;
  
  if (firstArea->rightX < secondArea->rightX)
    overlap->rightX = firstArea->rightX;
  else
    overlap->rightX = secondArea->rightX;

  if (firstArea->bottomY < secondArea->bottomY)
    overlap->bottomY = firstArea->bottomY;
  else
    overlap->bottomY = secondArea->bottomY;
}


static void addBorder(kernelWindow *window)
{
  // Adds the border components around the window

  componentParameters params;

  if (window->flags & WINFLAG_HASBORDER)
    {
      kernelError(kernel_error, "Window already has a border");
      return;
    }

  kernelMemClear(&params, sizeof(componentParameters));

  window->borders[0] =
    kernelWindowNewBorder(window->sysContainer, border_top, &params);

  window->borders[1] =
    kernelWindowNewBorder(window->sysContainer, border_left, &params);

  window->borders[2] =
    kernelWindowNewBorder(window->sysContainer, border_bottom, &params);

  window->borders[3] =
    kernelWindowNewBorder(window->sysContainer, border_right, &params);

  window->flags |= WINFLAG_HASBORDER;

  return;
}


static void removeBorder(kernelWindow *window)
{
  // Removes the borders from the window

  int count;

  if (!(window->flags & WINFLAG_HASBORDER))
    {
      kernelError(kernel_error, "Window doesn't have a border");
      return;
    }

  // Destroy the borders
  for (count = 0; count < 4; count ++)
    if (window->borders[count])
      kernelWindowComponentDestroy(window->borders[count]);

  window->flags &= ~WINFLAG_HASBORDER;

  return;
}


static void addTitleBar(kernelWindow *window)
{
  // Draws the title bar atop the window

  componentParameters params;

  if (window->titleBar)
    {
      kernelError(kernel_error, "Window already has a title bar");
      return;
    }

  // Standard parameters for a title bar
  kernelMemClear(&params, sizeof(componentParameters));

  window->titleBar = kernelWindowNewTitleBar(window, &params);
  if (window->titleBar == NULL)
    return;

  return;
}


static void removeTitleBar(kernelWindow *window)
{
  // Removes the title bar from atop the window

  if (!(window->titleBar))
    {
      kernelError(kernel_error, "Window doesn't have a title bar");
      return;
    }

  kernelWindowRemoveMinimizeButton(window);
  kernelWindowRemoveCloseButton(window);
  kernelWindowComponentDestroy(window->titleBar);

  return;
}


static int tileBackgroundImage(kernelWindow *window)
{
  // This will tile the supplied image as the background of the window.
  // Naturally, any other components in the window's client are need to be
  // drawn after this bit.
  
  int status = 0;
  int clientAreaX = window->mainContainer->xCoord;
  int clientAreaY = window->mainContainer->yCoord;
  unsigned clientAreaWidth = window->buffer.width;//mainContainer->width;
  unsigned clientAreaHeight = window->buffer.height;//mainContainer->height;
  unsigned count1, count2;

  kernelDebug(debug_gui, "Window buffer size=%ux%u mainContainer size=%ux%u "
	      "(%sdone layout)", window->buffer.width, window->buffer.height,
	      window->mainContainer->width, window->mainContainer->height,
	      (((kernelWindowContainer *) window->mainContainer->data)
	       ->doneLayout? "" : "not "));

  // The window needs to have been assigned a background image
  if (window->backgroundImage.data == NULL)
    return (status = ERR_NULLPARAMETER);

  if ((window->backgroundImage.width >= (clientAreaWidth / 2)) ||
      (window->backgroundImage.height >= (clientAreaHeight / 2)))
    {
      // Clear the window with its background color
      kernelGraphicClearArea(&(window->buffer),
			     (color *) &(window->background), 0, 0,
			     clientAreaWidth, clientAreaHeight);
      // Center the image in the window's client area
      status = kernelGraphicDrawImage(&(window->buffer), (image *)
	      &(window->backgroundImage), draw_normal,
	     ((clientAreaWidth - window->backgroundImage.width) / 2),
	     ((clientAreaHeight - window->backgroundImage.height) / 2),
	      0, 0, 0, 0);
      window->flags &= ~WINFLAG_BACKGROUNDTILED;
    }
  else
    {
      // Tile the image into the window's client area
      for (count1 = clientAreaY; count1 < clientAreaHeight;
	   count1 += window->backgroundImage.height)
	for (count2 = clientAreaX; count2 < clientAreaWidth;
	     count2 += window->backgroundImage.width)
	  status = kernelGraphicDrawImage(&(window->buffer), (image *)
				  &(window->backgroundImage), draw_normal,
				  count2, count1, 0, 0, 0, 0);
      window->flags |= WINFLAG_BACKGROUNDTILED;
    }

  return (status);
}


static void getCoveredAreas(screenArea *visibleClip, screenArea *coveringClip,
			    screenArea *coveredAreas, int *numCoveredAreas)
{
  // Utility function to simplify the renderVisiblePortions() function,
  // below

  int count;

  getIntersectingArea(visibleClip, coveringClip,
		      &(coveredAreas[*numCoveredAreas]));
  *numCoveredAreas += 1;

  // If the intersecting area is already covered by one of the other covered
  // areas, skip it.  Likewise if it covers another one, replace the other
  for (count = 0; count < (*numCoveredAreas - 1); count ++)
    {
      if (isAreaInside(&(coveredAreas[*numCoveredAreas - 1]),
		       &(coveredAreas[count])))
	{
	  *numCoveredAreas -= 1;
	  break;
	}
      else if (isAreaInside(&(coveredAreas[count]),
			    &(coveredAreas[*numCoveredAreas - 1])))
	{
	  coveredAreas[count].leftX =
	    coveredAreas[*numCoveredAreas - 1].leftX;
	  coveredAreas[count].topY =
	    coveredAreas[*numCoveredAreas - 1].topY;
	  coveredAreas[count].rightX =
	    coveredAreas[*numCoveredAreas - 1].rightX;
	  coveredAreas[count].bottomY =
	    coveredAreas[*numCoveredAreas - 1].bottomY;
	  *numCoveredAreas -= 1;
	  break;
	}
    }
}


static void renderVisiblePortions(kernelWindow *window, screenArea *bufferClip)
{
  // Takes the window supplied, and renders the portions of the supplied clip
  // which are visible (i.e. not covered by other windows).  Calls
  // kernelGraphicRenderBuffer() for all the visible bits.
  
  screenArea clipCopy;
  int numCoveredAreas = 0;
  screenArea coveredAreas[64];
  int numVisibleAreas = 1;
  screenArea visibleAreas[64];
  int count1, count2;

  // Make a copy of the screen area in case we modify it
  kernelMemCopy(bufferClip, &clipCopy, sizeof(screenArea));
  bufferClip = &clipCopy;

  // Make sure we're not trying to draw outside the window buffer
  if (bufferClip->leftX < 0)
    bufferClip->leftX = 0;
  if (bufferClip->topY < 0)
    bufferClip->topY = 0;
  if (bufferClip->rightX >= window->buffer.width)
    bufferClip->rightX = (window->buffer.width - 1);
  if (bufferClip->bottomY >= window->buffer.height)
    bufferClip->bottomY = (window->buffer.height - 1);

  visibleAreas[0].leftX = (window->xCoord + bufferClip->leftX);
  visibleAreas[0].topY =  (window->yCoord + bufferClip->topY);
  visibleAreas[0].rightX =  (window->xCoord + bufferClip->rightX);
  visibleAreas[0].bottomY =  (window->yCoord + bufferClip->bottomY);

  // Lock the window list
  if (kernelLockGet(&windowListLock) < 0)
    return;
 
  // Loop through the window list.  Any window which intersects this area
  // and is at a higher level will reduce the visible area
  for (count1 = 0; count1 < numberWindows; count1 ++)
    if ((windowList[count1] != window) &&
	(windowList[count1]->flags & WINFLAG_VISIBLE) &&
	(windowList[count1]->level < window->level))
      {
	// The current window list item may be covering the supplied
	// window.  Find out if it totally covers it, in which case we
	// are finished
	if (isAreaInside(&(visibleAreas[0]),
			 makeWindowScreenArea(windowList[count1])))
	  {
	    // Done
	    kernelLockRelease(&windowListLock);
	    return;
	  }

	// Find out whether it otherwise intersects our window
	if (doAreasIntersect(&(visibleAreas[0]),
			     makeWindowScreenArea(windowList[count1])))
	  // Yeah, this window is covering ours somewhat.  We will need
	  // to get the area of the windows that overlap
	  getCoveredAreas(&(visibleAreas[0]),
			  makeWindowScreenArea(windowList[count1]),
			  coveredAreas, &numCoveredAreas);
      }

  kernelLockRelease(&windowListLock);

  // Likewise, any active menu covers all windows
  if (activeMenu && (activeMenu->flags & WINFLAG_VISIBLE))
    {
      // Find out if it covers any part of our window

      // The active menu may be covering the supplied window.  Find out if
      // it totally covers it, in which case we are finished
      if (isAreaInside(&(visibleAreas[0]),
		       makeComponentScreenArea(activeMenu)))
	// Done
	return;

      // Find out whether it otherwise intersects our window
      if (doAreasIntersect(&(visibleAreas[0]),
			   makeComponentScreenArea(activeMenu)))
	// Yeah, the active menu is covering our window somewhat.  We will
	// need to get the area of the windows that overlap
	getCoveredAreas(&(visibleAreas[0]),
			makeComponentScreenArea(activeMenu),
			coveredAreas, &numCoveredAreas);
    }

  // Now that we have a list of all the non-visible portions of the window,
  // we can make a list of the remaining parts that are visible

  // For each covering area, examine each visible area.  If the areas
  // intersect, then anywhere from 1 to 4 new visible areas will be
  // created
  for (count1 = 0; count1 < numCoveredAreas; count1 ++)
    for (count2 = 0; count2 < numVisibleAreas; count2 ++)
      {
	if (!doAreasIntersect(&(coveredAreas[count1]),
			      &(visibleAreas[count2])))
	  continue;

	if (visibleAreas[count2].leftX < coveredAreas[count1].leftX)
	  {
	    // The leftmost area of the visible area is unaffected.  Split
	    // it by copying the rest to the end of the list, and narrowing
	    // this area
	    visibleAreas[numVisibleAreas].leftX = coveredAreas[count1].leftX;
	    visibleAreas[numVisibleAreas].topY = visibleAreas[count2].topY;
	    visibleAreas[numVisibleAreas].rightX = visibleAreas[count2].rightX;
	    visibleAreas[numVisibleAreas++].bottomY =
	      visibleAreas[count2].bottomY;
	    
	    visibleAreas[count2].rightX = (coveredAreas[count1].leftX - 1);
	  }

	else if (visibleAreas[count2].topY < coveredAreas[count1].topY)
	  {
	    // The topmost area of the visible area is unaffected.  Split
	    // it by copying the rest to the end of the list, and shortening
	    // this area
	    visibleAreas[numVisibleAreas].leftX = visibleAreas[count2].leftX;
	    visibleAreas[numVisibleAreas].topY = coveredAreas[count1].topY;
	    visibleAreas[numVisibleAreas].rightX =
	      visibleAreas[count2].rightX;
	    visibleAreas[numVisibleAreas++].bottomY =
	      visibleAreas[count2].bottomY;
	    
	    visibleAreas[count2].bottomY = (coveredAreas[count1].topY - 1);
	  }

	else if (visibleAreas[count2].rightX > coveredAreas[count1].rightX)
	  {
	    // The rightmost area of the visible area is unaffected.  Split
	    // it by copying the rest to the end of the list, and narrowing
	    // this area
	    visibleAreas[numVisibleAreas].leftX = visibleAreas[count2].leftX;
	    visibleAreas[numVisibleAreas].topY = visibleAreas[count2].topY;
	    visibleAreas[numVisibleAreas].rightX = coveredAreas[count1].rightX;
	    visibleAreas[numVisibleAreas++].bottomY =
	      visibleAreas[count2].bottomY;
	    
	    visibleAreas[count2].leftX = (coveredAreas[count1].rightX + 1);
	  }

	else if (visibleAreas[count2].bottomY > coveredAreas[count1].bottomY)
	  {
	    // The bottom area of the visible area is unaffected.  Split
	    // it by copying the rest to the end of the list, and shortening
	    // this area
	    visibleAreas[numVisibleAreas].leftX = visibleAreas[count2].leftX;
	    visibleAreas[numVisibleAreas].topY = visibleAreas[count2].topY;
	    visibleAreas[numVisibleAreas].rightX = visibleAreas[count2].rightX;
	    visibleAreas[numVisibleAreas++].bottomY =
	      coveredAreas[count1].bottomY;

	    visibleAreas[count2].topY = (coveredAreas[count1].bottomY + 1);
	  }

	else if (isAreaInside(&(visibleAreas[count2]),
			      &(coveredAreas[count1])))
	  {
	    // This area is not visible.  Get rid of it
	    numVisibleAreas--;
	    visibleAreas[count2].leftX = visibleAreas[numVisibleAreas].leftX;
	    visibleAreas[count2].topY = visibleAreas[numVisibleAreas].topY;
	    visibleAreas[count2].rightX = visibleAreas[numVisibleAreas].rightX;
	    visibleAreas[count2].bottomY =
	      visibleAreas[numVisibleAreas].bottomY;
	    count2--;
	  }
      }

  // Render all of the visible portions
  for (count1 = 0; count1 < numVisibleAreas; count1 ++)
    {
      // Adjust it so that it's a clip inslide the window buffer
      visibleAreas[count1].leftX -= window->xCoord;
      visibleAreas[count1].topY -= window->yCoord;
      visibleAreas[count1].rightX -= window->xCoord;
      visibleAreas[count1].bottomY -= window->yCoord;

      // Adjust it so that it's fully on the screen
      if (visibleAreas[count1].leftX < 0)
	visibleAreas[count1].leftX = 0;
      if (visibleAreas[count1].topY < 0)
	visibleAreas[count1].topY = 0;
      if (visibleAreas[count1].rightX >= screenWidth)
	visibleAreas[count1].rightX = (screenWidth - 1);
      if (visibleAreas[count1].bottomY >= screenHeight)
	visibleAreas[count1].bottomY = (screenHeight - 1);

      kernelGraphicRenderBuffer(&(window->buffer),
	window->xCoord, window->yCoord, visibleAreas[count1].leftX,
	visibleAreas[count1].topY, (visibleAreas[count1].rightX -
				    visibleAreas[count1].leftX + 1),
	(visibleAreas[count1].bottomY - visibleAreas[count1].topY + 1));
    }

  return;
}


static int countComponents(kernelWindowContainer *container)
{
  // Recursively count up the number of compontents in a container and any
  // subcontainers.

  int numComponents = 0;
  kernelWindowComponent *component = NULL;
  int count;

  numComponents = container->numComponents;

  for (count = 0; count < container->numComponents; count ++)
    {
      component = container->components[count];
      
      if (component->type == containerComponentType)
	numComponents += countComponents(component->data);
    }

  return (numComponents);
}


static void flattenContainer(kernelWindowContainer *container,
			     kernelWindowComponent **array, int *numItems,
			     unsigned flags)
{
  // Given a container, recurse through any of its subcontainers (if
  // applicable) and return a flattened array of components

  kernelWindowComponent *component = NULL;
  int count;

  for (count = 0; count < container->numComponents; count ++)
    {
      component = container->components[count];

      if (((component->flags & flags) == flags) &&
	  // Don't include pure container components
	  ((component->type != containerComponentType) ||
	   (component->subType != genericComponentType)))
	{
	  array[*numItems] = component;
	  *numItems += 1;
	}

      // If this component is a container, recurse it
      if (component->type == containerComponentType)
	flattenContainer(component->data, array, numItems, flags);
    }
}


static int drawWindowClip(kernelWindow *window, int xCoord, int yCoord,
			  int width, int height)
{
  // Draws a clip of the client area of a window.  First blanks the bounded
  // area with the background color, then draws the appropriate clip of any
  // background image (which must already have been tiled, etc., from a
  // previous call to tileBackgroundImage()), then draws any visible
  // components that are entirely or partially within the bounded area (in
  // order of their 'level', lowermost to uppermost).  Finally calls
  // renderVisiblePortions() to put all the visible bits of the bounded area
  // on the screen.

  int status = 0;
  kernelWindowContainer *mainContainer = window->mainContainer->data;
  kernelWindowComponent **array = NULL;
  int numComponents = 0;
  kernelWindowComponent *component = NULL;
  int xOffset, yOffset;
  int lowestLevel = 0;
  int count1, count2;

  if (!(window->flags & WINFLAG_VISIBLE))
    return (status = 0);

  // Put the clip within the boundaries of the window
  if (xCoord < 0)
    {
      width += xCoord;
      xCoord = 0;
    }
  if (yCoord < 0)
    {
      height += yCoord;
      yCoord = 0;
    }
  if ((xCoord >= window->buffer.width) || (yCoord >= window->buffer.height))
    return (status = 0);
  if ((xCoord + width) > window->buffer.width)
    width -= ((xCoord + width) - window->buffer.width);
  if ((yCoord + height) > window->buffer.height)
    height -= ((yCoord + height) - window->buffer.height);
  if ((width <= 0) || (height <= 0))
    return (status = 0);

  // Blank the area with the window's background color
  kernelGraphicDrawRect(&(window->buffer),
			(color *) &(window->background), draw_normal,
			xCoord, yCoord, width, height, 0, 1);

  // If the window has a background image, draw it in this space
  if (window->backgroundImage.data)
    {
      if (window->flags & WINFLAG_BACKGROUNDTILED)
	{
	  // If you want to study this next bit, and send me emails telling
	  // me how clever I am, please do.
	  yOffset = (yCoord % window->backgroundImage.height);
	  for (count2 = yCoord; (count2 < (yCoord + height)); )
	    {
	      xOffset = (xCoord % window->backgroundImage.width);
	      for (count1 = xCoord; (count1 < (xCoord + width)); )
		{
		  kernelGraphicDrawImage(&(window->buffer), (image *)
					 &(window->backgroundImage),
					 draw_normal, count1, count2,
					 xOffset, yOffset,
					 ((xCoord + width) - count1),
					 ((yCoord + height) - count2));
		  count1 += (window->backgroundImage.width - xOffset);
		  xOffset = 0;
		}
	      
	      count2 += (window->backgroundImage.height - yOffset);
	      yOffset = 0;
	    }
	}
      else
	{
	  // Draw the background once into our clip

	  // The coordinates of the image depend on its size.
	  count1 = ((window->buffer.width -
		     window->backgroundImage.width) / 2);
	  count2 = ((window->buffer.height -
		     window->backgroundImage.height) / 2);
	  kernelGraphicDrawImage(&(window->buffer), (image *)
				 &(window->backgroundImage), draw_normal,
				 xCoord, yCoord, (count1 + xCoord),
				 (count2 + yCoord), width, height);
	}
    }

  // Loop through all the regular window components that fall (partially)
  // within this space and draw them
  array = kernelMalloc((countComponents(window->sysContainer->data) +
			countComponents(window->mainContainer->data)) *
		       sizeof(kernelWindowComponent *));
  if (array == NULL)
    return (status = ERR_MEMORY);

  flattenContainer(window->sysContainer->data, array, &numComponents,
		   WINFLAG_VISIBLE);
  flattenContainer(window->mainContainer->data, array, &numComponents,
		   WINFLAG_VISIBLE);

  // NULL all components that are *not* at this location
  for (count1 = 0; count1 < numComponents; count1 ++)
    {
      component = array[count1];

      if (!doAreasIntersect(&((screenArea)
      { xCoord, yCoord, (xCoord + width - 1), (yCoord + height - 1)} ),
			    &((screenArea)
			    { component->xCoord, component->yCoord,
			(component->xCoord + component->width - 1),
			(component->yCoord + component->height - 1) } )))
	{
	  array[count1] = NULL;
	}
      else
	{
	  if (component->level > lowestLevel)
	    lowestLevel = component->level;
	}
    }
  
  // Draw all the components by level

  for (count1 = lowestLevel; count1 >= 0; count1 --) 
    for (count2 = 0; count2 < numComponents; count2 ++)
      {
        component = array[count2];

	if ((component != NULL) && (component->level >= count1))
	  {
	    if (component->draw)
	      component->draw(component);

	    array[count2] = NULL;
	  }
      }

  kernelFree(array);

  if (window->flags & WINFLAG_DEBUGLAYOUT)
    mainContainer->containerDrawGrid(window->mainContainer);

  // Only render the visible portions of the window
  renderVisiblePortions(window, &((screenArea)
  { xCoord, yCoord, (xCoord + width - 1), (yCoord + height - 1) } ));

  return (status = 0);
}


static int drawWindow(kernelWindow *window)
{
  // Draws the whole window.  First blanks the buffer with the background
  // color, then calls tileBackgroundImage() to draw any background image,
  // then calls drawWindowClip() to draw and render the visible contents
  // on the screen. 

  int status = 0;

  // Draw a blank background
  kernelGraphicDrawRect(&(window->buffer), (color *) &(window->background),
			draw_normal, 0, 0, window->buffer.width,
			window->buffer.height, 0, 1);

  // If the window has a background image, draw it
  if (window->backgroundImage.data)
    tileBackgroundImage(window);

  drawWindowClip(window, 0, 0, window->buffer.width, window->buffer.height);

  // Done
  return (status = 0);
}


static int windowUpdate(kernelWindow *window, int clipX, int clipY, int width,
			int height)
{
  // A component is trying to tell us that it has updated itself in the
  // supplied window, and would like the bounded area of the relevent window
  // to be redrawn on screen.  Does nothing if the window is not currently
  // visible.  Calls renderVisiblePortions() to draw only the visible
  // portions of the window clip.

  int status = 0;

  // Check parameters
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  if (!(window->flags & WINFLAG_VISIBLE))
    // It's not currently on the screen
    return (status = 0);

  // Render the parts of this window's buffer that are currently visible.
  renderVisiblePortions(window, &((screenArea) { clipX, clipY,
			   (clipX + (width - 1)), (clipY + (height - 1)) } ));

  // Redraw the mouse
  kernelMouseDraw();

  return (status = 0);
}


static int getWindowGraphicBuffer(kernelWindow *window, int width,
				  int height)
{
  // Allocate and attach memory to a kernelWindow for its kernelGraphicBuffer

  int status = 0;
  unsigned bufferBytes = 0;

  window->buffer.width = width;
  window->buffer.height = height;
  
  // Get the number of bytes of memory we need to reserve for this window's
  // kernelGraphicBuffer, depending on the size of the window
  bufferBytes = kernelGraphicCalculateAreaBytes(width, height);
  
  // Get some memory for it
  window->buffer.data = kernelMalloc(bufferBytes);
  if (window->buffer.data == NULL)
    return (status = ERR_MEMORY);

  return (status = 0);
}


static int setWindowSize(kernelWindow *window, int width, int height)
{
  // Sets the size of a window

  int status = 0;
  void *oldBufferData = NULL;

  // Constrain to minimum width and height
  if (width < WINDOW_MIN_WIDTH)
    width = WINDOW_MIN_WIDTH;
  if (height < WINDOW_MIN_HEIGHT)
    height = WINDOW_MIN_HEIGHT;

  // Save the old graphic buffer data just in case
  oldBufferData = window->buffer.data;
  window->buffer.data = NULL;

  // Set the size.  We need to get a new graphics buffer if it's bigger
  status = getWindowGraphicBuffer(window, width, height);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to get new window graphic buffer for "
		  "resize operation");
      window->buffer.data = oldBufferData;
      return (status);
    }

  // Release the memory from the old buffer
  kernelFree(oldBufferData);

  // Resize the system container.  This will also cause the main container
  // to be resized and moved appropriately
  if (window->sysContainer)
    {
      if (window->sysContainer->resize)
	window->sysContainer->resize(window->sysContainer, width, height);
  
      window->sysContainer->width = width;
      window->sysContainer->height = height;
    }

  // If the window is visible, redraw it
  if (window->flags & WINFLAG_VISIBLE)
    drawWindow(window);

  // Return success
  return (status = 0);
}


static int autoSizeWindow(kernelWindow *window)
{
  // This will automatically set the size of a window based on the sizes
  // and locations of the components therein.
  
  int newWidth = 0;
  int newHeight = 0;

  newWidth = (window->mainContainer->xCoord + window->mainContainer->width);
  newHeight = (window->mainContainer->yCoord + window->mainContainer->height);

  // Adjust for borders
  if (window->flags & WINFLAG_HASBORDER)
    {
      newWidth += WINDOW_BORDER_THICKNESS;
      newHeight += WINDOW_BORDER_THICKNESS;
    }

  // Resize it.
  return (setWindowSize(window, newWidth, newHeight));
}


static int layoutWindow(kernelWindow *window)
{
  // Repositions all the window's components based on their parameters

  int status = 0;
  kernelWindowContainer *sysContainer = window->sysContainer->data;
  kernelWindowContainer *mainContainer = window->mainContainer->data;

  status = sysContainer->containerLayout(window->sysContainer);
  if (status < 0)
    return (status);

  status = mainContainer->containerLayout(window->mainContainer);
  if (status < 0)
    return (status);
  
  return (status = 0);
}


static int makeConsoleWindow(void)
{
  // Create the temporary console window

  int status = 0;
  componentParameters params;
  kernelWindowTextArea *textArea = NULL;
  kernelTextArea *oldArea = NULL;
  kernelTextArea *newArea = NULL;
  unsigned char *lineAddress = NULL;
  char lineBuffer[1024];
  int lineBufferCount = 0;
  int rowCount, columnCount;

  consoleWindow = kernelWindowNew(KERNELPROCID, WINNAME_TEMPCONSOLE);
  if (consoleWindow == NULL)
    return (status = ERR_NOCREATE);

  kernelMemClear(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.font = defaultFont;

  consoleTextArea =
    kernelWindowNewTextArea(consoleWindow, 80, 50,
			    TEXT_DEFAULT_SCROLLBACKLINES, &params);
  if (consoleTextArea == NULL)
    {
      kernelError(kernel_warn, "Unable to switch text areas to console "
		  "window");
      return (status = ERR_NOCREATE);
    }

  textArea = consoleTextArea->data;
  oldArea = kernelTextGetConsoleOutput()->textArea;
  newArea = textArea->area;

  // Turn off the cursor
  newArea->cursorState = 0;
  
  // Redirect console and current text IO to this new area
  kernelTextSetConsoleInput(newArea->inputStream);
  kernelTextSetConsoleOutput(newArea->outputStream);

  kernelTextSetCurrentInput(newArea->inputStream);
  kernelTextSetCurrentOutput(newArea->outputStream);

  // Set the kernel's input and output streams as well
  kernelMultitaskerSetTextInput(KERNELPROCID, newArea->inputStream);
  kernelMultitaskerSetTextOutput(KERNELPROCID, newArea->outputStream);

  // Loop through contents of the current console area, and put them into
  // the buffer belonging to this new text area.  Remember that the new
  // text area might not (probably won't) have the same dimensions as the
  // previous one.  Note that this is not really important, and is mostly
  // just for showing off.
  for (rowCount = 0; ((rowCount < oldArea->rows) &&
		      (rowCount < newArea->rows)); rowCount ++)
    {
      lineAddress =
	(oldArea->visibleData + (rowCount * oldArea->columns));
      lineBufferCount = 0;
      
      for (columnCount = 0; (columnCount < oldArea->columns) &&
	     (columnCount < newArea->columns); columnCount ++)
	{
	  lineBuffer[lineBufferCount++] = lineAddress[columnCount];
	  if (lineAddress[columnCount] == '\n')
	    break;
	}
      
      // Make sure there's a NULL
      lineBuffer[lineBufferCount] = '\0';
      
      if (lineBufferCount > 0)
	// Print the line to the new text area
	kernelTextStreamPrint(newArea->outputStream, lineBuffer);
    }

  // Deallocate the old, temporary area, but don't let it deallocate the
  // input/output streams.
  kernelWindowComponent *component = oldArea->windowComponent;
  if (component)
    {
      if (component->buffer)
	kernelFree((void *) component->buffer);

      kernelFree((void *) component);
    }
  kernelTextAreaDestroy(oldArea);

  return (status = 0);
}


static void changeComponentFocus(kernelWindow *window,
				 kernelWindowComponent *component)
{
  // Gets called when a component acquires the focus

  if (component && !(component->flags & WINFLAG_CANFOCUS))
    {
      kernelError(kernel_error, "Component %p cannot focus", component);
      return;
    }

  if (activeMenu && (component != activeMenu))
    {
      activeMenu->flags &= ~WINFLAG_HASFOCUS;
      if (activeMenu->focus)
	activeMenu->focus(activeMenu, 0);
      activeMenu = NULL;
    }
  else if (window->focusComponent && (component != window->focusComponent))
    {
      window->focusComponent->flags &= ~WINFLAG_HASFOCUS;
      if (window->focusComponent->focus)
	window->focusComponent->focus(window->focusComponent, 0);
      window->oldFocusComponent = window->focusComponent;
    }

  // This might be NULL.  That is okay.
  window->focusComponent = component;

  if (component)
    {
      if ((component->flags & WINFLAG_VISIBLE) &&
	  (component->flags & WINFLAG_CANFOCUS))
	{
	  component->flags |= WINFLAG_HASFOCUS;
	  if (component->focus)
	    component->focus(component, 1);
	}
	
      // If it's a menu, make it the active menu
      if (component->type == menuComponentType)
	activeMenu = component;
    }
  
  return;
}


static void focusFirstComponent(kernelWindow *window)
{
  // Set the focus to the first focusable component
  
  kernelWindowComponent **array = NULL;
  int numComponents = 0;
  int count;
  
  // Flatten the window container so we can iterate through it
  array = kernelMalloc(countComponents(window->mainContainer->data) *
		       sizeof(kernelWindowComponent *));
  if (array == NULL)
    return;

  flattenContainer(window->mainContainer->data, array, &numComponents,
		   (WINFLAG_VISIBLE | WINFLAG_ENABLED | WINFLAG_CANFOCUS));
  
  if (numComponents)
    {
      // If the window has any sort of text area or field inside it, set the
      // input/output streams to that process.
      for (count = 0; count < numComponents; count ++)
	if (array[count]->type == textAreaComponentType)
	  {
	    changeComponentFocus(window, array[count]);
	    break;
	  }

      // Still no focus?  Give it to the first component that can focus
      if (!window->focusComponent)
	changeComponentFocus(window, array[0]);
    }

  kernelFree(array);
}


static void focusNextComponent(kernelWindow *window)
{
  // Change the focus the next component
  
  kernelWindowComponent **array = NULL;
  int numComponents = 0;
  kernelWindowComponent *nextFocus = NULL;
  int count = 0;

  if (!window->focusComponent)
    {
      focusFirstComponent(window);
      return;
    }

  // Get all the window components in a flat container
  array = kernelMalloc(countComponents(window->mainContainer->data) *
		       sizeof(kernelWindowComponent *));
  if (array == NULL)
    return;

  flattenContainer(window->mainContainer->data, array, &numComponents,
		   (WINFLAG_VISIBLE | WINFLAG_ENABLED | WINFLAG_CANFOCUS));
  
  for (count = 0; count < numComponents; count ++)
    if (array[count] == window->focusComponent)
      {
	if (count < (numComponents - 1))
	  nextFocus = array[count + 1];
	else
	  nextFocus = array[0];
	
	break;
      }

  kernelFree(array);

  if (nextFocus)
    changeComponentFocus(window, nextFocus);
  else
    focusFirstComponent(window);

  return;
}


static void changeWindowFocus(kernelWindow *window)
{
  // Gets called when a window acquires the focus
  
  int count;
  
  // The root window never gets or loses the focus
  if ((window == NULL) || (window == rootWindow))
    {
      focusWindow = NULL;
      return;
    }

  if (window != focusWindow)
    {
      // Lock the window list
      if (kernelLockGet(&windowListLock) < 0)
       	return;

      // Decrement the levels of all windows that used to be above us
      for (count = 0; count < numberWindows; count ++)
	if ((windowList[count] != window) &&
	    (windowList[count]->level <= window->level))
	  windowList[count]->level++;
  
      kernelLockRelease(&windowListLock);

      if (focusWindow)
	{
	  // Remove the focus from the previously focused window

	  // This is not the focus window any more.  We do this so that the
	  // title bar 'draw' routine won't make it look focused
	  focusWindow->flags &= ~WINFLAG_HASFOCUS;
	  
	  if (focusWindow->titleBar)
	    focusWindow->titleBar->draw(focusWindow->titleBar);

	  // Redraw the window's title bar area only
	  windowUpdate(focusWindow, focusWindow->titleBar->xCoord,
		       focusWindow->titleBar->yCoord,
		       focusWindow->titleBar->width,
		       focusWindow->titleBar->height);
	}
    }

  // This window becomes topmost
  window->level = 0;

  // Mark it as focused
  window->flags |= WINFLAG_HASFOCUS;
  focusWindow = window;

  // Draw the title bar as focused
  if (window->titleBar)
    window->titleBar->draw(window->titleBar);

  if (window->focusComponent)
    changeComponentFocus(window, window->focusComponent);
  else
    focusFirstComponent(window);

  // Redraw the whole window, since all of it is now visible (otherwise we
  // would call drawVisiblePortions()).
  windowUpdate(window, 0, 0, window->buffer.width, window->buffer.height);
  return;
}


static int componentDrawBorder(kernelWindowComponent *component, int draw)
{
  // Draw or erase a simple little border around the supplied component
  
  if (draw)
    kernelGraphicDrawRect(component->buffer,
			  (color *) &(component->parameters.foreground),
			  draw_normal, (component->xCoord - 2),
			  (component->yCoord - 2), (component->width + 4),
			  (component->height + 4), 1, 0);
  else
    kernelGraphicDrawRect(component->buffer,
			  (color *) &(component->window->background),
			  draw_normal, (component->xCoord - 2),
			  (component->yCoord - 2), (component->width + 4),
			  (component->height + 4), 1, 0);
  return (0);
}


static int componentErase(kernelWindowComponent *component)
{
  // Simple erasure of a component, by drawing a filled rectangle of the
  // window's background color over the component's area

  kernelGraphicDrawRect(component->buffer,
			(color *) &(component->window->background),
			draw_normal, component->xCoord, component->yCoord,
			component->width, component->height, 1, 1);
  return (0);
}


static int componentGrey(kernelWindowComponent *component)
{
  // Filter the component with the default background color

  // If the component has a draw function (stored in its 'grey' pointer)
  // call it first.
  if (component->grey)
    component->grey(component);

  kernelGraphicFilter(component->buffer,
		      (color *) &(component->parameters.background),
		      component->xCoord, component->yCoord, component->width,
		      component->height);
  return (0);
}


static kernelWindowComponent *findTopmostComponent(kernelWindow *window,
						   int xCoord, int yCoord)
{
  kernelWindowComponent **array = NULL;
  int numComponents = 0;
  int topmostLevel = MAXINT;
  kernelWindowComponent *topmostComponent = NULL;
  int count;

  // Make a flat container so we can loop through all the components
  array = kernelMalloc((countComponents(window->sysContainer->data) +
			countComponents(window->mainContainer->data)) *
		       sizeof(kernelWindowComponent *));
  if (array == NULL)
    return (NULL);

  flattenContainer(window->sysContainer->data, array, &numComponents,
		   (WINFLAG_VISIBLE | WINFLAG_ENABLED));
  flattenContainer(window->mainContainer->data, array, &numComponents,
		   (WINFLAG_VISIBLE | WINFLAG_ENABLED));

  // Find the window's topmost component at this location
  for (count = 0; count < numComponents; count ++)
    {
      if ((array[count]->level < topmostLevel) &&
	  isPointInside(xCoord, yCoord, makeComponentScreenArea(array[count])))
	{
	  topmostComponent = array[count];
	  topmostLevel = array[count]->level;
	}
    }

  kernelFree(array);

  return (topmostComponent);
}


static void processEvents(void)
{
  windowEvent event;
  windowEvent tmpEvent;
  kernelWindow *window = NULL;
  kernelWindowComponent *targetComponent = NULL;
  kernelWindowComponent *contextMenu = NULL;
  int count;

  while (kernelWindowEventStreamRead(&mouseEvents, &event) > 0)
    {
      // It was a mouse event

      // Shortcut: If we are dragging a component, give the event right
      // to the component
      if (draggingComponent)
	{
	  // If there's another dragging event pending, skip to it
	  if (kernelWindowEventStreamPeek(&mouseEvents) == EVENT_MOUSE_DRAG)
	    continue;

	  if (draggingComponent->mouseEvent)
	    draggingComponent->mouseEvent(draggingComponent, &event);

	  // Adjust to the coordinates of the component
	  event.xPosition -=
	    (draggingComponent->window->xCoord + draggingComponent->xCoord);
	  event.yPosition -=
	    (draggingComponent->window->yCoord + draggingComponent->yCoord);
	  
	  // Put this mouse event into the component's windowEventStream
	  kernelWindowEventStreamWrite(&(draggingComponent->events), &event);

	  if (event.type != EVENT_MOUSE_DRAG)
	    draggingComponent = NULL;

	  // We can stop here
	  continue;
	}

      // Figure out which window this is happening to, if any
      
      // Lock the window list
      if (kernelLockGet(&windowListLock) < 0)
	return;
	 
      for (count = 0; count < numberWindows; count ++)
	if ((windowList[count]->flags & WINFLAG_VISIBLE) &&
	    (isPointInside(event.xPosition, event.yPosition,
			   makeWindowScreenArea(windowList[count]))))
	  {
	    // The mouse is inside this window's coordinates.  Is it the
	    // topmost such window we've found?
	    if ((window == NULL) || (windowList[count]->level < window->level))
	      window = windowList[count];
	  }
	  
      kernelLockRelease(&windowListLock);
      
      // Was it inside a window?
      if (window == NULL)
	// This should never happen.  Anyway, ignore.
	continue;
      
      // The event was inside a window

      // If the window has a dialog window, focus the dialog instead and
      // we're finished
      if (window->dialogWindow)
	{
	  if (window->dialogWindow != focusWindow)
	    changeWindowFocus(window->dialogWindow);
	  return;
	}
  
      // If it was a click and the window is not in focus, and not
      // the root window, give it the focus
      if ((event.type & EVENT_MOUSE_DOWN) && (window != focusWindow) &&
	  (window != rootWindow))
	// Give the window the focus
	changeWindowFocus(window);

      // Find out if it was inside of any of the window's components,
      // and if so, put a windowEvent into its windowEventStream
      targetComponent =
	findTopmostComponent(window, event.xPosition, event.yPosition);

      if ((event.type & EVENT_MOUSE_DOWN) &&
	  (window->focusComponent != targetComponent))
	{
	  if (targetComponent && (targetComponent->flags & WINFLAG_CANFOCUS))
	    changeComponentFocus(window, targetComponent);
	  else if (activeMenu &&
		   !isPointInside(event.xPosition, event.yPosition,
				  makeComponentScreenArea(activeMenu)))
	    {
	      if (window->oldFocusComponent)
		changeComponentFocus(window, window->oldFocusComponent);
	      else
		focusFirstComponent(window);
	    }
	}

      if (targetComponent)
	{
	  if (targetComponent->mouseEvent)
	    targetComponent->mouseEvent(targetComponent, &event);

	  kernelMemCopy(&event, &tmpEvent, sizeof(event));
	  
	  // Adjust to the coordinates of the component
	  tmpEvent.xPosition -= (window->xCoord + targetComponent->xCoord);
	  tmpEvent.yPosition -= (window->yCoord + targetComponent->yCoord);

	  // Put this mouse event into the component's windowEventStream
	  kernelWindowEventStreamWrite(&(targetComponent->events), &tmpEvent);

	  if (event.type == EVENT_MOUSE_DRAG)
	    draggingComponent = targetComponent;
	}

      if (event.type == EVENT_MOUSE_RIGHTDOWN)
	{
	  if (targetComponent)
	    contextMenu = targetComponent->contextMenu;

	  if (!contextMenu)
	    contextMenu = window->contextMenu;

	  if (contextMenu)
	    {
	      kernelDebug(debug_gui, "Show context menu");

	      contextMenu->xCoord = (event.xPosition - window->xCoord);
	      contextMenu->yCoord = (event.yPosition - window->yCoord);

	      // Shouldn't go off the screen
	      if ((event.xPosition + contextMenu->width) > screenWidth)
		contextMenu->xCoord -=
		  ((event.xPosition + contextMenu->width) - screenWidth);
	      if ((event.yPosition + contextMenu->height) > screenHeight)
		contextMenu->yCoord -=
		  ((event.yPosition + contextMenu->height) - screenHeight);

	      kernelWindowComponentSetVisible(contextMenu, 1);
	    }
	}
    }

  while (kernelWindowEventStreamRead(&keyEvents, &event) > 0)
    {
      // It was a keyboard event

      // Find the target component
      if (focusWindow)
	{
	  // If it was a [tab] down, focus the next component
	  if (((focusWindow->focusComponent == NULL) ||
	       !(focusWindow->focusComponent->parameters.flags &
		 WINDOW_COMPFLAG_STICKYFOCUS)) &&
	      ((event.type == EVENT_KEY_DOWN) && (event.key == 9)))
	    focusNextComponent(focusWindow);
	  
	  else if (focusWindow->focusComponent)
	    {
	      targetComponent = focusWindow->focusComponent;
	      
	      if (targetComponent->keyEvent)
		targetComponent->keyEvent(targetComponent, &event);
	  
	      // Put this key event into the component's windowEventStream
	      kernelWindowEventStreamWrite(&(targetComponent->events), &event);
	    }
	}
    }

  return;
}


static void windowThread(void)
{
  // This thread runs as the 'window thread' to watch for window events
  // on 'system' GUI components such as window close buttons.

  kernelWindow *window = NULL;
  kernelWindowContainer *container = NULL;
  kernelWindowComponent *component = NULL;  
  windowEvent event;
  int processId = 0;
  static int winCount, compCount;

  while(1)
    {
      // Process the pending event streams
      processEvents();

      // Loop through all of the windows, looking for events in 'system'
      // components

      // Lock the window list
      if (kernelLockGet(&windowListLock) < 0)
	continue;
	 
      for (winCount = 0; winCount < numberWindows; winCount ++)
	{
	  window = windowList[winCount];
	  
	  if (!window)
	    continue;
	  
	  processId = window->processId;

	  // Check to see whether the process that owns the window is still
	  // alive.  If not, destroy the window and quit for this event.
	  if (!kernelMultitaskerProcessIsAlive(processId))
	    {
	      kernelWindowDestroy(window);
	      // Restart the loop
	      winCount = -1;
	      continue;
	    }

	  container = window->sysContainer->data;

	  // Loop through the system components
	  for (compCount = 0; compCount < container->numComponents;
	       compCount ++)
	    {
	      component = container->components[compCount];
 
	      // Any handler for the event?  Any events pending?
	      if (component->eventHandler &&
		  (kernelWindowEventStreamRead(&(component->events),
					       &event) > 0))
		{
		  component->eventHandler((objectKey) component, &event);

		  // Window closed?  Don't want to loop here any more.
		  if (!kernelMultitaskerProcessIsAlive(processId))
		    break;
		}
	    }
	}
    
      kernelLockRelease(&windowListLock);

      // Done
      kernelMultitaskerYield();
    }
}


static int spawnWindowThread(void)
{
  // Spawn the window thread
  winThreadPid = kernelMultitaskerSpawnKernelThread(windowThread,
						    "window thread", 0, NULL);
  if (winThreadPid < 0)
    return (winThreadPid);

  kernelLog("Window thread started");
  return (0);
}


static kernelWindow *findTopmostWindow(void)
{
  int topmostLevel = MAXINT;
  kernelWindow *topmostWindow = NULL;
  int count;

  // Lock the window list
  if (kernelLockGet(&windowListLock) < 0)
    return (topmostWindow = NULL);
	 
  // Find the topmost window
  for (count = 0; count < numberWindows; count ++)
    {
      if ((windowList[count]->flags & WINFLAG_VISIBLE) &&
	  (windowList[count]->level < topmostLevel))
	{
	  topmostWindow = windowList[count];
	  topmostLevel = windowList[count]->level;
	}
    }

  kernelLockRelease(&windowListLock);
  return (topmostWindow);
}


static int windowStart(void)
{
  // This does all of the startup stuff.  Gets called once during
  // system initialization.

  int status = 0;
  variableList settings;
  kernelTextOutputStream *output = NULL;
  char propertyName[128];
  char propertyValue[128];
  int count;

  char *mousePointerTypes[][2] = {
    { "default", WINDOW_DEFAULT_MOUSEPOINTER_DEFAULT },
    { "busy", WINDOW_DEFAULT_MOUSEPOINTER_BUSY }
  };

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Read the config file
  status = kernelConfigurationReader(WINDOW_MANAGER_DEFAULT_CONFIG, &settings);
  if (status < 0)
    {
      // Argh.  No file?  Create a reasonable, empty list for us to use
      status = kernelVariableListCreate(&settings);
      if (status < 0)
	return (status);
    }

  // Variables and whatnot are dealt with elsewhere.
  // Set the temporary text area to the current desktop color, for neatness'
  // sake if there are any error messages before we create the console window
  output = kernelMultitaskerGetTextOutput();
  output->textArea->background.red = kernelDefaultDesktop.red;
  output->textArea->background.green = kernelDefaultDesktop.green;
  output->textArea->background.blue = kernelDefaultDesktop.blue;

  // Load the mouse pointers
  for (count = 0; count < 2; count ++)
    {
      strcpy(propertyName, "mouse.pointer.");
      strcat(propertyName, mousePointerTypes[count][0]);

      if (kernelVariableListGet(&settings, propertyName, propertyValue, 128))
	{
	  // Nothing specified.  Use the default.
	  strcpy(propertyValue, mousePointerTypes[count][1]);
	  // Save it so it can be written out later to the configuration file
	  kernelVariableListSet(&settings, propertyName, propertyValue);
	}

      // When you load a mouse pointer it automatically switches to it,
      // so load the 'busy' one last
      status = kernelMouseLoadPointer(mousePointerTypes[count][0],
				      propertyValue);
      if (status < 0)
	kernelError(kernel_warn, "Unable to load mouse pointer %s=\"%s\"",
		    propertyName, propertyValue);
    }

  // Initialize the event streams
  if ((kernelWindowEventStreamNew(&mouseEvents) < 0) ||
      (kernelWindowEventStreamNew(&keyEvents) < 0))
    {
      kernelVariableListDestroy(&settings);
      return (status = ERR_NOTINITIALIZED);
    }

  // Spawn the window thread
  spawnWindowThread();

  // Draw the main, root window.
  rootWindow = kernelWindowMakeRoot(&settings);
  if (rootWindow == NULL)
    {
      kernelVariableListDestroy(&settings);
      return (status = ERR_NOTINITIALIZED);
    }

  // Draw the console and root windows, but don't make them visible
  makeConsoleWindow();
  layoutWindow(consoleWindow);
  autoSizeWindow(consoleWindow);
  //kernelWindowSetLocation(consoleWindow, 0, 0);
  //kernelWindowSetVisible(consoleWindow, 1);

  // Mouse housekeeping.
  kernelMouseDraw();

  // Re-write config file
  status = kernelConfigurationWriter(WINDOW_MANAGER_DEFAULT_CONFIG, &settings);
  if (status >= 0)
    kernelLog("Updated window configuration");

  // Done
  kernelVariableListDestroy(&settings);
  return (status = 0);
}


static kernelWindow *createWindow(int processId, const char *title)
{
  // Creates a new window using the supplied values.  Not visible by default.

  int status = 0;
  kernelWindow *window = NULL;
  componentParameters params;
  int bottomLevel = 0;
  int count;

  kernelMemClear(&params, sizeof(componentParameters));

  // Get some memory for window data
  window = kernelMalloc(sizeof(kernelWindow));
  if (window == NULL)
    return (window);

  window->type = windowType;

  // Set the process Id
  window->processId = processId;

  // The title
  strncpy((char *) window->title, title, WINDOW_MAX_TITLE_LENGTH);
  window->title[WINDOW_MAX_TITLE_LENGTH - 1] = '\0';

  // Set the coordinates to -1 initially
  window->xCoord = -1;
  window->yCoord = -1;

  // New windows get put at the bottom level until they are marked as
  // visible
  window->level = 0;
  if (numberWindows > 1)
    {
      for (count = 0; count < (numberWindows - 1); count ++)
	if ((windowList[count] != rootWindow) &&
	    (windowList[count]->level > bottomLevel))
	  bottomLevel = windowList[count]->level;
      window->level = (bottomLevel + 1);
    }

  // By default windows are movable and resizable
  window->flags |= (WINFLAG_MOVABLE | WINFLAG_RESIZABLE);
  window->backgroundImage.data = NULL;

  // A new window doesn't have the focus until it is marked as visible,
  // and it's not visible until someone tells us to make it visible
  window->flags &= ~(WINFLAG_HASFOCUS | WINFLAG_VISIBLE);

  // Get the window's graphic buffer all set up
  status = getWindowGraphicBuffer(window, WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT);
  if (status < 0)
    {
      // Eek!  No new window for you!
      kernelFree((void *) window);
      return (window = NULL);
    }

  // Set the window's background color to the default
  window->background.red = kernelDefaultBackground.red;
  window->background.green = kernelDefaultBackground.green;
  window->background.blue = kernelDefaultBackground.blue;

  // Add an event stream for the window
  status = kernelWindowEventStreamNew(&(window->events));
  if (status < 0)
    {
      kernelFree((void *) window);
      return (window = NULL);
    }

  // Add top-level containers for other components
  window->sysContainer = kernelWindowNewSysContainer(window, &params);
  window->mainContainer =
    kernelWindowNewContainer(window, "mainContainer", &params);

  // Add the border components
  addBorder(window);

  // Add the title bar component
  addTitleBar(window);

  // Set up the functions
  window->draw = &drawWindow;
  window->drawClip = &drawWindowClip;
  window->update = &windowUpdate;

  // Add the window to the list

  // Lock the window list
  if (kernelLockGet(&windowListLock) < 0)
    {
      kernelFree((void *) window);
      return (window = NULL);
    }

  windowList[numberWindows++] = window;

  kernelLockRelease(&windowListLock);

  // Return the window
  return (window);
}


static void moveConsoleTextArea(kernelWindow *oldWindow,
				kernelWindow *newWindow)
{
  // Moves the console text area component from the old window to the new
  // window

  kernelWindowTextArea *textArea = consoleTextArea->data;
  kernelWindowContainer *container = NULL;

  if (newWindow == oldWindow)
    return;

  // Remove it from the old window
  container = oldWindow->mainContainer->data;
  if (container->containerRemove)
    container->containerRemove(oldWindow->mainContainer, consoleTextArea);

  // Add it to the new window
  container = newWindow->mainContainer->data;
  if (container->containerAdd)
    container->containerAdd(newWindow->mainContainer, consoleTextArea);

  consoleTextArea->window = newWindow;
  consoleTextArea->buffer = &(newWindow->buffer);

  if (textArea->scrollBar)
    {
      textArea->scrollBar->window = newWindow;
      textArea->scrollBar->buffer = &(newWindow->buffer);
    }
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelWindowInitialize(void)
{
  // Called during kernel initialization

  int status = 0;

  // Don't bother if graphics are not enabled
  if (!kernelGraphicsAreEnabled())
    {
      kernelError(kernel_error, "The window environment can not run without "
		  "graphics enabled");
      return (status = ERR_NOTINITIALIZED);
    }

  kernelLog("Starting window system initialization");

  // Initialize the lock for the window list
  kernelMemClear((void *) &windowListLock, sizeof(lock));

  // Screen parameters
  screenWidth = kernelGraphicGetScreenWidth();
  screenHeight = kernelGraphicGetScreenHeight();

  // What's the default font?
  status = kernelFontGetDefault(&defaultFont);
  if (status < 0)
    // Couldn't get the default font
    return (status);

  // We're initialized
  initialized = 1;

  status = windowStart();
  if (status < 0)
    return (status);

  // Switch to the 'default' mouse pointer
  kernelMouseSwitchPointer("default");

  kernelLog("Window system initialization complete");

  return (status = 0);
}


int kernelWindowLogin(const char *userName)
{
  // This gets called after the user has logged in.

  int winShellPid = 0;
  
  // Make sure we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  // Spawn the window shell thread
  winShellPid = kernelWindowShell(kernelUserGetPrivilege(userName), 1);
  if (winShellPid < 0)
    return (winShellPid);

  // Make the root window visible
  kernelWindowSetVisible(rootWindow, 1);
  kernelMouseDraw();

  // Make its input and output streams be the console
  kernelMultitaskerSetTextInput(winShellPid, kernelTextGetConsoleInput());
  kernelMultitaskerSetTextOutput(winShellPid, kernelTextGetConsoleOutput());

  return (winShellPid);
}


int kernelWindowLogout(void)
{
  // This gets called after the user has logged out.

  // Loop through all the windows, closing everything except the console
  // window and the root window
  
  int status = 0;
  kernelWindow *window = NULL;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Lock the window list
  if (kernelLockGet(&windowListLock) < 0)
    return (status = ERR_NOLOCK);

  // Destroy all the windows
  for (count = 0; count < numberWindows; count ++)
    {
      window = windowList[count];
      
      if ((window != rootWindow) && (window != consoleWindow))
        {
          // Kill the process that owns the window
          kernelMultitaskerKillProcess(window->processId, 0);
	  
          // Destroy the window
          kernelWindowDestroy(window);

          // Need to restart the loop, since the window list will have changed
          count = -1;
        }
    }

  kernelLockRelease(&windowListLock);
 
  // Hide the root window
  kernelWindowSetVisible(rootWindow, 0);

  kernelMouseDraw();
  return (status = 0);
}


void kernelWindowRefresh(void)
{
  // Redraws all the windows using calls to renderVisiblePortions()

  int count;

  for (count = 0; count < numberWindows; count ++)
    if (windowList[count]->flags & WINFLAG_VISIBLE)
      renderVisiblePortions(windowList[count],
			    makeWindowScreenArea(windowList[count]));
}


kernelWindow *kernelWindowNew(int processId, const char *title)
{
  // Creates a new window using the supplied values.  Not visible by default.

  kernelWindow *window = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (window = NULL);

  // Check params
  if (title == NULL)
    return (window = NULL);

  window = createWindow(processId, title);
  if (window == NULL)
    return (window);

  kernelWindowShellUpdateList(windowList, numberWindows);

  // Return the window
  return (window);
}


kernelWindow *kernelWindowNewDialog(kernelWindow *parentWindow,
				    const char *title)
{
  // Creates a new 'dialog box' window, tied to the parent window, using the
  // supplied values.  Not visible by default.

  kernelWindow *newDialog = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (newDialog = NULL);

  // Check params
  if ((parentWindow == NULL) || (title == NULL))
    return (newDialog = NULL);

  // Make a new window based on the parent
  newDialog = createWindow(parentWindow->processId, title);
  if (newDialog == NULL)
    return (newDialog);

  // Attach the dialog window to the parent window and vice-versa
  parentWindow->dialogWindow = newDialog;
  newDialog->parentWindow = parentWindow;

  // Dialog windows do not have minimize buttons, by default, since they
  // do not appear in the taskbar window list
  kernelWindowRemoveMinimizeButton(newDialog);

  // Return the dialog window
  return (newDialog);
}


int kernelWindowDestroy(kernelWindow *window)
{
  // Delete the window.

  int status = 0;
  int listPosition = 0;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  // If this window *has* a dialog window, first destroy the dialog
  if (window->dialogWindow)
    {
      status = kernelWindowDestroy(window->dialogWindow);
      if (status < 0)
	return (status);
    }

  // If this window *is* a dialog window, dissociate it from its parent
  if (window->parentWindow)
    window->parentWindow->dialogWindow = NULL;

  // Lock the window list
  if (kernelLockGet(&windowListLock) < 0)
    return (status = ERR_NOLOCK);

  // First try to find the window's position in the list
  for (count = 0; count < numberWindows; count ++)
    if (windowList[count] == window)
      {
	listPosition = count;
	break;
      }

  if (windowList[listPosition] != window)
    {
      // No such window (any more)
      kernelLockRelease(&windowListLock);
      return (status = ERR_NOSUCHENTRY);
    }

  // Not visible anymore
  kernelWindowSetVisible(window, 0);

  // Remove this window from our list.  If there will be at least 1 remaining
  // window and this window was not the last one, swap the last one into the
  // spot this one occupied
  numberWindows -= 1;
  if ((numberWindows > 0) && (listPosition < numberWindows))
    {
      windowList[listPosition] = windowList[numberWindows];
      windowList[numberWindows] = window;
    }

  // Raise the levels of all windows that were below this one (except the root
  // window).  Give the topmost window the focus
  for (count = 0; count < numberWindows; count ++)
    if ((windowList[count] != rootWindow) &&
	(windowList[count]->level >= window->level))
      windowList[count]->level--;

  kernelLockRelease(&windowListLock);

  // Call the 'destroy' function for all the window's top-level components

  if (window->sysContainer)
    {
      kernelWindowComponentDestroy(window->sysContainer);
      window->sysContainer = NULL;
    }

  if (window->mainContainer)
    {
      kernelWindowComponentDestroy(window->mainContainer);
      window->mainContainer = NULL;
    }

  // If the window has a background image, free it
  if (window->backgroundImage.data)
    {
      kernelFree(window->backgroundImage.data);
      window->backgroundImage.data = NULL;
    }

  // Free the window's event stream buffer
  if (window->events.s.buffer)
    {
      kernelFree(window->events.s.buffer);  
      window->events.s.buffer = NULL;
    }

  // Free the window's graphic buffer
  if (window->buffer.data)
    {
      kernelFree(window->buffer.data);
      window->buffer.data = NULL;
    }

  // Free the window memory itself
  kernelFree((void *) window);

  // Redraw the mouse
  kernelMouseDraw();

  kernelWindowShellUpdateList(windowList, numberWindows);

  return (status);
}


int kernelWindowUpdateBuffer(kernelGraphicBuffer *buffer, int clipX, int clipY,
			     int width, int height)
{
  // A component is trying to tell us that it has updated itself in the
  // supplied buffer, and would like the bounded area of the relevent window
  // to be redrawn on screen.  This function finds the relevent window and
  // calls windowUpdate()

  int status = 0;
  kernelWindow *window = NULL;
  int count;

  // Check parameters
  if (buffer == NULL)
    return (status = ERR_NULLPARAMETER);

  // Lock the window list
  if (kernelLockGet(&windowListLock) < 0)
    return (status = ERR_NOLOCK);

  // First try to find the window

  for (count = 0; count < numberWindows; count ++)
    if (&(windowList[count]->buffer) == buffer)
      {
	window = windowList[count];
	break;
      }

  kernelLockRelease(&windowListLock);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);

  return (status = windowUpdate(window, clipX, clipY, width, height));
}


int kernelWindowSetTitle(kernelWindow *window, const char *title)
{
  // Sets the title on a window

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);

  // Set the title
  strcpy((char *) window->title, title);

  // If the window is visible, draw it
  if (window->flags & WINFLAG_VISIBLE)
    drawWindow(window);

  // Return success
  return (status = 0);
}


int kernelWindowGetSize(kernelWindow *window, int *width, int *height)
{
  // Returns the size of the supplied window.

  int status = 0;
  kernelWindowContainer *sysContainer = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((window == NULL)  || (width == NULL) || (height == NULL))
    return (status = ERR_NULLPARAMETER);

  sysContainer = window->sysContainer->data;

  // If layout has not been done, do it now
  if (!(sysContainer->doneLayout))
    {
      status = layoutWindow(window);
      if (status < 0)
	return (status);

      status = autoSizeWindow(window);
      if (status < 0)
	return (status);
    }

  *width = window->buffer.width;
  *height = window->buffer.height;

  return (status);
}


int kernelWindowSetSize(kernelWindow *window, int width, int height)
{
  // Sets the size of a window

  int status = 0;
  kernelWindowContainer *mainContainer = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  mainContainer = window->mainContainer->data;

  status = setWindowSize(window, width, height);
  if (status < 0)
    return (status);

  // See if we need to call the 'resize' operation for the main container
  if (mainContainer->doneLayout)
    {
      width = (window->buffer.width - window->mainContainer->xCoord);
      height = (window->buffer.height - window->mainContainer->yCoord);

      // Adjust for any borders
      if (window->flags & WINFLAG_HASBORDER)
	{
	  width -= WINDOW_BORDER_THICKNESS;
	  height -= WINDOW_BORDER_THICKNESS;
	}

      status = window->mainContainer
	->resize(window->mainContainer, width, height);
      window->mainContainer->width = width;
      window->mainContainer->height = height;
    }

  return (status);
}


int kernelWindowGetLocation(kernelWindow *window, int *xCoord, int *yCoord)
{
  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((window == NULL) || (xCoord == NULL) || (yCoord == NULL))
    return (status = ERR_NULLPARAMETER);

  *xCoord = window->xCoord;
  *yCoord = window->yCoord;

  // Return success
  return (status = 0);
}


int kernelWindowSetLocation(kernelWindow *window, int xCoord, int yCoord)
{
  // Sets the screen location of a window

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);

  // Erase any visible bits of the window
  if (window->flags & WINFLAG_VISIBLE)
    {
      window->flags &= ~WINFLAG_VISIBLE;
      kernelWindowRedrawArea(window->xCoord, window->yCoord,
			     window->buffer.width, window->buffer.height);
      window->flags |= WINFLAG_VISIBLE;
    }

  // Set the location
  window->xCoord = xCoord;
  window->yCoord = yCoord;

  // If the window is visible, draw it
  if (window->flags & WINFLAG_VISIBLE)
    drawWindow(window);

  // Return success
  return (status = 0);
}


int kernelWindowCenter(kernelWindow *window)
{
  // Centers a window on the screen

  int status = 0;
  int xCoord = 0;
  int yCoord = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);

  if (window->buffer.width < screenWidth)
    xCoord = ((screenWidth - window->buffer.width) / 2);

  if (window->buffer.height < screenHeight)
    yCoord = ((screenHeight - window->buffer.height) / 2);

  return (kernelWindowSetLocation(window, xCoord, yCoord));
}


int kernelWindowSnapIcons(objectKey parent)
{
  // Snap all icons to a grid in the supplied window

  int status = 0;
  kernelWindow *window = NULL;
  kernelWindowComponent *containerComponent = NULL;
  kernelWindowContainer *container = NULL;
  int iconRow = 0, count1, count2;

  // Check params
  if (parent == NULL)
    {
      kernelError(kernel_error, "Parent container is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  if (((kernelWindow *) parent)->type == windowType)
    {
      window = parent;
      containerComponent = window->mainContainer;
      container = containerComponent->data;

      // Make sure the window has been laid out
      if (!container->doneLayout)
	layoutWindow(window);
    }
  else if (((kernelWindowComponent *) parent)->type == containerComponentType)
    {
      window = getWindow(parent);
      containerComponent = parent;
      container = containerComponent->data;

      // Make sure the container has been laid out
      if (!container->doneLayout && container->containerLayout)
	container->containerLayout(containerComponent);
    }
  else
    {
      kernelError(kernel_error, "Parent is neither a window nor container");
      return (status = ERR_INVALID);
    }

  for (count1 = 0; count1 < container->numComponents; count1 ++)
    {
      if ((container->components[count1]->type == iconComponentType) &&
	  ((container->components[count1]->yCoord +
	    container->components[count1]->parameters.padTop +
	    container->components[count1]->height +
	    container->components[count1]->parameters.padBottom) >=
	   (window->buffer.height)))
	{
	  iconRow = 1;

	  for (count2 = count1 ; count2 < container->numComponents; count2 ++)
	    {
	      if (container->components[count2]->type == iconComponentType)
		{
		  container->components[count2]->parameters.gridX += 1;
		  container->components[count2]->parameters.gridY = iconRow++;
		}
	    }

	  // Set the new coordinates
	  if (container->containerLayout)
	    container->containerLayout(containerComponent);
	}
    }

  return (status = 0);
}


int kernelWindowSetHasBorder(kernelWindow *window, int trueFalse)
{
  // Sets the 'has border' attribute

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);

  // Whether true or false, we need to remove any existing border components,
  // since even if true we don't want any existing ones hanging
  // around.
  if (window->flags & WINFLAG_HASBORDER)
    removeBorder(window);

  if (trueFalse)
    {
      window->flags |= WINFLAG_HASBORDER;
      addBorder(window);
    }
  else
    window->flags &= ~WINFLAG_HASBORDER;

  // Return success
  return (status = 0);
}


int kernelWindowSetHasTitleBar(kernelWindow *window, int trueFalse)
{
  // Sets the 'has title bar' attribute, and destroys the title bar component
  // if false (since new windows have them by default)

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);
    
  if (trueFalse)
    addTitleBar(window);
  else
    removeTitleBar(window);
  
  return (status = 0);
}


int kernelWindowSetMovable(kernelWindow *window, int trueFalse)
{
  // Sets the 'is movable' attribute

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);

  if (trueFalse)
    window->flags |= WINFLAG_MOVABLE;
  else
    window->flags &= ~WINFLAG_MOVABLE;

  // Return success
  return (status = 0);
}


int kernelWindowSetResizable(kernelWindow *window, int trueFalse)
{
  // Sets the 'is resizable' attribute

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);

  if (trueFalse)
    window->flags |= WINFLAG_RESIZABLE;
  else
    window->flags &= ~WINFLAG_RESIZABLE;

  // Return success
  return (status = 0);
}


int kernelWindowRemoveMinimizeButton(kernelWindow *window)
{
  // Removes any minimize button component.

  int status = 0;
  kernelWindowTitleBar *titleBar = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  if (window->titleBar)
    {
      titleBar = window->titleBar->data;

      if (titleBar->minimizeButton)
	{
	  kernelWindowComponentDestroy(titleBar->minimizeButton);
	  titleBar->minimizeButton = NULL;
	}
    }

  // Return success
  return (status = 0);
}


int kernelWindowRemoveCloseButton(kernelWindow *window)
{
  // Removes any close button component.

  int status = 0;
  kernelWindowTitleBar *titleBar = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  if (window->titleBar)
    {
      titleBar = window->titleBar->data;

      if (titleBar->closeButton)
	{
	  kernelWindowComponentDestroy(titleBar->closeButton);
	  titleBar->closeButton = NULL;
	}
    }

  // Return success
  return (status = 0);
}


int kernelWindowSetColors(kernelWindow *window, color *background)
{
  // Set the colors for the window

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  if (background == NULL)
    background = &kernelDefaultBackground;

  kernelMemCopy(background, (void *) &(window->background), sizeof(color));

  if ((window->flags & WINFLAG_VISIBLE) && window->draw)
    window->draw(window);

  return (status = 0);
}


int kernelWindowSetVisible(kernelWindow *window, int visible)
{
  // Sets a window to visible or not

  int status = 0;
  kernelWindowContainer *sysContainer = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  sysContainer = window->sysContainer->data;

  if (visible)
    {
      // If the window is becoming visible and hasn't yet had its layout done,
      // do that now
      if (!(sysContainer->doneLayout))
	{
	  status = layoutWindow(window);
	  if (status < 0)
	    return (status);

	  status = autoSizeWindow(window);
	  if (status < 0)
	    return (status);
	}

      // If no coordinates have been specified, center the window
      if ((window->xCoord == -1) && (window->yCoord == -1))
	{
	  status = kernelWindowCenter(window);
	  if (status < 0)
	    return (status);
	}
    }

  // Set the visible value
  if (visible)
    window->flags |= WINFLAG_VISIBLE;
  else
    window->flags &= ~WINFLAG_VISIBLE;

  if (visible)
    {
      // Draw the window
      status = drawWindow(window);
      if (status < 0)
	return (status);

      // Automatically give any newly-visible windows the focus.
      changeWindowFocus(window);
    }
  else
    {
      // Take away the focus, if applicable
      if (window == focusWindow)
      	changeWindowFocus(findTopmostWindow());

      // Erase any visible bits of the window
      kernelWindowRedrawArea(window->xCoord, window->yCoord,
			     window->buffer.width, window->buffer.height);
    }

  // Return success
  return (status = 0);
}


void kernelWindowSetMinimized(kernelWindow *window, int minimize)
{
  // Minimize or restore a window (with visuals!)

  int count1, count2;

  kernelWindowSetVisible(window, !minimize);

  if (minimize)
    {
      // Show the minimize graphically.  Draw xor'ed outlines
      for (count1 = 0; count1 < 2; count1 ++)
	{
	  for (count2 = 0; count2 < WINDOW_MINREST_TRACERS; count2 ++)
	    {
	      kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
		    draw_xor,
		    (window->xCoord -
		     (count2 * (window->xCoord / WINDOW_MINREST_TRACERS))),
		    (window->yCoord -
		     (count2 * (window->yCoord / WINDOW_MINREST_TRACERS))),
		    (window->buffer.width -
		     (count2 * (window->buffer.width /
				WINDOW_MINREST_TRACERS))),
		    (window->buffer.height -
		     (count2 * (window->buffer.height /
				WINDOW_MINREST_TRACERS))), 1, 0);
	    }

	  if (!count1)
	    // Delay a bit
	    kernelMultitaskerYield();
	}
    }

  // Redraw the mouse
  kernelMouseDraw();

  return;
}


int kernelWindowAddConsoleTextArea(kernelWindow *window)
{
  // Moves the console text area component from our 'hidden' console window
  // to the supplied window

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  // Make sure the console text area is not already assigned to some other
  // window (other than the consoleWindow)
  if (consoleTextArea->window != consoleWindow)
    return (status = ERR_ALREADY);

  moveConsoleTextArea(consoleWindow, window);

  return (status = 0);
}


void kernelWindowRedrawArea(int xCoord, int yCoord, int width, int height)
{
  // This function will redraw an arbitrary area of the screen.  Initially
  // written to allow the mouse functions to erase the mouse without
  // having to know what's under it.  Could be useful for other things as
  // well.

  kernelWindow *window = NULL;
  screenArea area;
  screenArea intersectingArea;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return;

  area.leftX = xCoord;
  area.topY = yCoord;
  area.rightX = (xCoord + (width - 1));
  area.bottomY = (yCoord + (height - 1));

  // If the root window has not yet been shown, clear the area first
  if ((rootWindow == NULL) || !(rootWindow->flags & WINFLAG_VISIBLE))
    kernelGraphicClearArea(NULL, &kernelDefaultDesktop, xCoord, yCoord,
			   width, height);

  // Loop through the window list, looking for any visible ones that
  // intersect this area
  for (count = 0; count < numberWindows; count ++)
    {
      window = windowList[count];
      
      if ((window->flags & WINFLAG_VISIBLE) &&
	  doAreasIntersect(&area, makeWindowScreenArea(window)))
	{
	  getIntersectingArea(makeWindowScreenArea(window), &area,
			      &intersectingArea);
	  intersectingArea.leftX -= window->xCoord;
	  intersectingArea.topY -= window->yCoord;
	  intersectingArea.rightX -= window->xCoord;
	  intersectingArea.bottomY -= window->yCoord;
	  renderVisiblePortions(window, &intersectingArea);
	}
    }
  
  // If there's an active menu that intersects this area, draw the relevent
  // part of that too, since they have buffers that are separate from their
  // windows' buffers
  if (activeMenu && (activeMenu->flags & WINFLAG_VISIBLE) &&
      doAreasIntersect(&area, makeComponentScreenArea(activeMenu)))
    {
      window = activeMenu->window;

      getIntersectingArea(makeComponentScreenArea(activeMenu), &area,
			  &intersectingArea);
      intersectingArea.leftX -= (window->xCoord + activeMenu->xCoord);
      intersectingArea.topY -= (window->yCoord + activeMenu->yCoord);
      intersectingArea.rightX -= (window->xCoord + activeMenu->xCoord);
      intersectingArea.bottomY -= (window->yCoord + activeMenu->yCoord);

      kernelGraphicRenderBuffer(activeMenu->buffer,
				(window->xCoord + activeMenu->xCoord),
				(window->yCoord + activeMenu->yCoord),
				intersectingArea.leftX, intersectingArea.topY,
				(intersectingArea.rightX -
				 intersectingArea.leftX + 1),
				(intersectingArea.bottomY -
				 intersectingArea.topY + 1));
    }

  return;
}


void kernelWindowDrawAll(void)
{
  // This function will redraw all windows.  Useful for example when the user
  // has changed the color scheme

  kernelWindow *window = NULL;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return;

  // Lock the window list
  if (kernelLockGet(&windowListLock) < 0)
    return;

  // Loop through the window list, looking for any visible ones that
  // intersect this area
  for (count = 0; count < numberWindows; count ++)
    {
      window = windowList[count];

      if (window->flags & WINFLAG_VISIBLE)
	drawWindow(window);
    }

  kernelLockRelease(&windowListLock);

  kernelMouseDraw();

  return;
}


void kernelWindowResetColors(void)
{
  // This function will reset the colors of all the windows and their
  // components.  Useful for example when the user has changed the color
  // scheme

  kernelWindow *window = NULL;
  kernelWindowComponent **array = NULL;
  int numComponents = 0;
  int count1, count2;

  // Make sure we've been initialized
  if (!initialized)
    return;

  // Lock the window list
  if (kernelLockGet(&windowListLock) < 0)
    return;

  // Loop through the window list, looking for any visible ones that
  // intersect this area
  for (count1 = 0; count1 < numberWindows; count1 ++)
    {
      window = windowList[count1];

      kernelMemCopy(&kernelDefaultBackground, (color *) &(window->background),
		    sizeof(color));

      // Loop through all the regular window components and set the colors
      array = kernelMalloc((countComponents(window->sysContainer->data) +
			    countComponents(window->mainContainer->data)) *
			   sizeof(kernelWindowComponent *));
      if (array == NULL)
	break;

      numComponents = 0;
      flattenContainer(window->sysContainer->data, array, &numComponents, 0);
      flattenContainer(window->mainContainer->data, array, &numComponents, 0);

      for (count2 = 0; count2 < numComponents; count2 ++)
	{
	  if (!(array[count2]->parameters.flags &
		WINDOW_COMPFLAG_CUSTOMFOREGROUND))
	    kernelMemCopy(&kernelDefaultForeground,
			  (color *) &(array[count2]->parameters.foreground),
			  sizeof(color));

	  if (!(array[count2]->parameters.flags &
		WINDOW_COMPFLAG_CUSTOMBACKGROUND))
	    kernelMemCopy(&kernelDefaultBackground,
			  (color *) &(array[count2]->parameters.background),
			  sizeof(color));
	}

      kernelFree(array);
    }

  kernelLockRelease(&windowListLock);
  
  kernelWindowDrawAll();

  return;
}


void kernelWindowProcessEvent(windowEvent *event)
{
  // A user has clicked or unclicked somewhere
  
  // Make sure we've been initialized
  if (!initialized)
    return;

  // Check to make sure the window thread is still running
  if (!kernelMultitaskerProcessIsAlive(winThreadPid))
    spawnWindowThread();

  if (event->type & EVENT_MASK_MOUSE)
    {
      // If it was just a move, skip it for now
      if (event->type == EVENT_MOUSE_MOVE)
	return;

      kernelWindowEventStreamWrite(&mouseEvents, event);
    }
  else if (event->type & EVENT_MASK_KEY)
    kernelWindowEventStreamWrite(&keyEvents, event);

  return;
}


int kernelWindowRegisterEventHandler(kernelWindowComponent *component,
				     void (*function)(kernelWindowComponent *,
						      windowEvent *))
{
  // This function is called to register a windowEvent callback handler for
  // a component.

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((component == NULL) || (function == NULL))
    return (status = ERR_NULLPARAMETER);

  component->eventHandler = function;

  return (status = 0);
}


int kernelWindowComponentEventGet(objectKey key, windowEvent *event)
{
  // This function is called to read an event from the component's
  // windowEventStream

  int status = 0;
  kernelWindow *window = NULL;
  kernelWindowComponent *component = NULL;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((key == NULL) || (event == NULL))
    return (status = ERR_NULLPARAMETER);

  // First, determine whether the request is for a window or a component
  for (count = 0; count < numberWindows; count ++)
    if (windowList[count] == key)
      {
	window = windowList[count];
	break;
      }

  if (window)
    {
      // The request is for a window windowEvent
      status = kernelWindowEventStreamRead(&(window->events), event);
    }
  else
    {
      // The request must be for a component windowEvent
      component = key;
      
      status = kernelWindowEventStreamRead(&(component->events), event);
    }

  return (status);
}


int kernelWindowSetBackgroundImage(kernelWindow *window, image *imageCopy)
{
  // Set the window's background image

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((window == NULL) || (imageCopy == NULL))
    return (status = ERR_NULLPARAMETER);
  
  // If there was a previous background image, deallocate its memory
  if (window->backgroundImage.data)
    {
      kernelFree(window->backgroundImage.data);
      window->backgroundImage.data = NULL;
    }

  // Copy the image information into the window's background image
  kernelMemCopy(imageCopy, (void *) &(window->backgroundImage), sizeof(image));

  // Get some new memory for the image data
  window->backgroundImage.data =
    kernelMalloc(window->backgroundImage.dataLength);
  if (window->backgroundImage.data == NULL)
    return (status == ERR_MEMORY);
  
  // Copy the image data into new memory
  kernelMemCopy(imageCopy->data, window->backgroundImage.data,
		window->backgroundImage.dataLength);

  return (status = 0);
}


int kernelWindowTileBackground(const char *filename)
{
  // This will tile the supplied image as the background image of the
  // root window
  
  int status = 0;
  image backgroundImage;
  variableList settings;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((filename == NULL) || (rootWindow == NULL))
    return (status = ERR_NULLPARAMETER);
  
  // Try to load the new background image
  status = kernelImageLoad(filename, 0, 0, &backgroundImage);
  if (status < 0)
    {
      kernelError(kernel_error, "Error loading background image");
      return (status);
    }

  // Put the background image into our window.
  kernelWindowSetBackgroundImage(rootWindow, &backgroundImage);

  // Release the image memory
  kernelMemoryRelease(backgroundImage.data);

  // Redraw the root window
  drawWindow(rootWindow);

  kernelMouseDraw();

  // Save the settings variable
  status = kernelConfigurationReader(WINDOW_MANAGER_DEFAULT_CONFIG, &settings);
  if (status >= 0)
    {
      kernelVariableListSet(&settings, "background.image", filename);
      kernelConfigurationWriter(WINDOW_MANAGER_DEFAULT_CONFIG, &settings);
      kernelVariableListDestroy(&settings);
    }

  return (status = 0);
}


int kernelWindowCenterBackground(const char *filename)
{
  // This will center the supplied image as the background
  
  // For the moment, this is not really implemented.  The 'tile' routine
  // will automatically center the image if it's wider or higher than half
  // the screen size anyway.
  return (kernelWindowTileBackground(filename));
}


int kernelWindowScreenShot(image *saveImage)
{
  // This will grab the entire screen as a screen shot, and put the
  // data into the supplied image object
  
  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  status = kernelGraphicGetImage(NULL, saveImage, 0, 0, screenWidth,
				 screenHeight);

  return (status);
}


int kernelWindowSaveScreenShot(const char *name)
{
  // Save a screenshot in the current directory

  int status = 0;
  char *filename = NULL;
  char *labelText = NULL;
  image saveImage;
  kernelWindow *dialog = NULL;
  componentParameters params;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  filename = kernelMalloc(MAX_PATH_NAME_LENGTH);
  if (filename == NULL)
    return (status = ERR_MEMORY);
  labelText = kernelMalloc(MAX_PATH_NAME_LENGTH + 40);
  if (labelText == NULL)
    return (status = ERR_MEMORY);

  if (name == NULL)
    {
      kernelMultitaskerGetCurrentDirectory(filename, MAX_PATH_NAME_LENGTH);
      if (filename[strlen(filename) - 1] != '/')
	strcat(filename, "/");
      strcat(filename, "screenshot1.bmp");
    }
  else
    {
      strncpy(filename, name, MAX_PATH_NAME_LENGTH);
      filename[MAX_PATH_NAME_LENGTH - 1] = '\0';
    }

  kernelMemClear(&saveImage, sizeof(image));
  status = kernelWindowScreenShot(&saveImage);
  if (status == 0)
    {
      dialog = kernelWindowNew(NULL, "Screen shot");
      if (dialog)
	{
	  sprintf(labelText, "Saving screen shot as \"%s\"...", filename);
	  
	  kernelMemClear(&params, sizeof(componentParameters));
	  params.gridWidth = 1;
	  params.gridHeight = 1;
	  params.padLeft = 5;
	  params.padRight = 5;
	  params.padTop = 5;
	  params.padBottom = 5;
	  params.orientationX = orient_center;
	  params.orientationY = orient_middle;
	  
	  kernelWindowNewTextLabel(dialog, labelText, &params);
	  kernelWindowSetVisible(dialog, 1);
	}

      status = kernelImageSave(filename, IMAGEFORMAT_BMP, &saveImage);
      if (status < 0)
	kernelError(kernel_error, "Error %d saving image\n", status);

      kernelMemoryRelease(saveImage.data);

      if (dialog)
	kernelWindowDestroy(dialog);
    }
  else
    kernelError(kernel_error, "Error %d getting screen shot\n", status);

  kernelFree(filename);
  kernelFree(labelText);

  return (status);
}


int kernelWindowSetTextOutput(kernelWindowComponent *component)
{
  // This will set the text output stream for the given process to be
  // the supplied window component

  int status = 0;
  int processId = 0;
  kernelWindowTextArea *textArea = NULL;
  kernelTextInputStream *inputStream = NULL;
  kernelTextOutputStream *outputStream = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (component == NULL)
    return (status = ERR_NULLPARAMETER);

  processId = kernelMultitaskerGetCurrentProcessId();
    
  // Do different things depending on the type of the component
  if (component->type == textAreaComponentType)
    {
      // Switch the text area of the output stream to the supplied text
      // area component.

      textArea = component->data;

      inputStream = textArea->area->inputStream;
      outputStream = textArea->area->outputStream;

      kernelMultitaskerSetTextInput(processId, inputStream);
      kernelMultitaskerSetTextOutput(processId, outputStream);

      return (status = 0);
    }
  else
    {
      // Invalid component type
      kernelError(kernel_error, "Unable to switch text output; invalid "
		  "window component type");
      return (status = ERR_INVALID);
    }
}


int kernelWindowLayout(kernelWindow *window)
{
  // Layout, or re-layout, the requested window.  This function can be used
  // when components are added to or removed from and already laid-out window.

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  return (status = layoutWindow(window));
}


void kernelWindowDebugLayout(kernelWindow *window)
{
  // Sets the 'debug layout' flag on the window so that layout grids get
  // drawn around the components

  // Make sure we've been initialized
  if (!initialized)
    return;

  // Check parameters
  if (window == NULL)
    return;

  window->flags |= WINFLAG_DEBUGLAYOUT;

  if (window->flags & WINFLAG_VISIBLE)
    drawWindow(window);

  return;
}


int kernelWindowContextAdd(objectKey parent, windowMenuContents *contents)
{
  // This function allows the caller to add context menu items to the supplied
  // parent object (can be a window or a component).  The function supplies
  // the pointers to the new menu items in the caller's structure, which can
  // then be manipulated to some extent (enable/disable, destroy, etc) using
  // regular component functions.

  int status = 0;
  kernelWindow *parentWindow = NULL;
  kernelWindowComponent *parentComponent = NULL;
  kernelWindowComponent *containerComponent = NULL;
  kernelWindowComponent *menuComponent = NULL;
  componentParameters params;
  int count;

  // Check params
  if ((parent == NULL) || (contents == NULL))
    {
      kernelError(kernel_error, "NULL parent or contents pointer");
      return (status = ERR_NULLPARAMETER);
    }

  // If the parent is a window, then we will add the content to a context
  // menu in the window's system container.
  if (((kernelWindow *) parent)->type == windowType)
    {
      parentWindow = parent;
      menuComponent = parentWindow->contextMenu;
    }
  else
    {
      parentComponent = parent;
      menuComponent = parentComponent->contextMenu;
    }

  // If there's no existing context menu, create one with the contents
  if (menuComponent == NULL)
    {
      kernelMemClear(&params, sizeof(componentParameters));
  
      if (parentWindow)
	containerComponent = parentWindow->mainContainer;

      else
	{
	  containerComponent = parentComponent->container;

	  // Get any custom colors from the parent
	  if (parentComponent->parameters.flags &
	      WINDOW_COMPFLAG_CUSTOMFOREGROUND)
	    {
	      params.flags |= WINDOW_COMPFLAG_CUSTOMFOREGROUND;
	      kernelMemCopy((void *) &(parentComponent->parameters.foreground),
			    &(params.foreground), sizeof(color));
	    }

	  if (parentComponent->parameters.flags &
	      WINDOW_COMPFLAG_CUSTOMBACKGROUND)
	    {
	      params.flags |= WINDOW_COMPFLAG_CUSTOMBACKGROUND;
	      kernelMemCopy((void *) &(parentComponent->parameters.background),
			    &(params.background), sizeof(color));
	    }
	}

      menuComponent = kernelWindowNewMenu(containerComponent, "contextMenu",
					  contents, &params);
      if (menuComponent == NULL)
	{
	  kernelError(kernel_error, "Couldn't create context menu");
	  return (status = ERR_NOCREATE);
	}

      if (parentWindow)
	parentWindow->contextMenu = menuComponent;
      else
	parentComponent->contextMenu = menuComponent;
    }
  else
    {
      // Loop through the contents and add the individual items to the
      // existing context menu

      kernelMemCopy((void *) &(menuComponent->parameters), &params,
		    sizeof(componentParameters));

      for (count = 0; count < contents->numItems; count ++)
	{
	  contents->items[count].key = (objectKey)
	    kernelWindowNewMenuItem(menuComponent, contents->items[count].text,
				    &params);
	  if (contents->items[count].key == NULL)
	    {
	      kernelError(kernel_error, "Couldn't add \"%s\" to context menu",
			  contents->items[count].text);
	      return (status = ERR_NOCREATE);
	    }
	}
    }

  return (status = 0);
}


int kernelWindowContextSet(objectKey parent,
			   kernelWindowComponent *menuComponent)
{
  // This function allows the caller to set the context menu directly using
  // the supplied parent object (can be a window or a component) and menu
  // component.

  int status = 0;
  kernelWindow *parentWindow = NULL;
  kernelWindowComponent *parentComponent = NULL;

  // Check params
  if ((parent == NULL) || (menuComponent == NULL))
    {
      kernelError(kernel_error, "NULL parent or contents pointer");
      return (status = ERR_NULLPARAMETER);
    }

  // If the parent is a window, then we will set the context menu of the
  // window's system container.
  if (((kernelWindow *) parent)->type == windowType)
    {
      parentWindow = parent;
      if (parentWindow->contextMenu)
	kernelWindowComponentDestroy(parentWindow->contextMenu);
      parentWindow->contextMenu = menuComponent;
    }
  else
    {
      parentComponent = parent;
      if (parentComponent->contextMenu)
	kernelWindowComponentDestroy(parentComponent->contextMenu);
      parentComponent->contextMenu = menuComponent;
    }

  return (status = 0);
}


kernelWindowComponent *kernelWindowComponentNew(objectKey parent,
						componentParameters *params)
{
  // Creates a new component and adds it to the main container of the
  // window.

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowContainer *tmpContainer = NULL;

  // Check params
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  // Get memory for the basic component
  component = kernelMalloc(sizeof(kernelWindowComponent));
  if (component == NULL)
    return (component);

  component->type = genericComponentType;
  component->subType = genericComponentType;
  component->window = getWindow(parent);
  
  // Use the window's buffer by default
  if (component->window)
    component->buffer = &(component->window->buffer);

  // Visible and enabled by default
  component->flags |= (WINFLAG_VISIBLE | WINFLAG_ENABLED);

  // If the parameter flags indicate the component should be focusable,
  // set the appropriate component flag
  if (params->flags & WINDOW_COMPFLAG_CANFOCUS)
    component->flags |= WINFLAG_CANFOCUS;

  // Copy the parameters into the component
  kernelMemCopy(params, (void *) &(component->parameters),
		sizeof(componentParameters));

  // If the default colors are requested, copy them into the component
  // parameters
  if (!(component->parameters.flags & WINDOW_COMPFLAG_CUSTOMFOREGROUND))
    {
      component->parameters.foreground.blue = kernelDefaultForeground.blue;
      component->parameters.foreground.green = kernelDefaultForeground.green;
      component->parameters.foreground.red = kernelDefaultForeground.red;
    }

  if (!(component->parameters.flags & WINDOW_COMPFLAG_CUSTOMBACKGROUND))
    {
      component->parameters.background.blue = kernelDefaultBackground.blue;
      component->parameters.background.green = kernelDefaultBackground.green;
      component->parameters.background.red = kernelDefaultBackground.red;
    }

  // Initialize the event stream
  status = kernelWindowEventStreamNew(&(component->events));
  if (status < 0)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  // Default functions
  component->drawBorder = &componentDrawBorder;
  component->erase = &componentErase;
  component->grey = &componentGrey;

  // Now we need to add the component somewhere.  If 'parent' is a window,
  // we will attempt to add it there (a couple of different possibilities).
  // Otherwise, we assume it is a container of some sort and use the container
  // add routine.
  if (((kernelWindow *) parent)->type == windowType)
    {
      kernelWindow *tmpWindow = parent;
      if (tmpWindow->mainContainer)
	{
	  tmpContainer = tmpWindow->mainContainer->data;
	  status =
	    tmpContainer->containerAdd(tmpWindow->mainContainer, component);
	}
    }
  else if (((kernelWindowComponent *) parent)->type == containerComponentType)
    {
      // Not a window
      kernelWindowComponent *tmpComponent = parent;
      tmpContainer = tmpComponent->data;
      status = tmpContainer->containerAdd(tmpComponent, component);
    }
  else
    {
      kernelError(kernel_error, "Invalid parent object for new component");
      kernelFree((void *) component);
      return (component = NULL);
    }

  if (status < 0)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }
  else
    return (component);
}


void kernelWindowComponentDestroy(kernelWindowComponent *component)
{
  kernelWindowComponent *containerComponent = NULL;
  kernelWindowContainer *container = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return;

  // Check params.  
  if (component == NULL)
    return;

  // If the component is a container, recurse for each thing that's in it.
  if (component->type == containerComponentType)
    {
      container = component->data;

      while (container->numComponents)
	kernelWindowComponentDestroy(container->components[0]);
    }

  // Make sure the component is removed from any containers it's in
  if (component->container != NULL)
    {
      containerComponent = component->container;

      if (containerComponent->type == containerComponentType)
	{
	  container = containerComponent->data;
	  if (container == NULL)
	    {
	      kernelError(kernel_error, "Container data is null");
	      return;
	    }

	  container->containerRemove(containerComponent, component);
	}
    }

  // Never destroy the console text area.  If this is the console text area,
  // move it back to our console window
  if (component == consoleTextArea)
    {
      moveConsoleTextArea(component->window, consoleWindow);
      return;
    }

  // Call the component's own destroy function
  if (component->destroy)
    component->destroy(component);
  component->data = NULL;

  // Deallocate generic things
  if (component->events.s.buffer)
    {
      kernelFree((void *)(component->events.s.buffer));
      component->events.s.buffer = NULL;
    }

  // Free the component itself
  kernelFree((void *) component);

  return;
}


int kernelWindowComponentSetVisible(kernelWindowComponent *component,
				    int visible)
{
  // Set a component visible or not visible

  int status = 0;
  kernelWindowContainer *container = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (component == NULL)
    return (status = ERR_NULLPARAMETER);

  container = component->container->data;

  if (visible)
    {
      component->flags |= WINFLAG_VISIBLE;
      if (component->draw)
	component->draw(component);
    }
  else
    {
      if (component->window->focusComponent == component)
	// Make sure it doesn't have the focus
	focusNextComponent(component->window);

      component->flags &= ~WINFLAG_VISIBLE;
      if (component->erase)
	component->erase(component);
    }

  // Redraw a clip of that part of the window
  if (container->doneLayout)
    drawWindowClip(component->window, component->xCoord, component->yCoord,
		   component->width, component->height);

  // Redraw the mouse just in case it was within this area
  kernelMouseDraw();

  return (status = 0);
}


int kernelWindowComponentSetEnabled(kernelWindowComponent *component,
				    int enabled)
{
  // Set a component enabled or not enabled.  What we do is swap the 'draw'
  // and 'grey' functions of the component.

  int status = 0;
  kernelWindowContainer *container = NULL;
  int (*tmpDraw) (kernelWindowComponent *) = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (component == NULL)
    return (status = ERR_NULLPARAMETER);

  container = component->container->data;

  if (enabled && !(component->flags & WINFLAG_ENABLED))
    {
      component->flags |= WINFLAG_ENABLED;

      tmpDraw = component->grey;
      component->grey = component->draw;
      component->draw = tmpDraw;
    }
  else if (!enabled && (component->flags & WINFLAG_ENABLED))
    {
      if (component->window->focusComponent == component)
	// Make sure it doesn't have the focus
	focusNextComponent(component->window);

      component->flags &= ~WINFLAG_ENABLED;

      tmpDraw = component->grey;
      component->grey = component->draw;
      component->draw = tmpDraw;
    }

  // Redraw a clip of that part of the window
  if (container->doneLayout)
    drawWindowClip(component->window, component->xCoord, component->yCoord,
		   component->width, component->height);

  // Redraw the mouse just in case it was within this area
  kernelMouseDraw();

  return (status = 0);
}


int kernelWindowComponentGetWidth(kernelWindowComponent *component)
{
  // Return the width parameter of the component
  if (!initialized || (component == NULL))
    return (0);
  else
    return (component->width);
}


int kernelWindowComponentSetWidth(kernelWindowComponent *component, int width)
{
  // Set the width parameter of the component

  int status = 0;

  if (!initialized)
    return (ERR_NOTINITIALIZED);
  if (component == NULL)
    return (ERR_NULLPARAMETER);

  // If the component wants to know about resize events...
  if (component->resize)
    status = component->resize(component, component->width, component->height);

  component->width = width;

  return (status);
}


int kernelWindowComponentGetHeight(kernelWindowComponent *component)
{
  // Return the height parameter of the component
  if (!initialized || (component == NULL))
    return (0);
  else
    return (component->height);
}


int kernelWindowComponentSetHeight(kernelWindowComponent *component,
				   int height)
{
  // Set the width parameter of the component

  int status = 0;

  if (!initialized)
    return (ERR_NOTINITIALIZED);
  if (component == NULL)
    return (ERR_NULLPARAMETER);
  
  // Sizes are generally padded by 2 pixels on either side
  height += 2;

  // If the component wants to know about resize events...
  if (component->resize)
    status = component->resize(component, component->width, component->height);

  component->height = height;
  
  return (status);
}


int kernelWindowComponentFocus(kernelWindowComponent *component)
{
  // Gives the supplied component the focus, puts it on top of any other
  // components it intersects, etc.
  
  int status = 0;
  kernelWindow *window = NULL;
  kernelWindowComponent **array = NULL;
  int numComponents = 0;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (component == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the window
  window = component->window;
  if (window == NULL)
    {
      kernelError(kernel_error, "Component to focus has no window");
      return (status = ERR_NODATA);
    }

  array = kernelMalloc(countComponents(window->mainContainer->data) *
		       sizeof(kernelWindowComponent *));
  if (array == NULL)
    return (NULL);

  flattenContainer(window->mainContainer->data, array, &numComponents, 0);

  // Find all the window's components at this location 
  for (count = 0; count < numComponents; count ++)
    if (array[count]->level <= component->level)
      {
	if (doAreasIntersect(makeComponentScreenArea(component),
			     makeComponentScreenArea(array[count])))
	  array[count]->level++;
      }
  
  kernelFree(array);

  component->level = 0;

  changeComponentFocus(window, component);

  return (status = 0);
}


int kernelWindowComponentUnfocus(kernelWindowComponent *component)
{
  // Removes the focus from the supplied component
  
  int status = 0;
  kernelWindow *window = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (component == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the window
  window = component->window;
  if (window == NULL)
    {
      kernelError(kernel_error, "Component to unfocus has no window");
      return (status = ERR_NODATA);
    }

  if (window->oldFocusComponent)
    changeComponentFocus(window, window->oldFocusComponent);
  else
    focusNextComponent(window);

  return (status = 0);
}


int kernelWindowComponentDraw(kernelWindowComponent *component)
{
  // Draw  a component
  
  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (component == NULL)
    return (status = ERR_NULLPARAMETER);

  if (!component->draw)
    return (status = ERR_NOTIMPLEMENTED);

  return (component->draw(component));
}


int kernelWindowComponentGetData(kernelWindowComponent *component,
				 void *buffer, int size)
{
  // Get (generic) data from a component
  
  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((component == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  if (!component->getData)
    return (status = ERR_NOTIMPLEMENTED);

  return (component->getData(component, buffer, size));
}


int kernelWindowComponentSetData(kernelWindowComponent *component,
				 void *buffer, int size)
{
  // Set (generic) data in a component
  
  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params.  buffer can only be NULL if size is NULL
  if ((component == NULL) || ((buffer == NULL) && size))
    return (status = ERR_NULLPARAMETER);

  if (!component->setData)
    return (status = ERR_NOTIMPLEMENTED);

  status = component->setData(component, buffer, size);

  return (status);
}


int kernelWindowComponentGetSelected(kernelWindowComponent *component,
				     int *selection)
{
  // Calls the 'get selected' method of the component, if applicable

  // Make sure we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  // Check parameters
  if ((component == NULL) || (selection == NULL))
    return (ERR_NULLPARAMETER);

  if (component->getSelected == NULL)
    return (ERR_NOSUCHFUNCTION);

  return (component->getSelected(component, selection));
}


int kernelWindowComponentSetSelected(kernelWindowComponent *component,
				     int selected)
{
  // Calls the 'set selected' method of the component, if applicable

  // Make sure we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  // Check parameters
  if (component == NULL)
    return (ERR_NULLPARAMETER);

  if (component->setSelected == NULL)
    return (ERR_NOSUCHFUNCTION);

  return (component->setSelected(component, selected));
}
