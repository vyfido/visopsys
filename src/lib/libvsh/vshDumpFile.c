// 
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  vshDumpFile.c
//

// This contains some useful functions written for the shell

#include <stdio.h>
#include <sys/vsh.h>
#include <sys/api.h>


_X_ int vshDumpFile(const char *fileName)
{
  // Desc: Print the contents of the file, specified by 'fileName', to standard output.  'fileName' must be an absolute pathname, beginning with '/'.

  int status = 0;
  int count;
  file theFile;
  char *fileBuffer = NULL;
  static char *cmdName = "dump file";

  // Make sure file name isn't NULL
  if (fileName == NULL)
    return (status = -1);
  
  for (count = 0; count < sizeof(file); count ++)
    ((char *) &theFile)[count] = NULL;

  // Call the "find file" routine to see if we can get the first file
  status = fileFind(fileName, &theFile);
  if (status < 0)
    {
      errno = status;
      perror(cmdName);
      return (status);
    }

  // Make sure the file isn't empty.  We don't want to try reading
  // data from a nonexistent place on the disk.
  if (theFile.size == 0)
    // It is empty, so just return
    return status;

  // The file exists and is non-empty.  That's all we care about (we don't 
  // care at this point, for example, whether it's a file or a directory.  
  // Read it into memory and print it on the screen.
  
  // Allocate a buffer to store the file contents in
  fileBuffer = memoryGet(((theFile.blocks * theFile.blockSize) + 1),
			 "temporary file data");
  if (fileBuffer == NULL)
    {
      errno = ERR_MEMORY;
      perror(cmdName);
      return (status = ERR_MEMORY);
    }

  status = fileOpen(fileName, OPENMODE_READ, &theFile);
  if (status < 0)
    {
      errno = status;
      perror(cmdName);
      memoryRelease(fileBuffer);
      return (status);
    }

  status = fileRead(&theFile, 0, theFile.blocks, fileBuffer);
  if (status < 0)
    {
      errno = status;
      perror(cmdName);
      memoryRelease(fileBuffer);
      return (status);
    }


  // Print the file
  count = 0;
  while (count < theFile.size)
    {
      // Look out for tab characters
      if (fileBuffer[count] == (char) 9)
	textTab();

      // Look out for newline characters
      else if (fileBuffer[count] == (char) 10)
	printf("\n");

      else
	putchar(fileBuffer[count]);

      count += 1;
    }

  // If the file did not end with a newline character...
  if (fileBuffer[count - 1] != '\n')
    printf("\n");

  // Free the memory
  memoryRelease(fileBuffer);

  return status;
}
