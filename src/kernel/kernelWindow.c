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
//  kernelWindow.c
//

// This is the code that does all of the generic stuff for setting up
// GUI windows.

#include "kernelWindow.h"
#include "kernelWindowEventStream.h"
#include "kernelParameters.h"
#include "kernelMemoryManager.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelLock.h"
#include "kernelLoader.h"
#include "kernelFilesystem.h"
#include "kernelFileStream.h"
#include "kernelUser.h"
#include "kernelMiscFunctions.h"
#include "kernelLog.h"
#include "kernelError.h"
#include <string.h>
#include <stdio.h>
#include <values.h>

static int initialized = 0;
static int screenWidth = 0;
static int screenHeight = 0;

static kernelAsciiFont *defaultFont = NULL;
static kernelWindow *rootWindow = NULL;
static kernelWindowComponent *consoleTextArea = NULL;
static kernelWindow *consoleWindow = NULL;
static int titleBarHeight = DEFAULT_TITLEBAR_HEIGHT;
static int borderThickness = DEFAULT_BORDER_THICKNESS;

// Keeps the data for all the windows
static kernelWindow *windowList[WINDOW_MAXWINDOWS];
static kernelWindow *windowData = NULL;
static volatile int numberWindows = 0;
static lock windowListLock;

static kernelWindow *focusWindow = NULL;
static int winThreadPid = 0;

static kernelWindowComponent *draggingComponent = NULL;
static windowEventStream mouseEvents;
static windowEventStream keyEvents;

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

  kernelWindowComponent *border = NULL;
  componentParameters params;

  if (window->flags & WINFLAG_HASBORDER)
    return;

  kernelMemClear((void *) &params, sizeof(componentParameters));

  border = kernelWindowNewBorder(window->sysContainer, border_top, &params);
  window->borders[border_top] = border;

  border = kernelWindowNewBorder(window->sysContainer, border_left, &params);
  window->borders[border_left] = border;

  border = kernelWindowNewBorder(window->sysContainer, border_bottom, &params);
  window->borders[border_bottom] = border;

  border = kernelWindowNewBorder(window->sysContainer, border_right, &params);
  window->borders[border_right] = border;

  window->flags |= WINFLAG_HASBORDER;

  return;
}


static void removeBorder(kernelWindow *window)
{
  // Removes the borders from the window

  int count;

  if (!(window->flags & WINFLAG_HASBORDER))
    return;

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

  int width = window->buffer.width;
  componentParameters params;

  if (window->flags & WINFLAG_HASTITLEBAR)
    return;

  // Standard parameters for a title bar
  kernelMemClear((void *) &params, sizeof(componentParameters));
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  window->titleBar = kernelWindowNewTitleBar(window->sysContainer, width,
					     &params);
  if (window->titleBar)
    {
      window->titleBar->xCoord = 0;
      window->titleBar->yCoord = 0;

      if (window->flags & WINFLAG_HASBORDER)
	{
	  window->titleBar->xCoord += borderThickness;
	  window->titleBar->yCoord += borderThickness;
	}

      window->flags |= WINFLAG_HASTITLEBAR;
    }

  return;
}


static void removeTitleBar(kernelWindow *window)
{
  // Removes the title bar from atop the window

  if (!(window->flags & WINFLAG_HASTITLEBAR))
    return;

  kernelWindowComponentDestroy(window->titleBar);
  window->titleBar = NULL;

  window->flags &= ~WINFLAG_HASTITLEBAR;

  return;
}


static int tileBackgroundImage(kernelWindow *window)
{
  // This will tile the supplied image as the background of the window.
  // Naturally, any other components in the window's client are need to be
  // drawn after this bit.
  
  int status = 0;
  int clientAreaX = 0;
  int clientAreaY = 0;
  unsigned clientAreaWidth = window->buffer.width;
  unsigned clientAreaHeight = window->buffer.height;
  unsigned count1, count2;

  // The window needs to have been assigned a background image
  if (window->backgroundImage.data == NULL)
    return (status = ERR_NULLPARAMETER);

  // Adjust the dimensions of our drawing area if necessary to accommodate
  // other things outside the client area
  if (window->flags & WINFLAG_HASBORDER)
    {
      clientAreaX += borderThickness;
      clientAreaY += borderThickness;
      clientAreaWidth -= (borderThickness * 2);
      clientAreaHeight -= (borderThickness * 2);
    }
  if (window->flags & WINFLAG_HASTITLEBAR)
    {
      clientAreaY += titleBarHeight;
      clientAreaHeight -= titleBarHeight;
    }

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


static void renderVisiblePortions(kernelWindow *window, screenArea *bufferClip)
{
  // Take the window supplied, and render the portions of the supplied clip
  // which are visible (i.e. not covered by other windows)
  
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
	  {
	    // Yeah, this window is covering ours somewhat.  We will need
	    // to get the area of the windows that overlap
	    getIntersectingArea(&(visibleAreas[0]),
				makeWindowScreenArea(windowList[count1]),
				&(coveredAreas[numCoveredAreas]));
	    numCoveredAreas++;

	    // If the intersecting area is already covered by one of
	    // our other covered areas, skip it.  Likewise if it covers
	    // another one, replace the other
	    for (count2 = 0; count2 < (numCoveredAreas - 1); count2 ++)
	      {
		if (isAreaInside(&(coveredAreas[numCoveredAreas - 1]),
				 &(coveredAreas[count2])))
		  {
		    numCoveredAreas--;
		    break;
		  }
		else if (isAreaInside(&(coveredAreas[count2]),
				      &(coveredAreas[numCoveredAreas - 1])))
		  {
		    coveredAreas[count2].leftX =
		      coveredAreas[numCoveredAreas - 1].leftX;
		    coveredAreas[count2].topY =
		      coveredAreas[numCoveredAreas - 1].topY;
		    coveredAreas[count2].rightX =
		      coveredAreas[numCoveredAreas - 1].rightX;
		    coveredAreas[count2].bottomY =
		      coveredAreas[numCoveredAreas - 1].bottomY;
		    numCoveredAreas--;
		    break;
		  }
	      }
	  }
      }
  
  kernelLockRelease(&windowListLock);

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


static void flattenContainer(kernelWindowContainer *originalContainer,
			     kernelWindowContainer *flatContainer,
			     unsigned flags)
{
  // Given a container, recurse through any of its subcontainers (if
  // applicable) and return a flattened one

  kernelWindowComponent *component = NULL;
  int count;

  for (count = 0; count < originalContainer->numComponents; count ++)
    {
      component = originalContainer->components[count];

      if ((component->flags & flags) == flags)
	{
	  flatContainer
	    ->components[flatContainer->numComponents++] = component;

	  // If this component is a container, recurse it
	  if (component->type == containerComponentType)
	    flattenContainer((kernelWindowContainer *) component->data,
			     flatContainer, flags);
	}
    }
}


static int drawWindow(kernelWindow *window)
{
  // Draws the specified window from scratch

  int status = 0;
  int clientAreaX = 0;
  int clientAreaY = 0;
  int clientAreaWidth = 0;
  int clientAreaHeight = 0;
  kernelWindowContainer flatContainer;
  kernelWindowComponent *component = NULL;
  int count;

  status = kernelWindowGetSize(window, &clientAreaWidth, &clientAreaHeight);
  if (status < 0)
    return (status);

  // Adjust the dimensions of our drawing area if necessary to accommodate
  // other things outside the client area
  if (window->flags & WINFLAG_HASBORDER)
    {
      clientAreaX += borderThickness;
      clientAreaY += borderThickness;
      clientAreaWidth -= (borderThickness * 2);
      clientAreaHeight -= (borderThickness * 2);
    }
  if (window->flags & WINFLAG_HASTITLEBAR)
    {
      clientAreaY += titleBarHeight;
      clientAreaHeight -= titleBarHeight;
    }

  // Draw a blank background
  kernelGraphicDrawRect(&(window->buffer), (color *) &(window->background),
			draw_normal, 0, 0, window->buffer.width,
			window->buffer.height, 0, 1);

  // If the window has a background image, draw it
  if (window->backgroundImage.data)
    tileBackgroundImage(window);

  // Loop through all the regular window components and draw them
  flatContainer.numComponents = 0;
  flattenContainer((kernelWindowContainer *) window->mainContainer->data,
		   &flatContainer, WINFLAG_VISIBLE);
  for (count = 0; count < flatContainer.numComponents; count ++)
    {
      component = flatContainer.components[count];

      if (component->draw)
	component->draw((void *) component);
    }

  // If the window has a titlebar, draw it
  if (window->titleBar && window->flags & WINFLAG_HASTITLEBAR)
    {
      status = window->titleBar->draw((void *) window->titleBar);
      if (status < 0)
	return (status);
    }

  // If the window has a border, draw it
  if (window->flags & WINFLAG_HASBORDER)
    {
      for (count = 0; count < 4; count ++)
	if (window->borders[count])
	  {
	    status = window->borders[count]
	      ->draw((void *) window->borders[count]);
	    if (status < 0)
	      return (status);
	  }
    }

  if (window->flags & WINFLAG_DEBUGLAYOUT)
    ((kernelWindowContainer *) window->mainContainer->data)
      ->containerDrawGrid(window->mainContainer);

  // Only render the visible portions of the window
  renderVisiblePortions(window, &((screenArea)
  { 0, 0, (window->buffer.width - 1), (window->buffer.height - 1) } ));

  // Done
  return (status = 0);
}


static int drawWindowClip(kernelWindow *window, int xCoord, int yCoord,
			  int width, int height)
{
  // Draw a clip of the client area of a window.  In actual fact it doesn't
  // simply draw what's inside the bounded area supplied.  It redraws all
  // of any window component that is even partially contained in the area
  // (i.e. we don't draw *partial* widgets), but only re-renders the area
  // requested.  Sort of the same.

  int status = 0;
  kernelWindowContainer flatContainer;
  kernelWindowComponent *component = NULL;
  int xOffset, yOffset;
  int lowestLevel = 0;
  int count1, count2;

  // Don't actually try to do negative clips.  No point at all.
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

  flatContainer.numComponents = 0;
  flattenContainer((kernelWindowContainer *) window->mainContainer->data,
		   &flatContainer, WINFLAG_VISIBLE);

  // NULL all components that are *not* at this location
  for (count1 = 0; count1 < flatContainer.numComponents; count1 ++)
    {
      component = flatContainer.components[count1];
      if (!doAreasIntersect(&((screenArea)
      { xCoord, yCoord, (xCoord + width - 1), (yCoord + height - 1)} ),
			    &((screenArea)
	    { component->xCoord, component->yCoord,
			(component->xCoord + component->width - 1),
			(component->yCoord + component->height - 1) } )))
	{
	  flatContainer.components[count1] = NULL;
	}
      else
	{
	  if (component->level > lowestLevel)
	    lowestLevel = component->level;
	}
    }
  
  // Draw all the components by level

  for (count1 = lowestLevel; count1 >= 0; count1 --) 
    for (count2 = 0; count2 < flatContainer.numComponents; count2 ++)
      {
        component = flatContainer.components[count2];
	if ((component != NULL) && isComponentVisible(component) &&
	    (component->level >= count1))
	  {
	    if (component->draw)
	      component->draw((void *) component);

	    flatContainer.components[count2] = NULL;
	  }
      }

  if (window->flags & WINFLAG_DEBUGLAYOUT)
    ((kernelWindowContainer *) window->mainContainer->data)
      ->containerDrawGrid(window->mainContainer);

  // Only render the visible portions of the window
  renderVisiblePortions(window, &((screenArea)
  { xCoord, yCoord, (xCoord + width - 1), (yCoord + height - 1) } ));

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
  int newTitleBarWidth = 0;
  void *oldBufferData = NULL;

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
  
  // Tell the title bar component to resize
  if (window->flags & WINFLAG_HASTITLEBAR)
    {
      newTitleBarWidth = width;
      if (window->flags & WINFLAG_HASBORDER)
	newTitleBarWidth -= (borderThickness * 2);

      window->titleBar->resize((void *) window->titleBar, newTitleBarWidth,
			       titleBarHeight);

      window->titleBar->width = newTitleBarWidth;
      window->titleBar->height = titleBarHeight;
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
  
  int status = 0;
  int newWidth = 0;
  int newHeight = 0;

  newWidth = (window->mainContainer->xCoord + window->mainContainer->width);
  newHeight = (window->mainContainer->yCoord + window->mainContainer->height);

  // Adjust for title bars, borders, etc.
  if (window->flags & WINFLAG_HASBORDER)
    {
      newWidth += borderThickness;
      newHeight += borderThickness;
    }

  // Resize it.
  status = setWindowSize(window, newWidth, newHeight);
  return (status);
}


static int layoutWindow(kernelWindow *window)
{
  // Repositions all the window's components based on their parameters

  int status = 0;

  // Do layout of the base container
  window->mainContainer->xCoord = 0;
  window->mainContainer->yCoord = 0;
  window->mainContainer->width = window->buffer.width;
  window->mainContainer->height = window->buffer.height;
    
  if (window->flags & WINFLAG_HASTITLEBAR)
    {
      window->mainContainer->yCoord += titleBarHeight;
      window->mainContainer->height -= titleBarHeight;
    }
  if (window->flags & WINFLAG_HASBORDER)
    {
      window->mainContainer->xCoord += borderThickness;
      window->mainContainer->yCoord += borderThickness;
      window->mainContainer->width -= (borderThickness * 2);
      window->mainContainer->height -= (borderThickness * 2);
    }

  status = ((kernelWindowContainer *) window->mainContainer->data)
    ->containerLayout(window->mainContainer);
  if (status < 0)
    return (status);
  
  if (window->flags & WINFLAG_HASTITLEBAR)
    {
      window->titleBar->xCoord = 0;
      window->titleBar->yCoord = 0;

      if (window->flags & WINFLAG_HASBORDER)
	{
	  window->titleBar->xCoord += borderThickness;
	  window->titleBar->yCoord += borderThickness;
	}
    }

  return (status = 0);
}


static int makeConsoleWindow(void)
{
  // Create the temporary console window

  int status = 0;
  componentParameters params;
  kernelTextArea *oldArea = NULL;
  kernelTextArea *newArea = NULL;
  unsigned char *lineAddress = NULL;
  unsigned char lineBuffer[1024];
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
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;
  params.font = defaultFont;

  consoleTextArea =
    kernelWindowNewTextArea(consoleWindow, 80, 50, DEFAULT_SCROLLBACKLINES,
			    &params);
  if (consoleTextArea == NULL)
    {
      kernelError(kernel_warn, "Unable to switch text areas to console "
		  "window");
      return (status = ERR_NOCREATE);
    }

  oldArea = kernelTextGetConsoleOutput()->textArea;
  newArea = ((kernelWindowTextArea *) consoleTextArea->data)->area;

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
      lineAddress = (unsigned char *)
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
  // oldArea->inputStream = NULL;
  // oldArea->outputStream = NULL;
  // oldArea->visibleData = NULL;
  kernelTextAreaDestroy(oldArea);

  return (status = 0);
}


static void changeComponentFocus(kernelWindow *window,
				 kernelWindowComponent *component)
{
  // Gets called when a component acquires the focus
  
  if (window->focusComponent && (component != window->focusComponent))
    {
      window->focusComponent->flags &= ~WINFLAG_HASFOCUS;
      if (window->focusComponent->focus)
	window->focusComponent->focus((void *) window->focusComponent, 0);
    }

  // This might be NULL.  That is okay.
  window->focusComponent = component;

  if (component && (component->flags & WINFLAG_VISIBLE) &&
      (component->flags & WINFLAG_CANFOCUS))
    {
      component->flags |= WINFLAG_HASFOCUS;
      if (component->focus)
	component->focus((void *) component, 1);
    }
  
  return;
}


static void focusFirstComponent(kernelWindow *window)
{
  // Set the focus to the first focusable component
  
  kernelWindowContainer flatContainer;
  int count;
  
  // Flatten the window container so we can iterate through it
  flatContainer.numComponents = 0;
  flattenContainer((kernelWindowContainer *) window->mainContainer->data,
		   &flatContainer, (WINFLAG_VISIBLE | WINFLAG_ENABLED));
  
  // If the window has any sort of text area or field inside it, set the
  // input/output streams to that process.
  for (count = 0; count < flatContainer.numComponents; count ++)
    if (flatContainer.components[count]->type == textAreaComponentType)
      {
	changeComponentFocus(window, flatContainer.components[count]);
	break;
      }

  // Still no focus?  Give it to the first component that can focus
  if (!window->focusComponent)
    for (count = 0; count < flatContainer.numComponents; count ++)
      if (flatContainer.components[count]->flags & WINFLAG_CANFOCUS)
	{
	  changeComponentFocus(window, flatContainer.components[count]);
	  break;
	}
}


static void focusNextComponent(kernelWindow *window)
{
  // Change the focus the next component
  
  kernelWindowContainer flatContainer;
  kernelWindowComponent *nextFocus = NULL;
  int count = 0;

  if (!window->focusComponent)
    {
      focusFirstComponent(window);
      return;
    }

  // Get all the window components in a flat container
  flatContainer.numComponents = 0;
  flattenContainer((kernelWindowContainer *) window->mainContainer->data,
		   &flatContainer, (WINFLAG_VISIBLE | WINFLAG_ENABLED));
  
  for (count = 0; count < flatContainer.numComponents; count ++)
    if (flatContainer.components[count] == window->focusComponent)
      {
	count ++;

	for ( ; count < flatContainer.numComponents; count ++)
	  {
	    if (flatContainer.components[count]->flags & WINFLAG_CANFOCUS)
	      {
		nextFocus = flatContainer.components[count];
		break;
	      }
	  }

	if (!nextFocus)
	  for (count = 0; count < flatContainer.numComponents; count ++)
	    {
	      if (flatContainer.components[count]->flags & WINFLAG_CANFOCUS)
		{
		  nextFocus = flatContainer.components[count];
		  break;
		}
	    }
      }
  
  if (nextFocus == window->focusComponent)
    nextFocus = NULL;

  // 'nextFocus' might be NULL, but that's okay as it will simply unfocus
  // the currently focused component
  changeComponentFocus(window, nextFocus);
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
	  
	  if (focusWindow->flags & WINFLAG_HASTITLEBAR)
	    focusWindow->titleBar->draw((void *) focusWindow->titleBar);

	  // Redraw the window's title bar area only
	  kernelWindowUpdateBuffer(&(focusWindow->buffer), borderThickness,
				   borderThickness,
				   (focusWindow->buffer.width -
				    (borderThickness * 2)), titleBarHeight);
	}
    }

  // This window becomes topmost
  window->level = 0;

  // Mark it as focused
  window->flags |= WINFLAG_HASFOCUS;
  focusWindow = window;

  // Draw the title bar as focused
  if (window->flags & WINFLAG_HASTITLEBAR)
    window->titleBar->draw((void *) window->titleBar);

  if (window->focusComponent)
    changeComponentFocus(window, window->focusComponent);
  else
    focusFirstComponent(window);

  // Redraw the whole window, since all of it is now visible (otherwise we
  // would call drawVisiblePortions()).
  kernelWindowUpdateBuffer(&(window->buffer), 0, 0, window->buffer.width,
			   window->buffer.height);
  return;
}


static void componentDrawBorder(void *componentData, int draw)
{
  // Draw a simple little border around the supplied component
  
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);

  if (draw)
    kernelGraphicDrawRect(buffer,
			  (color *) &(component->parameters.foreground),
			  draw_normal, (component->xCoord - 2),
			  (component->yCoord - 2), (component->width + 4),
			  (component->height + 4), 1, 0);
  else
    kernelGraphicDrawRect(buffer, (color *) &(window->background),
			  draw_normal, (component->xCoord - 2),
			  (component->yCoord - 2), (component->width + 4),
			  (component->height + 4), 1, 0);
  return;
}


static void componentErase(void *componentData)
{
  // Erase the component

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelGraphicDrawRect(&(((kernelWindow *) component->window)->buffer),
			&kernelDefaultBackground, draw_normal,
			component->xCoord, component->yCoord,
			component->width, component->height, 1, 1);
  return;
}


static int componentGrey(void *componentData)
{
  // Filter the component with the default background color

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;

  // If the component has a draw function (stored in its 'grey' pointer)
  // call it first.
  if (component->grey)
    status = component->grey((void *) component);

  kernelGraphicFilter(&(((kernelWindow *) component->window)->buffer),
		      (color *) &(window->background), component->xCoord,
		      component->yCoord, component->width, component->height);
  return (status);
}


static kernelWindowComponent *findTopmostComponent(kernelWindow *window,
						   int xCoord, int yCoord)
{
  kernelWindowContainer flatContainer;
  int topmostLevel = MAXINT;
  kernelWindowComponent *topmostComponent = NULL;
  int count;

  // Check whether the event happened in any of the top-level system components
  if ((window->flags & WINFLAG_HASTITLEBAR) &&
      (isPointInside(xCoord, yCoord,
		     makeComponentScreenArea(window->titleBar))))
    return (window->titleBar);

  if (window->flags & WINFLAG_HASBORDER)
    for (count = 0; count < 4; count ++)
      if (isPointInside(xCoord, yCoord,
			makeComponentScreenArea(window->borders[count])))
	return (window->borders[count]);

  // Make a flat container so we can loop through all the components
  flatContainer.numComponents = 0;
  flattenContainer((kernelWindowContainer *) window->mainContainer->data,
		   &flatContainer, (WINFLAG_VISIBLE | WINFLAG_ENABLED));

  // Find the window's topmost component at this location
  for (count = 0; count < flatContainer.numComponents; count ++)
    {
      if ((flatContainer.components[count]->level < topmostLevel) &&
	  isPointInside(xCoord, yCoord,
		makeComponentScreenArea(flatContainer.components[count])))
	{
	  topmostComponent = flatContainer.components[count];
	  topmostLevel = flatContainer.components[count]->level;
	}
    }
    
  return (topmostComponent);
}


static void processEvents(void)
{
  windowEvent event;
  kernelWindow *window = NULL;
  kernelWindowComponent *targetComponent = NULL;
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
	    draggingComponent->mouseEvent((void *) draggingComponent, &event);

	  // Adjust to the coordinates of the component
	  event.xPosition -= (((kernelWindow *) draggingComponent->window)
			      ->xCoord + draggingComponent->xCoord);
	  event.yPosition -= (((kernelWindow *) draggingComponent->window)
			      ->yCoord + draggingComponent->yCoord);
	  
	  // Put this mouse event into the component's windowEventStream
	  kernelWindowEventStreamWrite(&(draggingComponent->events), &event);

	  if (event.type != EVENT_MOUSE_DRAG)
	    draggingComponent = NULL;

	  // We can stop here
	  continue;
	}

      else
	{
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
		if ((window == NULL) ||
		    (windowList[count]->level < window->level))
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
	  if ((event.type == EVENT_MOUSE_LEFTDOWN) &&
	      (window != focusWindow) && (window != rootWindow))
	    // Give the window the focus
	    changeWindowFocus(window);

	  // Find out if it was inside of any of the window's components,
	  // and if so, put a windowEvent into its windowEventStream
	  targetComponent =
	    findTopmostComponent(window, event.xPosition, event.yPosition);
	}

      if (targetComponent)
	{
	  if ((window->focusComponent != targetComponent) &&
	      (event.type == EVENT_MOUSE_LEFTDOWN))
	    {
	      if (targetComponent->flags & WINFLAG_CANFOCUS)
		changeComponentFocus(window, targetComponent);
	    }

	  if (targetComponent->mouseEvent)
	    targetComponent->mouseEvent((void *) targetComponent, &event);
	  
	  // Adjust to the coordinates of the component
	  event.xPosition -= (((kernelWindow *) targetComponent->window)
			      ->xCoord + targetComponent->xCoord);
	  event.yPosition -= (((kernelWindow *) targetComponent->window)
			      ->yCoord + targetComponent->yCoord);

	  // Put this mouse event into the component's windowEventStream
	  kernelWindowEventStreamWrite(&(targetComponent->events), &event);
	  
	  if (event.type == EVENT_MOUSE_DRAG)
	    draggingComponent = targetComponent;
	}

      else if (window->focusComponent)
	changeComponentFocus(window, NULL);
    }

  while (kernelWindowEventStreamRead(&keyEvents, &event) > 0)
    {
      // It was a keyboard event

      // Find the target component
      if (focusWindow)
	{
	  // If it was a [tab] down, focus the next component
	  if (((focusWindow->focusComponent == NULL) ||
	       !(focusWindow->focusComponent->parameters.stickyFocus)) &&
	      ((event.type == EVENT_KEY_DOWN) && (event.key == 9)))
	    focusNextComponent(focusWindow);
	  
	  else if (focusWindow->focusComponent)
	    {
	      targetComponent = focusWindow->focusComponent;
	      
	      if (targetComponent->keyEvent)
		targetComponent->keyEvent((void *) targetComponent, &event);
	  
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

	  container = (kernelWindowContainer *) window->sysContainer->data;

	  // Loop through the system components
	  for (compCount = 0; compCount < container->numComponents;
	       compCount ++)
	    {
	      component = container->components[compCount];
 
	      // Any events pending?
	      if ((kernelWindowEventStreamRead(&(component->events),
					       &event) > 0) &&
		  // Any handler for the event?
		  component->eventHandler)
		{
		  component->eventHandler((objectKey) component, &event);
		  
		  // Window closed?  Don't want to loop here any more.
		  if (!kernelMultitaskerProcessIsAlive(processId))
		    compCount = container->numComponents;
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


static variableList *getConfiguration(void)
{
  variableList *settings = NULL;

  // Read the config file
  settings = kernelConfigurationReader(DEFAULT_WINDOWMANAGER_CONFIG);
  if (settings == NULL)
    // Argh.  No file?  Create a reasonable, empty list for us to use
    settings = kernelVariableListCreate(255, 1024, "window configuration");

  return (settings);
}


static int windowStart(void)
{
  // This does all of the startup stuff.  Gets called once during
  // system initialization.

  int status = 0;
  variableList *settings = NULL;
  kernelTextOutputStream *output = NULL;
  char propertyName[128];
  char propertyValue[128];
  int count;

  char *mousePointerTypes[][2] = {
    { "default", DEFAULT_MOUSEPOINTER_DEFAULT },
    { "busy", DEFAULT_MOUSEPOINTER_BUSY }
  };

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  settings = getConfiguration();
  if (settings == NULL)
    return (status = ERR_NOTINITIALIZED);

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

      if (kernelVariableListGet(settings, propertyName, propertyValue, 128))
	{
	  // Nothing specified.  Use the default.
	  strcpy(propertyValue, mousePointerTypes[count][1]);
	  // Save it so it can be written out later to the configuration file
	  kernelVariableListSet(settings, propertyName, propertyValue);
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
      kernelMemoryRelease(settings);
      return (status = ERR_NOTINITIALIZED);
    }

  // Spawn the window thread
  spawnWindowThread();

  // Draw the main, root window.
  rootWindow = kernelWindowMakeRoot(settings);
  if (rootWindow == NULL)
    {
      kernelMemoryRelease(settings);
      return (status = ERR_NOTINITIALIZED);
    }

  // Draw the console and root windows, but don't make them visible
  makeConsoleWindow();
  // kernelWindowSetLocation(consoleWindow, 0, 0);
  // kernelWindowSetVisible(consoleWindow, 1);

  // Mouse housekeeping.
  kernelMouseDraw();

  // Re-write config file
  status = kernelConfigurationWriter(DEFAULT_WINDOWMANAGER_CONFIG, settings);
  if (status < 0)
    kernelError(kernel_error, "Error updating window configuration");
  else
    kernelLog("Updated window configuration");

  // Done
  kernelMemoryRelease(settings);
  return (status = 0);
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
  int count;

  // Don't bother if graphics are not enabled
  if (!kernelGraphicsAreEnabled())
    {
      kernelError(kernel_error, "The window environment can not run without "
		  "graphics enabled");
      return (status = ERR_NOTINITIALIZED);
    }

  kernelLog("Starting window system initialization");

  // Allocate memory to hold all the window information
  windowData = kernelMalloc((sizeof(kernelWindow) * WINDOW_MAXWINDOWS));
  if (windowData == NULL)
    // Eek
    return (status = ERR_MEMORY);

  // Set up the array of pointers to memory within the window data that
  // will be used for the window structures
  for (count = 0; count < WINDOW_MAXWINDOWS; count ++)
    windowList[count] = &(windowData[count]);

  numberWindows = 0;

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
  winShellPid = kernelWindowShell(kernelUserGetPrivilege(userName));
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
  // Redraw all the windows

  int count;

  for (count = 0; count < numberWindows; count ++)
    renderVisiblePortions(windowList[count],
			  makeWindowScreenArea(windowList[count]));
}


kernelWindow *kernelWindowNew(int processId, const char *title)
{
  // Creates a new window using the supplied values.  Not visible by default.

  int status = 0;
  kernelWindow *newWindow = NULL;
  componentParameters params;
  int bottomLevel = 0;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (newWindow = NULL);

  // Check params
  if (title == NULL)
    return (newWindow = NULL);

  kernelMemClear(&params, sizeof(componentParameters));

  // Lock the window list
  if (kernelLockGet(&windowListLock) < 0)
    return (newWindow = NULL);

  // Get some memory for window data
  newWindow = windowList[numberWindows];

  // Make sure it's all empty
  kernelMemClear((void *) newWindow, sizeof(kernelWindow));

  newWindow->type = windowType;

  // Set the process Id
  newWindow->processId = processId;

  // The title
  strncpy((char *) newWindow->title, title, WINDOW_MAX_TITLE_LENGTH);
  newWindow->title[WINDOW_MAX_TITLE_LENGTH - 1] = '\0';

  // Set the coordinates to -1 initially
  newWindow->xCoord = -1;
  newWindow->yCoord = -1;

  // New windows get put at the bottom level until they are marked as
  // visible
  newWindow->level = 0;
  if (numberWindows > 1)
    {
      for (count = 0; count < (numberWindows - 1); count ++)
	if ((windowList[count] != rootWindow) &&
	    (windowList[count]->level > bottomLevel))
	  bottomLevel = windowList[count]->level;
      newWindow->level = (bottomLevel + 1);
    }

  // New windows don't have the focus until they are marked as visible
  newWindow->flags &= ~WINFLAG_HASFOCUS;

  // Not visible until someone tells us to make it visible
  newWindow->flags &= ~WINFLAG_VISIBLE;

  // By default windows are movable and resizable
  newWindow->flags |= (WINFLAG_MOVABLE | WINFLAG_RESIZABLE);
  newWindow->backgroundImage.data = NULL;

  // Get the window's graphic buffer all set up
  status = getWindowGraphicBuffer(newWindow, 1, 1);
  if (status < 0)
    {
      // Eek!  No new window for you!
      kernelLockRelease(&windowListLock);
      kernelError(kernel_warn, "Error getting memory for window graphic "
		  "buffer");
      return (newWindow = NULL);
    }

  // Set the window's background color to the default
  newWindow->background.red = kernelDefaultBackground.red;
  newWindow->background.green = kernelDefaultBackground.green;
  newWindow->background.blue = kernelDefaultBackground.blue;

  // Add an event stream for the window
  status = kernelWindowEventStreamNew(&(newWindow->events));
  if (status < 0)
    {
      kernelLockRelease(&windowListLock);
      return (newWindow = NULL);
    }

  // Add top-level containers for other components
  newWindow->sysContainer =
    kernelWindowNewContainer(newWindow, "sysContainer", &params);
  newWindow->mainContainer =
    kernelWindowNewContainer(newWindow, "mainContainer", &params);

  newWindow->mainContainer->xCoord = borderThickness;
  newWindow->mainContainer->yCoord = (borderThickness + titleBarHeight);
  
  // Add the title bar component
  addTitleBar(newWindow);

  // Add the border components
  addBorder(newWindow);

  // Set up the functions
  newWindow->draw = (int (*)(void *)) &drawWindow;
  newWindow->drawClip = (int (*) (void *, int, int, int, int)) &drawWindowClip;

  numberWindows += 1;
  kernelLockRelease(&windowListLock);

  kernelWindowShellUpdateList(windowList, numberWindows);

  // Return the window
  return (newWindow);
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
  newDialog = kernelWindowNew(parentWindow->processId, title);
  if (newDialog == NULL)
    return (newDialog);

  // Attach the dialog window to the parent window and vice-versa
  parentWindow->dialogWindow = (void *) newDialog;
  newDialog->parentWindow = (void *) parentWindow;

  // Dialog windows do not have minimize buttons, by default, since they
  // do not appear in the taskbar window list
  kernelWindowSetHasMinimizeButton(newDialog, 0);

  // Return the dialog window
  return (newDialog);
}


int kernelWindowDestroy(kernelWindow *window)
{
  // Delete the window.

  int status = 0;
  int listPosition = 0;
  kernelWindowContainer flatContainer;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  // If this window *has* a dialog window, first destroy the dialog
  if (window->dialogWindow)
    {
      status = kernelWindowDestroy((kernelWindow *) window->dialogWindow);
      if (status < 0)
	return (status);
    }

  // If this window *is* a dialog window, dissociate it from its parent
  if (window->parentWindow)
    ((kernelWindow *) window->parentWindow)->dialogWindow = NULL;

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

  // Remove this window from our list.  If there will be at least 1 remaining
  // window and this window was not the last one, swap the last one into the
  // spot this one occupied
  numberWindows--;
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

  // Not visible anymore
  kernelWindowSetVisible(window, 0);

  // Call the 'destroy' function for all the window's top-level components

  if (window->mainContainer)
    {
      flatContainer.numComponents = 0;
      flattenContainer((kernelWindowContainer *) window->mainContainer->data,
		       &flatContainer, 0 /* No flags/conditions */);
      for (count = 0; count < flatContainer.numComponents; count ++)
	kernelWindowComponentDestroy(flatContainer.components[count]);
      window->mainContainer = NULL;
    }

  if (window->sysContainer)
    {
      flatContainer.numComponents = 0;
      flattenContainer((kernelWindowContainer *) window->sysContainer->data,
		       &flatContainer, 0 /* No flags/conditions */);
      for (count = 0; count < flatContainer.numComponents; count ++)
	kernelWindowComponentDestroy(flatContainer.components[count]);
      window->sysContainer = NULL;
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

  // Redraw the mouse
  kernelMouseDraw();

  kernelWindowShellUpdateList(windowList, numberWindows);

  return (status);
}


int kernelWindowUpdateBuffer(kernelGraphicBuffer *buffer, int clipX, int clipY,
			     int width, int height)
{
  // A component is trying to tell us that it has updated itself and
  // would like the window to be redrawn.

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

  if (!(window->flags & WINFLAG_VISIBLE))
    // It's not currently on the screen
    return (status = 0);

  // Render the parts of this window's buffer that are currently visible.
  // We only want to render those parts
  if (window->level != 0)
    renderVisiblePortions(window, &((screenArea) { clipX, clipY,
		     (clipX + (width - 1)), (clipY + (height - 1)) } ));
  else
    status = kernelGraphicRenderBuffer(buffer, window->xCoord, window->yCoord,
				       clipX, clipY, width, height);
  // Redraw the mouse
  kernelMouseDraw();

  return (status);
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

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((window == NULL)  || (width == NULL) || (height == NULL))
    return (status = ERR_NULLPARAMETER);

  // If layout has not been done, do it now
  if (!((kernelWindowContainer *) window->mainContainer->data)->doneLayout)
    {
      status = layoutWindow(window);
      if (status < 0)
	return (status);
      status = autoSizeWindow(window);
    }

  *width = window->buffer.width;
  *height = window->buffer.height;

  return (status);
}


int kernelWindowSetSize(kernelWindow *window, int width, int height)
{
  // Sets the size of a window

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  status = setWindowSize(window, width, height);
  if (status < 0)
    return (status);

  // See if we need to call the 'resize' operation for the main container
  if (!(window->flags & WINFLAG_PACKED) &&
      (((kernelWindowContainer *) window->mainContainer->data)->doneLayout))
    {
      width = window->buffer.width;
      height = window->buffer.height;
      if (window->flags & WINFLAG_HASTITLEBAR)
	height -= titleBarHeight;
      if (window->flags & WINFLAG_HASBORDER)
	{
	  width -= (borderThickness * 2);
	  height -= (borderThickness * 2);
	}

      status = window->mainContainer->resize((void *) window->mainContainer,
					     width, height);
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


void kernelWindowSnapIcons(kernelWindow *window)
{
  // Snap all icons to a grid in the supplied window

  kernelWindowContainer *container = (kernelWindowContainer *)
    window->mainContainer->data;
  int iconRow;
  int count1, count2;

  // Make sure the window has been laid out
  if (!container->doneLayout)
    layoutWindow(window);

  for (count1 = 0; count1 < container->numComponents; count1 ++)
    {
      if ((container->components[count1]->type == iconComponentType) &&
	  ((container->components[count1]->yCoord + 
	    container->components[count1]->height) >= window->buffer.height))
	{
	  iconRow = 1;

	  for (count2 = count1 ; count2 < container->numComponents; count2 ++)
	    {
	      container->components[count2]->parameters.gridX += 1;
	      container->components[count2]->parameters.gridY = iconRow++;
	    }

	  // Set the new coordinates
	  layoutWindow(window);
	}
    }

  return;
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
  
  return (status);
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


int kernelWindowSetHasMinimizeButton(kernelWindow *window, int trueFalse)
{
  // Sets the 'has minimize button' attribute and, if false, removes any
  // minimize button component.

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  if (trueFalse)
    window->flags |= WINFLAG_HASMINIMIZEBUTTON;
  else
    window->flags &= ~WINFLAG_HASMINIMIZEBUTTON;

  // Return success
  return (status = 0);
}


int kernelWindowSetHasCloseButton(kernelWindow *window, int trueFalse)
{
  // Sets the 'has close button' attribute and, if false, removes any
  // close button component.

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  if (trueFalse)
    window->flags |= WINFLAG_HASCLOSEBUTTON;
  else
    window->flags &= ~WINFLAG_HASCLOSEBUTTON;

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
    window->draw((void *) window);

  return (status = 0);
}


int kernelWindowSetVisible(kernelWindow *window, int visible)
{
  // Sets a window to visible or not

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  if (visible)
    {
      // If the window is becoming visible and hasn't yet had its layout done,
      // do that now
      if (!((kernelWindowContainer *) window->mainContainer->data)->doneLayout)
	{
	  status = layoutWindow(window);
	  if (status < 0)
	    return (status);
	}

      if ((window->buffer.width == 1) || (window->buffer.height == 1))
	{
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

  if (minimize)
    kernelWindowSetVisible(window, 0);

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

  if (!minimize)
    kernelWindowSetVisible(window, 1);
}


int kernelWindowAddConsoleTextArea(kernelWindow *window,
				   componentParameters *params)
{
  // Moves the console text area component from our 'hidden' console window
  // to the supplied window

  int status = 0;
  kernelWindowContainer *consoleContainer = NULL;
  kernelWindowContainer *targetContainer = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((window == NULL) || (params == NULL))
    return (status = ERR_NULLPARAMETER);

  // Make sure the console text area is not already assigned to some other
  // window (other than the consoleWindow)
  if (consoleTextArea->window != consoleWindow)
    return (status = ERR_ALREADY);

  consoleContainer =
    (kernelWindowContainer *) consoleWindow->mainContainer->data;
  targetContainer = (kernelWindowContainer *) window->mainContainer->data;

  // Remove it from the console window
  status = consoleContainer
    ->containerRemove(consoleWindow->mainContainer, consoleTextArea);
  if (status < 0)
    return (status);

  // Change the text area's buffer to be that of the new window
  ((kernelWindowTextArea *) consoleTextArea->data)
    ->area->graphicBuffer = &(window->buffer);

  // Now add it to the window
  status = targetContainer->containerAdd(window->mainContainer,
					 consoleTextArea, params);
  return (status);
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


int kernelWindowRegisterEventHandler(objectKey key, void (*function)(objectKey,
							     windowEvent *))
{
  // This function is called to register a windowEvent callback handler for
  // a component.

  int status = 0;
  kernelWindowComponent *component = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((key == NULL) || (function == NULL))
    return (status = ERR_NULLPARAMETER);

  component = (kernelWindowComponent *) key;
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
      component = (kernelWindowComponent *) key;
      
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
  variableList *settings = NULL;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((filename == NULL) || (rootWindow == NULL))
    return (status = ERR_NULLPARAMETER);
  
  // Try to load the new background image
  status = kernelImageLoadBmp(filename, &backgroundImage);
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
  settings = getConfiguration();
  if (settings)
    {
      kernelVariableListSet(settings, "background.image", filename);
      kernelConfigurationWriter(DEFAULT_WINDOWMANAGER_CONFIG, settings);
      kernelMemoryRelease(settings);
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
	  params.useDefaultForeground = 1;
	  params.useDefaultBackground = 1;
	  
	  kernelWindowNewTextLabel(dialog, labelText, &params);
	  kernelWindowSetVisible(dialog, 1);
	}

      status = kernelImageSaveBmp(filename, &saveImage);
      if (status < 0)
	kernelError(kernel_error, "Error %d saving bitmap\n", status);

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
      inputStream =
	(kernelTextInputStream *) ((kernelWindowTextArea *)
				   component->data)->area->inputStream;
      outputStream =
	(kernelTextOutputStream *) ((kernelWindowTextArea *)
				    component->data)->area->outputStream;

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


kernelWindowComponent *kernelWindowComponentNew(volatile void *parent,
						componentParameters *params)
{
  // Creates a new component and adds it to the main container of the
  // window.

  int status = 0;
  kernelWindowComponent *component = NULL;

  component = kernelMalloc(sizeof(kernelWindowComponent));
  if (component == NULL)
    return (component);

  component->type = genericComponentType;
  component->flags |= (WINFLAG_VISIBLE | WINFLAG_ENABLED);
  component->window = (void *) getWindow(parent);
  // Everything else NULL.

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
      kernelWindow *tmpWindow = (kernelWindow *) parent;
      if (tmpWindow->mainContainer)
	status = ((kernelWindowContainer *) tmpWindow->mainContainer->data)
	  ->containerAdd(tmpWindow->mainContainer, component, params);
    }
  else if (((kernelWindowComponent *) parent)->type == containerComponentType)
    {
      // Not a window
      kernelWindowComponent *tmpComponent = (kernelWindowComponent *) parent;
      status = ((kernelWindowContainer *) tmpComponent->data)
	->containerAdd(tmpComponent, component, params);
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
  kernelWindow *window = NULL;
  kernelWindowComponent *containerComponent = NULL;
  kernelWindowContainer *container = NULL;
  componentParameters params;

  // Make sure we've been initialized
  if (!initialized)
    return;

  // Check params.  Never destroy the console text area
  if (component == NULL)
    return;

  window = (kernelWindow *) component->window;

  // Make sure the component is removed from any containers it's in
  if (component->container != NULL)
    {
      containerComponent = (kernelWindowComponent *) component->container;
      container = (kernelWindowContainer *) containerComponent->data;
      if (container == NULL)
	{
	  kernelError(kernel_error, "Container data is null");
	  return;
	}
      container->containerRemove(containerComponent, component);
    }

  // If this is the console text area, move it back to our console window
  if (component == consoleTextArea)
    {
      kernelMemClear(&params, sizeof(componentParameters));
      params.gridWidth = 1;
      params.gridHeight = 1;
      params.orientationX = orient_center;
      params.orientationY = orient_middle;
      params.useDefaultForeground = 1;
      params.useDefaultBackground = 1;
      ((kernelWindowTextArea *) component->data)
	->area->graphicBuffer = &(consoleWindow->buffer);
      container = (kernelWindowContainer *) consoleWindow->mainContainer->data;
      container
	->containerAdd(consoleWindow->mainContainer, component, &params);
      return;
    }

  // Call the component's own destroy function
  if (component->destroy)
    component->destroy((void *) component);
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
  kernelWindowComponent *containerComponent =
    (kernelWindowComponent *) component->container;
  kernelWindow *window = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (component == NULL)
    return (status = ERR_NULLPARAMETER);

  window = (kernelWindow *) component->window;

  if (visible)
    {
      component->flags |= WINFLAG_VISIBLE;
      if (component->draw)
	component->draw((void *) component);
    }
  else
    {
      if (window->focusComponent == component)
	// Make sure it doesn't have the focus
	focusNextComponent(window);

      component->flags &= ~WINFLAG_VISIBLE;
      if (component->erase)
	component->erase((void *) component);
    }
  
  // Redraw a clip of that part of the window
  if (((kernelWindowContainer *) containerComponent->data)->doneLayout)
    drawWindowClip(window, component->xCoord, component->yCoord,
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
  kernelWindow *window = NULL;
  void *tmp = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (component == NULL)
    return (status = ERR_NULLPARAMETER);

  window = (kernelWindow *) component->window;

  if (enabled && !(component->flags & WINFLAG_ENABLED))
    {
      component->flags |= WINFLAG_ENABLED;

      tmp = component->grey;
      component->grey = component->draw;
      component->draw = tmp;
    }
  else if (!enabled && (component->flags & WINFLAG_ENABLED))
    {
      if (window->focusComponent == component)
	// Make sure it doesn't have the focus
	focusNextComponent(window);

      component->flags &= ~WINFLAG_ENABLED;

      tmp = component->grey;
      component->grey = component->draw;
      component->draw = tmp;
    }

  // Redraw a clip of that part of the window
  drawWindowClip(window, component->xCoord, component->yCoord,
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
    status = component
      ->resize((void *) component, component->width, component->height);

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
    status = component
      ->resize((void *) component, component->width, component->height);

  component->height = height;
  
  return (status);
}


int kernelWindowComponentFocus(kernelWindowComponent *component)
{
  // Put the supplied component on top of any other components it intersects
  
  int status = 0;
  kernelWindow *window = NULL;
  kernelWindowContainer flatContainer;
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

  flatContainer.numComponents = 0;
  flattenContainer((kernelWindowContainer *) window->mainContainer->data,
		   &flatContainer, 0 /* No flags/conditions */);

  // Find all the window's components at this location 
  for (count = 0; count < flatContainer.numComponents; count ++)
    if (flatContainer.components[count]->level <= component->level)
      {
	if (doAreasIntersect(makeComponentScreenArea(component),
		     makeComponentScreenArea(flatContainer.components[count])))
	  flatContainer.components[count]->level++;
      }
  
  component->level = 0;

  if (component->flags & WINFLAG_CANFOCUS)
    changeComponentFocus(window, component);

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

  return (component->draw((void *) component));
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

  return (component->getData((void *) component, buffer, size));
}


int kernelWindowComponentSetData(kernelWindowComponent *component,
				 void *buffer, int size)
{
  // Set (generic) data in a component
  
  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((component == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  if (!component->setData)
    return (status = ERR_NOTIMPLEMENTED);

  status = component->setData((void *) component, buffer, size);

  return (status);
}


int kernelWindowComponentGetSelected(kernelWindowComponent *component)
{
  // Calls the 'get selected' method of the component, if applicable

  // Make sure we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  // Check parameters
  if (component == NULL)
    return (ERR_NULLPARAMETER);

  if (component->getSelected == NULL)
    return (ERR_NOSUCHFUNCTION);

  return (component->getSelected((void *) component));
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

  return (component->setSelected((void *) component, selected));
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
