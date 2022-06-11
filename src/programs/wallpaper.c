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
//  wallpaper.c
//

// Calls the kernel's window manager to change the background image

#include <stdio.h>
#include <string.h>
#include <sys/vsh.h>
#include <sys/api.h>


static objectKey window = NULL;
static objectKey textField = NULL;


static void promptWindow(int processId)
{
  int status = 0;
  componentParameters params;
  objectKey font = NULL;
  objectKey textLabel = NULL;

  // Create a new window, with small, arbitrary size and location
  window = windowManagerNewWindow(processId, "Enter filename", 0, 0, 400, 400);
  if (window == NULL)
    return;

  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 0;
  params.orientationX = orient_left;
  params.orientationY = orient_top;
  params.hasBorder = 0;
  params.useDefaultForeground = 0;
  params.foreground.red = 40;
  params.foreground.green = 93;
  params.foreground.blue = 171;
  params.useDefaultBackground = 1;

  // Try to load our favorite font
  status = fontLoad("/system/arial-bold-12.bmp", "arial-bold-12", &font);
  if (status < 0)
    // Not found.  Just use the default fonts
    font = NULL;

  // Put a text label in the window to prompt the user
  textLabel = windowNewTextLabel(window, NULL, "Please enter an image file "
				 "to use:");
  if (textLabel != NULL)
    {
      // Put it in the client area of the window
      params.gridY = 1;
      windowAddClientComponent(window, textLabel, &params);
    }

  // Put a text field in the window for the user to type
  textField = windowNewTextField(window, 30, NULL /* default font*/);
  if (textField != NULL)
    {
      // Put it in the client area of the window
      params.gridY = 3;
      params.padTop = 5;
      params.padBottom = 5;
      params.hasBorder = 1;
      params.useDefaultBackground = 0;
      params.background.red = 255;
      params.background.green = 255;
      params.background.blue = 255;
      windowAddClientComponent(window, textField, &params);
    }

  // Don't want the user closing this window.  It will just confuse them later.
  windowSetHasCloseButton(window, 0);

  windowLayout(window);

  // Autosize the window to fit our text area
  windowAutoSize(window);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char bufferCharacter = '\0';
  int currentCharacter = 0;
  char tmpFilename[128];
  char filename[MAX_PATH_NAME_LENGTH];
  int processId = 0;
  objectKey oldTextInput = NULL;
  objectKey oldTextOutput = NULL;
  unsigned windowWidth = 0;
  unsigned windowHeight = 0;
  int count;

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  // We need our process ID to create the windows
  processId = multitaskerGetCurrentProcessId();

  if (argc < 2)
    {
      // The user did not specify a file.  We will prompt them.
      promptWindow(processId);

      // Get the size of our new window
      windowGetSize(window, &windowWidth, &windowHeight);

      // Set the position to the middle of the screen
      windowSetLocation(window, ((graphicGetScreenWidth() - windowWidth) / 2),
			((graphicGetScreenHeight() - windowHeight) / 2));
      
      windowSetVisible(window, 1);
      
      // Use the text field for all our input and output
      oldTextInput = multitaskerGetTextInput();
      oldTextOutput = multitaskerGetTextOutput();
      windowManagerSetTextOutput(textField);
      printf("\n");

      // Set the current character to 0
      currentCharacter = 0;

      // This loop grabs characters
      while(1)
	{
	  bufferCharacter = getchar();

	  if (errno)
	    // Eek, we can't get input.  Quit.
	    return (status = errno);

	  if (bufferCharacter == (unsigned char) 8)
	    {
	      if (currentCharacter > 0)
		{
		  // Move the current character back by 1
		  tmpFilename[currentCharacter] = '\0';
		  currentCharacter--;
		}
	    }

	  else if (bufferCharacter == (unsigned char) 9)
	    {
	      // This is the TAB key.  Attempt to complete a filename.
	      
	      // Get rid of any tab characters printed on the screen
	      textSetColumn(currentCharacter);
	      
	      for (count = (strlen(tmpFilename)); count >= 0; count --)
		if (tmpFilename[count] == '\"')
		  {
		    count++;
		    break;
		  }

	      if (count < 0)
		for (count = (strlen(tmpFilename)); count >= 0; count --)
		  if (tmpFilename[count] == ' ')
		    {
		      count++;
		      break;
		    }

	      if (count < 0)
		count = 0;
	      
	      vshCompleteFilename(tmpFilename + count);
	      textSetColumn(0);
	      printf("\n");
	      printf(tmpFilename);
	      currentCharacter = strlen(tmpFilename);
	    }
	  
	  else if (bufferCharacter == (unsigned char) 10)
	    {
	      // Put a null in at the end of the buffer
	      tmpFilename[currentCharacter] = '\0';
	  
	      if (currentCharacter > 0)
		// Now we interpret the login
		break;

	      else
		// The user hit 'enter' without typing anything.
		continue;
	    }
      
	  else
	    // Add the current character to the login buffer
	    tmpFilename[currentCharacter++] = bufferCharacter;
	}

      // We got some filename
      multitaskerSetTextInput(processId, oldTextInput);
      multitaskerSetTextOutput(processId, oldTextOutput);
      if (window != NULL)
	windowManagerDestroyWindow(window);
    }
  
  else
    strcpy(tmpFilename, argv[1]);

  // Make sure the file name is complete
  vshMakeAbsolutePath(tmpFilename, filename);

  status = windowManagerTileBackground(filename);

  errno = status;
  return (status);
}
