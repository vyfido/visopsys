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
//  kernelTextConsoleDriver.c
//

// This is the text console screen driver.  It is a re-written version of
// the original one which was done in assembler.  It seemed like a good idea
// to use C instead, except in one or two places where we need asm.

#include "kernelText.h"
#include "kernelProcessorX86.h"
#include "kernelMisc.h"
#include "kernelMalloc.h"
#include "kernelError.h"
#include <string.h>
#include <stdlib.h>


static void scrollBuffer(kernelTextArea *area, int lines)
{
  // Scrolls back everything in the area's buffer

  int dataLength = (lines * area->columns * 2);

  // Increasing the stored scrollback lines?
  if ((area->rows + area->scrollBackLines) < area->maxBufferLines)
    area->scrollBackLines += min(lines, (area->maxBufferLines -
				 (area->rows + area->scrollBackLines)));
    
  kernelMemCopy((TEXTAREA_FIRSTSCROLLBACK(area) + dataLength),
		TEXTAREA_FIRSTSCROLLBACK(area),
		((area->rows + area->scrollBackLines) * (area->columns * 2)));
}


static void setCursor(kernelTextArea *area, int onOff)
{
  // This sets the cursor on or off at the requested cursor position

  int idx = (TEXTAREA_CURSORPOS(area) * 2);

  if (onOff)
    area->visibleData[idx + 1] = ((area->foreground.blue & 0x0F) << 4) |
      ((area->foreground.blue & 0xF0) >> 4);
  else
    area->visibleData[idx + 1] = area->foreground.blue;

  area->cursorState = onOff;

  return;
}


static void scrollLine(kernelTextArea *area)
{
  // This will scroll the screen by 1 line
  
  int cursorState = area->cursorState;
  int lineLength = (area->columns * area->bytesPerChar);
  char *lastRow = NULL;
  int count;

  if (cursorState)
    // Temporarily, cursor off
    setCursor(area, 0);

  // Move the buffer up by one
  scrollBuffer(area, 1);

  // Clear out the bottom row
  lastRow = TEXTAREA_LASTVISIBLE(area);
  for (count = 0; count < lineLength; )
    {
      lastRow[count++] = '\0';
      lastRow[count++] = area->foreground.blue;
    }

  // Copy our buffer data to the visible area
  kernelMemCopy(TEXTAREA_FIRSTVISIBLE(area), area->visibleData,
		(area->rows * lineLength));

  // Move the cursor up by one row.
  area->cursorRow -= 1;

  if (cursorState)
    // Cursor back on
    setCursor(area, 1);

  return;
}


static int getCursorAddress(kernelTextArea *area)
{
  // Returns the cursor address as an integer
  return ((area->cursorRow * area->columns) + area->cursorColumn);
}


static int drawScreen(kernelTextArea *area)
{
  // Draws the current screen as specified by the area data

  unsigned char *bufferAddress = NULL;

  // Copy from the buffer to the visible area, minus any scrollback lines
  bufferAddress = TEXTAREA_FIRSTVISIBLE(area);
  bufferAddress -= (area->scrolledBackLines * area->columns * 2);
  
  kernelMemCopy(bufferAddress, area->visibleData,
		(area->rows * area->columns * 2));

  // If we aren't scrolled back, show the cursor again
  if (area->cursorState && !(area->scrolledBackLines))
    setCursor(area, 1);

  return (0);
}


static int setCursorAddress(kernelTextArea *area, int row, int col)
{
  // Moves the cursor

  int cursorState = area->cursorState;

  // If we are currently scrolled back, this puts us back to normal
  if (area->scrolledBackLines)
    {
      area->scrolledBackLines = 0;
      drawScreen(area);
    }

  if (cursorState)
    setCursor(area, 0);

  area->cursorRow = row;
  area->cursorColumn = col;

  if (cursorState)
    setCursor(area, 1);

  return (0);
}


static int getForeground(kernelTextArea *area)
{
  // Gets the foreground color

  // We just use the first byte of the foreground color structure (the 'blue'
  // byte) to store our 1-byte foreground/background color value
  return (area->foreground.blue & 0x0F);
}


static int setForeground(kernelTextArea *area, int newForeground)
{
  // Sets a new foreground color

  // Check to make sure it's a valid color
  if ((newForeground < 0) || (newForeground > 15))
    return (-1);

  // We just use the first byte of the foreground color structure (the 'blue'
  // byte) to store our 1-byte foreground/background color value
  area->foreground.blue &= 0xF0;
  area->foreground.blue |= (newForeground & 0x0F);

  return (0);
}


static int getBackground(kernelTextArea *area)
{
  // Gets the background color

  // We just use the first byte of the foreground color structure (the 'blue'
  // byte) to store our 1-byte foreground/background color value
  return ((area->foreground.blue & 0xF0) >> 4);
}


static int setBackground(kernelTextArea *area, int newBackground)
{
  // Sets a new background color

  // Check to make sure it's a valid color
  if ((newBackground < 0) || (newBackground > 15))
    return (-1);

  // We just use the first byte of the foreground color structure (the 'blue'
  // byte) to store our 1-byte foreground/background color value
  area->foreground.blue &= 0x0F;
  area->foreground.blue |= (newBackground & 0x0F) << 4;

  return (0);
}


static int print(kernelTextArea *area, const char *string)
{
  // Prints ascii text strings to the text console.

  int cursorState = area->cursorState;
  unsigned char *bufferAddress = NULL;
  unsigned char *visibleAddress = NULL;
  int length = 0;
  int count;

  // How long is the string?
  length = strlen(string);

  // If we are currently scrolled back, this puts us back to normal
  if (area->scrolledBackLines)
    {
      area->scrolledBackLines = 0;
      drawScreen(area);
    }

  if (cursorState)
    // Turn off the cursor
    setCursor(area, 0);

  bufferAddress = (TEXTAREA_FIRSTVISIBLE(area) +
		   (TEXTAREA_CURSORPOS(area) * 2));
  visibleAddress = (area->visibleData + (TEXTAREA_CURSORPOS(area) * 2));

  // Loop through the string, putting one byte into every even-numbered
  // screen address.  Put the color byte into every odd address
  for (count = 0; count < length; count ++)
    {
      if (string[count] != '\n')
	{
	  *(bufferAddress++) = string[count];
	  *(visibleAddress++) = string[count];
	  *(bufferAddress++) = area->foreground.blue;
	  *(visibleAddress++) = area->foreground.blue;
	  area->cursorColumn += 1;
	}

      // Newline?
      if ((string[count] == '\n') || (area->cursorColumn >= area->columns))
	{
	  if (area->cursorRow >= (area->rows - 1))
	    scrollLine(area);
	  area->cursorRow += 1;
	  area->cursorColumn = 0;
	  bufferAddress =
	    (TEXTAREA_FIRSTVISIBLE(area) + (TEXTAREA_CURSORPOS(area) * 2));
	  visibleAddress =
	    (area->visibleData + (TEXTAREA_CURSORPOS(area) * 2));
	}
    }

  if (cursorState)
    // Turn the cursor back on
    setCursor(area, 1);
  
  return (0);
}


static int delete(kernelTextArea *area)
{
  // Erase the character at the current position

  int cursorState = area->cursorState;
  int position = (TEXTAREA_CURSORPOS(area) * 2);

  // If we are currently scrolled back, this puts us back to normal
  if (area->scrolledBackLines)
    {
      area->scrolledBackLines = 0;
      drawScreen(area);
    }

  if (cursorState)
    // Turn off da cursor
    setCursor(area, 0);

  // Delete the character in our buffers
  *(TEXTAREA_FIRSTVISIBLE(area) + position) = '\0';
  *(area->visibleData + position) = '\0';

  if (cursorState)
    // Turn on the cursor
    setCursor(area, 1);

  return (0);
}


static int clearScreen(kernelTextArea *area)
{
  // Clears the screen, and puts the cursor in the top left (starting)
  // position

  unsigned tmpData = 0;
  int dwords = 0;

  // Construct the dword of data that we will replicate all over the screen.
  // It consists of the NULL character twice, plus the color byte twice
  tmpData = ((area->foreground.blue << 24) | (area->foreground.blue << 8));

  // Calculate the number of dwords that make up the screen
  // Formula is ((COLS * ROWS) / 2)
  dwords = (area->columns * area->rows) / 2;

  kernelProcessorWriteDwords(tmpData, TEXTAREA_FIRSTVISIBLE(area), dwords);

  // Copy to the visible area
  kernelMemCopy(TEXTAREA_FIRSTVISIBLE(area), area->visibleData,
		(area->rows * area->columns * 2));

  // Make the cursor go to the top left
  area->cursorColumn = 0;
  area->cursorRow = 0;

  if (area->cursorState)
    setCursor(area, 1);

  return (0);
}


static int saveScreen(kernelTextArea *area)
{
  // This routine saves the current contents of the screen

  // Get memory for a new save area
  area->savedScreen = kernelMalloc(area->columns * area->rows * 2);
  if (area->savedScreen == NULL)
    return (ERR_MEMORY);

  kernelMemCopy(TEXTAREA_FIRSTVISIBLE(area), area->savedScreen,
		(area->rows * area->columns * 2));

  area->savedCursorColumn = area->cursorColumn;
  area->savedCursorRow = area->cursorRow;

  return (0);
}


static int restoreScreen(kernelTextArea *area)
{
  // This routine restores the saved contents of the screen

  kernelMemCopy(area->savedScreen, TEXTAREA_FIRSTVISIBLE(area), 
		(area->rows * area->columns * 2));

  // Copy to the visible area
  kernelMemCopy(area->savedScreen, area->visibleData, 
		(area->rows * area->columns * 2));

  area->cursorColumn = area->savedCursorColumn;
  area->cursorRow = area->savedCursorRow;
  
  return (0);
}


// Our kernelTextOutputDriver structure
static kernelTextOutputDriver textModeDriver = {
  setCursor,
  getCursorAddress,
  setCursorAddress,
  getForeground,
  setForeground,
  getBackground,
  setBackground,
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


int kernelTextConsoleInitialize(void)
{
  // Called before the first use of the text console.

  // Register our driver
  return (kernelDriverRegister(textConsoleDriver, &textModeDriver));
}
