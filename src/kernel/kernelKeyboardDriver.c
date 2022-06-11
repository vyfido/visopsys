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
//  kernelKeyboardDriver.c
//

// Driver for standard PC keyboards

#include "kernelDriverManagement.h" // Contains my prototypes
#include "kernelProcessorX86.h"
#include "kernelMultitasker.h"
#include "kernelShutdown.h"
#include "kernelMiscFunctions.h"
#include <sys/window.h>
#include <sys/errors.h>
#include <sys/stream.h>


int kernelKeyboardDriverSetStream(stream *);
void kernelKeyboardDriverReadData(void);

// Some special scan values that we care about
#define KEY_RELEASE 128
#define EXTENDED    224
#define LEFT_SHIFT  42
#define RIGHT_SHIFT 54
#define LEFT_CTRL   29
#define LEFT_ALT    56
#define F1_KEY      59
#define F2_KEY      60
#define DEL_KEY     83
#define CAPSLOCK    58
#define NUMLOCK     69
#define SCROLLLOCK  70

#define SCROLLLOCK_LIGHT 0
#define NUMLOCK_LIGHT    1
#define CAPSLOCK_LIGHT   2

static kernelKeyboardDriver defaultKeyboardDriver =
{
  kernelKeyboardDriverInitialize,
  NULL, // driverRegisterDevice
  kernelKeyboardDriverReadData
};

typedef struct {
  char name[32];
  char regMap[86];
  char shiftMap[86];
  char controlMap[86];
} keyMap;

static keyMap EN_US = {
  "EN_US",
  { 27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,'q',    // 00-0F
    'w','e','r','t','y','u','i','o','p','[',']',10,0,'a','s','d',  // 10=1F
    'f','g','h','j','k','l',';',39,'`',0,'\\','z','x','c','v','b', // 20-2F
    'n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,                 // 30-3F
    0,0,0,0,0,0,13,17,11,'-',18,'5',19,'+','1',20,                 // 40-4F
    12,'0',127,0,0,0                                               // 50-55
  },
  { 27,'!','@','#','$','%','^','&','*','(',')','_','+',8,9,'Q',	   // 00-0F
    'W','E','R','T','Y','U','I','O','P','{','}',10,0,'A','S','D',  // 10=1F 
    'F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B', // 20-2F
    'N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0,0,        	   // 30-3F
    0,0,0,0,0,0,'7','8','9','-','4','5','6','+','1','2',	   // 40-4F
    '3','0','.',0,0,0                				   // 50-55
  },
  { 27, '1','2','3','4','5','6','7','8','9','0','-','=',8,9,17,	   // 00-0F
    23,5,18,20,25,21,9,15,16,'[',']',10,0,1,19,4,		   // 10=1F 
    6,7,8,10,11,12,';','"','`',0,0,26,24,3,22,2,    		   // 20-2F
    14,13,',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,		   // 30-3F
    0,0,0,0,0,0,13,17,11,'-',18,'5',19,'+','1',20,		   // 40-4F
    12,'0',127,0,0,0                				   // 50-55
  }
};

static keyMap EN_UK = {
  "EN_UK",
  { 27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,'q',    // 00-0F
    'w','e','r','t','y','u','i','o','p','[',']',10,0,'a','s','d',  // 10=1F
    'f','g','h','j','k','l',';',39,'`',0,'#','z','x','c','v','b',  // 20-2F
    'n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,                 // 30-3F
    0,0,0,0,0,0,13,17,11,'-',18,'5',19,'+','1',20,                 // 40-4F
    12,'0',127,0,0,'\\'                                            // 50-55
  },
  { 27,'!','"',156,'$','%','^','&','*','(',')','_','+',8,9,'Q',	   // 00-0F
    'W','E','R','T','Y','U','I','O','P','{','}',10,0,'A','S','D',  // 10=1F 
    'F','G','H','J','K','L',':', '@',170,0,'~','Z','X','C','V','B',// 20-2F
    'N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0,0,        	   // 30-3F
    0,0,0,0,0,0,'7','8','9','-','4','5','6','+','1','2',	   // 40-4F
    '3','0','.',0,0,'|'                				   // 50-55
  },
  { 27, '1','2','3','4','5','6','7','8','9','0','-','=',8,9,17,	   // 00-0F
    23,5,18,20,25,21,9,15,16,'[',']',10,0,1,19,4,		   // 10=1F 
    6,7,8,10,11,12,';', '@','`',0,0,26,24,3,22,2,    		   // 20-2F
    14,13,',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,		   // 30-3F
    0,0,0,0,0,0,13,17,11,'-',18,'5',19,'+','1',20,		   // 40-4F
    12,'0',127,0,0,0                				   // 50-55
  }
};

keyMap *allMaps[2] = {
  &EN_US, &EN_UK
};

static int initialized = 0;
static int shiftDown = 0;
static int controlDown = 0;
static int altDown = 0;
static int capsLock = 0;
static int numLock = 0;
static int scrollLock = 0;
static int extended = 0;
static keyMap *currentMap = &EN_US;


static void setLight(int whichLight, int onOff)
{
  // Turns the keyboard lights on/off

  unsigned char data = 0;
  static unsigned char lights = 0x00;

  // Wait for port 60h to be ready for a command.  We know it's
  // ready when port 0x64 bit 1 is 0
  data = 0x02;
  while (data & 0x02)
    kernelProcessorInPort8(0x64, data);

  // Tell the keyboard we want to change the light status
  kernelProcessorOutPort8(0x60, 0xED);

  if (onOff)
    lights |= (0x01 << whichLight);
  else
    lights &= (0xFF ^ (0x01 << whichLight));

  // Wait for port 60h to be ready for a command.  We know it's
  // ready when port 0x64 bit 1 is 0
  data = 0x02;
  while (data & 0x02)
    kernelProcessorInPort8(0x64, data);

  // Tell the keyboard to change the lights
  kernelProcessorOutPort8(0x60, lights);

  // Read the ACK
  data = 0;
  while (!(data & 0x01))
    kernelProcessorInPort8(0x64, data);
  kernelProcessorInPort8(0x60, data);
  
  return;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelKeyboardDriverInitialize(void)
{
  // This routine issues the appropriate commands to the keyboard controller
  // to set keyboard settings.

  unsigned char data;

  // Wait for port 64h to be ready for a command.  We know it's ready when
  // port 64 bit 1 is 0
  data = 0x02;
  while (data & 0x02)
    kernelProcessorInPort8(0x64, data);

  // Tell the keyboard to enable
  kernelProcessorOutPort8(0x64, 0xAE);

  // By default, numlock on.  We do this here instead of in the initialize
  // routine since that locks up the input for some reason (maybe because
  // the mouse still hasn't initialized or something?)
  //setLight(NUMLOCK_LIGHT, 1);
  //numLock = 1;

  initialized = 1;
  return (kernelDriverRegister(keyboardDriver, &defaultKeyboardDriver));
}


void kernelKeyboardDriverReadData(void)
{
  // This routine reads the keyboard data and returns it to the keyboard
  // console text input stream

  unsigned char data = 0;
  unsigned char tmp = 0;

  if (!initialized)
    return;

  // Wait for data to be available
  while (!(data & 0x01))
    kernelProcessorInPort8(0x64, data);

  // Read the data from port 60h
  kernelProcessorInPort8(0x60, data);

  // ACK the data by disabling, then reenabling, the controller
  kernelProcessorInPort8(0x61, tmp);
  tmp |= 0x80; // Disable (bit 7 on)
  kernelProcessorOutPort8(0x61, tmp);
  tmp &= 0x7F; // Enable (bit 7 off)
  kernelProcessorOutPort8(0x61, tmp);

  // Key press or key release?
  if (data >= KEY_RELEASE)
    {
      // This is a key release.  We only care about a couple of cases if
      // it's a key release.

      // If an extended scan code is coming next...
      if (data == EXTENDED)
	// The next thing coming is an extended scan code.  Set the flag
	// so it can be collected next time
	extended = 1;

      else
	{
	  // If the last one was an extended, but this is a key release,
	  // then we have to make sure we clear the extended flag even though
	  // we're ignoring it
	  extended = 0;

	  switch (data)
	    {
	    case (KEY_RELEASE + LEFT_SHIFT):
	    case (KEY_RELEASE + RIGHT_SHIFT):
	      // Left or right shift release.  Reset the value of the
	      // shiftDown flag
	      shiftDown = 0;
	      break;
	    case (KEY_RELEASE + LEFT_CTRL):
	      // Left control release.  Reset the value of the controlDown flag
	      controlDown = 0;
	      break;
	    case (KEY_RELEASE + LEFT_ALT):
	      // Left Alt release.  Reset the value of the altDown flag
	      altDown = 0;
	      break;
	    default:
	      // Don't care
	      break;
	    }

	  // Notify the keyboard function of the event
	  kernelKeyboardInput((int)(data - KEY_RELEASE), EVENT_KEY_UP);
	}
    }
  else
    {
      // This was a key press.  Check whether the last key pressed was
      // one with an extended scan code.

      if (extended)
	// The last thing was an extended flag.  Clear the flag
	extended = 0;

      // Check for a few 'special action' keys
      
      switch (data)
	{
	case LEFT_SHIFT:
	case RIGHT_SHIFT:
	  // Left shift or right shift.  Set the shiftDown flag.
	  shiftDown = 1;
	  break;
	case LEFT_CTRL:
	  // Left control.  Set the controlDown flag.
	  controlDown = 1;
	  break;
	case LEFT_ALT:
	  // Left alt.  Set the altDown flag.
	  altDown = 1;
	  break;
	case CAPSLOCK:
	  if (capsLock)
	    // Capslock off
	    capsLock = 0;
	  else
	    // Capslock on
	    capsLock = 1;
	  setLight(CAPSLOCK_LIGHT, capsLock);
	  break;
	case NUMLOCK:
	  if (numLock)
	    // Numlock off
	    numLock = 0;
	  else
	    // Numlock on
	    numLock = 1;
	  setLight(NUMLOCK_LIGHT, numLock);
	  break;
	case SCROLLLOCK:
	  if (scrollLock)
	    // Scroll lock off
	    scrollLock = 0;
	  else
	    // Scroll lock on
	    scrollLock = 1;
	  setLight(SCROLLLOCK_LIGHT, scrollLock);
	  break;
	case F1_KEY:
	  kernelConsoleLogin();
	  break;
	case F2_KEY:
	  kernelMultitaskerDumpProcessList();
	  break;
	default:
	  // Regular key.

	  // Check whether the control or shift keys are pressed.  Shift
	  // overrides control.
	  if (shiftDown)
	    data = currentMap->shiftMap[data - 1];
	  else if (controlDown)
	    {
	      if (altDown && (data == DEL_KEY)) // DEL key
		{
		  // CTRL-ALT-DEL means reboot
		  kernelShutdown(reboot, 1 /*force*/);
		  while(1);
		}
	      else
		data = currentMap->controlMap[data - 1];
	    }
	  else
	    data = currentMap->regMap[data - 1];

	  // If capslock is on, uppercase any alphabetic characters
	  if (capsLock && ((data >= 'a') && (data <= 'z')))
	    data -= 32;

	  // Notify the keyboard function of the event
	  kernelKeyboardInput((int) data, EVENT_KEY_DOWN);

	  break;
	}
    }

  return;
}
