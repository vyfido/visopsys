//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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

#include "kernelTextConsoleDriver.h"
#include "kernelMiscAsmFunctions.h"
#include <string.h>

static volatile void *screenAddress = NULL;
static volatile int screenColumns = 0;
static volatile int screenRows = 0;
static volatile int cursorPosition = 0;
static char colourByte = 0;


static void scrollLine(void)
{
  // This will scroll the screen by 1 line
  
  // The start of the new screen is one row down
  void *newScreenTop = (void *) screenAddress + (screenColumns * 2);
  char *lastRow =
    (void *) screenAddress + (screenColumns * (screenRows - 1) * 2);
  int count;

  // Copy
  kernelMemCopy(newScreenTop, (void *) screenAddress,
		(screenColumns * (screenRows - 1) * 2));
  
  // Move the cursor up by one row.  Don't use the SetCursorAddress
  // routine because we don't want to actually move the cursor like
  // normal
  cursorPosition -= screenColumns;

  // Clear out the bottom row
  for (count = 0; count < (screenColumns * 2); )
    {
      lastRow[count++] = ' ';
      lastRow[count++] = colourByte;
    }

  return;
}


static void newline(void)
{
  // Just moves the cursor to the next new line, unless we are on the last
  // line of the screen, in which case we need to scroll 1 line also.

  if (cursorPosition >= (screenColumns * (screenRows - 1)))
    {
      scrollLine();
      cursorPosition = (screenColumns * (screenRows - 1));
    }
  else
    // Calculate the new position
    cursorPosition = (((cursorPosition / screenColumns) * screenColumns) +
		      screenColumns);

  return;
}


static void toggleCursor(int position)
{
  // This just reverses the foreground and background colours at the
  // requested cursor position

  char colour;
  char *cursorAddress = NULL;

  cursorAddress = (char *) screenAddress + (position * 2);

  // Reverse foreground and background colours
  colour = *(cursorAddress + 1);
  colour = (((colour & 0x0F) << 4) | ((colour & 0xF0) >> 4));
  *(cursorAddress + 1) = colour;

  return;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelTextConsoleInitialize(void *sAddr, int sCols, int sRows,
				 int foregroundColour, int backgroundColour)
{
  // Called before the first use of the text console

  // Save information about our screen
  screenAddress = sAddr;
  screenColumns = sCols;
  screenRows = sRows;

  kernelTextConsoleSetForeground(foregroundColour);
  kernelTextConsoleSetBackground(backgroundColour);

  // Clear the screen
  kernelTextConsoleClearScreen();

  // We're ready
  return;
}


int kernelTextConsoleGetCursorAddress(void)
{
  // This routine gathers the current cursor address and returns it
  return (cursorPosition);
}


void kernelTextConsoleSetCursorAddress(int newPosition)
{
  // This routine takes a new cursor address offset as a parameter.

  toggleCursor(cursorPosition);

  // Set the new position
  cursorPosition = newPosition;

  toggleCursor(cursorPosition);

  return;
}


void kernelTextConsoleSetForeground(int newForeground)
{
  // Sets a new foreground colour
  colourByte &= 0xF0;
  colourByte |= (newForeground & 0x0F);
  return;
}


void kernelTextConsoleSetBackground(int newBackground)
{
  // Sets a new background colour
  colourByte &= 0x0F;
  colourByte |= (newBackground & 0x0F) << 4;
  return;
}


void kernelTextConsolePrint(const char *string)
{
  // Prints ascii text strings to the text console.

  char *cursorAddress = NULL;
  int length = 0;
  int overFlow = 0;
  int scrollLines = 0;
  int advanceCursor = 0;
  int count;

  // Turn off the cursor
  toggleCursor(cursorPosition);

  // How long is the string?
  length = strlen(string);

  // Will this printing cause our screen to scroll?  If so, do it in advance
  // so we can get on with our business
  if ((cursorPosition + length) > ((screenColumns * screenRows) - 1))
    {
      overFlow = (cursorPosition + length) - (screenColumns * screenRows) + 1;
      scrollLines = overFlow / screenColumns;
      if (overFlow % screenColumns)
	scrollLines++;

      for (count = 0; count < scrollLines; count ++)
	scrollLine();
    }

  // Where is the cursor currently?
  cursorAddress = (char *) screenAddress + (cursorPosition * 2);

  // Loop through the string, putting one byte into every even-numbered
  // screen address.  Put the colour byte into every odd address
  for (count = 0; count < length; count ++)
    {
      // Check for a newline
      if (string[count] == '\n')
	{
	  newline();
	  cursorAddress = (char *) screenAddress + (cursorPosition * 2);
	  advanceCursor = 0;
	  continue;
	}
	    
      *(cursorAddress++) = string[count];
      *(cursorAddress++) = colourByte;
      advanceCursor++;
    }

  // Increment the cursor position
  cursorPosition += advanceCursor;

  // Turn on the cursor
  toggleCursor(cursorPosition);

  return;
}


void kernelTextConsoleClearScreen(void)
{
  // Clears the screen, and puts the cursor in the top left (starting)
  // position

  int tmpData = 0;
  int dwords = 0;
  int *position = NULL;
  int count;

  // Construct the dword of data that we will replicate all over the screen.
  // It consists of the 'space' character twice, plus the colour byte twice
  tmpData = ((colourByte << 24) | (0x20 << 16) | (colourByte << 8) | 0x20);

  // Calculate the number of dwords that make up the screen
  // Formula is ((COLS * ROWS) / 2)
  dwords = (screenColumns * screenRows) / 2;

  // Start at the beginning of screen memory
  position = (int *) screenAddress;

  // Write tmpData to the screen dwords times
  for (count = 0; count < dwords; count ++)
    position[count] = tmpData;

  // Make the cursor go to the top left
  cursorPosition = 0;
  toggleCursor(cursorPosition);

  return;
}
