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
//  vshCompleteFilename.c
//

// This contains some useful functions written for the shell

#include <string.h>
#include <sys/vsh.h>
#include <sys/api.h>


_X_ void vshCompleteFilename(char *buffer)
{
  // Desc: Attempts to complete a portion of a filename, 'buffer'.  The function will append either the remainder of the complete filename, or if possible, some portion thereof.  The result simply depends on whether a good completion or partial completion exists.  'buffer' must of course be large enough to contain any potential filename completion.

  int status = 0;
  char cwd[MAX_PATH_LENGTH];
  char prefixPath[MAX_PATH_LENGTH];
  char filename[MAX_NAME_LENGTH];
  int filenameLength = 0;
  char matchname[MAX_NAME_LENGTH];
  int lastSeparator = -1;
  file aFile;
  int match = 0;
  int longestMatch = 0;
  int longestIsDir = 0;
  int prefixLength;
  int count;

  prefixPath[0] = NULL;
  filename[0] = NULL;
  matchname[0] = NULL;

  // Does the buffer name begin with a separator?  If not, we need to
  // prepend the cwd
  if ((buffer[0] != '/') &&
      (buffer[0] != '\\'))
    {
      // Get the current directory
      multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);

      strcpy(prefixPath, cwd);

      prefixLength = strlen(prefixPath);
      if ((prefixPath[prefixLength - 1] != '/') &&
	  (prefixPath[prefixLength - 1] != '\\'))
	strncat(prefixPath, "/", 1);
    }

  // We should now have an absolute path up to the cwd

  // Find the last occurrence of a separator character
  for (count = (strlen(buffer) - 1); count >= 0 ; count --)
    {
      if ((buffer[count] == '/') ||
	  (buffer[count] == '\\'))
	{
	  lastSeparator = count;
	  break;
	}
    }

  // If there was a separator, append it and everything before it to
  // prefixPath and copy everything after it into filename
  if (count >= 0)
    {
      strncat(prefixPath, buffer, (lastSeparator + 1));
      strcpy(filename, (buffer + lastSeparator + 1));
    }
  else
    // Copy the whole buffer into the filename string
    strcpy(filename, buffer);

  filenameLength = strlen(filename);

  // Now, prefixPath must have something in it.  Preferably this is the
  // name of the last directory of the path we're searching.  Try to look
  // it up
  status = fileFind(prefixPath, &aFile);
  if (status < 0)
    // The directory doesn't exist
    return;

  // Get the first file of the directory
  status = fileFirst(prefixPath, &aFile);
  if (status < 0)
    // No files in the directory
    return;

  // If filename is empty, and there is only one non-'.' or '..' entry,
  // complete that one
  if (filenameLength == 0)
    {
      while (!strcmp(aFile.name, ".") || !strcmp(aFile.name, ".."))
	{
	  status = fileNext(prefixPath, &aFile);
	  if (status < 0)
	    return;
	}

      file tmpFile;
      memcpy(&tmpFile, &aFile, sizeof(file));
      if (fileNext(prefixPath, &tmpFile) < 0)
	{
	  strcpy((buffer + lastSeparator + 1), aFile.name);
	  if (aFile.type == dirT)
	    strcat((buffer + lastSeparator + 1), "/");
	}
      return;
    }

  while (1)
    {
      match = strspn(filename, aFile.name);

      // File match some part of our current file (but not if the thing to
      // complete is longer than the filename)?
      if (match && (match >= filenameLength))
	{
	  if (match == longestMatch)
	    {
	      // We have a multiple substring match.  This file matches a
	      // substring of equal length to that of another file, and thus
	      // there are multiple filenames that can complete this filename.
	      // Terminate the match string after the point that matches
	      // multiple files and quit.
	      int tmp = strspn(matchname, aFile.name);
	      strncpy(matchname, aFile.name, tmp);
	      matchname[tmp] = '\0';
	      longestIsDir = 0;
	    }
	  else if (match > longestMatch)
	    {
	      // This is the mew longest match so far
	      longestMatch = match;
	      strcpy(matchname, aFile.name);
	      if (aFile.type == dirT)
		longestIsDir = 1;
	      else
		longestIsDir = 0;
	    }
	}

      // Get the next file of the directory
      status = fileNext(prefixPath, &aFile);
      if (status < 0)
	break;
    }

  // If we fall through, then the longest match so far wins.
  if (longestMatch)
    {
      strcpy((buffer + lastSeparator + 1), matchname);
      if (longestIsDir)
	strcat((buffer + lastSeparator + 1), "/");
    }

  return;
}
