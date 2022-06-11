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
//  view.c
//

// This command will display supported file types in the appropriate way.

/* This is the text that appears when a user requests help about this program
<help>

 -- view --

View a file.

Usage:
  view [file]

(Only available in graphics mode)

This command will launch a window in which the requested file is displayed.
If no file name is supplied on the command line (or for example if the
program is launched by clicking on its icon), the user will be prompted for
the file to display.

The currently-supported file formats are:

- Images (currently only uncompressed, 8-bit and 24-bit bitmap images)
- Text files

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/window.h>
#include <sys/api.h>
#include <sys/cdefs.h>

static objectKey window = NULL;


static void error(const char *format, ...)
{
  // Generic error message code for either text or graphics modes
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  _expandFormatString(output, format, list);
  va_end(list);

  windowNewErrorDialog(NULL, "Error", output);
}


static int countTextLines(int columns, char *data, int size)
{
  // Count the lines of text
  
  int lines = 1;
  int columnCount = 0;
  int count;

  for (count = 0; count < size; count ++)
    {
      if ((columnCount >= columns) || (data[count] == '\n'))
	{
	  lines += 1;
	  columnCount = 0;
	}

      else if (data[count] == '\0')
	break;

      else
	columnCount += 1;
    }
  
  return (lines);
}


static void printTextLines(char *data, int size)
{
  // Cut the text data into lines and print them individually

  char *linePtr = data;
  int count;

  for (count = 0; count < size; count ++)
    if (data[count] == '\n')
      data[count] = '\0';

  while (linePtr < (data + size))
    {
      textPrintLine(linePtr);
      linePtr += (strlen(linePtr) + 1);
    }
}


static void eventHandler(objectKey key, windowEvent *event)
{
  // This is just to handle a window shutdown event.

  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    windowGuiStop();
}


int main(int argc, char *argv[])
{
  int status = 0;
  int processId = 0;
  char *shortFileName = NULL;
  char *fullFileName = NULL;
  char *windowTitle = NULL;
  file tmpFile;
  loaderFileClass class;
  componentParameters params;

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  processId = multitaskerGetCurrentProcessId();

  shortFileName = malloc(MAX_PATH_NAME_LENGTH);
  fullFileName = malloc(MAX_PATH_NAME_LENGTH);
  windowTitle = malloc(MAX_PATH_NAME_LENGTH + 8);

  if ((shortFileName == NULL) || (fullFileName == NULL) ||
      (windowTitle == NULL))
    {
      status = ERR_MEMORY;
      perror(argv[0]);
      goto deallocate;
    }

  if (argc < 2)
    {
      status =
	windowNewFileDialog(NULL, "Enter filename", "Please enter the name "
			    "of the file to view:", NULL, shortFileName,
			    MAX_PATH_NAME_LENGTH);
      if (status != 1)
	{
	  if (status != 0)
	    perror(argv[0]);
	  
	  goto deallocate;
	}
    }
  else
    strcpy(shortFileName, argv[argc - 1]);

  // Turn it into an absolute pathname
  vshMakeAbsolutePath(shortFileName, fullFileName);

  // Make sure the file exists
  if (fileFind(fullFileName, &tmpFile) < 0)
    {
      error("The file \"%s\" was not found", shortFileName);
      goto deallocate;
    }

  // Get the classification of the file.
  if (loaderClassifyFile(fullFileName, &class) == NULL)
    {
      error("Unable to classify the file \"%s\"", shortFileName);
      goto deallocate;
    }

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;

  // Create a new window, with small, arbitrary size and location
  sprintf(windowTitle, "View \"%s\"", shortFileName);
  window = windowNew(processId, windowTitle);

  if (class.flags & LOADERFILECLASS_IMAGE)
    {
      image showImage;

      // Try to load the image file
      status = imageLoad(fullFileName, 0, 0, &showImage);
      if (status < 0)
	{
	  error("Unable to load the image \"%s\"\n", shortFileName);
	  goto deallocate;
	}
  
      windowNewImage(window, &showImage, draw_normal, &params);
    }

  else if (class.flags & LOADERFILECLASS_TEXT)
    {
      // Try to load the text data

      file showFile;
      char *textData = NULL;
      int rows = 25;
      int textLines = 0;
      objectKey textAreaComponent = NULL;

      textData = loaderLoad(fullFileName, &showFile);
      if (textData == NULL)
	{
	  error("Unable to load the file \"%s\"\n", shortFileName);
	  goto deallocate;
	}

      textLines = countTextLines(80, textData, showFile.size);

      if (fontLoad("/system/fonts/xterm-normal-10.bmp", "xterm-normal-10",
		   &(params.font), 1) < 0)
	{
	  // Use the system font.  It can comfortably show more rows.
	  params.font = NULL;
	  rows = 40;
	}

      textAreaComponent =
	windowNewTextArea(window, 80, rows, textLines, &params);

      // Put the data into the component
      windowSetTextOutput(textAreaComponent);
      textSetCursor(0);
      textInputSetEcho(0);
      printTextLines(textData, showFile.size);
      // Scroll back to the very top
      textScroll(-(textLines / rows));

      memoryRelease(textData);
    }

  // Go live.
  windowSetVisible(window, 1);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  // Run the GUI
  windowGuiRun();

  // Destroy the window
  windowDestroy(window);

  status = 0;

 deallocate:
  windowGuiStop();
  if (shortFileName)
    free(shortFileName);
  if (fullFileName)
    free(fullFileName);
  if (windowTitle)
    free(windowTitle);

  errno = status;
  return (status);
}
