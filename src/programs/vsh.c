//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  vsh.c
//

/* This is the text that appears when a user requests help about this program
<help>

 -- vsh --

The Visopsys Shell.

Usage:
  vsh [-c command]

'vsh' is the Visopsys command line shell (interpreter).  In text mode the
login program automatically launches an instance of vsh for you.  In graphics
mode there is no 'default' command line shell, but clicking on the
'Command Window' icon or running the 'window' command will create a window
with an instance of vsh running inside it.

Normally, vsh operates interactively.  However, if the (optional) -c
parameter is supplied, vsh will execute the command that follows.  If the
command contains spaces or tab characters, it must be surrounded by
double-quotes (").

Options:
-c <command>  : Execute a command inside the shell

</help>
*/

#include <errno.h>
#include <libgen.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/ascii.h>
#include <sys/env.h>
#include <sys/file.h>
#include <sys/vsh.h>

#define _(string) gettext(string)

#define SIMPLESHELLPROMPT	"> "
#define MAX_ARGS			100
#define COMMANDHISTORY		20
#define MAX_ENVVAR_LENGTH	100

static int myProcId = 0;
static int myPrivilege = 0;
static char *cwd = NULL;
static int promptCatchup = 0;


static void showPrompt(void)
{
	char *dirName = NULL;

	// If there are characters already in the input buffer, tell the reader
	// function to put them after the prompt
	if (textInputCount())
		promptCatchup = 1;

	// This function puts a prompt on the screen
	multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);

	dirName = basename(cwd);

	printf("%s%s", dirName, SIMPLESHELLPROMPT);

	free(dirName);
}


static void interpretCommand(char *commandLine)
{
	int status = 0;
	int numArgs = 0;
	char *args[MAX_ARGS];
	char *commandName = NULL;
	char *fullCommand = NULL;
	char *getEnvBuff = NULL;
	int temp = 0;
	int block = 1;
	int count;

	// Initialize stack memory
	memset(args, 0, (MAX_ARGS * sizeof(char *)));

	commandName = malloc(MAX_PATH_NAME_LENGTH + 1);
	fullCommand = malloc(MAXSTRINGLENGTH + 1);
	if (!commandName || !fullCommand)
	{
		perror("malloc");
		goto out;
	}

	// We have to separate the command and arguments into an array of
	// strings
	status = vshParseCommand(commandLine, commandName, &numArgs, args);
	if (status < 0)
	{
		perror("vshParseCommand");
		goto out;
	}

	if (!numArgs)
		// Nothing
		goto out;

	// Try to match the command with the list of built-in commands

	if (!strcmp(args[0], "pwd"))
	{
		printf("%s\n", cwd);
	}

	else if (!strcmp(args[0], "cd"))
	{
		if (numArgs > 1)
			strcpy(cwd, args[1]);
		else
			// No arg means the user's home directory
			environmentGet(ENV_HOME, cwd, MAX_PATH_LENGTH);

		// Make it official
		temp = multitaskerSetCurrentDirectory(cwd);

		// Were we successful?  If not, make an error message.
		if (temp < 0)
		{
			errno = temp;
			perror(args[0]);
		}

		multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);
	}

	else if (!strcmp(args[0], "dir") || !strcmp(args[0], "ls"))
	{
		// Built in file-listing commands
		if (numArgs == 1)
		{
			status = vshFileList(cwd);
			if (status < 0)
				perror(args[0]);
		}
		else
		{
			for (count = 1; count < numArgs; count ++)
			{
				status = vshFileList(args[count]);
				if (status < 0)
					perror(args[0]);
			}
		}
	}

	else if (!strcmp(args[0], "type"))
	{
		// We want to dump the file to the screen
		if (numArgs > 1)
		{
			for (count = 1; count < numArgs; count ++)
			{
				status = vshDumpFile(args[count]);
				if (status < 0)
					perror(args[0]);
			}
		}
		else
		{
			printf(_("Usage: %s <file1> [file2] [...]\n"), args[0]);
		}
	}

	else if (!strcmp(args[0], "del"))
	{
		if (numArgs > 1)
		{
			for (count = 1; count < numArgs; count ++)
			{
				status = fileDelete(args[count]);
				if (status < 0)
				{
					errno = status;
					perror(args[0]);
				}
			}
		}
		else
		{
			printf(_("Usage: %s <file1> [file2] [...]\n"), args[0]);
		}
	}

	else if (!strcmp(args[0], "copy"))
	{
		if (numArgs > 2)
		{
			status = fileCopy(args[1], args[2]);
			if (status < 0)
			{
				errno = status;
				perror(args[0]);
			}
		}
		else
		{
			printf(_("Usage: %s <source file> <destination file>\n"),
				args[0]);
		}
	}

	else if (!strcmp(args[0], "ren") || !strcmp(args[0], "rename") ||
		!strcmp(args[0], "move"))
	{
		if (numArgs > 2)
		{
			status = fileMove(args[1], args[2]);
			if (status < 0)
			{
				errno = status;
				perror(args[0]);
			}
		}
		else
		{
			printf(_("Usage: %s <source file> <destination file>\n"),
				args[0]);
		}
	}

	else if (!strcmp(args[0], "getenv"))
	{
		if (numArgs == 2)
		{
			getEnvBuff = malloc(MAX_ENVVAR_LENGTH);
			if (getEnvBuff)
			{
				status = environmentGet(args[1], getEnvBuff,
					MAX_ENVVAR_LENGTH);
				if (status < 0)
				{
					errno = status;
					perror(args[0]);
				}
				else
				{
					printf("%s\n", getEnvBuff);
				}

				free(getEnvBuff);
			}
			else
			{
				perror("malloc");
			}
		}
		else
		{
			printf(_("Usage: %s <variable_name>\n"), args[0]);
		}
	}

	else if (!strcmp(args[0], "setenv"))
	{
		if (numArgs == 3)
		{
			if (strlen(args[2]) > MAX_ENVVAR_LENGTH)
				printf("%s", _("Shouldn't set an env variable that long\n"));

			status = environmentSet(args[1], args[2]);
			if (status < 0)
			{
				errno = status;
				perror(args[0]);
			}
		}
		else
		{
			printf(_("Usage: %s <variable_name> <variable_value>\n"),
				args[0]);
		}
	}

	else if (!strcmp(args[0], "unsetenv"))
	{
		if (numArgs == 2)
		{
			status = environmentUnset(args[1]);
			if (status < 0)
			{
				errno = status;
				perror(args[0]);
			}
		}
		else
		{
			printf(_("Usage: %s <variable_name>\n"), args[0]);
		}
	}

	else if (!strcmp(args[0], "printenv"))
	{
		environmentDump();
	}

	else if (commandName[0] != '\0')
	{
		// The user has typed the name of a program to execute

		// Should we block on the command?
		if (args[numArgs - 1][0] == '&')
		{
			block = 0;
			numArgs -= 1;
		}
		else if (args[numArgs - 1][strlen(args[numArgs - 1]) - 1] == '&')
		{
			block = 0;
			args[numArgs - 1][strlen(args[numArgs - 1]) - 1] = '\0';
		}

		// Reconstitute the full command line
		sprintf(fullCommand, "\"%s\" ", commandName);
		for (count = 1; count < numArgs; count ++)
			sprintf((fullCommand + strlen(fullCommand)), "\"%s\" ",
				args[count]);

		loaderLoadAndExec(fullCommand, myPrivilege, block);
	}

	else
	{
		printf(_("Unknown command \"%s\".\n"), args[0]);
	}

out:
	if (commandName)
		free(commandName);
	if (fullCommand)
		free(fullCommand);
}


static void simpleShell(void)
{
	// This is a very simple command shell.

	char *commandBuffer = NULL;
	char *commandHistory[COMMANDHISTORY];
	int currentCommand = 0;
	int selectedCommand = 0;
	unsigned char bufferCharacter;
	static int currentCharacter = 0;
	char *tmp;
	int count;

	commandBuffer = malloc(MAXSTRINGLENGTH + 1);
	tmp = malloc(COMMANDHISTORY * (MAXSTRINGLENGTH + 1));
	if (!commandBuffer || !tmp)
	{
		perror("malloc");
		return;
	}

	// Initialize stack data
	for (count = 0; count < COMMANDHISTORY; count ++)
		commandHistory[count] = (tmp + (count * (MAXSTRINGLENGTH + 1)));

	// This program runs in an infinite loop
	while (1)
	{
		// There might be nothing to do...  No keyboard input?
		if (textInputCount() <= 0)
			promptCatchup = 0;

		bufferCharacter = getchar();

		// These numbers are ASCII codes.  Some are Visopsys specific ASCII
		// codes that make use of 'unused' spots.  Specifically the DC1-DC4
		// codes are used for cursor control

		if (bufferCharacter == ASCII_CRSRUP)
		{
			// This is the UP cursor key, G's addition - bash style press up +
			// repeat the last command.  It allows users to cycle backwards
			// through their command history

			if ((selectedCommand > 0) &&
				((selectedCommand - 1) != currentCommand) &&
				(commandHistory[selectedCommand - 1][0] != '\0'))
			{
				selectedCommand -= 1;
			}

			else if (!selectedCommand &&
				(currentCommand != (COMMANDHISTORY - 1)) &&
				(commandHistory[COMMANDHISTORY - 1][0] != '\0'))
			{
				selectedCommand = (COMMANDHISTORY - 1);
			}

			else
			{
				continue;
			}

			// Delete the previous command from the command line
			for (count = currentCharacter; count > 0; count --)
				textBackSpace();

			// Copy the contents of the selected command into the current
			// command
			strcpy(commandBuffer, commandHistory[selectedCommand]);

			// Print result to the screen
			printf("%s", commandBuffer);

			// Correct currentCharacter length so that it's as if we've typed
			// it ourselves.
			currentCharacter = strlen(commandBuffer);
		}

		else if (bufferCharacter == ASCII_CRSRDOWN)
		{
			// This is the DOWN cursor key, which allows users to cycle
			// forwards through their command history

			if (selectedCommand == currentCommand)
			{
				continue;
			}

			else if (((selectedCommand < (COMMANDHISTORY - 1)) &&
				((selectedCommand + 1) == currentCommand)) ||
					((selectedCommand == (COMMANDHISTORY - 1)) &&
				!currentCommand))
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
			{
				selectedCommand += 1;
			}

			else if ((selectedCommand == (COMMANDHISTORY - 1)) &&
				currentCommand && (commandHistory[0][0] != '\0'))
			{
				selectedCommand = 0;
			}

			else
			{
				continue;
			}

			// Delete the previous command from the command line
			for (count = currentCharacter; count > 0; count --)
				textBackSpace();

			// Copy the contents of the selected command into the current
			// command
			strcpy(commandBuffer, commandHistory[selectedCommand]);

			// Print result to the screen
			printf("%s", commandBuffer);

			// Correct currentCharacter length so that it's as if we've typed
			// it ourselves.
			currentCharacter = strlen(commandBuffer);
		}

		/*
		// This is not useful without line editing, and can confuse people
		else if (bufferCharacter == ASCII_CRSRLEFT)
			// This is the LEFT cursor key
			textCursorLeft();

		else if (bufferCharacter == ASCII_CRSRRIGHT)
			// This is the RIGHT cursor key
			textCursorRight();
		*/

		else if (bufferCharacter == ASCII_HOME)
		{
			// This is the HOME key, which normally puts the cursor at
			// the beginning of the line, but we use it to clear the screen
			textScreenClear();

			// Show a new prompt
			showPrompt();
		}

		else if (bufferCharacter == ASCII_BACKSPACE)
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
			{
				// Don't allow backspace from the start position
				putchar(' ');
			}
		}

		else if (bufferCharacter == ASCII_TAB)
		{
			// This is the TAB key.  Attempt to complete a filename

			// Get rid of any tab characters printed on the screen
			textSetColumn(currentCharacter);

			for (count = (strlen(commandBuffer)); count >= 0; count --)
			{
				if (commandBuffer[count] == '\"')
				{
					count++;
					break;
				}
			}

			if (count < 0)
			{
				for (count = (strlen(commandBuffer)); count >= 0; count --)
				{
					if (commandBuffer[count] == ' ')
					{
						count++;
						break;
					}
				}
			}

			if (count < 0)
				count = 0;

			// Try to complete the file name
			vshCompleteFilename(commandBuffer + count);

			// Print result to the screen
			textSetColumn(0);
			showPrompt();
			printf("%s", commandBuffer);

			// Correct currentCharacter length so that it's as if we've typed
			// it ourselves.
			currentCharacter = strlen(commandBuffer);
		}

		else if (bufferCharacter == ASCII_ENTER)
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
				{
					goto logout;
				}

				strcpy(commandHistory[currentCommand], commandBuffer);

				if (((currentCommand > 0) &&
					strcmp(commandBuffer,
						commandHistory[currentCommand - 1])) ||
					(!currentCommand &&
					strcmp(commandBuffer,
						commandHistory[COMMANDHISTORY - 1])))
				{
					// We move to the next command buffer
					if (currentCommand < (COMMANDHISTORY - 1))
						currentCommand++;
					else
						currentCommand = 0;
				}

				// Check for special 'history' request
				if (!strcmp(commandBuffer, "history"))
				{
					for (count = (currentCommand + 1); ; count ++)
					{
						if (count >= COMMANDHISTORY)
							count = 0;

						if (count == currentCommand)
							break;

						if (commandHistory[count][0] == '\0')
							continue;

						printf("%s\n", commandHistory[count]);
					}
				}
				else
				{
					interpretCommand(commandBuffer);
				}
			}

			// Set the current character to 0
			currentCharacter = 0;
			selectedCommand = currentCommand;
			memset(commandBuffer, 0, MAXSTRINGLENGTH);

			// Show a new prompt
			showPrompt();
		}

		else if (bufferCharacter == ASCII_ENDOFFILE)
		{
			// CTRL-D.  Logout
			printf("%s", _("logout\n"));
			goto logout;
		}

		else if (bufferCharacter == ASCII_DEL)
		{
			// Do nothing.  Don't print.
		}

		else if (bufferCharacter)
		{
			// Something with no special meaning

			// Don't go beyond the maximum line length
			if (currentCharacter >= (MAXSTRINGLENGTH - 2))
			{
				if (promptCatchup)
					textBackSpace();
				continue;
			}

			if (currentCharacter)
			{
				// Make sure there's whitespace around special symbols
				if (bufferCharacter == '&')
				{
					if (commandBuffer[currentCharacter - 1] != ' ')
						commandBuffer[currentCharacter++] = ' ';
				}
				else if (commandBuffer[currentCharacter - 1] == '&')
				{
					if (bufferCharacter != ' ')
						commandBuffer[currentCharacter++] = ' ';
				}
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

logout:
	free(commandBuffer);
	free(commandHistory[0]);
}


int main(int argc, char *argv[])
{
	// This initializes the simple shell

	int status = 0;
	char fileName[MAX_PATH_NAME_LENGTH + 1];
	char *fullCommand = NULL;
	int count;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("vsh");

	// What is my process id?
	myProcId = multitaskerGetCurrentProcessId();

	// What is my privilege level?
	myPrivilege = multitaskerGetProcessPrivilege(myProcId);

	// If we have a -c option, we just execute the command
	if ((getopt(argc, argv, "c") == 'c') && (argc > 2))
	{
		// Operating in non-interactive mode.

		// If the command is a RELATIVE pathname, we will try inserting the
		// pwd before it.  This has the effect of always putting '.' in
		// the PATH
		vshMakeAbsolutePath(argv[2], fileName);

		// Does the file exist?
		status = fileFind(argv[2], NULL);
		if (status < 0)
		{
			// Not found in the current directory.  Let's try searching the
			// PATH for the file instead
			status = vshSearchPath(argv[2], fileName);
			if (status < 0)
			{
				// Not found
				errno = status;
				perror("vshSearchPath");
				return (status);
			}
		}

		fullCommand = malloc(MAXSTRINGLENGTH + 1);
		if (!fullCommand)
		{
			status = errno;
			perror("malloc");
			return (status);
		}

		strcpy(fullCommand, fileName);
		strcat(fullCommand, " ");
		for (count = 3; count < argc; count ++)
		{
			strcat(fullCommand, argv[count]);
			strcat(fullCommand, " ");
		}

		// Exec and block
		status = loaderLoadAndExec(fullCommand, myPrivilege, 1);

		free(fullCommand);

		return (status);
	}

	// Make a message
	printf("%s", _("\nVisopsys Shell.\n"));
	printf("%s", _("Type \"help\" for commands.\n"));

	// Get the starting current directory

	cwd = malloc(MAX_PATH_LENGTH + 1);
	if (!cwd)
	{
		status = errno;
		perror("malloc");
		return (status);
	}

	status = multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);
	if (status < 0)
	{
		printf("%s", _("Can't determine current directory\n"));
		free(cwd);
		return (status);
	}

	// Show a prompt
	showPrompt();

	// Run
	simpleShell();

	// We'll end up here at 'logout'

	printf("%s", _("exiting.\n"));
	free(cwd);
	return (status = 0);
}

