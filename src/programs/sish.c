//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
//  sish.c
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/api.h>


#define SIMPLESHELLPROMPT "> "
#define MAX_ARGS 100
#define MAX_STRING_LENGTH 100
#define COMMANDHISTORY 10
#define MAX_ENVVAR_LENGTH MAX_STRING_LENGTH

static char cwd[MAX_PATH_LENGTH];


static void printTime(unsigned int unformattedTime)
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


static void printDate(unsigned int unformattedDate)
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


static int shellCommandsDir(char *name, const char *item)
{
  // A file listing command

  int status = 0;
  int count;
  int numberFiles = 0;
  file theFile;
  unsigned int bytesFree = 0;


  // Make sure file name isn't NULL
  if (item == NULL)
    return -1;
  
  // Initialize the file structure
  for (count = 0; count < sizeof(file); count ++)
    ((char *) &theFile)[count] = NULL;

  // Call the "find file" routine to see if the file exists
  status = fileFind(item, &theFile);
  
  if (status < 0)
    {
      errno = status;
      perror(name);
      return (status);
    }

  // Get the bytes free for the filesystem
  bytesFree = filesystemGetFree(theFile.filesystem);

  // We do things differently depending upon whether the target is a 
  // file or a directory

  if (theFile.type == fileT)
    {
      // This means the item is a single file.  We just output
      // the appropriate information for that file
      printf(theFile.name);

      if (strlen(theFile.name) < 24)
	for (count = 0; count < (26 - strlen(theFile.name)); 
	     count ++)
	  putchar(' ');
      else
	printf("  ");

      // The date and time
      printDate(theFile.modifiedDate);
      putchar(' ');
      printTime(theFile.modifiedTime);
      printf("    ");

      // The file size
      printf("%u\n", theFile.size);
    }

  else
    {
      printf("\n  Directory of %s\n", (char *) item);

      // Get the first file
      status = fileFirst(item, &theFile);
  
      if (status < 0)
	{
	  errno = status;
	  perror(name);
	  return (status);
	}

      while (1)
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
	  printDate(theFile.modifiedDate);
	  putchar(' ');
	  printTime(theFile.modifiedTime);
	  printf("    ");

	  // The file size
	  printf("%u\n", theFile.size);

	  numberFiles += 1;

	  status = fileNext(item, &theFile);

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


static int shellCommandsType(const char *fileName)
{
  // Prints the contents of a file to the screen

  int status = 0;
  int count;
  file theFile;
  char *fileBuffer = NULL;


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
      perror("cat");
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
  fileBuffer = memoryRequestBlock(((theFile.blocks * theFile.blockSize) + 1),
				  0, "temporary file data");
  if (fileBuffer == NULL)
    {
      errno = ERR_MEMORY;
      perror("cat");
      return (status = ERR_MEMORY);
    }

  status = fileOpen(fileName, OPENMODE_READ, &theFile);

  if (status < 0)
    {
      errno = status;
      perror("cat");
      memoryReleaseByPointer(fileBuffer);
      return (status);
    }

  status = fileRead(&theFile, 0, theFile.blocks, fileBuffer);

  if (status < 0)
    {
      errno = status;
      perror("cat");
      memoryReleaseByPointer(fileBuffer);
      return (status);
    }


  // Print the file
  count = 0;
  while (count < theFile.size)
    {
      // Look out for tab characters
      if (fileBuffer[count] == (char) 9)
	printf("\t");

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
  memoryReleaseByPointer(fileBuffer);

  return status;
}


static int shellCommandsDeleteFile(const char *deleteFile)
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
      perror("rm");
      return (status);
    }

  // Return success
  return (status = 0);
}


static int shellCommandsCopyFile(char *name, const char *srcFile,
				 const char *destFile)
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
      perror(name);
      return (status);
    }
 
  // Return success
  return (status = 0);
}
 

static int shellCommandsRenameItem(const char *name, const char *srcFile,
				   const char *destFile)
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
      perror(name);
      return (status);
    }
 
  // Return success
  return (status = 0);
}


static void showPrompt(void)
{
  // This routine puts a prompt on the screen

  char cwd[MAX_PATH_LENGTH];

  multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);
  printf(cwd);
  printf(SIMPLESHELLPROMPT);

  return;
}


static void makeAbsolutePath(const char *orig, char *new)
{
  
  if ((orig[0] != '/') && (orig[0] != '\\'))
    {
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


static void completeFilename(char *buffer)
{
  int status = 0;
  char prefixPath[MAX_PATH_LENGTH];
  char filename[MAX_NAME_LENGTH];
  char matchname[MAX_NAME_LENGTH];
  int lastSeparator = -1;
  file aFile;
  int match = 0;
  int longestMatch = 0;
  int count;


  prefixPath[0] = NULL;
  filename[0] = NULL;
  matchname[0] = NULL;

  // Does the buffer name begin with a separator?  If not, we need to
  // prepend the cwd
  if ((buffer[0] != '/') &&
      (buffer[0] != '\\'))
    strcpy(prefixPath, cwd);

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
    {
      // Copy the whole buffer into the filename string
      strcpy(filename, buffer);
    }

  // Don't bother going any further if filename is empty
  if (filename[0] == '\0')
    return;

  // Now, prefixPath must have something in it.  Preferably this is the
  // name of the last directory of the path we're searching.  Try to look
  // it up
  status = fileFind(prefixPath, &aFile);
  
  if (status < 0)
    return;

  for (count = 0; ; count ++)
    {
      if (!count)
	{
	  // Get the first file of the directory
	  status = fileFirst(prefixPath, &aFile);

	  if (status < 0)
	    return;
	}
      else
	{
	  // Get the next file of the directory
	  status = fileNext(prefixPath, &aFile);

	  if (status < 0)
	    break;
	}

      
      // Does this match a substring?
      if (strstr(aFile.name, filename) == aFile.name)
	{
	  match = strlen(filename);
	  
	  if (match == strlen(aFile.name))
	    {
	      // We have an exact match
	      strcpy((buffer + lastSeparator + 1), aFile.name);
	      return;
	    }

	  else
	    {
	      // We have a substring match
	      if (match == longestMatch)
		{
		  // This file matches an equal substring, and thus there
		  // is no unique file that can complete this filename.
		  return;
		}
	      else if (match > longestMatch)
		{
		  // This is the closest match so far
		  strcpy(matchname, aFile.name);
		  longestMatch = match;
		}
	    }
	}
    }

  // If we fall through, then the longest match so far wins.
  if (longestMatch)
    strcpy((buffer + lastSeparator + 1), matchname);

  return;
}


static int searchPath(const char *orig, char *new)
{
  // First of all, we won't search the path if this is an absolute
  // pathname.  That would be pointless

  int status = 0;
  char path[1024];
  int pathCount = 0;
  char pathElement[MAX_PATH_NAME_LENGTH];
  int pathElementCount = 0;
  file theFile;

  
  if ((orig[0] == '/') || (orig[0] == '\\'))
    return (status = ERR_NOSUCHFILE);

  // Get the value of the PATH environment variable
  status = environmentGet("PATH", path, 1024);

  if (status < 0)
    return (status);

  pathCount = 0;

  // We need to loop once for each element in the PATH.  Elements are
  // separated by colon characters.  When we hit a NULL character we are
  // at the end.
  
  while(path[pathCount] != '\0')
    {
      pathElementCount = 0;

      // Copy characters from the path until we hit either a ':' or a NULL
      while ((path[pathCount] != ':') && (path[pathCount] != '\0'))
	{
	  pathElement[pathElementCount++] = path[pathCount++];
	  pathElement[pathElementCount] = '\0';
	}
      
      if (path[pathCount] == ':')
	pathCount++;

      // Append the name to the path
      strncat(pathElement, "/", 1);
      strcat(pathElement, orig);

      // Does the file exist in the PATH directory?
      status = fileFind(pathElement, &theFile);
      
      // Did we find it?
      if (status >= 0)
	{
	  // Copy the full path into the buffer supplied
	  strcpy(new, pathElement);

	  // Return success
	  return (status = 0);
	}
    }

  // If we fall through, no dice
  return (status = ERR_NOSUCHFILE);
}


static void interpretCommand(char *commandLine)
{
  int status = 0;
  int bufferCounter = 0;
  int argc = 0;
  char *argv[MAX_ARGS];
  char argv_memory[MAX_ARGS * MAX_STRING_LENGTH];
  char fileName[MAX_PATH_NAME_LENGTH];
  char srcName[MAX_PATH_NAME_LENGTH];
  char destName[MAX_PATH_NAME_LENGTH];
  char getEnvBuff[MAX_ENVVAR_LENGTH];
  file theFile;
  int count, count2;
  int temp = 0;


  // Initialize the strings

  for (count = 0; count < MAX_ARGS; count ++)
    argv[count] = (argv_memory + (count * MAX_STRING_LENGTH));
  for (count = 0; count < MAX_ARGS; count ++)
      for (count2 = 0; count2 < MAX_STRING_LENGTH; count2 ++)
	argv[count][count2] = (char) 0;

  for (count = 0; count < MAX_PATH_NAME_LENGTH; count ++)
    fileName[count] = (char) 0;
  

  // We have to separate the command and arguments into an array of
  // strings

  // Now copy each argument, if there are any
  for (count = 0; commandLine[0] != NULL; count ++)
    {
      // remove leading whitespace
      while ((commandLine[0] == ' ') && (commandLine[0] != NULL))
	commandLine += 1;

      if (commandLine[0] == NULL)
	break;

      bufferCounter = 0;

      // If the argument starts with a double-quote, we will discard
      // that character and copy characters (including whitespace)
      // until we hit another double-quote (or the end)
      if (commandLine[0] != '\"')
	{
	  // Copy the argument into argv[argc] until we hit some
	  // whitespace (or the end of the arguments)
	  while ((commandLine[0] != ' ') && (commandLine[0] != NULL))
	    {
	      argv[argc][bufferCounter++] = commandLine[0];
	      commandLine += 1;
	    }
	  argv[argc][bufferCounter] = NULL;
	}
      else
	{
	  // Discard the "
	  commandLine += 1;
	  
	  // Copy the argument into argv[argc] until we hit another
	  // double-quote (or the end of the arguments)
	  while ((commandLine[0] != '\"') && (commandLine[0] != NULL))
	    {
	      argv[argc][bufferCounter++] = commandLine[0];
	      commandLine += 1;
	    }
	  argv[argc][bufferCounter] = NULL;
	  
	  if (commandLine[0] != NULL)
	    // Discard the "
	    commandLine += 1;
	}

      argc += 1;
    }
  

  // Try to match the command with the list of known commands
  
  if (!strcmp(argv[0], "help"))
    {
      printf(" -- List of commands --\n");
      printf("uname:        Tells kernel version\n");
      printf("reboot:       Exits to real mode and reboots the computer\n");
      printf("shutdown:     Stops the computer\n");
      printf("uptime:       Time since last boot\n");
      printf("date:         The date\n");
      printf("ps:           Show list of current processes\n");
      printf("renice:       Change the priority of a running process\n");
      printf("kill:         Kill a running process\n");
      printf("mount/umount: Mount or unmount a filesystem\n");
      printf("sync:         Synchronize all filesystems\n");
      printf("cd:           Change directory\n");
      printf("pwd:          Show the current directory\n");
      printf("ls/dir:       Show the files in a directory\n");
      printf("cat/type:     Dump a file's contents to the screen\n");      
      printf("more:         Read a file one screenfull at a time\n");
      printf("touch:        Update a file or create a new (empty) file\n");
      printf("rm/del:       Delete a file\n");      
      printf("cp/copy:      Copy a file\n");      
      printf("mv/move:      Move a file (Also ren/rename, same thing)\n");    
      printf("mkdir/rmdir:  Make or remove a directory\n");
      printf("truncate:     Truncate a file to zero length\n");
      printf("logout/exit:  End the current session\n\n");
    }

  else if (!strcmp(argv[0], "pwd"))
    printf("%s\n", cwd);

  else if (!strcmp(argv[0], "cd"))
    {
      if (argc > 1)
	makeAbsolutePath(argv[1], fileName);

      else
	// No arg means / for now
	strncpy(fileName, "/", 1);

      // Fix up the cwd and make it official
      fileFixupPath(fileName, cwd);
      temp = multitaskerSetCurrentDirectory(cwd);

      // Were we successful?  If not, call back the multitasker to set it
      // back to the real cwd and make an error message
      if (temp < 0)
	{
	  errno = temp;
	  perror("cd");
	  multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);
	}
    }

  else if (!strcmp(argv[0], "dir") || !strcmp(argv[0], "ls"))
    {
      // Built in file-listing commands
      if (argc == 1)
	shellCommandsDir(argv[0], cwd);

      else
	for (count = 1; count < argc; count ++)
	  {
	    // If any of the arguments are RELATIVE pathnames, we should
	    // insert the pwd before it

	    makeAbsolutePath(argv[count], fileName);
	    shellCommandsDir(argv[0], fileName);
	  }
    }

  else if (!strcmp(argv[0], "type"))
    {
      // We want to dump the file to the screen
      if (argc > 1)
	{
	  for (count = 1; count < argc; count ++)
	    {
	      // If any of the arguments are RELATIVE pathnames, we should
	      // insert the pwd before it
	      makeAbsolutePath(argv[count], fileName);
	      shellCommandsType(fileName);
	    }
	}
      else
	{
	  errno = ERR_ARGUMENTCOUNT;
	  perror(argv[0]);
	  printf("Usage: %s <file1> [file2] [...]\n", argv[0]);
	}
    }

  else if (!strcmp(argv[0], "del"))
    {
      if (argc > 1)
	for (count = 1; count < argc; count ++)
	  {
	    // If any of the arguments are RELATIVE pathnames, we should
	    // insert the pwd before it
	    makeAbsolutePath(argv[count], fileName);
	    shellCommandsDeleteFile(fileName);
	  }
      
      else
	{
	  errno = ERR_ARGUMENTCOUNT;
	  perror(argv[0]);
	  printf("Usage: %s <file1> [file2] [...]\n", argv[0]);
	}
    }

  else if (!strcmp(argv[0], "copy"))
    {
      if (argc > 2)
	{
	  // If any of the arguments are RELATIVE pathnames, we should
	  // insert the pwd before it
	  makeAbsolutePath(argv[1], srcName);
	  makeAbsolutePath(argv[2], destName);
	  shellCommandsCopyFile(argv[0], srcName, destName);
	}
      else
	{
	  errno = ERR_ARGUMENTCOUNT;
	  perror(argv[0]);
	  printf("Usage: %s <source file> <destination file>\n", argv[0]);
	}
    }

  else if (!strcmp(argv[0], "ren") ||
	   !strcmp(argv[0], "rename"))
    {
      if (argc > 2)
	{
	  // If any of the arguments are RELATIVE pathnames, we should
	  // insert the pwd before it
	  makeAbsolutePath(argv[1], srcName);
	  makeAbsolutePath(argv[2], destName);
	  shellCommandsRenameItem(argv[0], srcName, destName);
	}
      else
	{
	  errno = ERR_ARGUMENTCOUNT;
	  perror(argv[0]);
	  printf("Usage: %s <source file> <destination file>\n", argv[0]);
	}
    }

  else if (!strcmp(argv[0], "getenv"))
    {
      if (argc == 2)
	{
	  status = environmentGet(argv[1], getEnvBuff, MAX_ENVVAR_LENGTH);

	  if (status < 0)
	    {
	      errno = status;
	      perror(argv[0]);
	    }
	  else
	    {
	      printf("%s\n", getEnvBuff);
	    }
	}
      
      else
	{
	  errno = ERR_ARGUMENTCOUNT;
	  perror(argv[0]);
	  printf("Usage: %s <variable_name>\n", argv[0]);
	}
    }

  else if (!strcmp(argv[0], "setenv"))
    {
      if (argc == 3)
	{
	  if (strlen(argv[2]) > MAX_ENVVAR_LENGTH)
	    printf("Shouldn't set an env variable that long\n");

	  status = environmentSet(argv[1], argv[2]);

	  if (status < 0)
	    {
	      errno = status;
	      perror(argv[0]);
	    }
	}
      
      else
	{
	  errno = ERR_ARGUMENTCOUNT;
	  perror(argv[0]);
	  printf("Usage: %s <variable_name> <variable_value>\n", argv[0]);
	}
    }

  else if (!strcmp(argv[0], "unsetenv"))
    {
      if (argc == 2)
	{
	  status = environmentUnset(argv[1]);

	  if (status < 0)
	    {
	      errno = status;
	      perror(argv[0]);
	    }
	}
      
      else
	{
	  errno = ERR_ARGUMENTCOUNT;
	  perror(argv[0]);
	  printf("Usage: %s <variable_name>\n", argv[0]);
	}
    }

  else if (!strcmp(argv[0], "printenv"))
    {
      environmentDump();
    }

  else
    {
      // We want to check for the case that the user has typed the
      // name of a program he wants to execute

      // If the command is a RELATIVE pathname, we will try inserting the 
      // pwd before it.  This has the effect of always putting '.' in
      // the PATH
      makeAbsolutePath(argv[0], fileName);

      // Does the file exist?
      status = fileFind(fileName, &theFile);

      // Did we find it?
      if (status < 0)
	{
	  // Let's try searching the PATH for the file instead
	  status = searchPath(argv[0], fileName);

	  if (status < 0)
	    {
	      errno = status;
	      perror(argv[0]);
	      return;
	    }
	}

      if ((argc > 1) && (argv[argc - 1][0] == '&'))
	loaderLoadAndExec(fileName, argc, argv, 0 /* no block */);
      else
	loaderLoadAndExec(fileName, argc, argv, 1 /* block */);
    }

  return;
}


static void simpleShell(void)
{
  // This is a very simple command shell intended only for development
  // purpose, although it might conceivably be used as a basis for a later
  // version of a real shell.

  char *commandBuffer = NULL;
  char commandHistory[COMMANDHISTORY][256];
  int currentCommand = 0;
  int selectedCommand = 0;
  unsigned char bufferCharacter;
  static int currentCharacter = 0;
  int count1, count2;


  for (count1 = 0; count1 < COMMANDHISTORY; count1 ++)
    for (count2 = 0; count2 < 256; count2++)
       commandHistory[count1][count2] = '\0';

  // Start at the first command buffer
  commandBuffer = commandHistory[0];

  // This program runs in an infinite loop
  while(1)
    {
      // There might be nothing to do...  No keyboard input?
      count1 = textInputCount();

      if (count1 < 0)
	{
	  // Eek.  We can't get input.  Quit.
	  errno = count1;
	  perror("sish");
	  return;
	}

      if (count1 == 0)
	{
	  // For good form, yield the remainder of the current timeslice
	  // back to the scheduler
	  multitaskerYield();
	  continue;
	}

      bufferCharacter = getchar();

      if (errno)
	{
	  // Eek.  We can't get input.  Quit.
	  perror("sish");
	  return;
	}

      if (bufferCharacter == (unsigned char) 0xFF)
	{
	  // We're expecting there to be two characters, however the second
	  // one might not have arrived yet.  If not, return and we'll get
	  // them both next time:
	  if (!textInputCount())
	    continue;
	  
	  bufferCharacter = getchar();

	  if (errno)
	    {
	      // Eek.  We can't get input.  Quit.
	      perror("sish");
	      return;
	    }

	  // These numbers are scan codes, since the reason they're behind
	  // this FF marker is that they have no equivalent ASCII
	  // representation.
	  
	  if (bufferCharacter == (unsigned char) 72)
	    {
	      // G's addition - bash style press up + repeat the last command
	      // This up cursor movement allows users to cycle backwards
	      // through their command history

	      if ((selectedCommand > 0) &&
		  ((selectedCommand - 1) != currentCommand) &&
		  (commandHistory[selectedCommand - 1][0] != '\0'))
		selectedCommand -= 1;

	      else if ((selectedCommand == 0) &&
		       (currentCommand != (COMMANDHISTORY - 1)) &&
		       (commandHistory[COMMANDHISTORY - 1][0] != '\0'))
		selectedCommand = (COMMANDHISTORY - 1);

	      else
		continue;

	      // Delete the previous command from the command line
	      for (count1 = currentCharacter; count1 > 0; count1 --)
		textBackSpace();

	      // Copy the contents of the selected command into the current
	      // command
	      strcpy(commandBuffer, commandHistory[selectedCommand]);
	      // Print result to the screen
	      printf(commandBuffer);
	      // Correct currentCharacter length so that
	      // it's as if we've typed it ourselves.
	      currentCharacter = strlen(commandBuffer);
	    }
	  
	  else if (bufferCharacter == (unsigned char) 80)
	    {
	      // This down cursor movement allows users to cycle forwards
	      // through their command history

	      if (selectedCommand == currentCommand)
		continue;

	      else if (((selectedCommand < (COMMANDHISTORY - 1)) &&
			((selectedCommand + 1) == currentCommand)) ||
		       ((selectedCommand == (COMMANDHISTORY - 1)) &&
			(currentCommand == 0)))
		{
		  selectedCommand = currentCommand;
		  commandBuffer[0] = '\0';
		  // Delete the previous command from the command line
		  for (count1 = currentCharacter; count1 > 0; count1 --)
		    textBackSpace();
		  currentCharacter = 0;
		  continue;
		}

	      else if ((selectedCommand < (COMMANDHISTORY - 1)) &&
		       ((selectedCommand + 1) != currentCommand) &&
		       (commandHistory[selectedCommand + 1][0] != '\0'))
		selectedCommand += 1;

	      else if ((selectedCommand == (COMMANDHISTORY - 1)) &&
		       (currentCommand != 0) &&
		       (commandHistory[0][0] != '\0'))
		selectedCommand = 0;

	      else
		continue;

	      // Delete the previous command from the command line
	      for (count1 = currentCharacter; count1 > 0; count1 --)
		textBackSpace();

	      // Copy the contents of the selected command into the current
	      // command
	      strcpy(commandBuffer, commandHistory[selectedCommand]);
	      // Print result to the screen
	      printf(commandBuffer);
	      // Correct currentCharacter length so that
	      // it's as if we've typed it ourselves.
	      currentCharacter = strlen(commandBuffer);
	    }
	  
	  else if (bufferCharacter == (unsigned char) 75)
	    textCursorLeft();
	  
	  else if (bufferCharacter == (unsigned char) 77)
	    textCursorRight();
	  
	  else if (bufferCharacter == (unsigned char) 71)
	    {
	      // Clear the screen
	      textClearScreen();
	      
	      // Show a new prompt
	      showPrompt();
	    }
	}
      
      // The rest of these numbers are ASCII codes.
      
      else if (bufferCharacter == (unsigned char) 8)
	{
	  if (currentCharacter > 0)
	    {
	      textBackSpace();
      
	      // Move the current character back by 1
	      currentCharacter--;
	      commandBuffer[currentCharacter] = '\0';
	    }
	}

      else if (bufferCharacter == (unsigned char) 9)
	{
	  // Tab.  Attempt to complete a filename.
	  for (count1 = (strlen(commandBuffer)); count1 >= 0; count1 --)
	    if (commandBuffer[count1] == '\"')
	      {
		count1++;
		break;
	      }
	  if (count1 < 0)
	    for (count1 = (strlen(commandBuffer)); count1 >= 0; count1 --)
	      if (commandBuffer[count1] == ' ')
		{
		  count1++;
		  break;
		}
	  if (count1 < 0)
	    count1 = 0;
	  completeFilename(commandBuffer + count1);
	  printf("\n");
	  showPrompt();
	  printf(commandBuffer);
	  currentCharacter = strlen(commandBuffer);
	}
      
      else if (bufferCharacter == (unsigned char) 10)
	{
	  // Print a newline
	  printf("\n");
  
	  // Put a null in at the end of the command buffer
	  commandBuffer[currentCharacter] = '\0';
	  
	  // Now we interpret the command
	  if (currentCharacter > 0)
	    {
	      if (!strcmp(commandBuffer, "logout") ||
		  !strcmp(commandBuffer, "exit"))
		return;
	      else
		interpretCommand(commandBuffer);

	      // We move to the next command buffer
	      if (currentCommand < (COMMANDHISTORY - 1))
		currentCommand++;
	      else
		currentCommand = 0;

	      selectedCommand = currentCommand;

	      commandBuffer = commandHistory[currentCommand];
	      commandBuffer[0] = '\0';
	    }

	  // Set the current character to 0
	  currentCharacter = 0;
	  
	  // Show a new prompt
	  showPrompt();
	}
      
      else
	{
	  // Add the current character to the command buffer and
	  // increment the current character count
	  commandBuffer[currentCharacter++] = bufferCharacter;
	  commandBuffer[currentCharacter] = '\0';

	  // Put it on the screen
	  putchar(bufferCharacter);
	}
    }

  // This program never returns unless the user does 'logout'
}


int main(void)
{
  // This initializes the simple shell

  int status = 0;


  // Make a message
  printf("\nVisopsys Simple Kernel-testing Shell.\n");
  printf("Type \"help\" for commands.\n");

  // Get the starting current directory
  status = multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);

  if (status < 0)
    {
      printf("can't determine current directory\n");
      return (-1);
    }

  // Set any default environment variables
  environmentSet("PATH", "/programs");
  
  // Show a prompt
  showPrompt();

  // Run
  simpleShell();

  // We'll end up here at 'logout'
  printf("exiting.\n");

  return status;
}
