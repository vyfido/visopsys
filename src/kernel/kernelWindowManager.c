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
//  kernelWindowManager.c
//

// This is the code that does all of the generic stuff for setting up
// GUI windows.

#include "kernelWindowManager.h"
#include "kernelWindowEventStream.h"
#include "kernelParameters.h"
#include "kernelMemoryManager.h"
#include "kernelMalloc.h"
#include "kernelVariableList.h"
#include "kernelMultitasker.h"
#include "kernelLock.h"
#include "kernelFileStream.h"
#include "kernelUser.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <values.h>
#include <sys/errors.h>


// This is only used internally, to define a coordinate area
typedef struct 
{
  int leftX;
  int topY;
  int rightX;
  int bottomY;

} screenArea;


#define makeWindowScreenArea(windowP)                                \
   &((screenArea) { windowP->xCoord, windowP->yCoord,                \
                   (windowP->xCoord + (windowP->buffer.width - 1)),  \
                   (windowP->yCoord + (windowP->buffer.height - 1)) } )

#define makeComponentScreenArea(componentP)                            \
   &((screenArea) { (((kernelWindow *) componentP->window)->xCoord +   \
		     componentP->xCoord),                              \
		      (((kernelWindow *) componentP->window)->yCoord + \
		       componentP->yCoord),                            \
		      (((kernelWindow *) componentP->window)->xCoord + \
		       componentP->xCoord + (componentP->width - 1)),  \
		      (((kernelWindow *) componentP->window)->yCoord + \
		       componentP->yCoord + (componentP->height - 1)) } )

static int initialized = 0;
static color rootColor = {
  DEFAULT_ROOTCOLOR_BLUE,
  DEFAULT_ROOTCOLOR_GREEN,
  DEFAULT_ROOTCOLOR_RED
};
static int screenWidth = 0;
static int screenHeight = 0;
static kernelVariableList *settings;

static kernelAsciiFont *defaultFont = NULL;
static kernelWindow *rootWindow = NULL;
static kernelWindowComponent *consoleTextArea = NULL;
static kernelWindow *consoleWindow = NULL;
static int winManPID = 0;

static int titleBarHeight = DEFAULT_TITLEBAR_HEIGHT;
static int borderThickness = DEFAULT_BORDER_THICKNESS;
static int borderShadingIncrement = DEFAULT_SHADING_INCREMENT;

static image splashImage;
static int haveSplash = 0;

// Keeps the data for all the windows
static kernelWindow *windowList[WINDOW_MAXWINDOWS];
static kernelWindow *windowData = NULL;
static volatile int numberWindows = 0;
static kernelLock windowListLock;

static char loggedInUser[USER_MAX_NAMELENGTH];

static kernelWindow *focusWindow = NULL;


static inline int isPointInside(int xCoord, int yCoord,
				screenArea *area)
{
  // Return 1 if point 1 is inside area 2
  
  if ((xCoord < area->leftX) || (xCoord > area->rightX) ||
      (yCoord < area->topY) || (yCoord > area->bottomY))
    return (0);
  else
    // Yup, it's inside
    return (1);
}


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


static int addWindowComponent(kernelWindow *window,
			      kernelWindowComponent *component,
			      componentParameters *params)
{
  // Adds a component to a window

  int status = 0;

  // Make sure there's room for more
  if (window->numberComponents >= WINDOW_MAX_COMPONENTS)
    return (status == ERR_BOUNDS);

  // Add it to the window
  window->componentList[window->numberComponents++] = component;

  // Give the component a reference to the window
  component->window = (void *) window;
  component->processId = window->processId;

  // Copy the parameters into the component
  kernelMemCopy(params, (void *) &(component->parameters),
		sizeof(componentParameters));

  // Success
  return (status = 0);
}


static int removeWindowComponent(kernelWindow *window,
				 kernelWindowComponent *component)
{
  // Removes a component from a window

  int status = 0;
  int count = 0;

  for (count = 0; count < window->numberComponents; count ++)
    if (window->componentList[count] == component)
      {
	// Replace the component with the last one, if applicable
	window->numberComponents--;
	if ((window->numberComponents > 0) &&
	    (count < window->numberComponents))
	  window->componentList[count] =
	    window->componentList[window->numberComponents];
	break;
      }

  return (status = 0);
}


static void drawBorder(kernelWindow *window)
{
  // Draws the plain border around the window
  
  int greyColor = 0;
  color drawColor;
  int count;

  if (!window->hasBorder)
    return;

  // These are the starting points of the 'inner' border lines
  int leftX = borderThickness;
  int rightX = (window->buffer.width - borderThickness - 1);
  int topY = borderThickness;
  int bottomY = (window->buffer.height - borderThickness - 1);

  // The top and left
  for (count = borderThickness; count > 0; count --)
    {
      greyColor = (DEFAULT_GREY + (count * borderShadingIncrement));
      if (greyColor > 255)
	greyColor = 255;
      drawColor.red = greyColor;
      drawColor.green = greyColor;
      drawColor.blue = greyColor;

      // Top
      kernelGraphicDrawLine(&(window->buffer), &drawColor, draw_normal, 
			    (leftX - count), (topY - count),
			    (rightX + count), (topY - count));
      // Left
      kernelGraphicDrawLine(&(window->buffer), &drawColor, draw_normal,
			    (leftX - count), (topY - count), (leftX - count),
			    (bottomY + count));
    }

  // The bottom and right
  for (count = borderThickness; count > 0; count --)
    {
      greyColor = (DEFAULT_GREY - (count * borderShadingIncrement));
      if (greyColor < 0)
	greyColor = 0;
      drawColor.red = greyColor;
      drawColor.green = greyColor;
      drawColor.blue = greyColor;

      // Bottom
      kernelGraphicDrawLine(&(window->buffer), &drawColor, draw_normal,
			    (leftX - count), (bottomY + count),
			    (rightX + count), (bottomY + count));
      // Right
      kernelGraphicDrawLine(&(window->buffer), &drawColor, draw_normal,
			    (rightX + count), (topY - count),
			    (rightX + count), (bottomY + count));

      greyColor += borderShadingIncrement;
    }

  return;
}


static void addTitleBar(kernelWindow *window)
{
  // Draws the title bar atop the window

  unsigned width = window->buffer.width;
  componentParameters params;

  kernelWindowComponent *titleBarComponent =
    kernelWindowNewTitleBar(window, width, titleBarHeight);

  if (window->hasBorder)
    {
      titleBarComponent->xCoord += borderThickness;
      titleBarComponent->yCoord += borderThickness;
      titleBarComponent->width -= (borderThickness * 2);
    }

  // Standard parameters for a title bar
  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = (WINDOW_MAX_COMPONENTS - 1);
  params.gridHeight = 1;
  params.padLeft = 0;
  params.padRight = 0;
  params.padTop = 0;
  params.padBottom = 0;
  params.orientationX = orient_left;
  params.orientationY = orient_top;
  params.hasBorder = 0;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Add the title bar component to the list of window components
  addWindowComponent(window, titleBarComponent, &params);

  return;
}


static void removeTitleBar(kernelWindow *window)
{
  // Removes the title bar from atop the window

  kernelWindowComponent *titleBar = NULL;
  int count;

  for (count = 0; count < window->numberComponents; count ++)
    {
      // Before we destroy it we need to find subcomponents of the title
      // bar and destroy them as well.

      if (window->componentList[count]->type == windowTitleBarComponent)
	{
	  titleBar = window->componentList[count];
	  
	  // Get rid of the close button, if applicable, since no title bar
	  // implies this.  (The caller might already have done this, we
	  // suppose.)
	  if (window->hasCloseButton)
	    kernelWindowSetHasCloseButton(window, 0);
	  
	  // Remove it from the list of components
	  removeWindowComponent(window, titleBar);

	  // Destroy the title bar
	  kernelWindowDestroyComponent(titleBar);

	  break;
	}
    }

  return;
}


static int setBackgroundImage(kernelWindow *window, image *imageCopy)
{
  // Set the window's background image

  int status = 0;

  // If there was a previous background image, deallocate its memory
  if (window->backgroundImage.data != NULL)
    kernelFree(window->backgroundImage.data);
  
  // Copy the image information into the window's background image
  kernelMemCopy(imageCopy, (void *) &(window->backgroundImage),
		sizeof(image));

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
  int count1, count2;

  // The window needs to have been assigned a background image
  if (window->backgroundImage.data == NULL)
    return (status = ERR_NULLPARAMETER);

  // Adjust the dimensions of our drawing area if necessary to accommodate
  // other things outside the client area
  if (window->hasBorder)
    {
      clientAreaX += borderThickness;
      clientAreaY += borderThickness;
      clientAreaWidth -= (borderThickness * 2);
      clientAreaHeight -= (borderThickness * 2);
    }
  if (window->hasTitleBar)
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
	     &(window->backgroundImage),
	     ((clientAreaWidth - window->backgroundImage.width) / 2),
	     ((clientAreaHeight - window->backgroundImage.height) / 2),
	      0, 0, 0, 0);
      window->backgroundTiled = 0;
    }
  else
    {
      // Tile the image into the window's client area
      for (count1 = clientAreaY; count1 < clientAreaHeight;
	   count1 += window->backgroundImage.height)
	for (count2 = clientAreaX; count2 < clientAreaWidth;
	     count2 += window->backgroundImage.width)
	  status = kernelGraphicDrawImage(&(window->buffer), (image *)
				  &(window->backgroundImage), count2, count1,
				  0, 0, 0, 0);
      window->backgroundTiled = 1;
    }

  return (status);
}


static void drawClientArea(kernelWindow *window)
{
  // Draws whatever is inside the window

  int clientAreaX = 0;
  int clientAreaY = 0;
  unsigned clientAreaWidth = window->buffer.width;
  unsigned clientAreaHeight = window->buffer.height;
  int count;

  // Adjust the dimensions of our drawing area if necessary to accommodate
  // other things outside the client area
  if (window->hasBorder)
    {
      clientAreaX += borderThickness;
      clientAreaY += borderThickness;
      clientAreaWidth -= (borderThickness * 2);
      clientAreaHeight -= (borderThickness * 2);
    }
  if (window->hasTitleBar)
    {
      clientAreaY += titleBarHeight;
      clientAreaHeight -= titleBarHeight;
    }

  // Draw a blank background
  kernelGraphicDrawRect(&(window->buffer),
			(color *) &(window->background), draw_normal,
			clientAreaX, clientAreaY, clientAreaWidth,
			clientAreaHeight, 0, 1);

  // If the window has a background image, draw it
  if (window->backgroundImage.data != NULL)
    tileBackgroundImage(window);

  // Loop through all the window components and draw them
  for (count = 0; count < window->numberComponents; count ++)
    if (window->componentList[count]->visible)
      window->componentList[count]
	->draw((void *) window->componentList[count]);

  return;
}


static void renderVisiblePortions(kernelWindow *window, screenArea *bufferClip)
{
  // Take the window supplied, and render the portions of the supplied clip
  // which are visible (i.e. not covered by other windows)

  //int status = 0;
  int numCoveredAreas = 0;
  screenArea coveredAreas[64];
  int numVisibleAreas = 0;
  screenArea visibleAreas[64];
  int count1, count2;

  visibleAreas[0].leftX = (window->xCoord + bufferClip->leftX);
  visibleAreas[0].topY =  (window->yCoord + bufferClip->topY);
  visibleAreas[0].rightX =  (window->xCoord + bufferClip->rightX);
  visibleAreas[0].bottomY =  (window->yCoord + bufferClip->bottomY);
  numVisibleAreas++;

  /*
  // Lock the window list
  status = kernelLockGet(&windowListLock);
  if (status < 0)
    return;
  */
 
  // Loop through the window list.  Any window which intersects this area
  // and is at a higher level will reduce the visible area
  for (count1 = 0; count1 < numberWindows; count1 ++)
    if ((windowList[count1] != window) && windowList[count1]->visible &&
	(windowList[count1]->level < window->level))
      {
	// The current window list item may be covering the supplied
	// window.  Find out if it totally covers it, in which case we
	// are finished
	if (isAreaInside(&(visibleAreas[0]),
			 makeWindowScreenArea(windowList[count1])))
	  {
	    // Done
	    //kernelLockRelease(&windowListLock);
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
  
  //kernelLockRelease(&windowListLock);

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


static int drawWindow(kernelWindow *window)
{
  // Draws the specified window from scratch

  int status = 0;

  drawClientArea(window);
  if (window->hasBorder)
    drawBorder(window);

  // Only render the visible portions
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
  kernelWindowComponent *component = NULL;
  unsigned xOffset, yOffset;
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
  if (window->backgroundImage.data != NULL)
    {
      if (window->backgroundTiled)
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
					 count1, count2, xOffset, yOffset,
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
				 &(window->backgroundImage), xCoord, yCoord,
				 (count1 + xCoord), (count2 + yCoord),
				 width, height);
	}
    }

  // Loop through all the regular window components that fall (partially)
  // within this space and draw them
  for (count1 = 0; count1 < window->numberComponents; count1 ++)
    {
      component = window->componentList[count1];

      if (component->visible)
	{
	  // Is it within our area?
	  if (doAreasIntersect(&((screenArea)
	  { xCoord, yCoord, (xCoord + width - 1), (yCoord + height - 1)} ),
			       &((screenArea)
	       { component->xCoord, component->yCoord,
		   (component->xCoord + width - 1),
		   (component->yCoord + height - 1) } )))
	    component->draw((void *) component);
	}
    }

  // Update visible portions of the area on the screen
  renderVisiblePortions(window, &((screenArea)
  { xCoord, yCoord, (xCoord + width - 1), (yCoord + height - 1) } ));

  return (status = 0);
}


static int updateBuffer(kernelWindow *window)
{
  // A component is trying to tell us that it has updated itself and
  // would like the window to be redrawn

  int status = 0;
  kernelGraphicBuffer *buffer = NULL;
  unsigned clipX, clipY;
  unsigned width, height;
  int count;

  // Check parameters
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  if (!window->visible)
    // It's not currently on the screen
    return (status = 0);

  buffer = &(window->buffer);

  // Loop through all of the scheduled updates
  for (count = 0; count < buffer->numUpdates; count ++)
    {
      buffer->numUpdates--;

      clipX = buffer->updates[window->buffer.numUpdates][0];
      clipY = buffer->updates[window->buffer.numUpdates][1];
      width = buffer->updates[window->buffer.numUpdates][2];
      height = buffer->updates[window->buffer.numUpdates][3];

      // Render the parts of this window's buffer are currently visible.
      // We only want to render those parts
      if (window->level != 0)
	renderVisiblePortions(window, &((screenArea) { clipX, clipY,
			 (clipX + (width - 1)), (clipY + (height - 1)) }));
      else
	// Render the area directly
	status =
	  kernelGraphicRenderBuffer(buffer, window->xCoord, window->yCoord,
				    clipX, clipY, width, height);

      // Redraw the mouse?
      kernelMouseDraw();

      if (status < 0)
	return (status);
    }

  return (status = 0);
}


static int getConfiguration(void)
{
  // Tries to get the window manager settings from the configuration file

  int status = 0;
  char value[128];
  kernelVariableList *newSettings = NULL;

  newSettings = kernelConfigurationReader(DEFAULT_WINDOWMANAGER_CONFIG);

  // Do we have a place to store variables?
  if (newSettings == NULL)
    {
      // Argh.  No file?  Create a reasonable, empty list for us to use
      newSettings = kernelVariableListCreate(255, 1024, "configuration data");

      if (newSettings == NULL)
	{
	  kernelError(kernel_warn, "Unable to create a variable list for "
		      "the window manager");
	  return (status = ERR_MEMORY);
	}
    }

  if (settings != NULL)
    kernelFree(settings);

  settings = newSettings;

  if (!kernelVariableListGet(settings, "splash.image", value, 128) &&
      strncmp(value, "", 128))
    {
      status = kernelImageLoadBmp(value, &splashImage);
    
      if (status == 0)
	haveSplash = 1;
      else
	haveSplash = 0;
    }

  if (!kernelVariableListGet(settings, "backgroundcolor.red", value, 128) &&
      strncmp(value, "", 128))
    rootColor.red = atoi(value);
  else
    {
      // Set the value in our variable list so it can be written back out
      // later to the config file
      sprintf(value, "%d", rootColor.red);
      kernelVariableListSet(settings, "backgroundcolor.red", value);
    }			    

  if (!kernelVariableListGet(settings, "backgroundcolor.green", value, 128) &&
      strncmp(value, "", 128))
    rootColor.green = atoi(value);
  else
    {
      // Set the value in our variable list so it can be written back out
      // later to the config file
      sprintf(value, "%d", rootColor.green);
      kernelVariableListSet(settings, "backgroundcolor.green", value);
    }			    

  if (!kernelVariableListGet(settings, "backgroundcolor.blue", value, 128) &&
      strncmp(value, "", 128))
    rootColor.blue = atoi(value);
  else
    {
      // Set the value in our variable list so it can be written back out
      // later to the config file
      sprintf(value, "%d", rootColor.blue);
      kernelVariableListSet(settings, "backgroundcolor.blue", value);
    }			    

  // Other variables and whatnot are dealt with elsewhere.

  return (status = 0);
}


static kernelWindow *makeStartWindow(void)
{
  char *title = "Starting the window manager...";
  unsigned windowWidth = 0;
  unsigned windowHeight = 0;
  int windowX, windowY;
  kernelWindow *window = NULL;
  kernelWindowComponent *imageComponent = NULL;
  componentParameters params;

  // Create the window with arbitrary size and location
  window = kernelWindowManagerNewWindow(KERNELPROCID, title, 0, 0, 100, 100);
  if (window == NULL)
    return (window);

  // No close button and not movable
  window->hasCloseButton = 0;
  window->movable = 0;

  if (!haveSplash)
    // Try to load the default splash image to use when starting/restarting
    if (kernelImageLoadBmp(DEFAULT_WINDOWMANAGER_SPLASH, &splashImage) == 0)
      haveSplash = 1;

  if (haveSplash)
    {
      // Get a new image component
      imageComponent = kernelWindowNewImage(window, &splashImage);

      params.gridX = 0;
      params.gridY = 0;
      params.gridWidth = 1;
      params.gridHeight = 1;
      params.padLeft = 0;
      params.padRight = 0;
      params.padTop = 0;
      params.padBottom = 0;
      params.orientationX = orient_center;
      params.orientationY = orient_middle;
      params.hasBorder = 0;
      params.useDefaultForeground = 1;
      params.useDefaultBackground = 1;

      addWindowComponent(window, imageComponent, &params);
    }

  kernelWindowLayout(window);
  kernelWindowAutoSize(window);
  kernelWindowGetSize(window, &windowWidth, &windowHeight);
  windowX = ((screenWidth - windowWidth) / 2);
  windowY = ((screenHeight - windowHeight) / 2);
  kernelWindowSetLocation(window, windowX, windowY);

  // Done.  We don't set it visible for now.
  return (window);
}


static kernelWindow *makeRootWindow(void)
{
  // Make a graphic buffer for the whole background

  int status = 0;
  char propertyName[128];
  char propertyValue[128];
  char iconList[128];
  char *iconName = NULL;
  char iconLabel[128];
  char iconImage[128];
  char iconCommand[128];
  image tmpImage;
  kernelWindowComponent *iconComponent = NULL;
  componentParameters params;
  int maxIconWidth = 0;
  int count, iconCount;

  // Get a new window object for the back
  rootWindow = kernelWindowManagerNewWindow(KERNELPROCID, "root window",
					    0, 0, screenWidth, screenHeight);
  if (rootWindow == NULL)
    return (rootWindow);

  // The window will have no border, title bar or close button and is not
  // movable
  rootWindow->hasBorder = 0;
  rootWindow->movable = 0;
  removeTitleBar(rootWindow);
  rootWindow->hasCloseButton = 0;
  rootWindow->hasTitleBar = 0;

  // The window is always at the bottom level
  rootWindow->level = WINDOW_MAXWINDOWS;

  // Set our background color preference
  rootWindow->background.red = rootColor.red;
  rootWindow->background.green = rootColor.green;
  rootWindow->background.blue = rootColor.blue;

  // Try to load the background image

  if (!kernelVariableListGet(settings, "background.image", propertyValue,
			     128) && strncmp(propertyValue, "", 128))
    {
      status = kernelImageLoadBmp(propertyValue, &tmpImage);

      if (status == 0)
	// Put the background image into our window.
	setBackgroundImage(rootWindow, &tmpImage);
      else
	kernelError(kernel_error, "Error loading background image %s",
		    propertyValue);

      if (tmpImage.data != NULL)
	// Release the image memory
	kernelMemoryRelease(tmpImage.data);
    }

  // Try to load icons

  // Ask for the list of icon names
  if (!kernelVariableListGet(settings, "icons", iconList, 128) &&
      strncmp(iconList, "", 128))
    {
      // Comma-separated list of icon names.
      iconName = iconList;

      iconCount = 0;

      while(1)
	{
	  count = 0;
	  if (iconName[0] == '\0')
	    break;

	  while ((iconName[count] != '\0') && (iconName[count] != ','))
	    count++;

	  // Get the rest of the recognized properties for this icon.
	  strcpy(propertyName, "icon.");
	  strncat(propertyName, iconName, count);
	  strcat(propertyName, ".label");
	  kernelVariableListGet(settings, propertyName, iconLabel, 128);
	  strcpy(propertyName, "icon.");
	  strncat(propertyName, iconName, count);
	  strcat(propertyName, ".image");
	  kernelVariableListGet(settings, propertyName, iconImage, 128);
	  strcpy(propertyName, "icon.");
	  strncat(propertyName, iconName, count);
	  strcat(propertyName, ".command");
	  kernelVariableListGet(settings, propertyName, iconCommand, 128);

	  status = kernelImageLoadBmp(iconImage, &tmpImage);

	  if (status == 0)
	    {
	      iconComponent =
		kernelWindowNewIcon(rootWindow, &tmpImage, iconLabel,
				    iconCommand);
	      
	      params.gridX = 0;
	      params.gridY = iconCount++;
	      params.gridWidth = 1;
	      params.gridHeight = 1;
	      params.padLeft = 5;
	      params.padRight = 5;
	      params.padTop = 5;
	      params.padBottom = 5;
	      params.orientationX = orient_left;
	      params.orientationY = orient_top;
	      params.hasBorder = 0;
	      params.useDefaultForeground = 1;
	      params.useDefaultBackground = 1;

	      addWindowComponent(rootWindow, iconComponent, &params);
	    }
	  else
	    kernelError(kernel_error, "Error loading icon image");

	  // Release the image memory
	  kernelMemoryRelease(tmpImage.data);

	  // Move to the next icon name, if applicable
	  if (iconName[count] == ',')
	    iconName += (count + 1);
	  else
	    break;
	}
    }

  // Do the basic layout
  kernelWindowLayout(rootWindow);
  
  // Line up the icons on a grid

  // Find the widest one
  for (count = 0; count < rootWindow->numberComponents; count ++)
    {
      iconComponent = rootWindow->componentList[count];

      if ((iconComponent->type == windowIconComponent) &&
	  (iconComponent->width > maxIconWidth))
	maxIconWidth = iconComponent->width;
    }

  // Move all the others over so that their X coordinate is an average
  for (count = 0; count < rootWindow->numberComponents; count ++)
    {
      iconComponent = rootWindow->componentList[count];

      if (iconComponent->type == windowIconComponent)
	{
	  if (iconComponent->width == maxIconWidth)
	    continue;

	  iconComponent->xCoord += ((maxIconWidth - iconComponent->width) / 2);

	  if (iconComponent->move != NULL)
	    iconComponent->move((void *) iconComponent, iconComponent->xCoord,
				iconComponent->yCoord);
	}
    }

  // Done.  We don't set it visible for now.
  return (rootWindow);
}


static int makeConsoleWindow(void)
{
  // Create the temporary console window

  int status = 0;
  componentParameters params;
  kernelTextArea *oldArea = NULL;
  kernelTextArea *newArea = NULL;
  unsigned char *lineAddress = NULL;
  unsigned char charAt;
  unsigned char lineBuffer[1024];
  int lineBufferCount = 0;
  int rowCount, columnCount;

  consoleWindow =
    kernelWindowManagerNewWindow(KERNELPROCID, "Temp Console Window",
				 0, 0, 1, 1);
  if (consoleWindow == NULL)
    return (status = ERR_NOCREATE);

  consoleTextArea =
    kernelWindowNewTextArea(consoleWindow, 80, 50, defaultFont);

  if (consoleTextArea != NULL)
    {
      kernelMemClear(&params, sizeof(componentParameters));
      params.gridWidth = 1;
      params.gridHeight = 1;
      params.orientationX = orient_center;
      params.orientationY = orient_middle;
      params.useDefaultForeground = 1;
      params.useDefaultBackground = 1;
      kernelWindowAddClientComponent(consoleWindow, consoleTextArea, &params);
      
      oldArea = kernelTextGetConsoleOutput()->textArea;
      newArea = (kernelTextArea *) consoleTextArea->data;

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
	    (oldArea->data + (rowCount * oldArea->columns));
	  lineBufferCount = 0;
	  
	  for (columnCount = 0; (columnCount < oldArea->columns) &&
		 (columnCount < newArea->columns); columnCount ++)
	    {
	      charAt = lineAddress[columnCount];
	      lineBuffer[lineBufferCount++] = charAt;
	      if (charAt == '\n')
		break;
	    }
	  
	  // Make sure there's a NULL
	  lineBuffer[lineBufferCount] = '\0';
	  
	  if (lineBufferCount > 0)
	    // Print the line to the new text area
	    kernelTextStreamPrint(newArea->outputStream, lineBuffer);
	}
    }
  else
    kernelError(kernel_warn, "Unable to switch text areas to console window");

  kernelWindowLayout(consoleWindow);
  kernelWindowAutoSize(consoleWindow);

  return (status = 0);
}


static int getWindowGraphicBuffer(kernelWindow *window, unsigned width,
				  unsigned height)
{
  // Allocate and attach memory to a kernelWindow for its kernelGraphicBuffer

  int status = 0;
  unsigned bufferBytes = 0;

  window->buffer.width = width;
  window->buffer.height = height;
  window->buffer.numUpdates = 0;
  
  // Get the number of bytes of memory we need to reserve for this window's
  // kernelGraphicBuffer, depending on the size of the window
  bufferBytes = kernelGraphicCalculateAreaBytes(width, height);
  
  // Get some memory for it
  window->buffer.data = kernelMalloc(bufferBytes);
  if (window->buffer.data == NULL)
    return (status = ERR_MEMORY);

  return (status = 0);
}


static void changeFocus(kernelWindow *window)
{
  // Gets called when a window acquires the focus

  //int status = 0;
  kernelTextInputStream *currentInput = NULL;
  kernelTextOutputStream *currentOutput = NULL;
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
      // status = kernelLockGet(&windowListLock);
      // if (status < 0)
      //   return;

      // Decrement the levels of all windows that used to be above us
      for (count = 0; count < numberWindows; count ++)
	if ((windowList[count] != window) &&
	    (windowList[count]->level <= window->level))
	  windowList[count]->level++;
  
      //kernelLockRelease(&windowListLock);

      if (focusWindow != NULL)
	{
	  // Remove the focus from the previously focussed window
	  
	  // This is not the focus window any more.  We do this so that the
	  // title bar 'draw' routine won't make it look focussed
	  focusWindow->hasFocus = 0;
	  
	  // Redraw the title bar of the window
	  if (focusWindow->hasTitleBar)
	    for (count = 0; count < focusWindow->numberComponents; count ++)
	      if (focusWindow->componentList[count]->type ==
		  windowTitleBarComponent)
		{
		  focusWindow->componentList[count]
		    ->draw((void *) focusWindow->componentList[count]);

		  // Redraw the window's title bar area only
		  kernelWindowManagerUpdateBuffer(&(focusWindow->buffer),
			  borderThickness, borderThickness,
			  (focusWindow->buffer.width - (borderThickness * 2)),
			  titleBarHeight);
		  break;
		}
	}
    }

  // This window becomes topmost
  window->level = 0;

  // Mark it as focussed
  window->hasFocus = 1;
  focusWindow = window;

  // If the window has any sort of text area or field inside it, set the
  // console input stream to that process.  Need to do something better
  // later to remember the 'focussed' component.
  for (count = 0; count < window->numberComponents; count ++)
    {
      if ((window->componentList[count]->type == windowTextAreaComponent) ||
	  (window->componentList[count]->type == windowTextFieldComponent))
	{
	  currentInput = (kernelTextInputStream *)
	    ((kernelWindowTextArea *)
	     window->componentList[count]->data)->inputStream;
	  kernelTextSetCurrentInput(currentInput);
	  currentOutput = (kernelTextOutputStream *)
	    ((kernelWindowTextArea *)
	     window->componentList[count]->data)->outputStream;
	  window->focusComponent = window->componentList[count];
	  kernelTextSetCurrentOutput(currentOutput);
	  break;
	}
    }
  
  // Draw the title bar as focussed
  if (window->hasTitleBar)
    for (count = 0; count < window->numberComponents; count ++)
      if (window->componentList[count]->type == windowTitleBarComponent)
	{
	  window->componentList[count]
	    ->draw((void *) window->componentList[count]);
	  break;
	}

  // Redraw the whole window, since all of it is now visible (otherwise we
  // would call drawVisiblePortions()).
  kernelWindowManagerUpdateBuffer(&(window->buffer), 0, 0,
				  window->buffer.width, window->buffer.height);
  return;
}


static void componentDrawBorder(void *componentData)
{
  // Draw a simple little border around the supplied component
  
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelGraphicDrawRect(&(((kernelWindow *) component->window)->buffer),
			&((color) { 100, 100, 100 }), draw_normal,
			(component->xCoord - 2), (component->yCoord - 2),
			(component->width + 4), (component->height + 4), 1, 0);
  return;
}


static void windowManagerThread(void)
{
  // This thread runs as the 'window manager' to watch for window events
  // on 'system' GUI components such as window close buttons.

  kernelWindow *window = NULL;
  kernelWindowComponent *component = NULL;
  windowEvent event;
  int processId = 0;
  int winCount, compCount;

  while(1)
    {
      // Loop through all of the components, looking for 'system' ones
      for (winCount = 0; winCount < numberWindows; winCount ++)
	{
	  window = windowList[winCount];
	  processId = window->processId;
 
	  // Loop through the components
	  for (compCount = 0; compCount < window->numberComponents;
	       compCount ++)
	    {
	      component = window->componentList[compCount];
 
	      if (component->processId == KERNELPROCID)
		{
		  // Any events pending?
		  if ((kernelWindowEventStreamRead((windowEventStream *)
			   &(component->events), &event) > 0) &&
		      // Any handler for the event?
		      component->eventHandler)
		    {
		      component->eventHandler((objectKey) component, &event);
 
		      // Was the window closed?
		      if (!kernelMultitaskerProcessIsAlive(processId))
			{
			  // Restart the loop
			  winCount = -1;
			  break;
			}
		    }
		}
	    }
	}

      // Done
      kernelMultitaskerYield();
    }
}


static inline kernelWindow *findTopmostWindow(void)
{
  int topmostLevel = MAXINT;
  kernelWindow *topmostWindow = NULL;
  int count;

  // Find the topmost window
  for (count = 0; count < numberWindows; count ++)
    {
      if (windowList[count]->visible &&
	  (windowList[count]->level < topmostLevel))
	{
	  topmostWindow = windowList[count];
	  topmostLevel = windowList[count]->level;
	}
    }

  return (topmostWindow);
}


static inline kernelWindowComponent *findTopmostComponent(kernelWindow *window,
						  int xCoord, int yCoord)
{
  int topmostLevel = MAXINT;
  kernelWindowComponent *topmostComponent = NULL;
  int count;

  // Find the window's topmost component at this location
  for (count = 0; count < window->numberComponents; count ++)
    if ((window->componentList[count]->level < topmostLevel) &&
	isPointInside(xCoord, yCoord,
		      makeComponentScreenArea(window->componentList[count])))
      {
	topmostComponent = window->componentList[count];
	topmostLevel = window->componentList[count]->level;
      }

  return (topmostComponent);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelWindowManagerInitialize(void)
{
  // Called during kernel initialization

  int status = 0;
  int count;

  // Don't bother if graphics are not enabled
  if (!kernelGraphicsAreEnabled())
    {
      kernelError(kernel_error, "The window manager can not run without "
		  "graphics enabled");
      return (status = ERR_NOTINITIALIZED);
    }

  // Allocate memory to hold all the window information
  windowData = kernelMalloc((sizeof(kernelWindow) * WINDOW_MAXWINDOWS));
  if (windowData == NULL)
    {
      // Eek
      kernelError(kernel_error, "Unable to get window data memory for the "
		  "window manager");
      return (status = ERR_MEMORY);
    }

  // Set up the array of pointers to memory within the window data that
  // will be used for the window structures
  for (count = 0; count < WINDOW_MAXWINDOWS; count ++)
    windowList[count] = &(windowData[count]);

  numberWindows = 0;

  // Initialize the lock for the window list
  kernelMemClear((void *) &windowListLock, sizeof(kernelLock));

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

  kernelWindowManagerStart();

  // Switch to the 'default' mouse pointer
  kernelMouseSwitchPointer("default");

  return (status = 0);
}


int kernelWindowManagerStart(void)
{
  // This does all of the startup stuff for the window manager, and should
  // be re-callable (as in, it should be able to restart the window manager).

  int status = 0;
  kernelTextOutputStream *output = NULL;
  kernelWindow *startWindow = NULL;
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

  // Read the config file
  getConfiguration();

  // Clear the screen with our default background color
  kernelGraphicClearScreen(&rootColor);

  // Set the temporary text area to the current background color, for neatness'
  // sake if there are any error messages before we create the console window
  output = kernelMultitaskerGetTextOutput();
  output->textArea->background.red = rootColor.red;
  output->textArea->background.green = rootColor.green;
  output->textArea->background.blue = rootColor.blue;

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

  // Draw the start window, visible
  startWindow = makeStartWindow();
  kernelWindowSetVisible(startWindow, 1);

  // Draw the console and root windows, but don't make them visible
  makeConsoleWindow();
  makeRootWindow();

  // Get rid of the start window
  kernelWindowManagerDestroyWindow(startWindow);

  // Mouse housekeeping.
  kernelMouseDraw();

  // Done
  return (status = 0);
}


int kernelWindowManagerLogin(const char *userName, const char *passwd)
{
  // This gets called after the user has logged in.

  // Make sure we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  strncpy(loggedInUser, userName, USER_MAX_NAMELENGTH);

  // Make the root window visible
  kernelWindowSetVisible(rootWindow, 1);
  kernelMouseDraw();

  // Spawn the window manager thread
  winManPID =
    kernelMultitaskerSpawnKernelThread(windowManagerThread, "window manager",
				       0, NULL);
  return (winManPID);
}


const char *kernelWindowManagerGetUser(void)
{
  // Returns a pointer to the name of the logged in user
  return (loggedInUser);
}


int kernelWindowManagerLogout(void)
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

  /*
  // Lock the window list
  status = kernelLockGet(&windowListLock);
  if (status < 0)
    return (status);
  */

  for (count = 0; count < numberWindows; count ++)
    {
      if ((windowList[count] != rootWindow) &&
	  (windowList[count] != consoleWindow))
	{
	  window = windowList[count];

	  // Can't keep the lock while calling the functions below
	  //kernelLockRelease(&windowListLock);

	  // Kill the process that owns the window
	  kernelMultitaskerKillProcess(windowList[count]->processId, 0);

	  // Destroy the window
	  kernelWindowManagerDestroyWindow(windowList[count]);

	  // Need to restart the loop, since the window list will have changed
	  count = 0;

	  /*
	  // Lock the window list again
	  status = kernelLockGet(&windowListLock);
	  if (status < 0)
	    return (status);
	  */
	}
    }

  //kernelLockRelease(&windowListLock);
 
  strcpy(loggedInUser, "");

  // Hide the root window
  kernelWindowSetVisible(rootWindow, 0);

  kernelMouseDraw();
  return (status = 0);
}


kernelWindow *kernelWindowManagerNewWindow(int processId, const char *title,
		   int xCoord, int yCoord, unsigned width, unsigned height)
{
  // Creates a new window using the supplied values.  Not visible by default.

  int status = 0;
  kernelWindow *newWindow = NULL;
  int bottomLevel = 0;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (newWindow = NULL);

  // Check params
  if (title == NULL)
    return (newWindow = NULL);

  /*
  // Lock the window list
  status = kernelLockGet(&windowListLock);
  if (status < 0)
    return (newWindow = NULL);
  */

  // Get some memory for window data
  newWindow = windowList[numberWindows++];

  // Make sure it's all empty
  kernelMemClear((void *) newWindow, sizeof(kernelWindow));

  // Set the process Id
  newWindow->processId = processId;

  // The title
  strncpy((char *) newWindow->title, title, WINDOW_MAX_TITLE_LENGTH);
  newWindow->title[WINDOW_MAX_TITLE_LENGTH - 1] = '\0';

  // Set the location, width, and height
  newWindow->xCoord = xCoord;
  newWindow->yCoord = yCoord;

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
  newWindow->hasFocus = 0;

  // Not visible until someone tells us to make it visible
  newWindow->visible = 0;

  // By default windows are movable, and have borders, title bars, and close
  // buttons, and no components.  No background image by default.
  newWindow->movable = 1;
  newWindow->hasTitleBar = 1;
  newWindow->hasCloseButton = 1;
  newWindow->hasBorder = 1;
  newWindow->backgroundImage.data = NULL;
  newWindow->numberComponents = 0;

  // Get the window's graphic buffer all set up
  status = getWindowGraphicBuffer(newWindow, width, height);
  if (status < 0)
    {
      // Eek!  No new window for you!
      numberWindows--;
      kernelError(kernel_warn, "Error getting memory for window graphic "
		  "buffer");
      //kernelLockRelease(&windowListLock);
      return (newWindow = NULL);
    }

  // Set the window's background color to the default
  newWindow->background.red = DEFAULT_GREY;
  newWindow->background.green = DEFAULT_GREY;
  newWindow->background.blue = DEFAULT_GREY;

  // Add an event stream for the window
  status =
    kernelWindowEventStreamNew((windowEventStream *) &(newWindow->events));
  if (status < 0)
    return (newWindow = NULL);

  // Add the title bar component
  addTitleBar(newWindow);

  // Set up the functions
  newWindow->draw = (int (*)(void *)) &drawWindow;
  newWindow->drawClip = (int (*) (void *, int, int, int, int)) &drawWindowClip;

  //kernelLockRelease(&windowListLock);

  // Return the window
  return (newWindow);
}


kernelWindow *kernelWindowManagerNewDialog(kernelWindow *parentWindow,
			   const char *title, int xCoord, int yCoord,
			   unsigned width, unsigned height)
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
  newDialog = kernelWindowManagerNewWindow(parentWindow->processId, title,
					   xCoord, yCoord, width, height);
  if (newDialog == NULL)
    return (newDialog);

  // Attach the dialog window to the parent window and vice-versa
  parentWindow->dialogWindow = (void *) newDialog;
  newDialog->parentWindow = (void *) parentWindow;

  // Return the dialog window
  return (newDialog);
}


int kernelWindowManagerDestroyWindow(kernelWindow *window)
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
  if (window->dialogWindow != NULL)
    {
      status = kernelWindowManagerDestroyWindow((kernelWindow *)
						window->dialogWindow);
      if (status < 0)
	return (status);
    }

  // If this window *is* a dialog window, dissociate it from its parent
  if (window->parentWindow != NULL)
    ((kernelWindow *) window->parentWindow)->dialogWindow = NULL;

  /*
  // Lock the window list
  status = kernelLockGet(&windowListLock);
  if (status < 0)
    return (status);
  */

  // First try to find the window's position in the list
  for (count = 0; count < numberWindows; count ++)
    if (windowList[count] == window)
      {
	listPosition = count;
	break;
      }

  //kernelLockRelease(&windowListLock);

  if (windowList[listPosition] != window)
    // No such window (any more)
    return (status = ERR_NOSUCHENTRY);

  /*
  // Lock the window list
  status = kernelLockGet(&windowListLock);
  if (status < 0)
    return (status);
  */

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

  //kernelLockRelease(&windowListLock);

  // Not visible anymore
  kernelWindowSetVisible(window, 0);

  // Call the 'destroy' function for all the window's components
  for (count = 0; count < window->numberComponents; count ++)
    kernelWindowDestroyComponent(window->componentList[count]);

  // If the window has a background image, free it
  if (window->backgroundImage.data != NULL)
    {
      status = kernelFree(window->backgroundImage.data);
      if (status < 0)
	kernelError(kernel_warn, "Unable to deallocate background image data");
    }

  // Free the window's graphic buffer
  status = kernelFree(window->buffer.data);
  if (status < 0)
    kernelError(kernel_warn, "Error releasing window graphic buffer memory");

  // Redraw the mouse
  kernelMouseDraw();

  return (status);
}


int kernelWindowManagerUpdateBuffer(kernelGraphicBuffer *buffer,
				    int clipX, int clipY,
				    unsigned width, unsigned height)
{
  // A component is trying to tell us that it has updated itself and
  // would like the window to be redrawn.

  int status = 0;
  kernelWindow *window = NULL;
  screenArea area1, area2;
  //kernelProcessState tmp;
  int count;

  // Check parameters
  if (buffer == NULL)
    return (status = ERR_NULLPARAMETER);

  // If this update covers the whole buffer, remove the others
  if ((clipX == 0) && (clipY == 0) &&
      (width == buffer->width) && (height == buffer->height))
    buffer->numUpdates = 0;

  else
    // If this update is already covered by another pending update,
    // skip it.  Similarly, if this update covers another, skip the
    // other one
    for (count = 0; count < buffer->numUpdates; count ++)
      {
	area1 = (screenArea) { clipX, clipY, (clipX + width),
			       (clipY + height) };
	
	area2 = (screenArea)
	{ buffer->updates[count][0], buffer->updates[count][1],
	  (buffer->updates[count][0] + buffer->updates[count][3]),
	  (buffer->updates[count][1] + buffer->updates[count][2]) };
	
	if (isAreaInside(&area1, &area2))
	  {
	    // Skip it, our current update is 'inside' this other one
	    return (status = 0);
	  }

	else if (isAreaInside(&area2, &area1))
	  {
	    // Replace the other one with this one
	    buffer->updates[count][0] = clipX;
	    buffer->updates[count][1] = clipY;
	    buffer->updates[count][2] = width;
	    buffer->updates[count][3] = height;
	    return (status = 0);
	  }
      }

  // Add this update to the list
  buffer->updates[buffer->numUpdates][0] = clipX;
  buffer->updates[buffer->numUpdates][1] = clipY;
  buffer->updates[buffer->numUpdates][2] = width;
  buffer->updates[buffer->numUpdates][3] = height;

  buffer->numUpdates++;

  // First try to find the window

  for (count = 0; count < numberWindows; count ++)
    if (&(windowList[count]->buffer) == buffer)
      {
	window = windowList[count];
	break;
      }

  if (window == NULL)
      return (status = ERR_NOSUCHENTRY);

  status = updateBuffer(window);

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
  if (window->visible)
    drawWindow(window);

  // Return success
  return (status = 0);
}


int kernelWindowGetSize(kernelWindow *window, unsigned *width,
			unsigned *height)
{
  // Returns the size of the supplied window.

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((window == NULL)  || (width == NULL) || (height == NULL))
    return (status = ERR_NULLPARAMETER);

  *width = window->buffer.width;
  *height = window->buffer.height;

  return (status = 0);
}


int kernelWindowSetSize(kernelWindow *window, unsigned width, unsigned height)
{
  // Sets the size of a window

  int status = 0;
  void *oldBufferData = NULL;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  // Save the old graphic buffer data just in case
  oldBufferData = window->buffer.data;

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

  if (window->hasTitleBar)
    {
      // Find the title bar component, and tell it to resize
      for (count = 0; count < window->numberComponents; count ++)
	{
	  if (window->componentList[count]->type == windowTitleBarComponent)
	    {
	      if (window->componentList[count]->resize != NULL)
		window->componentList[count]
		  ->resize((void *) window->componentList[count], (width -
			   (window->hasBorder? (borderThickness * 2) : 0)),
			   titleBarHeight);
	      break;
	    }
	}
    }

  // If the window is visible, draw it
  if (window->visible)
    drawWindow(window);

  // Return success
  return (status = 0);
}


int kernelWindowAutoSize(kernelWindow *window)
{
  // This will automatically set the size of a window based on the sizes
  // and locations of the components therein.
  
  int status = 0;
  unsigned newWidth = 0;
  unsigned newHeight = 0;
  unsigned componentWidth, componentHeight;
  kernelWindowComponent *titleBar = NULL;
  kernelWindowComponent *component = NULL;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  // Loop through all the components in the window besides the title bar
  // and any of its buttons.  If the window can be smaller or larger and
  // accommodate its components better, we resize the window.

  if (window->hasTitleBar)
    for (count = 0; count < window->numberComponents; count ++)
      if (window->componentList[count]->type == windowTitleBarComponent)
	{
	  titleBar = window->componentList[count];
	  break;
	}

  for (count = 0; count < window->numberComponents; count ++)
    {
      component = window->componentList[count];

      if (window->hasTitleBar && (titleBar != NULL))
	if ((component == titleBar) ||
	    (window->hasCloseButton && 
	    (component == ((kernelWindowTitleBar *) titleBar->data)
	     ->closeButton)))
	  continue;

      componentWidth = (component->xCoord + component->width +
			component->parameters.padRight);

      if (componentWidth > newWidth)
	newWidth = componentWidth;

      componentHeight = (component->yCoord + component->height +
			 component->parameters.padBottom);

      if (componentHeight > newHeight)
	newHeight = componentHeight;
    }

  // Adjust for title bars, borders, etc.
  if (window->hasBorder)
    {
      newWidth += borderThickness;
      newHeight += borderThickness;
    }

  // Resize it.
  status = kernelWindowSetSize(window, newWidth, newHeight);

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
  if (window->visible)
    {
      window->visible = 0;
      kernelWindowManagerRedrawArea(window->xCoord, window->yCoord,
				    window->buffer.width,
				    window->buffer.height);
      window->visible = 1;
    }

  // Set the location
  window->xCoord = xCoord;
  window->yCoord = yCoord;

  // If the window is visible, draw it
  if (window->visible)
    drawWindow(window);

  // Return success
  return (status = 0);
}


int kernelWindowCenter(kernelWindow *window)
{
  // Centers a window on the screen

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);

  return (kernelWindowSetLocation(window,
			  ((screenWidth - window->buffer.width) / 2),
			  ((screenHeight - window->buffer.height) / 2)));
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

  window->hasBorder = trueFalse;

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

  // Whether true or false, we need to remove any existing title bar
  // component, since even if true we don't want any existing ones hanging
  // around.
  if (window->hasTitleBar)
    removeTitleBar(window);

  window->hasTitleBar = trueFalse;

  // If true, we need to create one
  if (trueFalse)
    addTitleBar(window);
  
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

  window->movable = trueFalse;

  // Return success
  return (status = 0);
}


int kernelWindowSetHasCloseButton(kernelWindow *window, int trueFalse)
{
  // Sets the 'has close button' attribute and, if false, removes any
  // close button component.

  int status = 0;
  kernelWindowComponent *titleBar = NULL;
  kernelWindowComponent *closeButton = NULL;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);

  // Regardless of whether we are removing or adding the close button, we
  // need to make sure there isn't an existing close button hanging around.
  if (window->hasTitleBar && window->hasCloseButton)
    // Loop through the list of components and finds the title bar component.
    for (count = 0; count < window->numberComponents; count ++)
      {
	if (window->componentList[count]->type == windowTitleBarComponent)
	  {
	    titleBar = window->componentList[count];
	    
	    // Get the close button from it.
	    if (titleBar->data != NULL)
	      {
		closeButton = ((kernelWindowTitleBar *)
			       titleBar->data)->closeButton;
		
		// NULL the pointer in the title bar component
		((kernelWindowTitleBar *) titleBar->data)->closeButton = NULL;
		
		// Remove it from the window's list of components
		removeWindowComponent(window, closeButton);
		
		// Destroy the button
		kernelWindowDestroyComponent(closeButton);
	      
		break;
	      }
	  }
      }

  window->hasCloseButton = trueFalse;      

  // Return success
  return (status = 0);
}


int kernelWindowLayout(kernelWindow *window)
{
  // Repositions all the window's components based on their parameters

  int status = 0;
  kernelWindowComponent *titleBar = NULL;
  unsigned columnWidth[WINDOW_MAX_COMPONENTS];
  unsigned columnStartX[WINDOW_MAX_COMPONENTS];
  unsigned rowHeight[WINDOW_MAX_COMPONENTS];
  unsigned rowStartY[WINDOW_MAX_COMPONENTS];
  kernelWindowComponent *component = NULL;
  unsigned componentSize = 0;
  int clientAreaStartX = 0;
  int clientAreaStartY = 0;
  unsigned columnSpanWidth, rowSpanHeight;
  int column, row, count1, count2;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NOSUCHENTRY);

  // Clear our arrays
  for (count1 = 0; count1 < WINDOW_MAX_COMPONENTS; count1++)
    {
      columnWidth[count1] = 0;
      columnStartX[count1] = 0;
      rowHeight[count1] = 0;
      rowStartY[count1] = 0;
    }

  if (window->hasTitleBar)
    {
      // Find the title bar.  We don't want to do it or any of its
      // subcomponents
      for (count1 = 0; count1 < window->numberComponents; count1 ++)
	if (window->componentList[count1]->type == windowTitleBarComponent)
	  {
	    titleBar = window->componentList[count1];
	    break;
	  }
    }

  // Find the width and height of each column and row based on the widest
  // or tallest component of each.  After that, the starting X/Y coordinate
  // of the column or row is the sum of the widths/heights of all previous
  // columns
  for (count1 = 0; count1 < window->numberComponents; count1 ++)
    {
      component = window->componentList[count1];
      
      if (window->hasTitleBar &&
	  ((component == titleBar) ||
	   (component == ((kernelWindowTitleBar *) titleBar->data)
	    ->closeButton)))
	continue;

      componentSize = (component->width + component->parameters.padLeft +
		       component->parameters.padRight);
      if (component->parameters.gridWidth != 0)
	{
	  componentSize /= component->parameters.gridWidth;
	  for (count2 = 0; count2 < component->parameters.gridWidth;
	       count2 ++)
	    if (componentSize >
		columnWidth[component->parameters.gridX + count2])
	      columnWidth[component->parameters.gridX + count2] =
		componentSize;
	}

      componentSize = (component->height + component->parameters.padTop +
		       component->parameters.padBottom);
      if (component->parameters.gridHeight != 0)
	{
	  componentSize /= component->parameters.gridHeight;
	  for (count2 = 0; count2 < component->parameters.gridHeight;
	       count2 ++)
	    if (componentSize >
		rowHeight[component->parameters.gridY + count2])
	      rowHeight[component->parameters.gridY + count2] =
		componentSize;
	}
    }
  
  // Set the starting X coordinates of the columns
  for (column = 0; column < WINDOW_MAX_COMPONENTS; column ++)
    for (count1 = 0; (count1 < column); count1 ++)
      columnStartX[column] += columnWidth[count1];

  // Set the starting Y coordinates of the rows
  for (row = 0; row < WINDOW_MAX_COMPONENTS; row ++)
    for (count1 = 0; (count1 < row); count1 ++)
      rowStartY[row] += rowHeight[count1];

  // Find out whether our client area needs to be adjusted to account for
  // things like borders and title bars
  if (window->hasBorder)
    {
      clientAreaStartX += borderThickness;
      clientAreaStartY += borderThickness;
    }
  if (window->hasTitleBar)
    clientAreaStartY += titleBarHeight;

  // Loop through each component setting the coordinates
  for (count1 = 0; count1 < window->numberComponents; count1 ++)
    {
      component = window->componentList[count1];

      if (window->hasTitleBar &&
	  ((component == titleBar) ||
	   (component == ((kernelWindowTitleBar *) titleBar->data)
	    ->closeButton)))
	continue;

      component->xCoord =
	(clientAreaStartX + columnStartX[component->parameters.gridX]);

      columnSpanWidth = 0;
      for (column = 0; column < component->parameters.gridWidth; column ++)
	columnSpanWidth +=
	  columnWidth[component->parameters.gridX + column];

      switch(component->parameters.orientationX)
	{
	case orient_right:
	  component->xCoord += (columnSpanWidth - ((component->width + 1) +
					   component->parameters.padRight));
	  break;
	case orient_center:
	  component->xCoord += ((columnSpanWidth - component->width) / 2);
	  break;
	case orient_left:
	default:
	  component->xCoord += component->parameters.padLeft;
	  break;
	}
	  
      component->yCoord =
	(clientAreaStartY + rowStartY[component->parameters.gridY]); 

      rowSpanHeight = 0;
      for (row = 0; row < component->parameters.gridHeight; row ++)
	rowSpanHeight += rowHeight[component->parameters.gridY + row];

      switch (component->parameters.orientationY)
	{
	case orient_bottom:
	  component->yCoord += (rowSpanHeight - ((component->height + 1) +
					 component->parameters.padBottom));
	  break;
	case orient_middle:
	  component->yCoord += ((rowSpanHeight - component->height) / 2);
	  break;
	case orient_top:
	default:
	  component->yCoord += component->parameters.padTop;
	  break;
	}

      // Check whether this component overlaps any others, and if so,
      // decrement their levels
      for (count2 = 0; count2 < window->numberComponents; count2 ++)
	if (window->componentList[count2] != window->componentList[count1])
	  {
	    if (doAreasIntersect(
		 makeComponentScreenArea(window->componentList[count2]),
		 makeComponentScreenArea(window->componentList[count1])))
	      window->componentList[count2]->level++;
	  }

      // Tell the component that it has moved
      if (component->move != NULL)
	component->move((void *) component, component->xCoord,
			component->yCoord);
    }

  return (status = 0);
}


int kernelWindowSetVisible(kernelWindow *window, int visible)
{
  // Sets a window to visible or not

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  // Set the visible value
  window->visible = visible;

  // If the window is becoming visible, draw it
  if (visible)
    {
      drawWindow(window);
      // Automatically give any newly-visible windows the focus.
      changeFocus(window);
    }
  else
    {
      // Take away the focus, if applicable
      if (window == focusWindow)
	changeFocus(findTopmostWindow());

      // Erase any visible bits of the window
      kernelWindowManagerRedrawArea(window->xCoord, window->yCoord,
				    window->buffer.width,
				    window->buffer.height);
    }

  // Return success
  return (status = 0);
}


kernelWindowComponent *kernelWindowNewComponent(void)
{
  // Creates a new component.

  int status = 0;
  kernelWindowComponent *component =
    kernelMalloc(sizeof(kernelWindowComponent));
  if (component == NULL)
    return (component);

  // Make sure it's all empty
  kernelMemClear((void *) component, sizeof(kernelWindowComponent));

  component->type = windowGenericComponent;
  component->visible = 1;
  // Everything else NULL.

  // Initialize the event stream to use when we're pushed.
  status =
    kernelWindowEventStreamNew((windowEventStream *) &(component->events));
  if (status < 0)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  // Functions
  component->drawBorder = &componentDrawBorder;

  return (component);
}


void kernelWindowDestroyComponent(kernelWindowComponent *component)
{
  kernelWindow *window = NULL;
  componentParameters params;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return;

  // Check params.  Never destroy the console text area
  if (component == NULL)
    return;

  // If this is the console text area, move it back to our console window
  if (component == consoleTextArea)
    {
      window = (kernelWindow *) component->window;

      for (count = 0; count < window->numberComponents; count ++)
	if (component == consoleTextArea)
	  {
	    removeWindowComponent(window, component);
	    kernelMemClear(&params, sizeof(componentParameters));
	    params.gridWidth = 1;
	    params.gridHeight = 1;
	    params.orientationX = orient_center;
	    params.orientationY = orient_middle;
	    params.useDefaultForeground = 1;
	    params.useDefaultBackground = 1;
	    ((kernelWindowTextArea *) component->data)
	      ->graphicBuffer = &(consoleWindow->buffer);
	    kernelWindowAddClientComponent(consoleWindow, component, &params);
	    kernelWindowLayout(consoleWindow);
	    kernelWindowAutoSize(consoleWindow);
	    return;
	  }
    }

  // Call the component's own destroy function
  if (component->destroy != NULL)
    component->destroy((void *) component);

  // Deallocate generic things
  if (component->events.s.buffer != NULL)
    kernelFree((void *)(component->events.s.buffer));
  
  // The component itself
  kernelFree((void *) component);

  return;
}


int kernelWindowAddComponent(kernelWindow *window,
			     kernelWindowComponent *component,
			     componentParameters *params)
{
  // External add window component to a window.  Just a wrapper for the
  // internal function addWindowComponent

  // Make sure we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  if ((window == NULL) || (component == NULL) || (params == NULL))
    return (ERR_NULLPARAMETER);

  return (addWindowComponent(window, component, params));
}


int kernelWindowAddClientComponent(kernelWindow *window,
				   kernelWindowComponent *component,
				   componentParameters *params)
{
  // External add window component to a window, adjusted so that the X
  // and Y coordinates of the component are relative to the start of the
  // client area inside the window

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((window == NULL) || (component == NULL) || (params == NULL))
    return (status = ERR_NULLPARAMETER);

  component->xCoord += borderThickness;
  component->yCoord += borderThickness;
  if (window->hasTitleBar)
    component->yCoord += titleBarHeight;

  // Tell the component that it moved
  if (component->move != NULL)
    component->move((void *) component, component->xCoord,
		    component->yCoord);
  
  return (addWindowComponent(window, component, params));
}


int kernelWindowAddConsoleTextArea(kernelWindow *window,
				   componentParameters *params)
{
  // Moves the console text area component from our 'hidden' console window
  // to the supplied window

  int status = 0;

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

  // Change the text area's buffer to be that of the new window
  ((kernelWindowTextArea *) consoleTextArea->data)
    ->graphicBuffer = &(window->buffer);

  // Now add it to the window
  return (kernelWindowAddClientComponent(window, consoleTextArea, params));
}


unsigned kernelWindowComponentGetWidth(kernelWindowComponent *component)
{
  // Return the width parameter of the component
  if (!initialized || (component == NULL))
    return (-1);
  else
    return (component->width);
}


unsigned kernelWindowComponentGetHeight(kernelWindowComponent *component)
{
  // Return the height parameter of the component
  if (!initialized || (component == NULL))
    return (-1);
  else
    return (component->height);
}


void kernelWindowFocusComponent(kernelWindow *window,
				kernelWindowComponent *component)
{
  // Put the supplied component on top of any other components it intersects
  
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return;

  // Check params
  if ((window == NULL) || (component == NULL))
    return;

  // Find all the window's components at this location 
  for (count = 0; count < window->numberComponents; count ++)
    if (window->componentList[count]->level <= component->level)
      {
	if (doAreasIntersect(makeComponentScreenArea(component),
		     makeComponentScreenArea(window->componentList[count])))
	  window->componentList[count]->level++;
      }
  
  component->level = 0;
}


void kernelWindowManagerRedrawArea(int xCoord, int yCoord, unsigned width,
				   unsigned height)
{
  // This function will redraw an arbitrary area of the screen.  Initially
  // written to allow the mouse functions to erase the mouse without
  // having to know what's under it.  Could be useful for other things as
  // well.

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
  if ((rootWindow == NULL) || !(rootWindow->visible))
    kernelGraphicClearArea(NULL, &rootColor, xCoord, yCoord, width, height);

  // Loop through the window list, looking for any visible ones that
  // intersect this area
  for (count = 0; count < numberWindows; count ++)
    if (windowList[count]->visible &&
	doAreasIntersect(&area, makeWindowScreenArea(windowList[count])))
      {
	getIntersectingArea(makeWindowScreenArea(windowList[count]), &area,
			    &intersectingArea);
	intersectingArea.leftX -= windowList[count]->xCoord;
	intersectingArea.topY -= windowList[count]->yCoord;
	intersectingArea.rightX -= windowList[count]->xCoord;
	intersectingArea.bottomY -= windowList[count]->yCoord;
	renderVisiblePortions(windowList[count], &intersectingArea);
      }

  return;
}


void kernelWindowManagerProcessEvent(windowEvent *event)
{
  // A user has clicked or unclicked somewhere
  
  //int status = 0;
  static kernelWindowComponent *draggingComponent = NULL;
  kernelWindow *window = NULL;
  kernelWindowComponent *targetComponent = NULL;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return;

  if (event->type & EVENT_MASK_MOUSE)
    {
      // It was a mouse event

      // If it was just a move, skip it for now
      if (event->type == EVENT_MOUSE_MOVE)
	return;

      // Shortcut: If we are dragging a component, give the event right
      // to the component
      if (draggingComponent)
	{
	  if (draggingComponent->mouseEvent != NULL)
	    draggingComponent->mouseEvent((void *) draggingComponent, event);

	  if (!(event->type & EVENT_MOUSE_DRAG))
	    draggingComponent = NULL;
	  return;
	}

      // Figure out which window this is happening to, if any
      
      /*
      // Lock the window list
      status = kernelLockGet(&windowListLock);
      if (status < 0)
      return;
      */

      for (count = 0; count < numberWindows; count ++)
	if (windowList[count]->visible &&
	    (isPointInside(event->xPosition, event->yPosition,
			   makeWindowScreenArea(windowList[count]))))
	  {
	    // The mouse is inside this window's coordinates.  Is it the
	    // topmost such window we've found?
	    if ((window == NULL) ||
		(windowList[count]->level < window->level))
	      window = windowList[count];
	  }
      
      //kernelLockRelease(&windowListLock);
      
      // Was it inside a window?
      if (window == NULL)
	// Ignore
	return;
      
      // The event was inside a window

      // Check to see whether the process that owns the window is still
      // alive.  If not, destroy the window and quit for this event.
      if (!kernelMultitaskerProcessIsAlive(window->processId))
	{
	  kernelWindowManagerDestroyWindow(window);
	  kernelMouseDraw();
	  return;
	}

      // If the window has a dialog window, focus the dialog instead and we're
      // finished
      if (window->dialogWindow != NULL)
	{
	  if (window->dialogWindow != focusWindow)
	    changeFocus(window->dialogWindow);
	  return;
	}
  
      // If it was a click and the window is not in focus, and not
      // the root window, give it the focus
      if (((event->type == EVENT_MOUSE_DOWN) ||
	   (event->type == EVENT_MOUSE_UP)) &&
	  (window != focusWindow) && (window != rootWindow))
	// Give the window the focus
	changeFocus(window);

      // Find out if it was inside of any of the window's components,
      // and if so, put a windowEvent into its windowEventStream
      targetComponent = findTopmostComponent(window, event->xPosition,
					     event->yPosition);
      if (targetComponent != NULL)
	{
	  if (targetComponent->mouseEvent != NULL)
	    targetComponent->mouseEvent((void *) targetComponent, event);
	  
	  // Put this mouse event into the component's windowEventStream
	  kernelWindowEventStreamWrite((windowEventStream *)
				       &(targetComponent->events), event);
	  
	  if (event->type & EVENT_MOUSE_DRAG)
	    draggingComponent = targetComponent;
	}
    }

  else if (event->type & EVENT_MASK_KEY)
    {
      // It was a keyboard event

      // Find the target component
      if (focusWindow != NULL)
	{
	  if (focusWindow->focusComponent)
	    {
	      targetComponent = focusWindow->focusComponent;

	      if (targetComponent->keyEvent != NULL)
		targetComponent->keyEvent((void *) targetComponent, event);

	      // Put this key event into the component's windowEventStream
	      kernelWindowEventStreamWrite((windowEventStream *)
					   &(targetComponent->events),
					   event);
	    }
	}
    }

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

  if (window != NULL)
    {
      // The request is for a window windowEvent
      status = kernelWindowEventStreamRead((windowEventStream *)
					   &(window->events), event);
    }
  else
    {
      // The request must be for a component windowEvent
      component = (kernelWindowComponent *) key;
      
      status = kernelWindowEventStreamRead((windowEventStream *)
					   &(component->events), event);
    }

  return (status);
}


int kernelWindowManagerTileBackground(const char *filename)
{
  // This will tile the supplied image as the background image of the
  // root window
  
  int status = 0;
  image backgroundImage;

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
  setBackgroundImage(rootWindow, &backgroundImage);

  // Release the image memory
  kernelMemoryRelease(backgroundImage.data);

  // Redraw the root window
  drawWindow(rootWindow);

  kernelMouseDraw();

  // Save the settings variable
  kernelVariableListSet(settings, "background.image", filename);

  return (status = 0);
}


int kernelWindowManagerCenterBackground(const char *filename)
{
  // This will center the supplied image as the background
  
  int status = 0;

  // For the moment, this is not really implemented.  The 'tile' routine
  // will automatically center the image if it's wider or higher than half
  // the screen size anyway.
  status = kernelWindowManagerTileBackground(filename);
  if (status >= 0)
    // Save the settings variable
    kernelVariableListSet(settings, "background.image", filename);

  return (status);
}


int kernelWindowManagerScreenShot(image *saveImage)
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


int kernelWindowManagerSaveScreenShot(const char *name)
{
  // Save a screenshot in the current directory

  int status = 0;
  char filename[1024];
  image saveImage;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (name != NULL)
    {
      strncpy(filename, name, 1024);
      filename[1023] = '\0';
    }
  else
    {
      kernelMultitaskerGetCurrentDirectory(filename, 1024);
      if (filename[strlen(filename) - 1] != '/')
	strcat(filename, "/");
      strcat(filename, "scrnshot.bmp");
    }

  status = kernelWindowManagerScreenShot(&saveImage);
  if (status < 0)
    kernelError(kernel_error, "Error %d getting screen shot\n", status);

  kernelTextPrint("\nSaving screen shot as \"%s\"...  ", filename);

  status = kernelImageSaveBmp(filename, &saveImage);
  if (status < 0)
    kernelError(kernel_error, "Error %d saving bitmap\n", status);

  kernelMemoryRelease(saveImage.data);

  kernelTextPrintLine("Done");

  return (status = 0);
}


int kernelWindowManagerSetTextOutput(kernelWindowComponent *component)
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
  if ((component->type == windowTextAreaComponent) ||
      (component->type == windowTextFieldComponent))
    {
      // Switch the text area of the output stream to the supplied text
      // area component.
      inputStream =
	(kernelTextInputStream *) ((kernelWindowTextArea *)
				   component->data)->inputStream;
      outputStream =
	(kernelTextOutputStream *) ((kernelWindowTextArea *)
				    component->data)->outputStream;

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


int kernelWindowManagerShutdown(void)
{
  // Kill the window manager thread, and flush all the buffers.  Further
  // updates will be flushed automatically
  
  int status = 0;
  fileStream configFile;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  /*
  // Lock the window list, once and for all
  status = kernelLockGet(&windowListLock);
  if (status < 0)
    // Eek, couldn't lock it
    return (status);
  */

  // Flush all the buffers
  for (count = 0; count < numberWindows; count ++)
    if (windowList[count]->buffer.updates > 0)
      updateBuffer(windowList[count]);

  // Write out the configuration file
  status = kernelFileStreamOpen(DEFAULT_WINDOWMANAGER_CONFIG,
 				(OPENMODE_WRITE | OPENMODE_CREATE |
 				 OPENMODE_TRUNCATE), &configFile);
  if (status >= 0)
    {
      // Put a little message at the beginning of the file.
      kernelFileStreamWriteLine(&configFile, "# Do not manually edit this "
				"file while the window manager is running.");
      kernelFileStreamWriteLine(&configFile, "# It is automatically "
				"overwritten during shutdown.");
      status = kernelConfigurationWriter(settings, &configFile);
      kernelFileStreamClose(&configFile);
    }

  // Don't actually "disable" the window manager, as the shutdown routine
  // might want to do a few things still.

  return (status);
}
