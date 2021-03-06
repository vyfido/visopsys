//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
#include "kernelShutdown.h"
#include "kernelWindow.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/ascii.h>

// The default US-English keymap (but with the extra UK keys mapped, for me).
static keyMap defMap = {
	KEYMAP_MAGIC, 0x0200,
	"English (US)", { 'e', 'n' },
	// Regular map
	{
		// 6th row
		0, 0, 0, ASCII_SPACE, 0, 0, 0, 0,						// 00-07
		ASCII_CRSRLEFT, ASCII_CRSRDOWN, ASCII_CRSRRIGHT, 0,		// 08-0B
		ASCII_DEL, ASCII_ENTER,									// 0C-0D

		// 5th row
		0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm',				// 0E-16
		',', '.', '/', 0,										// 17-1A
		ASCII_CRSRUP, ASCII_END, ASCII_CRSRDOWN,				// 1B-1D
		ASCII_PAGEDOWN,											// 1E

		// 4th row
		0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	// 1F-29
		'\'', '#', ASCII_CRSRLEFT, 0, ASCII_CRSRRIGHT, '+',		// 2A-2F

		// 3rd row
		ASCII_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',		// 30-38
		'o', 'p', '[', ']', '\\', ASCII_DEL, ASCII_END,			// 39-3F
		ASCII_PAGEDOWN, ASCII_HOME, ASCII_CRSRUP, ASCII_PAGEUP,	// 40-43

		// 2nd row
		'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',	// 44-4E
		'-', '=', ASCII_BACKSPACE, 0, ASCII_HOME, ASCII_PAGEUP,	// 4F-54
		0, '/', '*', '-',										// 55-58

		// 1st row
		ASCII_ESC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	// 59-68
	},
	// Shift map
	{
		0, 0, 0, ASCII_SPACE, 0, 0, 0, 0,						// 00-07
		ASCII_CRSRLEFT, ASCII_CRSRDOWN, ASCII_CRSRRIGHT, 0,		// 08-0B
		ASCII_DEL, ASCII_ENTER,									// 0C-0D

		// 5th row
		0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M',				// 0E-16
		'<', '>', '?', 0,										// 17-1A
		ASCII_CRSRUP, ASCII_END, ASCII_CRSRDOWN,				// 1B-1D
		ASCII_PAGEDOWN,											// 1E

		// 4th row
		0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	// 1F-29
		'\"', '~', ASCII_CRSRLEFT, 0, ASCII_CRSRRIGHT, '+',		// 2A-2F

		// 3rd row
		ASCII_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',		// 30-38
		'O', 'P', '{', '}', '|', ASCII_DEL, ASCII_END,			// 39-3F
		ASCII_PAGEDOWN, ASCII_HOME, ASCII_CRSRUP, ASCII_PAGEUP,	// 40-43

		// 2nd row
		'~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',	// 44-4E
		'_', '+', ASCII_BACKSPACE, 0, ASCII_HOME, ASCII_PAGEUP,	// 4F-54
		0, '/', '*', '-',										// 55-58

		// 1st row
		ASCII_ESC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	// 59-68
	},
	// Control map.  Default is regular map value.
	{
		// 6th row
		0, 0, 0, ASCII_SPACE, 0, 0, 0, 0,						// 00-07
		ASCII_CRSRLEFT, ASCII_CRSRDOWN, ASCII_CRSRRIGHT, 0,		// 08-0B
		ASCII_DEL, ASCII_ENTER,									// 0C-0D

		// 5th row
		0, ASCII_FS, ASCII_SUB, ASCII_CAN, ASCII_ETX,			// 0E-12
		ASCII_SYN, ASCII_STX, ASCII_SO, ASCII_CR, ',', '.',		// 13-18
		'/', 0, ASCII_CRSRUP, ASCII_END, ASCII_CRSRDOWN,		// 19-1D
		ASCII_PAGEDOWN,											// 1E

		// 4th row
		0, ASCII_SOH, ASCII_DC3, ASCII_EOT, ASCII_ACK,			// 1F-23
		ASCII_BEL, ASCII_BS, ASCII_LF, ASCII_VT, ASCII_FF, ';',	// 24-29
		'\'', '#', ASCII_CRSRLEFT, 0, ASCII_CRSRRIGHT, '+',		// 2A-2F

		// 3rd row
		ASCII_TAB, ASCII_DC1, ASCII_ETB, ASCII_ENQ, ASCII_DC2,	// 30-34
		ASCII_DC4, ASCII_EM, ASCII_NAK, ASCII_HT, ASCII_SI,		// 35-39
		ASCII_DLE, ASCII_ESC, ASCII_GS, ASCII_FS, ASCII_DEL,	// 3A-3E
		ASCII_END, ASCII_PAGEDOWN, ASCII_HOME, ASCII_CRSRUP,	// 3F-42
		ASCII_PAGEUP,											// 43

		// 2nd row
		'`', '1', ASCII_NULL, '3', '4', '5', ASCII_RS, '7',		// 44-4B
		'8', '9', '0', ASCII_US, '=', ASCII_BACKSPACE, 0,		// 4C-52
		ASCII_HOME, ASCII_PAGEUP, 0, '/', '*', '-',				// 53-58

		// 1st row
		ASCII_ESC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	// 59-68
	},
	// AltGr map.  Same as the regular map for this keyboard.
	{
		// 6th row
		0, 0, 0, ASCII_SPACE, 0, 0, 0, 0,						// 00-07
		ASCII_CRSRLEFT, ASCII_CRSRDOWN, ASCII_CRSRRIGHT, 0,		// 08-0B
		ASCII_DEL, ASCII_ENTER,									// 0C-0D

		// 5th row
		0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm',				// 0E-16
		',', '.', '/', 0,										// 17-1A
		ASCII_CRSRUP, ASCII_END, ASCII_CRSRDOWN,				// 1B-1D
		ASCII_PAGEDOWN,											// 1E

		// 4th row
		0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	// 1F-29
		'\'', '#', ASCII_CRSRLEFT, 0, ASCII_CRSRRIGHT, '+',		// 2A-2F

		// 3rd row
		ASCII_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',		// 30-38
		'o', 'p', '[', ']', '\\', ASCII_DEL, ASCII_END,			// 39-3F
		ASCII_PAGEDOWN, ASCII_HOME, ASCII_CRSRUP, ASCII_PAGEUP,	// 40-43

		// 2nd row
		'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',	// 44-4E
		'-', '=', ASCII_BACKSPACE, 0, ASCII_HOME, ASCII_PAGEUP,	// 4F-54
		0, '/', '*', '-',										// 55-58

		// 1st row
		ASCII_ESC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	// 59-68
	},
	// Shift-AltGr map.  Same as the shift map for this keyboard.
	{
		0, 0, 0, ASCII_SPACE, 0, 0, 0, 0,						// 00-07
		ASCII_CRSRLEFT, ASCII_CRSRDOWN, ASCII_CRSRRIGHT, 0,		// 08-0B
		ASCII_DEL, ASCII_ENTER,									// 0C-0D

		// 5th row
		0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M',				// 0E-16
		'<', '>', '?', 0,										// 17-1A
		ASCII_CRSRUP, ASCII_END, ASCII_CRSRDOWN,				// 1B-1D
		ASCII_PAGEDOWN,											// 1E

		// 4th row
		0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	// 1F-29
		'\"', '~', ASCII_CRSRLEFT, 0, ASCII_CRSRRIGHT, '+',		// 2A-2F

		// 3rd row
		ASCII_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',		// 30-38
		'O', 'P', '{', '}', '|', ASCII_DEL, 0, ASCII_PAGEDOWN,	// 39-40
		ASCII_HOME, ASCII_CRSRUP, ASCII_PAGEUP,					// 41-43

		// 2nd row
		'~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',	// 44-4E
		'_', '+', ASCII_BACKSPACE, 0, ASCII_HOME, ASCII_PAGEUP,	// 4F-54
		0, '/', '*', '-',										// 55-58

		// 1st row
		ASCII_ESC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	// 59-68
	}
};

static kernelKeyboard *keyboards[MAX_KEYBOARDS];
static int numKeyboards = 0;
static keyMap *currentMap = NULL;
static kernelKeyboard *virtual = NULL;

// A buffer for input data waiting to be processed
static struct {
	kernelKeyboard *keyboard;
	int eventType;
	int scanCode;

} buffer[KEYBOARD_MAX_BUFFERSIZE];
static int bufferSize = 0;

static int graphics = 0;
static stream *consoleStream = NULL;
static int lastPressAlt = 0;
static int threadPid = 0;
static int initialized = 0;

#if 0
static const char *scan2String[KEYBOARD_SCAN_CODES] = {
	"LCtrl", "A0", "LAlt", "SpaceBar", "A2", "A3", "A4", "RCtrl",
	"LeftArrow", "DownArrow", "RightArrow", "Zero",
	"Period", "Enter", "LShift", "B0",
	"B1", "B2","B3", "B4", "B5", "B6", "B7", "B8",
	"B9", "B10", "RShift", "UpArrow", "One", "Two", "Three", "CapsLock",
	"C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8",
	"C9", "C10", "C11", "C12", "Four", "Five", "Six", "Plus",
	"Tab", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
	"D8", "D9", "D10", "D11", "D12", "D13", "Del", "End",
	"PgDn", "Seven", "Eight", "Nine", "E0", "E1", "E2", "E3",
	"E4", "E5", "E6", "E7", "E8", "E9", "E10", "E11",
	"E12", "BackSpace", "Ins", "Home", "PgUp", "NLck", "Slash", "Asterisk",
	"Minus", "Esc", "F1", "F2", "F3", "F4", "F5", "F6",
	"F7", "F8", "F9", "F10", "F11", "F12", "Print", "SLck", "Pause"
};
#endif


static void screenshotThread(void)
{
	// This gets called when the user presses the 'print screen' key

	int status = 0;
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

	status = kernelWindowSaveScreenShot(fileName);

	kernelMultitaskerTerminate(status);
}


static void loginThread(void)
{
	// This gets called when the user presses F1.

	// Launch a login process

	kernelConsoleLogin(NULL /* default login program */);
	kernelMultitaskerTerminate(0);
}


static void keyboardThread(void)
{
	// Check for keyboard input, and pass events to the window manager

	unsigned unicode = 0;
	windowEvent event;
	int count;

	while (1)
	{
		for (count = 0; count < bufferSize; count ++)
		{
			if (buffer[count].scanCode >= KEYBOARD_SCAN_CODES)
			{
				kernelError(kernel_error, "Scan code 0x%02x is out of range",
					buffer[count].scanCode);
				continue;
			}

			switch (buffer[count].eventType)
			{
				case WINDOW_EVENT_KEY_DOWN:
				{
					// Check for keys that change states or generate actions
					switch (buffer[count].scanCode)
					{
						// Check for ALT, CTRL, and shift keys
						case keyLAlt:
							buffer[count].keyboard->state.shiftState |=
								KEYBOARD_LEFT_ALT_PRESSED;
							lastPressAlt = 1;
							break;

						case keyA2:
							buffer[count].keyboard->state.shiftState |=
								KEYBOARD_RIGHT_ALT_PRESSED;
							lastPressAlt = 1;
							break;

						case keyLCtrl:
							buffer[count].keyboard->state.shiftState |=
								KEYBOARD_LEFT_CONTROL_PRESSED;
							break;

						case keyRCtrl:
							buffer[count].keyboard->state.shiftState |=
								KEYBOARD_RIGHT_CONTROL_PRESSED;
							break;

						case keyLShift:
							buffer[count].keyboard->state.shiftState |=
								KEYBOARD_LEFT_SHIFT_PRESSED;
							break;

						case keyRShift:
							buffer[count].keyboard->state.shiftState |=
								KEYBOARD_RIGHT_SHIFT_PRESSED;
							break;

						// Check for the *Lock keys
						case keyCapsLock:
							buffer[count].keyboard->state.toggleState ^=
								KEYBOARD_CAPS_LOCK_ACTIVE;
							break;

						case keyNLck:
							buffer[count].keyboard->state.toggleState ^=
								KEYBOARD_NUM_LOCK_ACTIVE;
							break;

						case keySLck:
							buffer[count].keyboard->state.toggleState ^=
								KEYBOARD_SCROLL_LOCK_ACTIVE;
							break;

						// A 'TAB' key with ALT down means raise the 'Window'
						// menu, in graphics mode
						case keyTab:
							if (graphics &&
								(buffer[count].keyboard->state.shiftState &
									KEYBOARD_ALT_PRESSED))
							{
								kernelWindowShellRaiseWindowMenu();
							}
							break;

						// A 'DEL' key with CTRL and ALT down means reboot
						case keyDel:
							if ((buffer[count].keyboard->state.shiftState &
									KEYBOARD_CONTROL_PRESSED) &&
								(buffer[count].keyboard->state.shiftState &
									KEYBOARD_ALT_PRESSED))
							{
								kernelSystemShutdown(1, 1);
								while (1);
							}
							break;

						// PrtScn means take a screenshot, in graphics mode
						case keyPrint:
							if (graphics)
							{
								kernelMultitaskerSpawn(screenshotThread,
									"screenshot", 0 /* no args */,
									NULL /* no args */, 1 /* run */);
							}
							break;

						// F1 launches a login process
						case keyF1:
							kernelMultitaskerSpawn(loginThread, "login",
								0 /* no args */, NULL /* no args */,
								1 /* run */);
							break;

						// F2 does something like a 'ps' command to the screen
						case keyF2:
							kernelMultitaskerDumpProcessList();
							break;

						default:
							// No special meaning
							break;
					}

					if ((buffer[count].scanCode != keyLAlt) &&
						(buffer[count].scanCode != keyA2))
					{
						lastPressAlt = 0;
					}

					break;
				}

				case WINDOW_EVENT_KEY_UP:
				{
					// Check for keys that change states or generate actions
					switch (buffer[count].scanCode)
					{
						// Check for ALT, CTRL, and shift keys
						case keyLAlt:
							buffer[count].keyboard->state.shiftState &=
								~KEYBOARD_LEFT_ALT_PRESSED;
							break;

						case keyA2:
							buffer[count].keyboard->state.shiftState &=
								~KEYBOARD_RIGHT_ALT_PRESSED;
							break;

						case keyLCtrl:
							buffer[count].keyboard->state.shiftState &=
								~KEYBOARD_LEFT_CONTROL_PRESSED;
							break;

						case keyRCtrl:
							buffer[count].keyboard->state.shiftState &=
								~KEYBOARD_RIGHT_CONTROL_PRESSED;
							break;

						case keyLShift:
							buffer[count].keyboard->state.shiftState &=
								~KEYBOARD_LEFT_SHIFT_PRESSED;
							break;

						case keyRShift:
							buffer[count].keyboard->state.shiftState &=
								~KEYBOARD_RIGHT_SHIFT_PRESSED;
							break;

						default:
							// No special meaning
							break;
					}

					// If we detect that ALT has been pressed and released
					// without any intervening keypresses, in graphics mode,
					// then we tell the windowing system so it can raise any
					// applicable menus in the active window.
					if ((buffer[count].scanCode == keyLAlt) ||
						(buffer[count].scanCode == keyA2))
					{
						if (graphics && lastPressAlt)
							kernelWindowToggleMenuBar(NULL /* any focused */);
						lastPressAlt = 0;
					}

					break;
				}
			}

			// Get the unicode value of this keypress, if any

			if (buffer[count].keyboard->state.shiftState &
				KEYBOARD_CONTROL_PRESSED)
			{
				unicode = currentMap->controlMap[buffer[count].scanCode];
			}

			else if (buffer[count].keyboard->state.shiftState &
				KEYBOARD_RIGHT_ALT_PRESSED)
			{
				if (buffer[count].keyboard->state.shiftState &
					KEYBOARD_SHIFT_PRESSED)
				{
					unicode =
						currentMap->shiftAltGrMap[buffer[count].scanCode];
				}
				else
				{
					unicode = currentMap->altGrMap[buffer[count].scanCode];
				}
			}

			else if (buffer[count].keyboard->state.shiftState &
				KEYBOARD_SHIFT_PRESSED)
			{
				unicode = currentMap->shiftMap[buffer[count].scanCode];

				// Check for Caps Lock
				if ((buffer[count].keyboard->state.toggleState &
					KEYBOARD_CAPS_LOCK_ACTIVE) && isalpha(unicode))
				{
					unicode = tolower(unicode);
				}
			}

			else
			{
				unicode = currentMap->regMap[buffer[count].scanCode];

				// Check for Caps Lock
				if ((buffer[count].keyboard->state.toggleState &
					KEYBOARD_CAPS_LOCK_ACTIVE) && isalpha(unicode))
				{
					unicode = toupper(unicode);
				}

				// Check for Num Lock
				if (buffer[count].keyboard->state.toggleState &
					KEYBOARD_NUM_LOCK_ACTIVE)
				{
					switch (buffer[count].scanCode)
					{
						case keyZero:
							unicode = '0';
							break;

						case keyPeriod:
							unicode = '.';
							break;

						case keyOne:
							unicode = '1';
							break;

						case keyTwo:
							unicode = '2';
							break;

						case keyThree:
							unicode = '3';
							break;

						case keyFour:
							unicode = '4';
							break;

						case keyFive:
							unicode = '5';
							break;

						case keySix:
							unicode = '6';
							break;

						case keySeven:
							unicode = '7';
							break;

						case keyEight:
							unicode = '8';
							break;

						case keyNine:
							unicode = '9';
							break;

						default:
							break;
					}
				}
			}

			if (graphics)
			{
				// Fill out our event
				event.type = buffer[count].eventType;
				event.key.scan = buffer[count].scanCode;
				event.key.unicode = unicode;

				// Notify the window manager of the event
				kernelWindowProcessEvent(&event);
			}
			else if (unicode)
			{
				if (consoleStream &&
					(buffer[count].eventType == WINDOW_EVENT_KEY_DOWN))
				{
					consoleStream->append(consoleStream, unicode);
				}
			}
		}

		bufferSize = 0;

		// Call the keyboard drivers, if applicable
		for (count = 0; count < numKeyboards; count ++)
		{
			if (keyboards[count]->threadCall)
				keyboards[count]->threadCall(keyboards[count]);
		}

		kernelMultitaskerYield();
	}

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

	currentMap = kernelMalloc(sizeof(keyMap));
	if (!currentMap)
		return (status = ERR_MEMORY);

	// Create a virtual keyboard
	virtual = kernelMalloc(sizeof(kernelKeyboard));
	if (virtual)
	{
		virtual->type = keyboard_virtual;
		keyboards[numKeyboards++] = virtual;
	}

	// We use US English as default, because, well, Americans would be so
	// confused if it wasn't.  Everyone else understands the concept of
	// setting it.
	memcpy(currentMap, &defMap, sizeof(keyMap));

	// Set the default keyboard data stream to be the console input
	consoleStream = &kernelTextGetConsoleInput()->s;

	// Spawn the keyboard thread
	threadPid = kernelMultitaskerSpawn(keyboardThread, "keyboard thread",
		0 /* no args */, NULL /* no args */, 1 /* run */);
	if (threadPid < 0)
		kernelError(kernel_warn, "Unable to start keyboard thread");

	initialized = 1;
	return (status = 0);
}


int kernelKeyboardAdd(kernelKeyboard *keyboard)
{
	// Called by a device driver (PS2, USB, etc) to add a keyboard to the list.
	// We allow this before we've been initialized, above, because hardware
	// drivers do their initial detection first.

	// Check params
	if (!keyboard)
		return (ERR_NULLPARAMETER);

	if (numKeyboards >= MAX_KEYBOARDS)
	{
		kernelError(kernel_error, "Max keyboards (%d) has been reached",
			MAX_KEYBOARDS);
		return (ERR_NOFREE);
	}

	keyboards[numKeyboards++] = keyboard;
	return (0);
}


int kernelKeyboardGetMap(keyMap *map)
{
	// Returns a copy of the current keyboard map in 'map'

	int status = 0;

	if (!initialized)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!map)
		return (status = ERR_NULLPARAMETER);

	memcpy(map, currentMap, sizeof(keyMap));

	return (status = 0);
}


int kernelKeyboardSetMap(const char *fileName)
{
	// Load the keyboard map from the supplied file name and set it as the
	// current mapping.  If the filename is NULL, then the default (US English)
	// mapping will be used.

	int status = 0;
	fileStream theFile;

	if (!initialized)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!fileName)
	{
		memcpy(currentMap, &defMap, sizeof(keyMap));
		return (status = 0);
	}

	memset(&theFile, 0, sizeof(fileStream));

	// Try to load the file
	status = kernelFileStreamOpen(fileName, OPENMODE_READ, &theFile);
	if (status < 0)
		return (status);

	status = kernelFileStreamRead(&theFile, sizeof(keyMap),
		(char *) currentMap);

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


int kernelKeyboardInput(kernelKeyboard *keyboard, int eventType,
	keyScan scanCode)
{
	// This gets called by the keyboard driver to tell us that a key has been
	// pressed.

	if (!initialized)
		return (ERR_NOTINITIALIZED);

	if (bufferSize < KEYBOARD_MAX_BUFFERSIZE)
	{
		buffer[bufferSize].keyboard = keyboard;
		buffer[bufferSize].eventType = eventType;
		buffer[bufferSize].scanCode = scanCode;
		bufferSize += 1;
	}

	return (0);
}


int kernelKeyboardVirtualInput(int eventType, keyScan scanCode)
{
	// Anyone can call this to supply virtual keyboard input.  Is this a
	// security problem?

	if (!initialized)
		return (ERR_NOTINITIALIZED);

	if (virtual)
	{
		if (bufferSize < KEYBOARD_MAX_BUFFERSIZE)
		{
			buffer[bufferSize].keyboard = virtual;
			buffer[bufferSize].eventType = eventType;
			buffer[bufferSize].scanCode = scanCode;
			bufferSize += 1;
		}
	}

	return (0);
}

