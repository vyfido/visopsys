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
//  vshParseCommand.c
//

// This contains some useful functions written for the shell

#include <sys/vsh.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>

_X_ int vshParseCommand(char *commandLine, char *command, int *argc,
			char *argv[])
{
  // Desc: Attempts to take a raw 'commandLine' string and parse it into a command filename and arguments, suitable for passing to the kernel API functionn loaderLoadAndExec.  The commandLine string will be modified, with NULLs placed at the end of each argument.  'command' must be a buffer suitable for a full filename.  'argc' will receive the number of argument pointers placed in the 'argv' array.  Returns 0 on success, negative otherwise.

  int status = 0;
  file theFile;
  int count;

  // Check params
  if ((commandLine == NULL) || (command == NULL) || (argc == NULL) ||
      (argv == NULL))
    return (status = ERR_NULLPARAMETER);

  *argc = 0;

  // Loop through the command string

  // Now copy each argument, if there are any
  for (count = 0; *commandLine != '\0'; count ++)
    {
      // remove leading whitespace
      while ((*commandLine == ' ') && (*commandLine != '\0'))
	commandLine += 1;

      if (*commandLine == '\0')
	break;

      // If the argument starts with a double-quote, we will discard
      // that character and accept characters (including whitespace)
      // until we hit another double-quote (or the end)
      if (*commandLine != '\"')
	{
	  argv[*argc] = commandLine;

	  // Accept characters until we hit some whitespace (or the end of
	  // the arguments)
	  while ((*commandLine != ' ') && (*commandLine != '\0'))
	    commandLine += 1;
	}
      else
	{
	  // Discard the "
	  commandLine += 1;
	  
	  argv[*argc] = commandLine;

	  // Accept characters  until we hit another double-quote (or the
	  // end of the arguments)
	  while ((*commandLine != '\"') && (*commandLine != '\0'))
	    commandLine += 1;
	}

      *argc += 1;

      if (*commandLine == '\0')
	break;
      *commandLine++ = '\0';
    }
  
  if (strlen(argv[0]) == 0)
    {
      // Nothing
      *argc = 0;
      return (status = 0);
    }

  // We want to check for the case that the user has typed the
  // name of a program (s)he wants to execute

  // If the command is a RELATIVE pathname, we will try inserting the 
  // pwd before it.  This has the effect of always putting '.' in
  // the PATH
  if ((argv[0][0] == '/') || (argv[0][0] == '\\'))
    strcpy(command, argv[0]);
  else
    vshMakeAbsolutePath(argv[0], command);

  // Can we find the file with the name, "as is"?
  status = fileFind(command, &theFile);
  if (status < 0)
    {
      // Not found in the current directory.  Try to search the PATH for
      // the file
      status = vshSearchPath(argv[0], command);
      if (status < 0)
	command[0] = '\0';
    }

  return (status = 0);
}