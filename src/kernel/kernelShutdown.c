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
//  kernelShutdown.c
//

// This code is responsible for an orderly shutdown and/or reboot of
// the kernel

#include "kernelShutdown.h"
#include "kernelGraphic.h"
#include "kernelWindowManager.h"
#include "kernelMultitasker.h"
#include "kernelLog.h"
#include "kernelFilesystem.h"
#include "kernelSysTimer.h"
#include "kernelDisk.h"
#include "kernelProcessorX86.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>
#include <stdio.h>


static void messageBox(kernelAsciiFont *font, int numLines, char *message[])
{
  // Prints a blue box, with lines of white text centered in it.
  // Useful for final shutdown messages.

  unsigned screenWidth = kernelGraphicGetScreenWidth();
  unsigned screenHeight = kernelGraphicGetScreenHeight();
  unsigned messageWidth = 0;
  unsigned messageHeight, boxWidth, boxHeight, tmp;
  int count;

  for (count = 0; count < numLines; count ++)
    {
      tmp = kernelFontGetPrintedWidth(font, message[count]);
      if (tmp > messageWidth)
	messageWidth = tmp;
    }
  messageHeight = (font->charHeight * numLines);
  boxWidth = (messageWidth + 30);
  boxHeight = (messageHeight + (font->charHeight * 2));

  // The box
  kernelGraphicDrawRect(NULL, &((color)
  { DEFAULT_ROOTCOLOR_BLUE, DEFAULT_ROOTCOLOR_GREEN, DEFAULT_ROOTCOLOR_RED }),
			draw_normal, ((screenWidth - boxWidth) / 2),
			((screenHeight - boxHeight) / 2), boxWidth, boxHeight,
			1, 1);
  // Nice white border
  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
			draw_normal, ((screenWidth - boxWidth) / 2),
			((screenHeight - boxHeight) / 2), boxWidth, boxHeight,
			2, 0);

  // The message
  for (count = 0; count < numLines; count ++)
    {
      kernelGraphicDrawText(NULL, &((color )
      { 255, 255, 255 }), font, message[count], draw_normal,
	    ((screenWidth -
	      kernelFontGetPrintedWidth(font, message[count])) / 2),
	    (((screenHeight - messageHeight) / 2) +
	     (font->charHeight * count)));
    }
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelShutdown(kernelShutdownType shutdownType, int force)
{
  // This function will shut down the kernel, and reboot the computer
  // if the shutdownType argument dictates.  This function must include
  // shutdown invocations for all of the major subsystems that support
  // and/or require such activity

  int status = 0;
  int graphics = 0;
  static int shutdownInProgress = 0;
  char *finalMessage = NULL;

  // We only use these if grapics are enabled
  kernelAsciiFont *font = NULL;
  kernelWindow *window = NULL;
  componentParameters params;
  kernelWindowComponent *label1 = NULL;
  kernelWindowComponent *label2 = NULL;
  unsigned screenWidth = 0;
  unsigned screenHeight = 0;
  unsigned windowWidth, windowHeight;

  if (shutdownInProgress && !force)
    {
      kernelError(kernel_error, "The system is already shutting down");
      return (status = ERR_ALREADY);
    }
  shutdownInProgress = 1;

  // Are grapics enabled?  If so, we will try to output our initial message
  // to a window
  graphics = kernelGraphicsAreEnabled();

  if (graphics)
    {
      screenWidth = kernelGraphicGetScreenWidth();
      screenHeight = kernelGraphicGetScreenHeight();

      // Try to load a nice-looking font
      status = kernelFontLoad(DEFAULT_VARIABLEFONT_MEDIUM_FILE,
			      DEFAULT_VARIABLEFONT_MEDIUM_NAME, &font);
      if (status < 0)
	{
	  // Font's not there, we suppose.  There's always a default.
	  kernelFontGetDefault(&font);
	}
 
      window =
	kernelWindowManagerNewWindow(kernelMultitaskerGetCurrentProcessId(),
				     "Shutting down", 0, 0, 100, 100);
      if (window != NULL)
	{
	  label1 = kernelWindowNewTextLabel(window, NULL /* default font */,
					    SHUTDOWN_MSG1);

	  if (label1 != NULL)
	    {
	      params.gridX = 0;
	      params.gridY = 0;
	      params.gridWidth = 1;
	      params.gridHeight = 1;
	      params.padLeft = 10;
	      params.padRight = 10;
	      params.padTop = 5;
	      params.padBottom = 5;
	      params.orientationX = orient_center;
	      params.orientationY = orient_top;
	      params.hasBorder = 0;
	      params.useDefaultForeground = 1;
	      params.useDefaultBackground = 1;

	      kernelWindowAddClientComponent(window, label1, &params);

	      if (shutdownType == halt)
		{
		  label2 = kernelWindowNewTextLabel(window,
						    NULL /* default font */,
						    SHUTDOWN_MSG2);
		  if (label2 != NULL)
		    {
		      params.gridY = 1;
		      params.padTop = 0;
		      kernelWindowAddClientComponent(window, label2, &params);
		    }
		}
	    }

	  kernelWindowSetHasCloseButton(window, 0);
	  kernelWindowLayout(window);
	  kernelWindowAutoSize(window);
	  kernelWindowGetSize(window, &windowWidth, &windowHeight);
	  kernelWindowSetLocation(window, ((screenWidth - windowWidth) / 2),
				  ((screenHeight - windowHeight) / 3));
	  kernelWindowSetVisible(window, 1);
	}
    }

  // Echo the appropriate message(s) to the console [as well]
  kernelTextPrintLine("\n%s", SHUTDOWN_MSG1);
  if (shutdownType == halt)
    kernelTextPrintLine(SHUTDOWN_MSG2);

  if (kernelGraphicsAreEnabled())
    {
      // Shut down the window manager
      kernelLog("Stopping window manager");
      status = kernelWindowManagerShutdown();
      if (status < 0)
	// Not fatal by any means
	kernelError(kernel_warn, "Unable to shut down the window manager");
    }

  // Detach from our parent process, if applicable, so we won't get killed
  // when our parent gets killed
  kernelMultitaskerDetach();

  // Kill all the processes, except this one and the kernel.
  kernelLog("Stopping all processes");
  status = kernelMultitaskerKillAll();

  if ((status < 0) && (!force))
    {
      // Eek.  We couldn't kill the processes nicely
      kernelError(kernel_error, "Unable to stop processes nicely.  "
		  "Shutdown aborted.");
      shutdownInProgress = 0;
      return (status);
    }

  // Shut down the multitasker
  status = kernelMultitaskerShutdown(1 /* nice shutdown */);

  if (status < 0)
    {
      if (!force)
	{
	  // Abort the shutdown
	  kernelError(kernel_error, "Unable to stop multitasker.  Shutdown "
		      "aborted.");
	  shutdownInProgress = 0;
	  return (status);
	}
      else
	{
	  // Attempt to shutdown the multitasker without the 'nice' flag.
	  // We won't bother to check whether this was successful
	  kernelMultitaskerShutdown(0 /* NOT nice shutdown */);
	}
    }

  // After this point, don't abort.  We're running the show.


  // Shut down kernel logging
  kernelLog("Stopping kernel logging");
  status = kernelLogShutdown();
  if (status < 0)
    kernelError(kernel_error, "The kernel logger could not be stopped.");


  // Unmount all filesystems.
  kernelLog("Unmounting filesystems");
  status = kernelFilesystemUnmountAll();
  if (status < 0)
    kernelError(kernel_error, "The filesystems were not all unmounted "
		"successfully");


  // Synchronize/shut down the disks
  kernelLog("Synchronizing disks");
  status = kernelDiskShutdown();
  if (status < 0)
    {
      // Eek.  We couldn't synchronize the filesystems.  We should
      // stop and allow the user to try to save their data
      kernelError(kernel_error, "Unable to syncronize disks.  Shutdown "
		  "aborted.");
      shutdownInProgress = 0;
      return (status);
    }


  // What final message will we be displaying today?
  if (shutdownType == reboot)
    finalMessage = SHUTDOWN_MSG_REBOOT;
  else
    finalMessage = SHUTDOWN_MSG_POWER;

  // Last words.
  kernelTextPrintLine("\n%s", finalMessage);

  if (graphics)
    {
      // Get rid of the window we showed.  No need to properly destroy it.
      if (window != NULL)
	kernelWindowSetVisible(window, 0);

      // Draw a box with the final message
      messageBox(font, 1, (char *[]){ finalMessage } );
    }

  // Finally, we either halt or reboot the computer
  if (shutdownType == reboot)
    {
      kernelSysTimerWaitTicks(20); // Wait ~2 seconds
      kernelProcessorReboot();
    }
  else
    kernelProcessorStop();

  // Just for good form
  return (status);
}


void kernelPanicOutput(const char *module, const char *function, int line,
		       const char *message, ...)
{
  // This is a quick shutdown for kernel panic which puts nice messages
  // on the screen in graphics mode as well.

  int graphics = 0;
  kernelAsciiFont *font = NULL;
  char *panicMessage = "SYSTEM HALTED";
  char errorText[MAX_ERRORTEXT_LENGTH];
  va_list list;

  extern int kernelProcessingInterrupt;

  // Expand the message if there were any parameters
  va_start(list, message);
  _expandFormatString(errorText, message, list);
  va_end(list);

  graphics = kernelGraphicsAreEnabled();
  
  if (graphics)
    {
      kernelFontGetDefault(&font);
      // Draw a box with the panic message
      messageBox(font, 2, (char *[]){ panicMessage, (char *) errorText } );
    }

  if (kernelProcessingInterrupt)
    kernelLogSetFile(NULL);

  kernelErrorOutput(module, function, line, kernel_panic, errorText);
      
  if (!kernelProcessingInterrupt)
    // Try to sync the disks
    kernelDiskSync();

  kernelTextPrintLine(panicMessage);

  kernelProcessorStop();
  return; // Compiler nice
}
