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
//  system.c
//

// This is the standard "system" function, as found in standard C libraries.
// Unlike UNIX it does not execute a shell program to run the command, but
// rather passes the command and arguments straight to the kernel's loader.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/errors.h>

#define MAX_ARGS 100


static void removeWhitespace(char **ptr)
{
  // Place NULLS in whitespace and increment ptr till it points to non-
  // whitespace
  while (*ptr[0] && ((*ptr[0] == ' ') || (*ptr[0] == '\t')))
    {
      *ptr[0] = '\0';
      *ptr += 1;
    }
}


static void makeAbsolutePath(const char *orig, char *new)
{
  // Use shared code
  #include "../shared/abspath.c"
}


static int searchPath(const char *orig, char *new)
{
  // Use shared code
  #include "../shared/srchpath.c"
}


int system(const char *string)
{
  int status = 0;
  char *buffer = NULL;
  char bufferspace[MAXSTRINGLENGTH];
  char *tmp = NULL;
  int argc = 0;
  char *argv[MAX_ARGS];
  char command[MAX_PATH_NAME_LENGTH];
  file theFile;
  int privilege = 0;
  int count;

  // Check params
  if (string == NULL)
    return (status = ERR_NULLPARAMETER);

  // Copy the supplied string into our buffer 
  strncpy(bufferspace, string, MAXSTRINGLENGTH);
  buffer = bufferspace;
 
  // We have to separate the command and arguments into an array of
  // strings

  // Now loop for each argument, if there are any
  for (count = 0; buffer[0] != NULL; count ++)
    {
      // remove leading whitespace
      removeWhitespace(&buffer);

      if (buffer[0] == NULL)
        break;

      tmp = buffer;

      // If the argument starts with a double-quote, we will skip characters
      // (including whitespace) until we hit another double-quote (or the end).
      // Otherwise, look for whitespace.

      if (buffer[0] != '\"')
	{
	  // Skip until we hit some whitespace (or the end of the arguments)
	  while (buffer[0] && (buffer[0] != ' ') && (buffer[0] != '\t'))
	    buffer += 1;
	}
      else
	{
	  // Discard the "
	  buffer += 1;
	  
	  // Skip until we hit another double-quote (or the end of the
          // arguments)
	  while (buffer[0] && (buffer[0] != '\"'))
	    buffer += 1;
	  
	  if (buffer[0] != NULL)
	    // Discard the "
	    buffer += 1;
	}

      argv[argc] = tmp;
      argc += 1;
    }
  
  // Try to make the command be an absolute pathname
  makeAbsolutePath(argv[0], command);

  // Does the file exist?
  status = fileFind(command, &theFile);
  if (status < 0)
    {
      // Not found in the current directory.  Let's try searching the
      // PATH for the file instead
      status = searchPath(argv[0], command);
      if (status < 0)
        {
          // Not found
          errno = status;
          return (status);
        }
    }

  if (!strlen(command))
    // Nothing
    return (status = ERR_NODATA);

  // What is my privilege level?
  privilege = multitaskerGetProcessPrivilege(multitaskerGetCurrentProcessId());
  if (privilege < 0)
    return (status = privilege);

  // Shift the arg list down by one, as the exec function will prepend
  // it when starting the program
  for (count = 1; count < argc; count ++)
    argv[count - 1] = argv[count];
  argc -= 1;

  // Try to execute the command
  status = loaderLoadAndExec(command, privilege, argc, argv, 1 /* block */);

  return (status);
}
