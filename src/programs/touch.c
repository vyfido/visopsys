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
//  touch.c
//

// This is the UNIX-style command for touching files

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <file1> [file2] [...]\n", name);
  return;
}


static void makeAbsolutePath(const char *orig, char *new)
{
  char cwd[MAX_PATH_LENGTH];

  if ((orig[0] != '/') && (orig[0] != '\\'))
    {
      multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);

      strcpy(new, cwd);

      if ((new[strlen(new) - 1] != '/') &&
	  (new[strlen(new) - 1] != '\\'))
	strncat(new, "/", 1);

      strcat(new, orig);
    }
  else
    strcpy(new, orig);

  return;
}


int main(int argc, char *argv[])
{
  // This command is the "touch" command.  It does one of two things based
  // on the filename argument.  If the file does not exist, it creates a
  // new, empty file.  If the file does exist, it updates the date and time
  // of the file to the current date and time.

  int status = 0;
  int argNumber = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  file theFile;
  int count;


  if (argc < 2)
    {
      usage(argv[0]);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Loop through all of our directory name arguments
  for (argNumber = 1; argNumber < argc; argNumber ++)
    {
      // Make sure the name isn't NULL
      if (argv[argNumber] == NULL)
	return (status = ERR_NULLPARAMETER);

      // Make sure the name is complete
      makeAbsolutePath(argv[argNumber], fileName);

      // Initialize the file structure
      for (count = 0; count < sizeof(file); count ++)
	((char *) &theFile)[count] = NULL;

      // Call the "find file" routine to see if the file exists
      status = fileFind(fileName, &theFile);

      // Now, either the file exists or it doesn't...
      
      if (status < 0)
	{
	  // The file doesn't exist.  We will create the file.
	  status = 
	    fileOpen(fileName, (OPENMODE_WRITE | OPENMODE_CREATE), &theFile);

	  if (status < 0)
	    {
	      errno = status;
	      perror(argv[0]);
	      return (status);
	    }

	  // Now close the file
	  fileClose(&theFile);
	}

      else
	{
	  // The file exists.  We need to update the date and time of the file
	  status = fileTimestamp(fileName);

	  if (status < 0)
	    {
	      errno = status;
	      perror(argv[0]);
	      return (status);
	    }
	}
    }

  // Return success
  return (status = 0);
}
