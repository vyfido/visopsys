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
//  vsh.c
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
  // If there are characters already in the input buffer, tell the reader
  // routine to put them after the prompt
  if (textInputCount)
    promptCatchup = 1;

  // This routine puts a prompt on the screen
  multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);
  printf("%s%s", cwd, SIMPLESHELLPROMPT);

  return;
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
  
  if (strlen(argv[0]) == 0)
    // Nothing
    return;

  // Try to match the command with the list of known commands
  
  if (!strcmp(argv[0], "help"))
    interpretCommand("more /system/helpinfo.txt");
  
  else if (!strcmp(argv[0], "pwd"))
    printf("%s\n", cwd);

  else if (!strcmp(argv[0], "cd"))
    {
      if (argc > 1)
	vshMakeAbsolutePath(argv[1], fileName);

      else
	// No arg means / for now
	strncpy(fileName, "/", 2);

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
	vshFileList(cwd);

      else
	for (count = 1; count < argc; count ++)
	  {
	    // If any of the arguments are RELATIVE pathnames, we should
	    // insert the pwd before it

	    vshMakeAbsolutePath(argv[count], fileName);
	    vshFileList(fileName);
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
	      vshMakeAbsolutePath(argv[count], fileName);
	      vshDumpFile(fileName);
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
	    vshMakeAbsolutePath(argv[count], fileName);
	    vshDeleteFile(fileName);
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
	  vshMakeAbsolutePath(argv[1], srcName);
	  vshMakeAbsolutePath(argv[2], destName);
	  vshCopyFile(srcName, destName);
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
	  vshMakeAbsolutePath(argv[1], srcName);
	  vshMakeAbsolutePath(argv[2], destName);
	  vshRenameFile(srcName, destName);
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
      vshMakeAbsolutePath(argv[0], fileName);

      // Does the file exist?
      status = fileFind(fileName, &theFile);
      if (status < 0)
	{
	  // Not found in the current directory.  Let's try searching the
	  // PATH for the file instead
	  status = vshSearchPath(argv[0], fileName);
	  if (status < 0)
	    {
	      // Not found
	      errno = status;
	      perror(argv[0]);
	      return;
	    }
	}

      if ((argc > 1) && (argv[argc - 1][0] == '&'))
	loaderLoadAndExec(fileName, myPrivilege, argc, argv, 0 /* no block */);
      else
	loaderLoadAndExec(fileName, myPrivilege, argc, argv, 1 /* block */);
    }

  return;
}


static void simpleShell(void)
{
  // This is a very simple command shell intended only for development
  // purpose, although it might conceivably be used as a basis for a later
  // version of a real shell.

  char *commandBuffer = NULL;
  char commandHistory[COMMANDHISTORY][MAX_LINELENGTH];
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

  // What is my process id?
  myProcId = multitaskerGetCurrentProcessId();

  // What is my privilege level?
  myPrivilege = multitaskerGetProcessPrivilege(myProcId);

  // This program runs in an infinite loop
  while(1)
    {
      // There might be nothing to do...  No keyboard input?
      if (textInputCount() <= 0)
	promptCatchup = 0;

      bufferCharacter = getchar();

      if (errno)
	{
	  // Eek.  We can't get input.  Quit.
	  perror("vsh");
	  return;
	}

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
	  
      else if (bufferCharacter == (unsigned char) 18)
	// This is the LEFT cursor key
	textCursorLeft();
	  
      else if (bufferCharacter == (unsigned char) 19)
	// This is the RIGHT cursor key
	textCursorRight();
	  
      else if (bufferCharacter == (unsigned char) 13)
	{
	  // This is the HOME key, which normally puts the cursor at
	  // the beginning of the line, but we use it to clear the screen
	  textClearScreen();
	      
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

	  vshCompleteFilename(commandBuffer + count1);
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
      
      else if (bufferCharacter == (unsigned char) 4)
	{
	  // CTRL-D.  Logout
	  printf("logout\n");
	  return;
	}

      else if (bufferCharacter == (unsigned char) 1) // 19?
	{
	  // CTRL-S.  Save a screenshot
	  interpretCommand("screenshot");

	  // Set the current character to 0
	  currentCharacter = 0;
	  
	  // Show a new prompt
	  showPrompt();
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


int main(void)
{
  // This initializes the simple shell

  int status = 0;


  // Make a message
  printf("\nVisopsys Shell.\n");
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
