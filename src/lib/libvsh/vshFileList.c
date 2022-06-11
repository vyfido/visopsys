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
//  vshFileList.c
//

// This contains some useful functions written for the shell

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/api.h>


_X_ int vshFileList(const char *itemName)
{
  // Desc: Print a listing of a file or directory named 'itemName'.  'itemName' must be an absolute pathname, beginning with '/'.

  int status = 0;
  int count;
  int numberFiles = 0;
  file theFile;
  unsigned int bytesFree = 0;
  static char *cmdName = "list files";

  // Make sure file name isn't NULL
  if (itemName == NULL)
    return -1;
  
  // Initialize the file structure
  for (count = 0; count < sizeof(file); count ++)
    ((char *) &theFile)[count] = NULL;

  // Call the "find file" routine to see if the file exists
  status = fileFind(itemName, &theFile);
  if (status < 0)
    {
      errno = status;
      perror(cmdName);
      return (status);
    }

  // Get the bytes free for the filesystem
  bytesFree = filesystemGetFree(theFile.filesystem);

  // We do things differently depending upon whether the target is a 
  // file or a directory

  if (theFile.type == fileT)
    {
      // This means the itemName is a single file.  We just output
      // the appropriate information for that file
      printf(theFile.name);

      if (strlen(theFile.name) < 24)
	for (count = 0; count < (26 - strlen(theFile.name)); 
	     count ++)
	  putchar(' ');
      else
	printf("  ");

      // The date and time
      vshPrintDate(theFile.modifiedDate);
      putchar(' ');
      vshPrintTime(theFile.modifiedTime);
      printf("    ");

      // The file size
      printf("%u\n", theFile.size);
    }

  else
    {
      printf("\n  Directory of %s\n", (char *) itemName);

      // Get the first file
      status = fileFirst(itemName, &theFile);
      if ((status != ERR_NOSUCHFILE) && (status < 0))
	{
	  errno = status;
	  perror(cmdName);
	  return (status);
	}

      else if (status >= 0) while (1)
	{
	  printf(theFile.name);

	  if (theFile.type == dirT)
	    putchar('/');
	  else 
	    putchar(' ');

	  if (strlen(theFile.name) < 23)
	    for (count = 0; count < (25 - strlen(theFile.name)); 
		 count ++)
	      putchar(' ');
	  else
	    printf("  ");

	  // The date and time
	  vshPrintDate(theFile.modifiedDate);
	  putchar(' ');
	  vshPrintTime(theFile.modifiedTime);
	  printf("    ");

	  // The file size
	  printf("%u\n", theFile.size);

	  numberFiles += 1;

	  status = fileNext(itemName, &theFile);
	  if (status < 0)
	    break;
	}
      
      printf("  ");

      if (numberFiles == 0)
	printf("No");
      else
	printf("%u", numberFiles);
      printf(" file");
      if ((numberFiles == 0) || (numberFiles > 1))
	putchar('s');

      printf("\n  %u bytes free\n\n", bytesFree);
    }

  return status;
}
