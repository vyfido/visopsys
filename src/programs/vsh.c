//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  vsh.c
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/file.h>
#include <sys/api.h>

#define SIMPLESHELLPROMPT "> "
#define MAX_ARGS 100
#define MAX_STRING_LENGTH 100
#define COMMANDHISTORY 10
#define MAX_ENVVAR_LENGTH MAX_STRING_LENGTH
#define MAX_LINELENGTH 256

static int myProcId, myPrivilege;
static char cwd[MAX_PATH_LENGTH];
static int promptCatchup = 0;


static void showPrompt(void)
{
  char tmpPath[MAX_PATH_LENGTH];
  char tmpFile[MAX_NAME_LENGTH];
  
  // If there are characters already in the input buffer, tell the reader
  // routine to put them after the prompt
  if (textInputCount())
    promptCatchup = 1;

  // This routine puts a prompt on the screen
  multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);
  fileSeparateLast(cwd, tmpPath, tmpFile);
  if (tmpFile[0] == '\0')
    strcpy(tmpFile, "/");
  printf("%s%s", tmpFile, SIMPLESHELLPROMPT);

  return;
}


static void interpretCommand(char *commandLine)
{
  int status = 0;
  int argc = 0;
  char *argv[MAX_ARGS];
  char fileName1[MAX_PATH_NAME_LENGTH];
  char fileName2[MAX_PATH_NAME_LENGTH];
  char getEnvBuff[MAX_ENVVAR_LENGTH];
  int temp = 0;
  int block = 1;
  int count;

  // Initialize stack memory
  for (count = 0; count < MAX_ARGS; count ++)
    argv[count] = NULL;
  fileName1[0] = '\0';
  fileName2[0] = '\0';

  // We have to separate the command and arguments into an array of
  // strings
  status = vshParseCommand(commandLine, fileName1, &argc, argv);
  if (status < 0)
    return;

  // Try to match the command with the list of built-in commands

  if (!strcmp(argv[0], "pwd"))
    printf("%s\n", cwd);

  else if (!strcmp(argv[0], "cd"))
    {
      if (argc > 1)
	vshMakeAbsolutePath(argv[1], fileName1);

      else
	// No arg means / for now
	strncpy(fileName1, "/", 2);

      // Fix up the cwd and make it official
      fileFixupPath(fileName1, cwd);
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
	vshFileList(cwd);

      else
	for (count = 1; count < argc; count ++)
	  {
	    // If any of the arguments are RELATIVE pathnames, we should
	    // insert the pwd before it
	    vshMakeAbsolutePath(argv[count], fileName1);
	    vshFileList(fileName1);
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
	      vshMakeAbsolutePath(argv[count], fileName1);
	      vshDumpFile(fileName1);
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
	    vshMakeAbsolutePath(argv[count], fileName1);
	    vshDeleteFile(fileName1);
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
	  vshMakeAbsolutePath(argv[1], fileName1);
	  vshMakeAbsolutePath(argv[2], fileName2);
	  vshCopyFile(fileName1, fileName2);
	}
      else
	{
	  errno = ERR_ARGUMENTCOUNT;
	  perror(argv[0]);
	  printf("Usage: %s <source file> <destination file>\n", argv[0]);
	}
    }

  else if (!strcmp(argv[0], "ren") || !strcmp(argv[0], "rename"))
    {
      if (argc > 2)
	{
	  // If any of the arguments are RELATIVE pathnames, we should
	  // insert the pwd before it
	  vshMakeAbsolutePath(argv[1], fileName1);
	  vshMakeAbsolutePath(argv[2], fileName2);
	  vshRenameFile(fileName1, fileName2);
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

  else if (fileName1[0] != '\0')
    {
      // The user has typed the name of a program (s)he wants to execute

      // Should we block on the command?
      if (argv[argc - 1][0] == '&')
	{
	  block = 0;
	  argc -= 1;
	}
      else if (argv[argc - 1][strlen(argv[argc - 1]) - 1] == '&')
	{
	  block = 0;
	  argv[argc - 1][strlen(argv[argc - 1]) - 1] = '\0';
	}

      // Shift the arg list down by one, as the exec function will prepend
      // it when starting the program
      for (count = 1; count < argc; count ++)
	argv[count - 1] = argv[count];
      argc -= 1;

      loaderLoadAndExec(fileName1, myPrivilege, argc, argv, block);
    }

  else
    printf("Unknown command \"%s\".\n", argv[0]);

  return;
}


static void simpleShell(void)
{
  // This is a very simple command shell intended only for development
  // purpose, although it might conceivably be used as a basis for a later
  // version of a real shell.

  char commandBuffer[MAX_LINELENGTH];
  char commandHistory[COMMANDHISTORY][MAX_LINELENGTH];
  int currentCommand = 0;
  int selectedCommand = 0;
  unsigned char bufferCharacter;
  static int currentCharacter = 0;
  int count;

  // Initialize stack data
  bzero(commandBuffer, MAX_LINELENGTH);
  bzero(commandHistory, (COMMANDHISTORY * MAX_LINELENGTH));

  // This program runs in an infinite loop
  while(1)
    {
      // There might be nothing to do...  No keyboard input?
      if (textInputCount() <= 0)
	promptCatchup = 0;

      bufferCharacter = getchar();

      // These numbers are ASCII codes.  Some are Visopsys specific ASCII
      // codes that make use of 'unused' spots.  Specifically the DC1-DC4
      // codes are used for cursor control
      
      if (bufferCharacter == (unsigned char) 17)
	{
	  // This is the UP cursor key, G's addition - bash style press up +
	  // repeat the last command.  It allows users to cycle backwards
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
	  for (count = currentCharacter; count > 0; count --)
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
	  
      else if (bufferCharacter == (unsigned char) 20)
	{
	  // This is the DOWN cursor key, which allows users to cycle forwards
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
	      for (count = currentCharacter; count > 0; count --)
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
	  for (count = currentCharacter; count > 0; count --)
	    textBackSpace();

	  // Copy the contents of the selected command into the current
	  // command
	  strcpy(commandBuffer, commandHistory[selectedCommand]);
	  // Print result to the screen
	  printf(commandBuffer);
	  // Correct currentCharacter length so that it's as if we've typed
	  // it ourselves.
	  currentCharacter = strlen(commandBuffer);
	}

      /*
      // This is not useful without line editing, and can confuse people
      else if (bufferCharacter == (unsigned char) 18)
	// This is the LEFT cursor key
	textCursorLeft();
	  
      else if (bufferCharacter == (unsigned char) 19)
	// This is the RIGHT cursor key
	textCursorRight();
      */
	  
      else if (bufferCharacter == (unsigned char) 13)
	{
	  // This is the HOME key, which normally puts the cursor at
	  // the beginning of the line, but we use it to clear the screen
	  textScreenClear();
	      
	  // Show a new prompt
	  showPrompt();
	}
      
      else if (bufferCharacter == (unsigned char) 8)
	{
	  // This is the BACKSPACE key
	  if (currentCharacter > 0)
	    {
	      // Move the current character back by 1
	      currentCharacter--;
	      commandBuffer[currentCharacter] = '\0';

	      if (promptCatchup)
		textBackSpace();
	    }
	  else
	    // Don't allow backspace from the start position
	    putchar(' ');
	}

      else if (bufferCharacter == (unsigned char) 9)
	{
	  // This is the TAB key.  Attempt to complete a filename.
	  
	  // Get rid of any tab characters printed on the screen
	  textSetColumn(currentCharacter);
	  
	  for (count = (strlen(commandBuffer)); count >= 0; count --)
	    if (commandBuffer[count] == '\"')
	      {
		count++;
		break;
	      }

	  if (count < 0)
	    for (count = (strlen(commandBuffer)); count >= 0; count --)
	      if (commandBuffer[count] == ' ')
		{
		  count++;
		  break;
		}

	  if (count < 0)
	    count = 0;

	  vshCompleteFilename(commandBuffer + count);
	  textSetColumn(0);
	  showPrompt();
	  printf(commandBuffer);
	  currentCharacter = strlen(commandBuffer);
	}
      
      else if (bufferCharacter == (unsigned char) 10)
	{
	  // This is the ENTER key

	  // Put a null in at the end of the command buffer
	  commandBuffer[currentCharacter] = '\0';

	  if (promptCatchup)
	    printf("\n");
	  
	  // Now we interpret the command
	  if (currentCharacter > 0)
	    {
	      if (!strcmp(commandBuffer, "logout") ||
		  !strcmp(commandBuffer, "exit"))
		return;

	      strcpy(commandHistory[currentCommand], commandBuffer);

	      if (((currentCommand > 0) &&
		   strcmp(commandBuffer,
			  commandHistory[currentCommand - 1])) ||
		  ((currentCommand == 0) &&
		   strcmp(commandBuffer,
			  commandHistory[COMMANDHISTORY - 1])))
		{
		  // We move to the next command buffer
		  if (currentCommand < (COMMANDHISTORY - 1))
		    currentCommand++;
		  else
		    currentCommand = 0;
		}

	      interpretCommand(commandBuffer);
	    }

	  // Set the current character to 0
	  currentCharacter = 0;
	  selectedCommand = currentCommand;
	  bzero(commandBuffer, MAX_LINELENGTH);

	  // Show a new prompt
	  showPrompt();
	}
      
      else if (bufferCharacter == (unsigned char) 4)
	{
	  // CTRL-D.  Logout
	  printf("logout\n");
	  return;
	}

      else
	{
	  // Something with no special meaning.

	  // Don't go beyond the maximum line length
	  if (currentCharacter >= (MAX_LINELENGTH - 2))
	    {
	      if (promptCatchup)
		textBackSpace();
	      continue;
	    }

	  // Add the current character to the command buffer and
	  // increment the current character count
	  commandBuffer[currentCharacter++] = bufferCharacter;
	  commandBuffer[currentCharacter] = '\0';

	  if (promptCatchup)
	    putchar(bufferCharacter);
	}
    }

  // This program never returns unless the user does 'logout'
}


int main(int argc, char *argv[])
{
  // This initializes the simple shell

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  file theFile;

  // What is my process id?
  myProcId = multitaskerGetCurrentProcessId();

  // What is my privilege level?
  myPrivilege = multitaskerGetProcessPrivilege(myProcId);

  // If we have a -c option, we just execute the command
  if ((getopt(argc, argv, "c") != -1) && (argc > 2))
    {
      // Operating in non-interactive mode.

      // If the command is a RELATIVE pathname, we will try inserting the 
      // pwd before it.  This has the effect of always putting '.' in
      // the PATH
      vshMakeAbsolutePath(argv[2], fileName);

      // Does the file exist?
      status = fileFind(fileName, &theFile);
      if (status < 0)
        {
          // Not found in the current directory.  Let's try searching the
          // PATH for the file instead
          status = vshSearchPath(argv[2], fileName);
          if (status < 0)
            {
              // Not found
              errno = status;
              perror(argv[0]);
              return (status);
            }
        }
        
        return (status = loaderLoadAndExec(fileName, myPrivilege, (argc - 2),
                                           &(argv[2]), 1 /* block */));
    }

  // Make a message
  printf("\nVisopsys Shell.\n");
  printf("Type \"help\" for commands.\n");

  // Get the starting current directory
  status = multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);
  if (status < 0)
    {
      printf("Can't determine current directory\n");
      return (-1);
    }

  // Show a prompt
  showPrompt();

  // Run
  simpleShell();

  // We'll end up here at 'logout'
  printf("exiting.\n");

  return status;
}
