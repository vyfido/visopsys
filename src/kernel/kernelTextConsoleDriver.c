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
//  kernelTextConsoleDriver.c
//

// This is the text console screen driver.  It is a re-written version of
// the original one which was done in assembler.  It seemed like a good idea
// to use C instead, except in one or two places where we need asm.

#include "kernelDriverManagement.h"
#include "kernelProcessorX86.h"
#include "kernelMiscFunctions.h"
#include <string.h>


int kernelTextConsoleGetCursorAddress(kernelTextArea *);
int kernelTextConsoleSetCursorAddress(kernelTextArea *, int, int);
int kernelTextConsoleGetForeground(kernelTextArea *);
int kernelTextConsoleSetForeground(kernelTextArea *, int);
int kernelTextConsoleGetBackground(kernelTextArea *);
int kernelTextConsoleSetBackground(kernelTextArea *, int);
int kernelTextConsolePrint(kernelTextArea *, const char *);
int kernelTextConsoleClearScreen(kernelTextArea *);

// Our kernelTextOutputDriver structure
static kernelTextOutputDriver textModeDriver = {
  kernelTextConsoleInitialize,
  kernelTextConsoleGetCursorAddress,
  kernelTextConsoleSetCursorAddress,
  kernelTextConsoleGetForeground,
  kernelTextConsoleSetForeground,
  kernelTextConsoleGetBackground,
  kernelTextConsoleSetBackground,
  kernelTextConsolePrint,
  kernelTextConsoleClearScreen
};

// Macro used strictly within this file
#define cursorPosition ((area->cursorRow * area->columns) + area->cursorColumn)


static void toggleCursor(kernelTextArea *area)
{
  // This just reverses the foreground and background colors at the
  // requested cursor position

  char foregroundColor;
  char *cursorAddress = NULL;

  cursorAddress = area->data + (((area->cursorRow * area->columns) +
				 area->cursorColumn) * 2);

  // Reverse foreground and background colors
  foregroundColor = *(cursorAddress + 1);

  *(cursorAddress + 1) = (((foregroundColor & 0x0F) << 4) |
			  ((foregroundColor & 0xF0) >> 4));

  return;
}


static void scrollLine(kernelTextArea *area)
{
  // This will scroll the screen by 1 line
  
  // The start of the new screen is one row down
  void *newScreenTop = area->data + (area->columns * 2);
  char *lastRow = area->data + (area->columns * (area->rows - 1) * 2);
  int count;

  toggleCursor(area);

  // Copy
  kernelMemCopy(newScreenTop, area->data,
		(area->columns * (area->rows - 1) * 2));
  
  // Clear out the bottom row
  for (count = 0; count < (area->columns * 2); )
    {
      lastRow[count++] = ' ';
      lastRow[count++] = area->foreground.blue;
    }

  // Move the cursor up by one row.  Don't use the SetCursorAddress
  // routine because we don't want to actually move the cursor like
  // normal
  area->cursorRow--;
  toggleCursor(area);

  return;
}


static void newline(kernelTextArea *area)
{
  // Just moves the cursor to the next new line, unless we are on the last
  // line of the screen, in which case we need to scroll 1 line also.

  // Will this cause a scroll?
  if (area->cursorRow >= (area->rows - 1))
    scrollLine(area);

  // Cursor advances one row, goes to column 0
  area->cursorRow += 1;
  area->cursorColumn = 0;

  // Simple
  return;
}


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


int kernelTextConsoleGetCursorAddress(kernelTextArea *area)
{
  // Returns the cursor address as an integer
  return ((area->cursorRow * area->columns) + area->cursorColumn);
}


int kernelTextConsoleSetCursorAddress(kernelTextArea *area, int row, int col)
{
  // Moves the cursor

  toggleCursor(area);
  area->cursorRow = row;
  area->cursorColumn = col;
  toggleCursor(area);

  return (0);
}


int kernelTextConsoleGetForeground(kernelTextArea *area)
{
  // Gets the foreground color

  // We just use the first byte of the foreground color structure (the 'blue'
  // byte) to store our 1-byte foreground/background color value
  return (area->foreground.blue & 0x0F);
}


int kernelTextConsoleSetForeground(kernelTextArea *area, int newForeground)
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


int kernelTextConsoleGetBackground(kernelTextArea *area)
{
  // Gets the background color

  // We just use the first byte of the foreground color structure (the 'blue'
  // byte) to store our 1-byte foreground/background color value
  return ((area->foreground.blue & 0xF0) >> 4);
}


int kernelTextConsoleSetBackground(kernelTextArea *area, int newBackground)
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


int kernelTextConsolePrint(kernelTextArea *area, const char *string)
{
  // Prints ascii text strings to the text console.

  char *cursorAddress = NULL;
  int length = 0;
  int overFlow = 0;
  int scrollLines = 0;
  int advanceCursor = 0;
  int count;

  // How long is the string?
  length = strlen(string);

  // Turn off the cursor
  toggleCursor(area);

  // Will this printing cause our screen to scroll?  If so, do it in advance
  // so we can get on with our business
  if ((cursorPosition + length) > ((area->columns * area->rows) - 1))
    {
      overFlow = (cursorPosition + length) - (area->columns * area->rows) + 1;
      scrollLines = overFlow / area->columns;
      if (overFlow % area->columns)
	scrollLines++;

      for (count = 0; count < scrollLines; count ++)
	scrollLine(area);
    }

  // Where is the cursor currently?
  cursorAddress = area->data + (cursorPosition * 2);

  // Loop through the string, putting one byte into every even-numbered
  // screen address.  Put the color byte into every odd address
  for (count = 0; count < length; count ++)
    {
      // Check for a newline
      if (string[count] == '\n')
	{
	  newline(area);
	  cursorAddress = area->data + (cursorPosition * 2);
	  advanceCursor = 0;
	}
      else
	{
	  *(cursorAddress++) = string[count];
	  *(cursorAddress++) = area->foreground.blue;
	  advanceCursor++;
	}
    }

  // Increment the cursor position
  area->cursorColumn += advanceCursor;

  // Turn on the cursor
  toggleCursor(area);

  return (0);
}


int kernelTextConsoleClearScreen(kernelTextArea *area)
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

  kernelProcessorWriteDwords(tmpData, area->data, dwords);

  // Make the cursor go to the top left
  area->cursorColumn = 0;
  area->cursorRow = 0;
  toggleCursor(area);

  return (0);
}
