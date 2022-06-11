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
//  kernelGraphicConsoleDriver.c
//

// This is the graphic console screen driver.  Manipulates character images
// using the kernelGraphic functions.

#include "kernelDriverManagement.h"
#include "kernelWindowManager.h"
#include "kernelMiscFunctions.h"
#include <sys/errors.h>
#include <string.h>


static void cursorOn(kernelTextArea *area)
{
  // Draws the cursor at the current cursor position
  
  int cursorPosition = (area->cursorRow * area->columns) + area->cursorColumn;
  char string[2];

  string[0] = area->data[cursorPosition];
  string[1] = '\0';

  kernelGraphicDrawRect(area->graphicBuffer,
			(color *) &(area->foreground), draw_normal, 
			(area->xCoord + (area->cursorColumn *
					 area->font->charWidth)),
			(area->yCoord + (area->cursorRow *
					 area->font->charHeight)),
			area->font->charWidth, area->font->charHeight, 1, 1);

  if (area->data[cursorPosition] != 0)
    kernelGraphicDrawText(area->graphicBuffer,
			  (color *) &(area->background),
			  area->font, string, draw_normal, 
			  (area->xCoord + (area->cursorColumn *
					   area->font->charWidth)),
			  (area->yCoord + (area->cursorRow *
					   area->font->charHeight)));

  // Tell the window manager to update the graphic buffer
  kernelWindowManagerUpdateBuffer(area->graphicBuffer,
				  (area->xCoord + (area->cursorColumn *
						   area->font->charWidth)),
				  (area->yCoord + (area->cursorRow *
						   area->font->charHeight)),
				  area->font->charWidth,
				  area->font->charHeight);
  return;
}


static void cursorOff(kernelTextArea *area)
{
  // Erases the cursor at the current cursor position

  int cursorPosition = (area->cursorRow * area->columns) + area->cursorColumn;
  char string[2];

  string[0] = area->data[cursorPosition];
  string[1] = '\0';

  // Clear out the position and redraw the character
  kernelGraphicClearArea(area->graphicBuffer,
			 (color *) &(area->background),
			 (area->xCoord + (area->cursorColumn *
					  area->font->charWidth)),
			 (area->yCoord + (area->cursorRow *
					  area->font->charHeight)),
			 area->font->charWidth, area->font->charHeight);

  if (area->data[cursorPosition] != 0)
    kernelGraphicDrawText(area->graphicBuffer,
			  (color *) &(area->foreground),
			  area->font, string, draw_normal, (area->xCoord +
			       (area->cursorColumn * area->font->charWidth)),
			  (area->yCoord +
			   (area->cursorRow * area->font->charHeight)));

  // Tell the window manager to update the graphic buffer
  kernelWindowManagerUpdateBuffer(area->graphicBuffer,
				  (area->xCoord + (area->cursorColumn *
						   area->font->charWidth)),
				  (area->yCoord + (area->cursorRow *
						   area->font->charHeight)),
				  area->font->charWidth,
				  area->font->charHeight);
  return;
}


static int scrollLine(kernelTextArea *area)
{
  // Scrolls the text by 1 line in the text area provided.

  int lineLength = 0;
  int longestLine = 0;
  int count;

  // Figure out the length of the longest line
  for (count = 0; count < area->rows; count ++)
    {
      lineLength = strlen(area->data + (count * area->columns));

      if (lineLength > area->columns)
	{
	  longestLine = area->columns;
	  break;
	}

      if (lineLength > longestLine)
	longestLine = lineLength;
    }

  kernelGraphicCopyArea(area->graphicBuffer, area->xCoord,
			(area->yCoord + area->font->charHeight),
			(longestLine * area->font->charWidth),
			((area->rows - 1) * area->font->charHeight),
			area->xCoord, area->yCoord);

  // Erase the last line
  kernelGraphicClearArea(area->graphicBuffer,
			 (color *) &(area->background),
			 area->xCoord,
			 (area->yCoord + ((area->rows - 1) *
					  area->font->charHeight)),
                         (longestLine * area->font->charWidth),
                         area->font->charHeight);

  // Move up the contents of the buffer
  kernelMemCopy((area->data + area->columns), area->data,
		((area->rows - 1) * area->columns));
  kernelMemClear(area->data + ((area->rows - 1) * area->columns), 
		 area->columns);

  // The cursor position is now 1 row up from where it was.
  area->cursorRow -= 1;
  
  // Tell the window manager to update the whole graphic buffer
  kernelWindowManagerUpdateBuffer(area->graphicBuffer,
 				  area->xCoord, area->yCoord,
 				  (longestLine * area->font->charWidth),
  				  (area->rows * area->font->charHeight));

  // Ok
  return (0);
}


static void newline(kernelTextArea *area)
{
  // Does a carriage return & linefeed.  If necessary, it scrolls the
  // screen

  // Will this cause a scroll?
  if (area->cursorRow >= (area->rows - 1))
    scrollLine(area);

  // Cursor advances one row, goes to column 0
  area->cursorColumn = 0;
  area->cursorRow += 1;

  // Simple
  return;
}


static int getCursorAddress(kernelTextArea *area)
{
  // Returns the cursor address as an integer

  if (area == NULL)
    return (ERR_NULLPARAMETER);

  return ((area->cursorRow * area->columns) + area->cursorColumn);
}


static int setCursorAddress(kernelTextArea *area, int row, int col)
{
  // Moves the cursor

  if (area == NULL)
    return (ERR_NULLPARAMETER);

  cursorOff(area);

  area->cursorRow = row;
  area->cursorColumn = col;

  cursorOn(area);
  return (0);
}


static int getForeground(kernelTextArea *area)
{
  return (0);
}


static int setForeground(kernelTextArea *area, int newForeground)
{
  return (0);
}


static int getBackground(kernelTextArea *area)
{
  return (0);
}


static int setBackground(kernelTextArea *area, int newBackground)
{
  return (0);
}


static int print(kernelTextArea *area, const char *text)
{
  // Prints text to the text area.

  int length = 0;
  int oldCursorRow = 0;
  char lineBuffer[1024];
  int inputCounter = 0;
  int bufferCounter = 0;

  if (area == NULL)
    return (ERR_NULLPARAMETER);

  // Save the current row number
  oldCursorRow = area->cursorRow;

  // How long is the string?
  length = strlen(text);

  // Turn off da cursor
  cursorOff(area);

  // Loop through the input string, adding characters to our line buffer.
  // If we reach the end of a line or encounter a newline character, do
  // a newline
  for (inputCounter = 0; inputCounter < length; inputCounter ++)
    {
      // Add this character to the lineBuffer
      lineBuffer[bufferCounter++] = text[inputCounter];

      // Will this character cause the cursor to move to the next line?

      // Is it a newline character?
      if (((area->cursorColumn + bufferCounter) >= area->columns) ||
	  (text[inputCounter] == '\n'))
	{
	  // Print out whatever is currently in the lineBuffer
	  if (bufferCounter > 0)
	    {
	      lineBuffer[bufferCounter] = '\0';

	      // Add it to our text area buffer
	      strcpy(area->data + (area->cursorRow * area->columns) +
		     area->cursorColumn, lineBuffer);

	      // Draw it
	      kernelGraphicDrawText(area->graphicBuffer,
				    (color *) &(area->foreground),
				    area->font, lineBuffer, draw_normal, 
				    (area->xCoord + (area->cursorColumn *
					     area->font->charWidth)),
				    (area->yCoord + (area->cursorRow *
					     area->font->charHeight)));

	      kernelWindowManagerUpdateBuffer(area->graphicBuffer,
			      (area->xCoord + (area->cursorColumn *
					       area->font->charWidth)),
			      (area->yCoord + (area->cursorRow *
					       area->font->charHeight)),
			      (bufferCounter * area->font->charWidth),
					      area->font->charHeight);
	    }
	  newline(area);
	  bufferCounter = 0;

	  if (text[inputCounter] == '\n')
	    continue;
	}
    }

  // Anything left in the lineBuffer after we've processed everything?
  if (bufferCounter != 0)
    {
      lineBuffer[bufferCounter] = '\0';

      // Add it to our text area buffer
      strcpy(area->data + (area->cursorRow * area->columns) +
	     area->cursorColumn, lineBuffer);

      // Draw it
      kernelGraphicDrawText(area->graphicBuffer,
			    (color *) &(area->foreground),
			    area->font, lineBuffer, draw_normal, 
			    (area->xCoord + (area->cursorColumn *
					     area->font->charWidth)),
			    (area->yCoord + (area->cursorRow *
					     area->font->charHeight)));
      kernelWindowManagerUpdateBuffer(area->graphicBuffer,
			      (area->xCoord + (area->cursorColumn *
					       area->font->charWidth)),
			      (area->yCoord + (area->cursorRow *
					       area->font->charHeight)),
			      (bufferCounter * area->font->charWidth),
				      area->font->charHeight);

      area->cursorColumn += bufferCounter;
    }

  // Turn on the cursor
  cursorOn(area);
  return (0);
}


static int clearScreen(kernelTextArea *area)
{
  // Yup, clears the text area

  if (area == NULL)
    return (ERR_NULLPARAMETER);

  // Empty all the data
  kernelMemClear(area->data, (area->columns * area->rows));

  // Turn off the cursor
  cursorOff(area);

  // Clear the area
  kernelGraphicClearArea(area->graphicBuffer,
			 (color *) &(area->background),
			 area->xCoord, area->yCoord,
			 (area->columns * area->font->charWidth),
			 (area->rows * area->font->charHeight));

  // Cursor to the top right
  area->cursorColumn = 0;
  area->cursorRow = 0;

  // Tell the window manager to update the whole area buffer
  kernelWindowManagerUpdateBuffer(area->graphicBuffer,
				  area->xCoord, area->yCoord,
				  (area->columns * area->font->charWidth),
				  (area->rows * area->font->charHeight));

  // Turn on the cursor
  cursorOn(area);
  return (0);
}


static kernelTextOutputDriver graphicModeDriver = {
  kernelGraphicConsoleInitialize,
  getCursorAddress,
  setCursorAddress,
  getForeground,
  setForeground,
  getBackground,
  setBackground,
  print,
  clearScreen
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
