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
//  help.c
//

// This is like the UNIX-style 'man' command for showing documentation

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>

#define HELPFILES_DIR  "/programs/helpfiles"


int main(int argc, char *argv[])
{
  int status = 0;
  char command[MAX_PATH_NAME_LENGTH];
  file tmpFile;
  int count;

  if (argc < 2)
    // If there are no arguments, print the general help file
    status = system("more " HELPFILES_DIR "/help.txt");

  else
    {
      for (count = 1; count < argc; count ++)
	{
	  // See if there is a help file for the argument
	  sprintf(command, "%s/%s.txt", HELPFILES_DIR, argv[count]);
	  status = fileFind(command, &tmpFile);
	  if (status < 0)
	    {
	      // No help file
	      printf("There is no help available for \"%s\"\n", argv[count]);
	      return (status = ERR_NOSUCHFILE);
	    }

	  // For each argument, look for a help file whose name matches
	  sprintf(command, "more %s/%s.txt", HELPFILES_DIR, argv[count]);

	  // Search
	  status = system(command);
	  if (status < 0)
	    break;
	}
    }

  return (status);
}
