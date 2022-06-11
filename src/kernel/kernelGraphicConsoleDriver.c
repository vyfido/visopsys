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
//  kernelGraphicConsoleDriver.c
//

// This is the graphic console screen driver.  Manipulates character images
// using the kernelGraphic functions.

#include "kernelWindow.h"
#include "kernelMisc.h"
#include "kernelMemory.h"
#include "kernelError.h"
#include <stdlib.h>
#include <string.h>


static inline void updateComponent(kernelTextArea *area)
{
  kernelWindowComponent *component =
    (kernelWindowComponent *) area->windowComponent;

  if (component && component->update)
    component->update(area->windowComponent);
}


static void scrollBuffer(kernelTextArea *area, int lines)
{
  // Scrolls back everything in the area's buffer

  int dataLength = (lines * area->columns);

  // Increasing the stored scrollback lines?
  if ((area->rows + area->scrollBackLines) < area->maxBufferLines)
    {
      area->scrollBackLines +=
	min(lines, (area->maxBufferLines -
		    (area->rows + area->scrollBackLines)));

      updateComponent(area);
    }

  kernelMemCopy((TEXTAREA_FIRSTSCROLLBACK(area) + dataLength),
		TEXTAREA_FIRSTSCROLLBACK(area),
		((area->rows + area->scrollBackLines) * area->columns));
}


static void setCursor(kernelTextArea *area, int onOff)
{
  // Draws or erases the cursor at the current position
  
  int cursorPosition = (area->cursorRow * area->columns) + area->cursorColumn;
  kernelGraphicBuffer *buffer =
    ((kernelWindowComponent *) area->windowComponent)->buffer;
  char string[2];

  string[0] = area->visibleData[cursorPosition];
  string[1] = '\0';

  if (onOff)
    {
      kernelGraphicDrawRect(buffer,
			    (color *) &(area->foreground), draw_normal, 
			    (area->xCoord + (area->cursorColumn *
					     area->font->charWidth)),
			    (area->yCoord + (area->cursorRow *
					     area->font->charHeight)),
			    area->font->charWidth, area->font->charHeight,
			    1, 1);
      kernelGraphicDrawText(buffer, (color *) &(area->background),
			    (color *) &(area->foreground),
			    area->font, string, draw_normal, 
			    (area->xCoord + (area->cursorColumn *
					     area->font->charWidth)),
			    (area->yCoord + (area->cursorRow *
					     area->font->charHeight)));
    }
  else
    {
      // Clear out the position and redraw the character
      kernelGraphicClearArea(buffer, (color *) &(area->background),
			     (area->xCoord + (area->cursorColumn *
					      area->font->charWidth)),
			     (area->yCoord + (area->cursorRow *
					      area->font->charHeight)),
			     area->font->charWidth, area->font->charHeight);
      kernelGraphicDrawText(buffer, (color *) &(area->foreground),
			    (color *) &(area->background),
			    area->font, string, draw_normal,
			    (area->xCoord + (area->cursorColumn *
					     area->font->charWidth)),
			    (area->yCoord + (area->cursorRow *
					     area->font->charHeight)));
    }

  // Tell the window manager to update the graphic buffer
  kernelWindowUpdateBuffer(buffer, (area->xCoord + (area->cursorColumn *
						    area->font->charWidth)),
			   (area->yCoord + (area->cursorRow *
					    area->font->charHeight)),
			   area->font->charWidth, area->font->charHeight);

  area->cursorState = onOff;

  return;
}


static int scrollLine(kernelTextArea *area)
{
  // Scrolls the text by 1 line in the text area provided.

  kernelGraphicBuffer *buffer =
    ((kernelWindowComponent *) area->windowComponent)->buffer;
  int longestLine = 0;
  int lineLength = 0;
  int count;

  // Figure out the length of the longest line
  for (count = 0; count < area->rows; count ++)
    {
      lineLength =
	  strlen((char *) area->visibleData + (count * area->columns));

      if (lineLength > area->columns)
	{
	  longestLine = area->columns;
	  break;
	}

      if (lineLength > longestLine)
	longestLine = lineLength;
    }

  if (buffer->height > area->font->charHeight)
    // Copy everything up by one line
    kernelGraphicCopyArea(buffer, area->xCoord,
			  (area->yCoord + area->font->charHeight),
			  (longestLine * area->font->charWidth),
			  ((area->rows - 1) * area->font->charHeight),
			  area->xCoord, area->yCoord);

  // Erase the last line
  kernelGraphicClearArea(buffer, (color *) &(area->background),
			 area->xCoord, (area->yCoord + ((area->rows - 1) *
						area->font->charHeight)),
                         (longestLine * area->font->charWidth),
                         area->font->charHeight);

  // Tell the window manager to update the whole graphic buffer
  kernelWindowUpdateBuffer(buffer, area->xCoord, area->yCoord,
			   (longestLine * area->font->charWidth),
			   (area->rows * area->font->charHeight));

  // Move the buffer up by one
  scrollBuffer(area, 1);

  // Clear out the bottom row
  kernelMemClear(TEXTAREA_LASTVISIBLE(area), area->columns);

  // Copy our buffer data to the visible area
  kernelMemCopy(TEXTAREA_FIRSTVISIBLE(area), area->visibleData,
		(area->rows * area->columns));

  // The cursor position is now 1 row up from where it was.
  area->cursorRow -= 1;
  
  return (0);
}


static int getCursorAddress(kernelTextArea *area)
{
  // Returns the cursor address as an integer

  return ((area->cursorRow * area->columns) + area->cursorColumn);
}


static int drawScreen(kernelTextArea *area)
{
  // Yup, draws the text area as currently specified

  kernelGraphicBuffer *buffer =
    ((kernelWindowComponent *) area->windowComponent)->buffer;
  unsigned char *bufferAddress = NULL;
  char lineBuffer[1024];
  int count;

  // Clear the area
  kernelGraphicClearArea(buffer, (color *) &(area->background),
			 area->xCoord, area->yCoord,
			 (area->columns * area->font->charWidth),
			 (area->rows * area->font->charHeight));

  // Copy from the buffer to the visible area, minus any scrollback lines
  bufferAddress = TEXTAREA_FIRSTVISIBLE(area);
  bufferAddress -= (area->scrolledBackLines * area->columns);
  
  for (count = 0; count < area->rows; count ++)
    {
      strncpy(lineBuffer, (char *) bufferAddress, area->columns);
      kernelGraphicDrawText(buffer, (color *) &(area->foreground),
			    (color *) &(area->background), area->font,
			    lineBuffer, draw_normal, area->xCoord,
			    (area->yCoord + (count * area->font->charHeight)));
      bufferAddress += area->columns;
    }

  // Tell the window manager to update the whole area buffer
  kernelWindowUpdateBuffer(buffer, area->xCoord, area->yCoord,
			   (area->columns * area->font->charWidth),
			   (area->rows * area->font->charHeight));

  // If we aren't scrolled back, show the cursor again
  if (area->cursorState && !(area->scrolledBackLines))
    setCursor(area, 1);

  return (0);
}


static int setCursorAddress(kernelTextArea *area, int row, int col)
{
  // Moves the cursor

  int cursorState = area->cursorState;
  char *line = NULL;

  // If we are currently scrolled back, this puts us back to normal
  if (area->scrolledBackLines)
    {
      area->scrolledBackLines = 0;
      drawScreen(area);

      updateComponent(area);
    }

  if (cursorState)
    setCursor(area, 0);

  area->cursorRow = row;
  area->cursorColumn = col;

  // If any of the preceding spots have NULLS in them, fill those with
  // spaces instead
  line =
      ((char *) TEXTAREA_FIRSTVISIBLE(area) + TEXTAREA_CURSORPOS(area) - col);
  for ( ; col >= 0; col --)
    if (line[col] == '\0')
      line[col] = ' ';
  
  if (cursorState)
    setCursor(area, 1);

  return (0);
}


static int print(kernelTextArea *area, const char *text, textAttrs *attrs)
{
  // Prints text to the text area.

  kernelGraphicBuffer *buffer =
    ((kernelWindowComponent *) area->windowComponent)->buffer;
  int cursorState = area->cursorState;
  int length = 0;
  color *foreground = (color *) &(area->foreground);
  color *background = (color *) &(area->background);
  char lineBuffer[1024];
  int inputCounter = 0;
  int bufferCounter = 0;
  unsigned count;

  // See whether we're printing with special attributes
  if (attrs)
    {
      if (attrs->flags & TEXT_ATTRS_FOREGROUND)
	foreground = &attrs->foreground;
      if (attrs->flags & TEXT_ATTRS_BACKGROUND)
	background = &attrs->background;
      if (attrs->flags & TEXT_ATTRS_REVERSE)
	{
	  color *tmpColor = foreground;
	  foreground = background;
	  background = tmpColor;
	}
    }

  // If we are currently scrolled back, this puts us back to normal
  if (area->scrolledBackLines)
    {
      area->scrolledBackLines = 0;
      drawScreen(area);

      updateComponent(area);
    }

  if (cursorState)
    // Turn off da cursor
    setCursor(area, 0);

  // How long is the string?
  length = strlen(text);

  // Loop through the input string, adding characters to our line buffer.
  // If we reach the end of a line or encounter a newline character, do
  // a newline
  for (inputCounter = 0; inputCounter < length; inputCounter++)
    {
      // Add this character to the lineBuffer
      if (((unsigned char) text[inputCounter]) > 126)
	lineBuffer[bufferCounter++] = '?';
      else
	lineBuffer[bufferCounter++] = text[inputCounter];

      // Is this the completion of the line?

      if ((inputCounter >= (length - 1)) ||
	  ((area->cursorColumn + bufferCounter) >= area->columns) ||
	  (text[inputCounter] == '\n'))
	{
	  lineBuffer[bufferCounter] = '\0';

	  // Add it to our buffers
	  strncpy((char *) (TEXTAREA_FIRSTVISIBLE(area) +
			    TEXTAREA_CURSORPOS(area)),
		  lineBuffer, (area->columns - area->cursorColumn));

	  if (area->hidden)
	    {
	      for (count = 0; count < strlen(lineBuffer); count ++)
		lineBuffer[count] = '*';
	      strncpy((char *) (area->visibleData + TEXTAREA_CURSORPOS(area)),
		     lineBuffer, (area->columns - area->cursorColumn));
	    }
	  else
	    strncpy((char *) (area->visibleData + TEXTAREA_CURSORPOS(area)),
		    (char *) (TEXTAREA_FIRSTVISIBLE(area) +
			      TEXTAREA_CURSORPOS(area)),
		    (area->columns - area->cursorColumn));

	  // Draw it
	  kernelGraphicDrawText(buffer, foreground, background,
				area->font, lineBuffer, draw_normal, 
				(area->xCoord + (area->cursorColumn *
						 area->font->charWidth)),
				(area->yCoord + (area->cursorRow *
						 area->font->charHeight)));
	  
	  kernelWindowUpdateBuffer(buffer,
				   (area->xCoord + (area->cursorColumn *
						    area->font->charWidth)),
				   (area->yCoord + (area->cursorRow *
						    area->font->charHeight)),
				   (bufferCounter * area->font->charWidth),
				   area->font->charHeight);

	  if (((area->cursorColumn + bufferCounter) >= area->columns) ||
	      (text[inputCounter] == '\n'))
	    {
	      // Will this cause a scroll?
	      if (area->cursorRow >= (area->rows - 1))
		{
		  if (!area->noScroll)
		    {
		      scrollLine(area);
		      area->cursorRow += 1;
		    }
		}
	      else
		area->cursorRow += 1;
	      area->cursorColumn = 0;
	      
	      bufferCounter = 0;
	    }
	  else
	    area->cursorColumn += bufferCounter;
	}
    }

  if (cursorState)
    // Turn on the cursor
    setCursor(area, 1);

  return (0);
}


static int delete(kernelTextArea *area)
{
  // Erase the character at the current position

  kernelGraphicBuffer *buffer =
    ((kernelWindowComponent *) area->windowComponent)->buffer;
  int cursorState = area->cursorState;
  int position = TEXTAREA_CURSORPOS(area);

  // If we are currently scrolled back, this puts us back to normal
  if (area->scrolledBackLines)
    {
      area->scrolledBackLines = 0;
      drawScreen(area);

      updateComponent(area);
    }

  if (cursorState)
    // Turn off da cursor
    setCursor(area, 0);

  // Delete the character in our buffers
  *(TEXTAREA_FIRSTVISIBLE(area) + position) = '\0';
  *(area->visibleData + position) = '\0';

  kernelWindowUpdateBuffer(buffer,
			   (area->xCoord + (area->cursorColumn *
					    area->font->charWidth)),
			   (area->yCoord + (area->cursorRow *
					    area->font->charHeight)),
			   area->font->charWidth, area->font->charHeight);
  if (cursorState)
    // Turn on the cursor
    setCursor(area, 1);

  return (0);
}


static int clearScreen(kernelTextArea *area)
{
  // Yup, clears the text area

  kernelGraphicBuffer *buffer =
    ((kernelWindowComponent *) area->windowComponent)->buffer;

  // Clear the area
  kernelGraphicClearArea(buffer, (color *) &(area->background),
			 area->xCoord, area->yCoord,
			 (area->columns * area->font->charWidth),
			 (area->rows * area->font->charHeight));

  // Tell the window manager to update the whole area buffer
  kernelWindowUpdateBuffer(buffer, area->xCoord, area->yCoord,
			   (area->columns * area->font->charWidth),
			   (area->rows * area->font->charHeight));

  // Empty all the data
  kernelMemClear(TEXTAREA_FIRSTVISIBLE(area), (area->columns * area->rows));

  // Copy to the visible area
  kernelMemCopy(TEXTAREA_FIRSTVISIBLE(area), area->visibleData,
		(area->rows * area->columns));

  // Cursor to the top right
  area->cursorColumn = 0;
  area->cursorRow = 0;

  if (area->cursorState)
    // Turn on the cursor
    setCursor(area, 1);

  updateComponent(area);

  return (0);
}


static int saveScreen(kernelTextArea *area, textScreen *screen)
{
  // This routine saves the current contents of the screen

  // Get memory for a new save area
  screen->data =
    kernelMemoryGet(area->columns * area->rows, "text screen data");
  if (screen->data == NULL)
    return (ERR_MEMORY);

  kernelMemCopy(TEXTAREA_FIRSTVISIBLE(area), screen->data,
		(area->rows * area->columns));

  screen->column = area->cursorColumn;
  screen->row = area->cursorRow;

  return (0);
}


static int restoreScreen(kernelTextArea *area, textScreen *screen)
{
  // This routine restores the saved contents of the screen

  if (screen->data)
    {
      kernelMemCopy(screen->data, TEXTAREA_FIRSTVISIBLE(area), 
		    (area->rows * area->columns));

      // Copy to the visible area
      kernelMemCopy(screen->data, area->visibleData,
		    (area->rows * area->columns));
    }

  area->cursorColumn = screen->column;
  area->cursorRow = screen->row;

  drawScreen(area);

  updateComponent(area);

  return (0);
}


static kernelTextOutputDriver graphicModeDriver = {
  setCursor,
  getCursorAddress,
  setCursorAddress,
  NULL, // setForeground
  NULL, // setBackground
  print,
  delete,
  drawScreen,
  clearScreen,
  saveScreen,
  restoreScreen
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelGraphicConsoleInitialize(void)
{
  // Called before the first use of the text console.

  // Register our driver
  return (kernelDriverRegister(graphicConsoleDriver, &graphicModeDriver));
}
