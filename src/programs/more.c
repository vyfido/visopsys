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
//  more.c
//

// This is the UNIX-style command for reading files page by page

/* This is the text that appears when a user requests help about this program
<help>

 -- more --

Display file's contents, one screenfull at a time.

Usage:
  more <file1> [file2] [...]

Each file name listed after the command name will be printed in sequence.
This is similar to the 'cat' command, except that the file contents are
displayed one screenfull at a time.  To page forward to the next screenfull,
press the [SPACE] key.  To quit, press the [Q] key.  To advance by a single
line, press any other key.

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/api.h>

static int screenColumns = 0;
static int screenRows = 0;
static int foregroundColor = 0;
static int backgroundColor = 0;


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <file1> [file2] [...]\n", name);
  return;
}


static int viewFile(const char *fileName)
{
  int status = 0;
  file theFile;
  char *fileBuffer = NULL;
  int charEntered = 0;
  int charsSoFar = 0;
  int cursorPos1, cursorPos2;
  unsigned count1;
  int count2;

  // Initialize the file structure
  bzero(&theFile, sizeof(file));

  // Call the "find file" routine to see if we can get the file
  status = fileFind(fileName, &theFile);
  if (status < 0)
    return (status);

  // Make sure the file isn't empty.  We don't want to try reading
  // data from a nonexistent place on the disk.
  if (theFile.size == 0)
    // It is empty, so skip it
    return (status = 0);

  // The file exists and is non-empty.  That's all we care about (we
  // don't care at this point, for example, whether it's a file or a
  // directory.  Read it into memory and print it on the screen.
      
  // Allocate a buffer to store the file contents in
  fileBuffer = malloc((theFile.blocks * theFile.blockSize) + 1);
  if (fileBuffer == NULL)
    return (status = ERR_MEMORY);

  status = fileOpen(fileName, OPENMODE_READ, &theFile);
  if (status < 0)
    {
      free(fileBuffer);
      return (status);
    }

  status = fileRead(&theFile, 0, theFile.blocks, fileBuffer);
  if (status < 0)
    {
      free(fileBuffer);
      return (status);
    }

  charsSoFar = 0;

  // Print the file, one screen at a time
  for (count1 = 0; count1 < theFile.size; count1 ++)
    {
      // Are we at the end of a screenful of data?
      if (charsSoFar >= (screenColumns * (screenRows - 1)))
	{
	  // Reverse the colors
	  textSetForeground(backgroundColor);
	  textSetBackground(foregroundColor);
	  
	  printf("--More--(%d%%)", ((count1 * 100) / theFile.size));
	  
	  // Restore the colors
	  textSetForeground(foregroundColor);
	  textSetBackground(backgroundColor);
	  
	  // Wait for user input
	  textInputSetEcho(0);
	  charEntered = getchar();
	  textInputSetEcho(1);
	  
	  // Erase the "more" thing
	  cursorPos1 = textGetColumn();
	  for (count2 = 0; count2 < cursorPos1; count2++)
	    textBackSpace();
	  
	  // Did the user want to quit or anything?
	  if (charEntered == (int) 'q')
	    break;
	  
	  // Another screenful?
	  else if (charEntered == (int) ' ')
	    charsSoFar = 0;
	  
	  // Another lineful
	  else
	    charsSoFar -= screenColumns;
	  
	  // Continue, fall through
	}
      
      // Look out for tab characters
      if (fileBuffer[count1] == (char) 9)
	{
	  // We need to keep track of how many characters get printed
	  cursorPos1 = textGetColumn();
	  
	  textTab();
	  
	  cursorPos2 = textGetColumn();
	  
	  if (cursorPos2 >= cursorPos1)
	    charsSoFar += (cursorPos2 - cursorPos1);
	  else
	    charsSoFar += (screenColumns - (cursorPos1 + 1)) + 
	      (cursorPos2 + 1);
	}
      
      // Look out for newline characters
      else if (fileBuffer[count1] == (char) 10)
	{
	  // We need to keep track of how many characters get printed
	  cursorPos1 = textGetColumn();
	  
	  textPutc('\n');
	  
	  charsSoFar += screenColumns - cursorPos1;
	}
      
      else
	{
	  textPutc(fileBuffer[count1]);
	  charsSoFar += 1;
	}
    }
  
  // Free the memory
  free(fileBuffer);

  return (status = 0);
}


int main(int argc, char *argv[])
{
  int status = 0;
  int argNumber = 0;
  char fileName[MAX_PATH_NAME_LENGTH];

  if (argc < 2)
    {
      usage(argv[0]);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Get screen parameters
  screenColumns = textGetNumColumns();
  screenRows = textGetNumRows();
  foregroundColor = textGetForeground();
  backgroundColor = textGetBackground();

  // Loop through all of our file name arguments
  for (argNumber = 1; argNumber < argc; argNumber ++)
    {
      // Make sure the name isn't NULL
      if (argv[argNumber] == NULL)
	return (status = ERR_NULLPARAMETER);

      // Make sure the name is complete
      vshMakeAbsolutePath(argv[argNumber], fileName);

      status = viewFile(fileName);
      if (status < 0)
	{
	  errno = status;
	  perror(argv[0]);
	  if (argNumber < (argc - 1))
	    continue;
	  else
	    return (status);
	}
    }

  // Return success
  return (status = 0);
}
