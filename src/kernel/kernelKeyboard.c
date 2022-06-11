//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  kernelKeyboard.c
//
//  German key mappings provided by Jonas Zaddach <jonaszaddach@gmx.de>
//  Italian key mappings provided by Davide Airaghi <davide.airaghi@gmail.com>
	
// This is the master code that wraps around the keyboard driver
// functionality

#include "kernelError.h"
#include "kernelFile.h"
#include "kernelFileStream.h"
#include "kernelKeyboard.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelProcessorX86.h"
#include "kernelShutdown.h"
#include "kernelWindow.h"
#include <stdio.h>
#include <string.h>

static int initialized = 0;
static int graphics = 0;
static stream *consoleStream = NULL;
static int lastPressAlt = 0;
keyMap *kernelKeyMap = NULL;

static keyMap defMap = {
  KEYMAP_MAGIC,
  "English (US)",
  // Regular map
  { 27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,'q',    // 00-0F
    'w','e','r','t','y','u','i','o','p','[',']',10,0,'a','s','d',  // 10-1F
    'f','g','h','j','k','l',';',39,'`',0,'\\','z','x','c','v','b', // 20-2F
    'n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,                 // 30-3F
    0,0,0,0,0,0,13,17,11,'-',18,'5',19,'+',0,20,                   // 40-4F
    12,0,127,0,0,0                                                 // 50-55
  },
  // Shift map
  { 27,'!','@','#','$','%','^','&','*','(',')','_','+',8,9,'Q',    // 00-0F
    'W','E','R','T','Y','U','I','O','P','{','}',10,0,'A','S','D',  // 10-1F 
    'F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B', // 20-2F
    'N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0,0,        	   // 30-3F
    0,0,0,0,0,0,'7','8','9','-','4','5','6','+','1','2',	   // 40-4F
    '3','0','.',0,0,0                				   // 50-55
  },
  // Control map.  Default is regular map value.
  { 27, '1','2','3','4','5','6','7','8','9','0','-','=',8,9,17,    // 00-0F
    23,5,18,20,25,21,9,15,16,'[',']',10,0,1,19,4,		   // 10-1F 
    6,7,8,10,11,12,';','"','`',0,0,26,24,3,22,2,    		   // 20-2F
    14,13,',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,		   // 30-3F
    0,0,0,0,0,0,13,17,11,'-',18,'5',19,'+','1',20,		   // 40-4F
    12,'0',127,0,0,0                				   // 50-55
  },
  // Alt-Gr map.  Same as the regular map for this keyboard.
  { 27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,'q',    // 00-0F
    'w','e','r','t','y','u','i','o','p','[',']',10,0,'a','s','d',  // 10-1F
    'f','g','h','j','k','l',';',39,'`',0,'\\','z','x','c','v','b', // 20-2F
    'n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,                 // 30-3F
    0,0,0,0,0,0,13,17,11,'-',18,'5',19,'+',0,20,                   // 40-4F
    12,0,127,0,0,0                                                 // 50-55
  }
};


__attribute__((noreturn))
static void rebootThread(void)
{
  // This gets called when the user presses CTRL-ALT-DEL.

  // Reboot, force
  kernelShutdown(1, 1);
  while(1);
}


static void screenshotThread(void)
{
  // This gets called when the user presses the 'print screen' key

  char fileName[32];
  int count = 1;

  // Determine the file name we want to use

  strcpy(fileName, "/screenshot1.bmp");

  // Loop until we get a filename that doesn't already exist
  while (!kernelFileFind(fileName, NULL))
    {
      count += 1;
      sprintf(fileName, "/screenshot%d.bmp", count);
    }

  kernelMultitaskerTerminate(kernelWindowSaveScreenShot(fileName));
}


static void loginThread(void)
{
  // This gets called when the user presses F1.

  // Launch a login process

  kernelConsoleLogin();
  kernelMultitaskerTerminate(0);
}



/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelKeyboardInitialize(void)
{
  // This function initializes the keyboard code, and sets the default
  // keyboard mapping.  Any keyboard driver should call this at the end
  // of successful device detection, to ensure that we will be able to
  // process their inputs.

  int status = 0;

  if (initialized)
    return (status = 0);

  kernelKeyMap = kernelMalloc(sizeof(keyMap));
  if (kernelKeyMap == NULL)
    return (status = ERR_MEMORY);

  // We use US English as default, because, well, Americans would be so
  // confused if it wasn't.  Everyone else understands the concept of
  // setting it.
  kernelMemCopy(&defMap, kernelKeyMap, sizeof(keyMap));

  // Set the default keyboard data stream to be the console input
  consoleStream = &(kernelTextGetConsoleInput()->s);

  initialized = 1;
  return (status = 0);
}


int kernelKeyboardGetMap(keyMap *map)
{
  // Returns a copy of the current keyboard map in 'map'

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (map == NULL)
    return (status = ERR_NULLPARAMETER);

  kernelMemCopy(kernelKeyMap, map, sizeof(keyMap));

  return (status = 0);
}


int kernelKeyboardSetMap(const char *fileName)
{
  // Load the keyboard map from the supplied file name and set it as the
  // current mapping.  If the filename is NULL, then the default (English US)
  // mapping will be used.
  
  int status = 0;
  fileStream theFile;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (fileName == NULL)
    {
      kernelMemCopy(&defMap, kernelKeyMap, sizeof(keyMap));
      return (status = 0);
    }

  kernelMemClear(&theFile, sizeof(fileStream));

  // Try to load the file
  status = kernelFileStreamOpen(fileName, OPENMODE_READ, &theFile);
  if (status < 0)
    return (status);

  status =
    kernelFileStreamRead(&theFile, sizeof(keyMap), (char *) kernelKeyMap);

  kernelFileStreamClose(&theFile);

  return (status);
}


int kernelKeyboardSetStream(stream *newStream)
{
  // Set the current stream used by the keyboard driver
  
  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Are graphics enabled?
  graphics = kernelGraphicsAreEnabled();

  // Save the address of the kernelStream we were passed to use for
  // keyboard data
  consoleStream = newStream;

  return (status = 0);
}


int kernelKeyboardSpecial(kernelKeyboardEvent event)
{
  // This function is called by the keyboard driver when it detects that
  // an ALT key has been pressed or released.  

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  switch (event)
    {
    case keyboardEvent_altPress:
      lastPressAlt = 1;
      break;

    case keyboardEvent_altRelease:
      // If we detect that ALT has been pressed and released without any
      // intervening keypresses, then we tell the windowing system so it can
      // raise any applicable menus in the active window.
      if (graphics && lastPressAlt)
	kernelWindowRaiseCurrentMenu();
      lastPressAlt = 0;
      break;

    case keyboardEvent_altTab:
      if (graphics)
	kernelWindowRaiseWindowMenu();
      break;

    case keyboardEvent_ctrlAltDel:
      // CTRL-ALT-DEL means reboot
      kernelMultitaskerSpawn(rebootThread, "reboot", 0, NULL);
      break;

    case keyboardEvent_printScreen:
      // PrtScn means take a screenshot
      kernelMultitaskerSpawn(screenshotThread, "screenshot", 0, NULL);
      break;

    case keyboardEvent_f1:
      // F1 launches a login process
      kernelMultitaskerSpawn(loginThread, "login", 0, NULL);
      break;

    case keyboardEvent_f2:
      // F2 does something like a 'ps' command to the screen
      kernelMultitaskerDumpProcessList();
      break;
    }

  return (status = 0);
}


int kernelKeyboardInput(int ascii, int eventType)
{
  // This gets called by the keyboard driver to tell us that a key has been
  // pressed.

  int status = 0;
  windowEvent event;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (graphics)
    {
      // Fill out our event
      event.type = eventType;
      event.xPosition = 0;
      event.yPosition = 0;
      event.key = ascii;

      // Notify the window manager of the event
      kernelWindowProcessEvent(&event);
    }
  else
    {
      if (consoleStream && (eventType & EVENT_KEY_DOWN))
	consoleStream->append(consoleStream, (char) ascii);
    }

  // This function isn't called for ALT key presses
  lastPressAlt = 0;

  return (status = 0);
}
