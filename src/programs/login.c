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
//  login.c
//

// This is the current login process for Visopsys.
// TEMP TEMP TEMP : At the moment it doesn't really do anything except
// require the user to pick a login name, and launch the VSH process.

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/errors.h>
#include <sys/api.h>

#define MAX_LOGIN_LENGTH 100

#define LOGINPROMPT1 "Please enter your login name."
#define LOGINPROMPT2 "[any login name is currently acceptable]"
#define LOGINPROMPT3 "login: "

static int numberLogins = 0;

// The following are only used if we are running a graphics mode login window.
static objectKey window = NULL;
static objectKey textField = NULL;


static void printMessage(void)
{
  // Print a message
  printf("Visopsys login v");
      
#if defined(RELEASE)
  printf("%s\n", RELEASE);
#else
  printf("(unknown)\n");
#endif

  return;
}


static void printPrompt(void)
{
  // Print the login: prompt
  printf("%s\n%s", LOGINPROMPT2, LOGINPROMPT3);
  return;
}


static void contructWindow(int myProcessId)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  int status = 0;
  componentParameters params;
  image splashImage;
  objectKey imageComponent = NULL;
  objectKey font = NULL;
  objectKey textLabel1 = NULL;
  objectKey textLabel2 = NULL;

  // Create a new window, with small, arbitrary size and location
  window = windowManagerNewWindow(myProcessId, "Login window", 0, 0, 400, 400);

  if (window == NULL)
    return;

  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 0;
  params.orientationX = orient_center;
  params.orientationY = orient_top;
  params.hasBorder = 0;
  params.useDefaultForeground = 0;
  params.foreground.red = 40;
  params.foreground.green = 93;
  params.foreground.blue = 171;
  params.useDefaultBackground = 1;

  // Try to load a splash image to go at the top of the window
  status = imageLoadBmp("/system/visopsys.bmp", &splashImage);

  if (status == 0)
    {
      // Create an image component from it, and add it to the window
      imageComponent = windowNewImageComponent(window, &splashImage);

      // Release splash image memory
      memoryReleaseBlock(splashImage.data);

      params.gridY = 0;
      windowAddClientComponent(window, imageComponent, &params);
    }

  // Try to load our favorite font
  status = fontLoad("/system/arial-bold-12.bmp", "arial-bold-12", &font);
  if (status < 0)
    // Not found.  Just use the default fonts
    font = NULL;

  // Put text labels in the window to prompt the user
  textLabel1 = windowNewTextLabelComponent(window, NULL, LOGINPROMPT1);
  if (textLabel1 != NULL)
    {
      // Put it in the client area of the window
      params.gridY = 1;
      windowAddClientComponent(window, textLabel1, &params);
    }

  textLabel2 = windowNewTextLabelComponent(window, NULL, LOGINPROMPT2);
  if (textLabel2 != NULL)
    {
      // Put it in the client area of the window
      params.gridY = 2;
      params.padTop = 0;
      windowAddClientComponent(window, textLabel2, &params);
    }

  // Put a text field in the window for the user to type
  textField = windowNewTextFieldComponent(window, 30, NULL /* default font*/);
  if (textField != NULL)
    {
      // Put it in the client area of the window
      params.gridY = 3;
      params.padTop = 5;
      params.padBottom = 5;
      params.hasBorder = 1;
      params.useDefaultBackground = 0;
      params.background.red = 255;
      params.background.green = 255;
      params.background.blue = 255;
      windowAddClientComponent(window, textField, &params);
    }

  // Don't want the user closing this window.  It will just confuse them later.
  windowSetHasCloseButton(window, 0);

  windowLayout(window);

  // Autosize the window to fit our text area
  windowAutoSize(window);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char bufferCharacter = '\0';
  char login[MAX_LOGIN_LENGTH];
  int myProcessId = 0;
  int privilege = 0;
  int currentCharacter = 0;
  int textColor = 0;
  int graphics = 0;
  int count;

  // The following are only used if we are running a graphics mode login
  // window.
  objectKey oldTextInput = NULL;
  objectKey oldTextOutput = NULL;
  unsigned screenWidth = 0;
  unsigned screenHeight = 0;
  unsigned windowWidth = 0;
  unsigned windowHeight = 0;

  // Asking for version?
  if (argc && !strcasecmp(argv[1], "-v"))
    {
      printMessage();
      return 0;
    }

  myProcessId = multitaskerGetCurrentProcessId();

  graphics = graphicsAreEnabled();

  if (graphics)
    {
      oldTextInput = multitaskerGetTextInput();
      oldTextOutput = multitaskerGetTextOutput();
      contructWindow(myProcessId);
      // Get the screen width and height.
      screenWidth = graphicGetScreenWidth();
      screenHeight = graphicGetScreenHeight();
      // Get the size of our new window
      windowGetSize(window, &windowWidth, &windowHeight);
    }

  while(1)
    {
      if (graphics && (window != NULL))
	{
	  // Set the position to the middle of the screen
	  windowSetLocation(window, ((screenWidth - windowWidth) / 2),
			    ((screenHeight - windowHeight) / 2));
	  windowSetVisible(window, 1);
	  // Use the text field for all our input and output
	  windowManagerSetTextOutput(textField);
	}

      printf("\n");
      printPrompt();

      // Clear the login name buffer
      for (count = 0; count < MAX_LOGIN_LENGTH; count ++)
	login[count] = '\0';

      // Turn keyboard echo off
      textInputSetEcho(0);

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
	      printf("\n");
	      textColor = textGetForeground();
	      textSetForeground(6);
	      printf("That login name is too long.\n");
	      textSetForeground(textColor);
	      printPrompt();
	      continue;
	    }

	  status = textInputGetc(&bufferCharacter);

	  if (status < 0)
	    {
	      // Eek, we can't get input.  Quit.
	      errno = status;
	      perror("login");
	      // Turn keyboard echo beck on
	      textInputSetEcho(1);
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
	      printf("\n");

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
	      textPutc(bufferCharacter);
	    }
	}

      // Turn keyboard echo beck on
      textInputSetEcho(1);

      if (graphics)
	{
	  multitaskerSetTextInput(myProcessId, oldTextInput);
	  multitaskerSetTextOutput(myProcessId, oldTextOutput);
	  if (window != NULL)
	    windowSetVisible(window, 0);
	  // Log the user into the window manager
	  windowManagerLogin(1);
	}

      // Now we have a login name to process.

      if (strcmp(login, "root") == 0)
	privilege = 0;
      else
	privilege = 3;

      // Set the login name as an environment variable
      environmentSet("USER", login);
      
      printf("Welcome %s\n", login);
      
      // Start a shell
      loaderLoadAndExec("/programs/vsh", privilege, 0, NULL, 1 /* block */);

      // If we return to here, the login session is over.  Start again.

      if (graphics)
	windowManagerLogout();

      // Clear the screen
      textClearScreen();

      numberLogins++;
    }

  // This function never returns under normal conditions.
}
