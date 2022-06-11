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
//  login.c
//

// This is the current login process for Visopsys.
// TEMP TEMP TEMP : At the moment it doesn't really do anything except
// require the user to pick a login name, and launch the VSH process.

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/window.h>
#include <sys/errors.h>
#include <sys/api.h>

#define LOGIN_SHELL "/programs/vsh"
#define LOGINPROMPT1 "Please enter your login name."
#define LOGINPROMPT2 "[any login name is currently acceptable]"
#define LOGINPROMPT3 "login: "
#define MAX_LOGIN_LENGTH 100

// The following are only used if we are running a graphics mode login window.
static int graphics = 0;
static objectKey window = NULL;
static objectKey textField = NULL;
static objectKey rebootButton = NULL;
static objectKey shutdownButton = NULL;

static char login[MAX_LOGIN_LENGTH];
static int currentCharacter = 0;

typedef enum 
{  
  halt, reboot

} shutdownType;


static void showVersion(void)
{
  // Print a message

  char tmp[64];

  strcpy(tmp, "Visopsys login");
      
#if defined(RELEASE)
  sprintf(tmp, "%s v%s", tmp, RELEASE);
#else
  sprintf(tmp, "%s (unknown version)");
#endif

  if (graphics)
    windowNewInfoDialog(NULL, "Version", tmp);
  else
    printf("%s\n", tmp);

  return;
}


static void printPrompt(void)
{
  // Print the login: prompt
  printf("%s\n%s", LOGINPROMPT2, LOGINPROMPT3);
  return;
}


static void processChar(unsigned char bufferCharacter)
{
  static char *tooLong = "That login name is too long.";

  // Make sure our buffer isn't full
  if (currentCharacter >= MAX_LOGIN_LENGTH)
    {
      currentCharacter = 0;
      login[currentCharacter] = '\0';
      printf("\n");

      if (graphics)
	windowNewErrorDialog(window, "Error", tooLong);
      else
	{
	  printf("%s\n", tooLong);
	  printPrompt();
	}
      return;
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

      currentCharacter = 0;
    }
  
  else
    {
      // Add the current character to the login buffer
      login[currentCharacter++] = bufferCharacter;
      textPutc(bufferCharacter);
    }

  return;
}


static void eventHandler(objectKey key, windowEvent *event)
{
  if (event->type == EVENT_MOUSE_UP)
    {
      if (key == rebootButton)
	shutdown(reboot, 0);
      else if (key == shutdownButton)
	shutdown(halt, 0);
    }

  else if (event->type == EVENT_KEY_DOWN)
    {
      processChar((unsigned char) event->key);

      if (event->key == 10)
	{
	  if (strcmp(login, ""))
	    // Now we interpret the login
	    windowGuiStop();
	}
    }
}


static void constructWindow(int myProcessId)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  int status = 0;
  componentParameters params;
  static image splashImage;
  objectKey imageComponent = NULL;
  objectKey textLabel1 = NULL;
  objectKey textLabel2 = NULL;

  // Create a new window, with small, arbitrary size and location
  window = windowManagerNewWindow(myProcessId, "Login Window", 0, 0, 400, 400);
  if (window == NULL)
    return;

  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 2;
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

  if (splashImage.data == NULL)
    // Try to load a splash image to go at the top of the window
    status = imageLoadBmp("/system/visopsys.bmp", &splashImage);

  if (splashImage.data != NULL)
    {
      // Create an image component from it, and add it to the window
      imageComponent = windowNewImage(window, &splashImage);
      if (imageComponent != NULL)
	{
	  params.gridY = 0;
	  windowAddClientComponent(window, imageComponent, &params);
	}
    }

  // Put text labels in the window to prompt the user
  textLabel1 = windowNewTextLabel(window, NULL, LOGINPROMPT1);
  if (textLabel1 != NULL)
    {
      // Put it in the client area of the window
      params.gridY = 1;
      windowAddClientComponent(window, textLabel1, &params);
    }

  textLabel2 = windowNewTextLabel(window, NULL, LOGINPROMPT2);
  if (textLabel2 != NULL)
    {
      // Put it in the client area of the window
      params.gridY = 2;
      params.padTop = 0;
      windowAddClientComponent(window, textLabel2, &params);
    }

  // Put a text field in the window for the user to type
  textField = windowNewTextField(window, 30, NULL /* default font*/);
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
      windowRegisterEventHandler(textField, &eventHandler);
    }

  // Create a 'reboot' button
  rebootButton = windowNewButton(window, 30, 20,
				 windowNewTextLabel(window, NULL, "Reboot"),
				 NULL);
  if (rebootButton != NULL)
    {
      // Put it in the client area of the window
      params.gridX = 0;
      params.gridY = 4;
      params.gridWidth = 1;
      params.padTop = 5;
      params.padBottom = 5;
      params.orientationX = orient_right;
      params.hasBorder = 0;
      params.useDefaultForeground = 0;
      params.useDefaultBackground = 1;
      windowAddClientComponent(window, rebootButton, &params);
      windowRegisterEventHandler(rebootButton, &eventHandler);
    }

  // Create a 'shutdown' button
  shutdownButton = windowNewButton(window, 30, 20, windowNewTextLabel(window,
					      NULL, "Shut down"), NULL);
  if (shutdownButton != NULL)
    {
      // Put it in the client area of the window
      params.gridX = 1;
      params.orientationX = orient_left;
      windowAddClientComponent(window, shutdownButton, &params);
      windowRegisterEventHandler(shutdownButton, &eventHandler);
    }

  // Don't want the user closing this window.  It will just confuse them later.
  windowSetHasCloseButton(window, 0);

  windowLayout(window);

  // Autosize the window to fit our text area
  windowAutoSize(window);

  windowCenter(window);

  return;
}


int main(int argc, char *argv[])
{
  char bufferCharacter = '\0';
  int myPid = 0;
  int shellPid = 0;
  int count;

  // The following are only used if we are running a graphics mode login
  // window.
  objectKey oldTextInput = NULL;
  objectKey oldTextOutput = NULL;

  // A lot of what we do is different depending on whether we're in graphics
  // mode or not.
  graphics = graphicsAreEnabled();

  // Asking for version?
  if (argc && !strcasecmp(argv[1], "-v"))
    {
      showVersion();
      return 0;
    }

  myPid = multitaskerGetCurrentProcessId();

  while(1)
    {
      if (graphics)
	{
	  oldTextInput = multitaskerGetTextInput();
	  oldTextOutput = multitaskerGetTextOutput();

	  constructWindow(myPid);

	  // Use the text field for all our input and output
	  windowManagerSetTextOutput(textField);

	  windowSetVisible(window, 1);
	}
      else
	{
	  printf("\n");
	  printPrompt();
	}

      // Clear the login name buffer
      for (count = 0; count < MAX_LOGIN_LENGTH; count ++)
	login[count] = '\0';

      // Turn keyboard echo off
      textInputSetEcho(0);

      // Set the current character to 0
      currentCharacter = 0;

      if (graphics)
	windowGuiRun();
      else
	{
	  // This loop grabs characters
	  while(1)
	    {
	      bufferCharacter = getchar();
	      processChar(bufferCharacter);

	      if (bufferCharacter == (unsigned char) 10)
		{
		  if (strcmp(login, ""))
		    // Now we interpret the login
		    break;
	      
		  else
		    {
		      // The user hit 'enter' without typing anything.  Make
		      // a new prompt
		      if (!graphics)
			printPrompt();
		      continue;
		    }
		}
	    }
	}

      // Turn keyboard echo back on
      textInputSetEcho(1);

      // Now we have a login name to process.

      // Here is where we will do authentication, later.

      // Set the login name as an environment variable
      environmentSet("USER", login);

      if (graphics)
	{
	  // Get rid of the login window
	  multitaskerSetTextInput(myPid, oldTextInput);
	  multitaskerSetTextOutput(myPid, oldTextOutput);
	  if (window != NULL)
	    windowManagerDestroyWindow(window);

	  // Log the user into the window manager
	  shellPid = windowManagerLogin(login, "");
	  if (shellPid < 0)
	    {
	      windowNewErrorDialog(window, "Login Failed",
				   "Unable to log in to the Window Manager!");
	      continue;
	    }
	}

      else
	{
	  printf("Welcome %s\n", login);
      
	  // Load a shell process
	  shellPid = loaderLoadProgram(LOGIN_SHELL, userGetPrivilege(login),
				       0, NULL);
	  if (shellPid < 0)
	    {
	      printf("Couldn't load login shell %s!", LOGIN_SHELL);
	      continue;
	    }
	}

      // Log the user into the system
      userLogin(login, shellPid);

      if (graphics)
	// Block on the window manager thread PID we were passed
	multitaskerBlock(shellPid);

      else
	// Run the text shell and block on it
	loaderExecProgram(shellPid, 1 /* block */);

      // If we return to here, the login session is over.  Start again.

      if (graphics)
	// Log the user out of the window manager
	windowManagerLogout();
      
      // Log the user out of the system
      userLogout(login);
    }

  // This function never returns under normal conditions.
}
