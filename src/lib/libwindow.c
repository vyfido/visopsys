// 
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  libwindow.c
//

// This contains functions for user programs to operate GUI components.

#include <stdio.h>
#include <sys/window.h>
#include <sys/api.h>
#include <sys/errors.h>


static volatile struct {
  objectKey key;
  unsigned eventMask;
  void (*function)(objectKey, windowEvent *);

} callBacks[WINDOW_MAX_COMPONENTS];

typedef enum {
  infoDialog, errorDialog
} dialogType;

static volatile int numCallBacks = 0;
static volatile int run = 0;
static volatile int guiThreadPid = 0;

#define INFOIMAGE_NAME "/system/infoicon.bmp"
static volatile image infoImage;
#define ERRORIMAGE_NAME "/system/bangicon.bmp"
static volatile image errorImage;
#define QUESTIMAGE_NAME "/system/questicon.bmp"
static volatile image questImage;


static void centerDialog(objectKey parentWindow, objectKey dialogWindow)
{
  // Centers the dialog on the parent window
  
  unsigned parentX, parentY, myWidth, myHeight, parentWidth, parentHeight;

  // Get the size and location of the parent window
  windowGetLocation(parentWindow, &parentX, &parentY);
  windowGetSize(parentWindow, &parentWidth, &parentHeight);
  // Get our size
  windowGetSize(dialogWindow, &myWidth, &myHeight);
  // Set our location
  windowSetLocation(dialogWindow, (parentX + ((parentWidth - myWidth) / 2)),
		    (parentY + ((parentHeight - myHeight) / 2)));
}


static int okDialog(dialogType type, objectKey parentWindow, char *title,
		    char *message)
{
  // This will make a simple "OK" dialog message, and wait until the button
  // has been pressed.

  int status = 0;
  objectKey dialogWindow = NULL;
  char *imageName = NULL;
  image *myImage = NULL;
  objectKey imageComp = NULL;
  objectKey mainLabel = NULL;
  objectKey okLabel = NULL;
  objectKey okButton = NULL;
  componentParameters params;
  windowEvent event;
  
  // Check params.  It's okay for parentWindow to be NULL.
  if ((title == NULL) || (message == NULL))
    return (status = ERR_NULLPARAMETER);

  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 0;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.hasBorder = 0;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Create the dialog.  Arbitrary size and coordinates
  if (parentWindow)
    dialogWindow = windowManagerNewDialog(parentWindow, title, 0, 0, 10, 10);
  else
    dialogWindow = windowManagerNewWindow(multitaskerGetCurrentProcessId(),
					  title, 0, 0, 10, 10);
  if (dialogWindow == NULL)
    return (status = ERR_NOCREATE);

  // If our 'info' image hasn't been loaded, try to load it
  if (type == infoDialog)
    {
      imageName = INFOIMAGE_NAME;
      myImage = (image *) &infoImage;
    }
  else if (type == errorDialog)
    {
      imageName = ERRORIMAGE_NAME;
      myImage = (image *) &errorImage;
    }

  if (myImage->data == NULL)
    status = imageLoadBmp(imageName, myImage);

  if (status == 0)
    {
      myImage->isTranslucent = 1;
      myImage->translucentColor.red = 0;
      myImage->translucentColor.green = 255;
      myImage->translucentColor.blue = 0;
      imageComp = windowNewImage(dialogWindow, myImage);
      if (imageComp != NULL)
	{
	  params.padRight = 0;
	  windowAddClientComponent(dialogWindow, imageComp, &params);
	}
    }

  // Create the label
  mainLabel = windowNewTextLabel(dialogWindow, NULL, message);
  if (mainLabel == NULL)
    return (status = ERR_NOCREATE);

  params.gridX = 1;
  params.padRight = 5;
  windowAddClientComponent(dialogWindow, mainLabel, &params);

  // Create the label for the OK button
  okLabel = windowNewTextLabel(dialogWindow, NULL, "OK");
  if (okLabel == NULL)
    return (status = ERR_NOCREATE);

  // Create the button
  okButton = windowNewButton(dialogWindow, 10, 10, okLabel, NULL);
  if (okButton == NULL)
    return (status = ERR_NOCREATE);

  params.gridX = 0;
  params.gridY = 1;
  params.gridWidth = 2;
  params.padBottom = 5;
  windowAddClientComponent(dialogWindow, okButton, &params);

  windowLayout(dialogWindow);
  windowAutoSize(dialogWindow);

  if (parentWindow)
    centerDialog(parentWindow, dialogWindow);
  else
    windowCenter(dialogWindow);
  windowSetVisible(dialogWindow, 1);

  while(1)
    {
      // Wait until we have an event for our button
      status = windowComponentEventGet(okButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_UP)))
	break;

      // Done
      multitaskerYield();
    }
      
  windowManagerDestroyWindow(dialogWindow);

  return (status = 0);
}


static void guiRun(int thread)
{
  // This is the thread that runs for each user GUI program polling
  // components' event queues for events.

  int status = 0;
  objectKey *key = NULL;
  windowEvent event;
  int count;

  run = 1;

  while(run)
    {
      // Loop through all of the registered callbacks looking for components
      // with pending events
      for (count = 0; count < numCallBacks; count ++)
	{
	  key = callBacks[count].key;

	  // Any events pending?
	  while (run && (status = windowComponentEventGet(key, &event)) > 0)
	    callBacks[count].function((objectKey) key, &event);
	}

      // Done
      multitaskerYield();
    }

  if (thread)
    multitaskerTerminate(0);
  else
    return;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int windowRegisterEventHandler(objectKey key, void (*function)(objectKey,
							       windowEvent *))
{
  // Register a callback function as an event handler for the GUI object
  // 'key'.  The GUI object can be a window component, or a window for
  // example.  GUI components are typically the target of mouse click or
  // key press events, whereas windows typically receive 'close window'
  // events.  For example, if you create a button component in a window, you
  // should call windowRegisterEventHandler() to receive a callback when the
  // button is pushed by a user.  You can use the same callback function for
  // all your objects if you wish -- the objectKey of the target component
  // can always be found in the windowEvent passed to your callback function.
  // It is necessary to use one of the 'run' functions, below, such as
  // windowGuiRun() or windowGuiThread() in order to receive the callbacks.

  int status = 0;

  // Check parameters
  if ((key == NULL) || (function == NULL))
    return (status = ERR_NULLPARAMETER);

  callBacks[numCallBacks].key = key;
  callBacks[numCallBacks].function = function;
  numCallBacks++;

  return (status = 0);
}


void windowGuiRun(void)
{
  // Run the GUI windowEvent polling as a blocking call.  In other words,
  // use this function when your program has completed its setup code, and
  // simply needs to watch for GUI events such as mouse clicks, key presses,
  // and window closures.  If your program needs to do other processing
  // (independently of windowEvents) you should use the windowGuiThread()
  // function instead.  This is really an external wrapper for the guiRun
  // function.

  guiRun(0 /* no thread */);

  return;
}


void windowGuiThread(void)
{
  // Run the GUI windowEvent polling as a non-blocking call.  In other words,
  // this function will launch a separate thread to monitor for GUI events
  // and return control to your program.  Your program can then continue
  // execution -- independent of GUI windowEvents.  If your program doesn't
  // need to do any processing after setting up all its window components
  // and event callbacks, use the windowGuiRun() function instead.
  // This is really an external wrapper for the guiRun function.

  static void *args[] = { (void *) 1 /* thread */ };

  guiThreadPid = multitaskerSpawn(&guiRun, "gui thread", 1, args);

  return;
}


void windowGuiStop(void)
{
  // Stop GUI event polling which has been started by a previous call to one
  // of the 'run' functions, such as windowGuiRun() or windowGuiThread().
  // Note that calling this function clears all callbacks registered with
  // the windowRegisterEventHandler() function, so if you want to resume GUI
  // execution you will need to re-register them.

  run = 0;
  
  if (guiThreadPid && (multitaskerGetCurrentProcessId() != guiThreadPid))
    {
      multitaskerKillProcess(guiThreadPid, 0);
      guiThreadPid = 0;
    }

  // Unregister event handlers
  numCallBacks = 0;

  return;
}


int windowNewInfoDialog(objectKey parentWindow, char *title, char *message)
{
  // Create an 'info' dialog box, with the parent window 'parentWindow', and
  // the given titlebar text and main message.  The dialog will have a single
  // 'OK' button for the user to acknowledge.  If 'parentWindow' is NULL,
  // the dialog box is actually created as an independent window that looks
  // the same as a dialog.  This is a blocking call that returns when the user
  // closes the dialog window (i.e. the dialog is 'modal').
  return(okDialog(infoDialog, parentWindow, title, message));
}


int windowNewErrorDialog(objectKey parentWindow, char *title, char *message)
{
  // Create an 'error' dialog box, with the parent window 'parentWindow', and
  // the given titlebar text and main message.  The dialog will have a single
  // 'OK' button for the user to acknowledge.  If 'parentWindow' is NULL,
  // the dialog box is actually created as an independent window that looks
  // the same as a dialog.  This is a blocking call that returns when the user
  // closes the dialog window (i.e. the dialog is 'modal').
  return(okDialog(errorDialog, parentWindow, title, message));
}


int windowNewQueryDialog(objectKey parentWindow, char *title, char *message)
{
  // Create an 'query' dialog box, with the parent window 'parentWindow', and
  // the given titlebar text and main message.  The dialog will have an 'OK'
  // button and a 'CANCEL' button.  If the user presses OK, the function
  // returns the value 1.  Otherwise it returns 0.  If 'parentWindow' is NULL,
  // the dialog box is actually created as an independent window that looks
  // the same as a dialog.  This is a blocking call that returns when the
  // user closes the dialog window (i.e. the dialog is 'modal').

  int status = 0;
  objectKey dialogWindow = NULL;
  objectKey imageComp = NULL;
  objectKey mainLabel = NULL;
  objectKey okLabel = NULL;
  objectKey okButton = NULL;
  objectKey cancelLabel = NULL;
  objectKey cancelButton = NULL;
  componentParameters params;
  windowEvent event;
  
  // Check params.  It's okay for parentWindow to be NULL.
  if ((title == NULL) || (message == NULL))
    return (status = ERR_NULLPARAMETER);

  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 0;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.hasBorder = 0;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Create the dialog.  Arbitrary size and coordinates
  if (parentWindow)
    dialogWindow = windowManagerNewDialog(parentWindow, title, 0, 0, 10, 10);
  else
    dialogWindow = windowManagerNewWindow(multitaskerGetCurrentProcessId(),
					  title, 0, 0, 10, 10);
  if (dialogWindow == NULL)
    return (status = ERR_NOCREATE);

  if (questImage.data == NULL)
    status = imageLoadBmp(QUESTIMAGE_NAME, (image *) &questImage);

  if (status == 0)
    {
      questImage.isTranslucent = 1;
      questImage.translucentColor.red = 0;
      questImage.translucentColor.green = 255;
      questImage.translucentColor.blue = 0;
      imageComp = windowNewImage(dialogWindow, (image *) &questImage);
      if (imageComp != NULL)
	{
	  params.padRight = 0;
	  windowAddClientComponent(dialogWindow, imageComp, &params);
	}
    }

  // Create the label
  mainLabel = windowNewTextLabel(dialogWindow, NULL, message);
  if (mainLabel == NULL)
    return (status = ERR_NOCREATE);

  params.gridX = 1;
  params.gridWidth = 2;
  params.orientationX = orient_center;
  windowAddClientComponent(dialogWindow, mainLabel, &params);

  // Create the label for the OK button
  okLabel = windowNewTextLabel(dialogWindow, NULL, "OK");
  if (okLabel == NULL)
    return (status = ERR_NOCREATE);

  // Create the OK button
  okButton = windowNewButton(dialogWindow, 10, 10, okLabel, NULL);
  if (okButton == NULL)
    return (status = ERR_NOCREATE);

  params.gridY = 1;
  params.gridWidth = 1;
  params.orientationX = orient_right;
  params.padBottom = 5;
  windowAddClientComponent(dialogWindow, okButton, &params);

  // Create the label for the Cancel button
  cancelLabel = windowNewTextLabel(dialogWindow, NULL, "Cancel");
  if (cancelLabel == NULL)
    return (status = ERR_NOCREATE);

  // Create the Cancel button
  cancelButton = windowNewButton(dialogWindow, 10, 10, cancelLabel, NULL);
  if (cancelButton == NULL)
    return (status = ERR_NOCREATE);

  params.gridX = 2;
  params.orientationX = orient_left;
  windowAddClientComponent(dialogWindow, cancelButton, &params);

  windowLayout(dialogWindow);
  windowAutoSize(dialogWindow);

  if (parentWindow)
    centerDialog(parentWindow, dialogWindow);
  else
    windowCenter(dialogWindow);
  windowSetVisible(dialogWindow, 1);
  
  while(1)
    {
      // Wait until we have an event for our button
      status = windowComponentEventGet(okButton, &event);
      if (status < 0)
	{
	  status = 0;
	  break;
	}
      else if ((status > 0) && (event.type == EVENT_MOUSE_UP))
	{
	  status = 1;
	  break;
	}

      status = windowComponentEventGet(cancelButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_UP)))
	{
	  status = 0;
	  break;
	}

      // Done
      multitaskerYield();
    }

  windowManagerDestroyWindow(dialogWindow);

  return (status);
}
