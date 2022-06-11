// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  libvsh.c
//

// This contains some useful functions written for the shell

#include <stdio.h>
#include <string.h>
#include <sys/vsh.h>
#include <sys/api.h>


void vshPrintTime(unsigned unformattedTime)
{
  int seconds = 0;
  int minutes = 0;
  int hours = 0;

  seconds = (unformattedTime & 0x0000003F);
  minutes = ((unformattedTime & 0x00000FC0) >> 6);
  hours = ((unformattedTime & 0x0003F000) >> 12);

  if (hours < 10)
    putchar('0');
  printf("%u:", hours);
  if (minutes < 10)
    putchar('0');
  printf("%u:", minutes);
  if (seconds < 10)
    putchar('0');
  printf("%u", seconds);

  return;
}


void vshPrintDate(unsigned unformattedDate)
{
  int day = 0;
  int month = 0;
  int year = 0;

  day = (unformattedDate & 0x0000001F);
  month = ((unformattedDate & 0x000001E0) >> 5);
  year = ((unformattedDate & 0xFFFFFE00) >> 9);

  switch(month)
    {
    case 1:
      printf("Jan");
      break;
    case 2:
      printf("Feb");
      break;
    case 3:
      printf("Mar");
      break;
    case 4:
      printf("Apr");
      break;
    case 5:
      printf("May");
      break;
    case 6:
      printf("Jun");
      break;
    case 7:
      printf("Jul");
      break;
    case 8:
      printf("Aug");
      break;
    case 9:
      printf("Sep");
      break;
    case 10:
      printf("Oct");
      break;
    case 11:
      printf("Nov");
      break;
    case 12:
      printf("Dec");
      break;
    default:
      printf("???");
      break;
    }

  putchar(' ');

  if (day < 10)
    putchar('0');
  printf("%u %u", day, year);

  return;
}


int vshFileList(const char *itemName)
{
  // A file listing command

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


int vshDumpFile(const char *fileName)
{
  // Prints the contents of a file to the screen

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


int vshDeleteFile(const char *deleteFile)
{
  // This command is like the "rm" or "del" command. 

  int status = 0;

  // Make sure file name isn't NULL
  if (deleteFile == NULL)
    return -1;
  
  status = fileDelete(deleteFile);
  if (status < 0)
    {
      errno = status;
      perror("delete file");
      return (status);
    }

  // Return success
  return (status = 0);
}


int vshCopyFile(const char *srcFile, const char *destFile)
{
  // This command is like the built-in DOS "copy" command. 
  
  int status = 0;
 
  // Make sure filenames aren't NULL
  if ((srcFile == NULL) || (destFile == NULL))
    return -1;
  
  // Attempt to copy the file
  status = fileCopy(srcFile, destFile);
  if (status < 0)
    {
      errno = status;
      perror("copy file");
      return (status);
    }
 
  // Return success
  return (status = 0);
}
 

int vshRenameFile(const char *srcFile, const char *destFile)
{
  // This command is like the built-in DOS "rename" command. 
 
  int status = 0;
  
  // Make sure filename arguments aren't NULL
  if ((srcFile == NULL) || (destFile == NULL))
    return -1;
   
  // Attempt to rename the file
  status = fileMove(srcFile, destFile);
  if (status < 0)
    {
      errno = status;
      perror("rename file");
      return (status);
    }
 
  // Return success
  return (status = 0);
}


void vshMakeAbsolutePath(const char *orig, char *new)
{
  // Use shared code
  #include "shared/abspath.c"
}


void vshCompleteFilename(char *buffer)
{
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


int vshSearchPath(const char *orig, char *new)
{
  // Use shared code
  #include "shared/srchpath.c"
}
