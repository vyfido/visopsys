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
//  login.c
//

// This is the current login process for Visopsys.
// TEMP TEMP TEMP : At the moment it doesn't really do anything except
// require the user to pick a login name, and launch the SISH process.

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/api.h>

#define MAX_LOGIN_LENGTH 100

static int numberLogins = 0;


static void printMessage(void)
{
  // Print a message
  textPrint("Visopsys login v");
      
#if defined(RELEASE)
  textPrintLine(RELEASE);
#else
  textPrintLine("(unknown)");
#endif

  return;
}


static void printPrompt(void)
{
  // Print the login: prompt

  textPrintLine("[any login name is currently acceptable]");
  textPrint("login: ");
  
  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char bufferCharacter = NULL;
  char login[MAX_LOGIN_LENGTH];
  int currentCharacter = 0;
  int colour = 0;
  int count;


  // Asking for version?
  if (argc && !strcasecmp(argv[1], "-v"))
    {
      printMessage();
      return 0;
    }

  while(1)
    {
      // If this is not the first login, clear the screen so that text
      // from the previous session is not visible (privacy)
      if (numberLogins != 0)
	{
	  for (count = 0; count < textStreamGetRows(); count ++)
	    textNewline();
	  for (count = 0; count < 4; count ++)
	    textCursorUp();
	}

      //printMessage();
      textNewline();
      printPrompt();

      // Clear the login name buffer
      for (count = 0; count < MAX_LOGIN_LENGTH; count ++)
	login[count] = NULL;

      // Set the current character to 0
      currentCharacter = 0;

      // This loop grabs characters
      while(1)
	{
	  // Make sure our buffer isn't full
	  if (currentCharacter >= MAX_LOGIN_LENGTH)
	    {
	      currentCharacter = 0;
	      login[currentCharacter] = '\0';
	      textNewline();
	      colour = textStreamGetForeground();
	      textStreamSetForeground(6);
	      textPrintLine("That login name is too long.");
	      textStreamSetForeground(colour);
	      printPrompt();
	      continue;
	    }

	  // There might be nothing to do...  No keyboard input?
	  count = textInputCount();

	  if (count < 0)
	    {
	      // Eek, we can't get input.  Quit.
	      errno = count;
	      perror("login");
	      return (status = ERR_INVALID);
	    }

	  if (count == 0)
	    {
	      // For good form, yield the remainder of the current timeslice
	      // back to the scheduler
	      multitaskerYield();
	      continue;
	    }

	  status = textInputGetc(&bufferCharacter);
     
	  if (status < 0)
	    {
	      // Eek, we can't get input.  Quit.
	      errno = status;
	      perror("login");
	      return (status = ERR_INVALID);
	    }

	  if (bufferCharacter == (unsigned char) 8)
	    {
	      if (currentCharacter > 0)
		{
		  textBackSpace();
      
		  // Move the current character back by 1
		  currentCharacter--;
		}
	    }

	  else if (bufferCharacter == (unsigned char) 10)
	    {
	      // Put a null in at the end of the login buffer
	      login[currentCharacter] = '\0';
	  
	      // Print the newline
	      textNewline();

	      if (currentCharacter > 0)
		// Now we interpret the login
		break;

	      else
		{
		  // The user hit 'enter' without typing anything.  Make
		  // a new prompt
		  printPrompt();
		  continue;
		}
	    }
      
	  else
	    {
	      // Add the current character to the login buffer
	      login[currentCharacter++] = bufferCharacter;
	  
	      // Put it on the screen
	      textPutc(bufferCharacter);
	    }
	}

      // Now we have a login name to process.

      // Set the login name as an environment variable
      environmentSet("USER", login);

      textPrint("Welcome ");
      textPrintLine(login);
      
      // Start a shell
      loaderLoadAndExec("/programs/sish", 0, NULL, 1 /* block */);

      // If we return to here, the login session is over.  Start again.
      numberLogins++;
    }

  // This function never returns under normal conditions.
}
