// 
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  windowProgressDialog.c
//

// This contains functions for user programs to operate GUI components.

#include <errno.h>
#include <libintl.h>
#include <string.h>
#include <sys/api.h>
#include <sys/window.h>

#define _(string) gettext(string)

extern int libwindow_initialized;
extern void libwindowInitialize(void);

static volatile image waitImage;
static objectKey dialogWindow = NULL;
static objectKey progressBar = NULL;
static objectKey statusLabel = NULL;
static objectKey cancelButton = NULL;
static progress *prog = NULL;
static int threadPid = 0;


static void progressThread(void)
{
  // This thread monitors the supplied progress structure for changes and
  // updates the dialog window until the progress percentage equals 100 or
  // until the (interruptible) operation is interrupted.

  int status = 0;
  windowEvent event;
  progress lastProg;

  // Copy the supplied progress structure so we'll notice changes
  memcpy((void *) &lastProg, (void *) prog, sizeof(progress));
  if (lockGet(&prog->progLock) >= 0)
    {
      // Set initial display values.  After this we only watch for changes to
      // these.
      windowComponentSetData(progressBar, (void *) prog->percentFinished, 1);
      windowComponentSetData(statusLabel, (char *) prog->statusMessage,
			     strlen((char *) prog->statusMessage));
      lockRelease(&prog->progLock);
    }

  windowComponentSetEnabled(cancelButton, prog->canCancel);
  if (prog->canCancel)
    windowSwitchPointer(dialogWindow, "default");
  else
    windowSwitchPointer(dialogWindow, "busy");

  while (1)
    {
      if (lockGet(&prog->progLock) >= 0)
	{
	  // Did the status change?
	  if (memcmp((void *) &lastProg, (void *) prog, sizeof(progress)))
	    {
	      // Look for progress percentage changes
	      if (prog->percentFinished != lastProg.percentFinished)
		windowComponentSetData(progressBar,
				       (void *) prog->percentFinished, 1);

	      // Look for status message changes
	      if (strncmp((char *) prog->statusMessage,
			  (char *) lastProg.statusMessage,
			  PROGRESS_MAX_MESSAGELEN))
		windowComponentSetData(statusLabel,
				       (char *) prog->statusMessage,
				       strlen((char *) prog->statusMessage));

	      // Look for 'can cancel' flag changes
	      if (prog->canCancel != lastProg.canCancel)
		{
		  windowComponentSetEnabled(cancelButton, prog->canCancel);
		  if (prog->canCancel)
		    windowSwitchPointer(dialogWindow, "default");
		  else
		    windowSwitchPointer(dialogWindow, "busy");
		}

	      // If the 'percent finished' is 100, quit
	      if (prog->percentFinished >= 100)
		break;

	      // Look for 'need confirmation' flag changes
	      if (prog->needConfirm)
		{
		  status =
		    windowNewQueryDialog(dialogWindow, _("Confirmation"),
					 (char *) prog->confirmMessage);
		  prog->needConfirm = 0;
		  if (status == 1)
		    prog->confirm = 1;
		  else
		    prog->confirm = -1;
		}

	      // Look for 'error' flag changes
	      if (prog->error)
		{
		  windowNewErrorDialog(dialogWindow, _("Error"),
				       (char *) prog->statusMessage);
		  prog->error = 0;
		}

	      // Copy the status
	      memcpy((void *) &lastProg, (void *) prog, sizeof(progress));
	    }

	  lockRelease(&prog->progLock);
	}

      // Check for our Cancel button
      status = windowComponentEventGet(cancelButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP)))
	{
	  prog->cancel = 1;
	  windowComponentSetEnabled(cancelButton, 0);
	  break;
	}

      // Done
      multitaskerYield();
    }

  lockRelease(&prog->progLock);

  // Exit.
  multitaskerTerminate(0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ objectKey windowNewProgressDialog(objectKey parentWindow, const char *title, progress *tmpProg)
{
  // Desc: Create a 'progress' dialog box, with the parent window 'parentWindow', and the given titlebar text and progress structure.  The dialog creates a thread which monitors the progress structure for changes, and updates the progress bar and status message appropriately.  If the operation is interruptible, it will show a 'CANCEL' button.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a non-blocking call that returns immediately (but the dialog box itself is 'modal').  A call to this function should eventually be followed by a call to windowProgressDialogDestroy() in order to destroy and deallocate the window.

  int status = 0;
  componentParameters params;
    
  if (!libwindow_initialized)
    libwindowInitialize();

  // Check params.  It's okay for parentWindow to be NULL.
  if ((title == NULL) || (tmpProg == NULL))
    return (dialogWindow = NULL);

  // Create the dialog.  Arbitrary size and coordinates
  if (parentWindow)
    dialogWindow = windowNewDialog(parentWindow, title);
  else
    dialogWindow = windowNew(multitaskerGetCurrentProcessId(), title);
  if (dialogWindow == NULL)
    return (dialogWindow);

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padTop = 5;
  params.orientationX = orient_right;
  params.orientationY = orient_middle;
  params.flags = WINDOW_COMPFLAG_FIXEDWIDTH;

  if (waitImage.data == NULL)
    {
      status = imageLoad(WAITIMAGE_NAME, 0, 0, (image *) &waitImage);
      if (status >= 0)
	{
	  waitImage.transColor.red = 0;
	  waitImage.transColor.green = 255;
	  waitImage.transColor.blue = 0;
	}
    }
  if (waitImage.data)
    windowNewImage(dialogWindow, (image *) &waitImage, draw_translucent,
		   &params);

  // Create the progress bar
  params.gridX++;
  params.padRight = 5;
  params.orientationX = orient_center;
  params.flags = 0;
  progressBar = windowNewProgressBar(dialogWindow, &params);
  if (progressBar == NULL)
    {
      windowDestroy(dialogWindow);
      return (dialogWindow = NULL);
    }

  // Create the status label
  params.gridY++;
  params.orientationX = orient_left;
  statusLabel =
    windowNewTextLabel(dialogWindow, "                                       "
		       "                                         ", &params);
  if (statusLabel == NULL)
    {
      windowDestroy(dialogWindow);
      return (dialogWindow = NULL);
    }

  // Create the Cancel button
  params.gridY++;
  params.padBottom = 5;
  params.flags = (WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
  params.orientationX = orient_center;
  cancelButton = windowNewButton(dialogWindow, _("Cancel"), NULL, &params);
  if (cancelButton == NULL)
    {
      windowDestroy(dialogWindow);
      return (dialogWindow = NULL);
    }

  // Disable it until we know the operation is cancel-able.
  windowComponentSetEnabled(cancelButton, 0);

  windowRemoveCloseButton(dialogWindow);
  if (parentWindow)
    windowCenterDialog(parentWindow, dialogWindow);
  windowSetVisible(dialogWindow, 1);

  prog = tmpProg;

  // Spawn our thread to monitor the progress
  threadPid = multitaskerSpawn(progressThread, "progress thread", 0, NULL);
  if (threadPid < 0)
    {
      windowDestroy(dialogWindow);
      return (dialogWindow = NULL);
    }

  return (dialogWindow);
}


_X_ int windowProgressDialogDestroy(objectKey window)
{
  // Desc: Given the objectKey for a progress dialog 'window' previously returned by windowNewProgressDialog(), destroy and deallocate the window.

  int status = 0;

  if (!libwindow_initialized)
    libwindowInitialize();

  if (window == NULL)
    return (status = ERR_NULLPARAMETER);

  if (window != dialogWindow)
    return (status = ERR_INVALID);

  if (prog)
    {
      // Get a final lock on the progress structure
      status = lockGet(&prog->progLock);
      if (status < 0)
	return (status);

      windowComponentSetData(progressBar, (void *) 100, 1);
      windowComponentSetData(statusLabel, (char *) prog->statusMessage,
			     strlen((char *) prog->statusMessage));
    }

  if (threadPid && multitaskerProcessIsAlive(threadPid))
    // Kill our thread
    status = multitaskerKillProcess(threadPid, 1);

  // Destroy the window
  windowDestroy(dialogWindow);

  if (prog)
    lockRelease(&prog->progLock);

  dialogWindow = NULL;
  progressBar = NULL;
  statusLabel = NULL;
  cancelButton = NULL;
  prog = NULL;
  threadPid = 0;

  return (status);
}
