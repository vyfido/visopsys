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
//  kernelText.c
//

#include "kernelText.h"
#include "kernelTextConsoleDriver.h"
#include "kernelParameters.h"
#include "kernelPageManager.h"
#include "kernelMultitasker.h"
#include "kernelError.h"
#include <stdio.h>
#include <sys/errors.h>
#include <string.h>


volatile void *screenAddress = (void *) 0x000B8000;
volatile int textColumns = 80;
volatile int textRows = 50;

static volatile int outputMode = DEFAULTVIDEOMODE;

// For the moment, there is only ONE kernelTextStream for input
static kernelTextStream consoleInput;

// For the moment, there is only ONE kernelTextStream for output as well.
static kernelTextStream consoleOutput;

// This structure contains pointers to all the routines for outputting
// text in text mode (as opposed to graphics modes)
static kernelTextOutputDriver textModeDriver =
{
  kernelTextConsoleInitialize,
  kernelTextConsoleGetCursorAddress,
  kernelTextConsoleSetCursorAddress,
  kernelTextConsoleSetForeground,
  kernelTextConsoleSetBackground,
  kernelTextConsolePrint,
  kernelTextConsoleClearScreen
};

// So nobody can use us until we're ready
static int initialized = 0;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelTextInitialize(int columns, int rows)
{
  // Initialize the console input and output streams

  int status = 0;
  void *newScreenAddress = NULL;


  // Check our arguments
  if ((columns == 0) || (rows == 0))
    return (status = ERR_INVALID);

  // Set the initial rows and columns
  textColumns = columns;
  textRows = rows;

  // Take the physical text stream address and turn it into a virtual
  // address in the kernel's address space.
  status = kernelPageMapToFree(KERNELPROCID, (void *) screenAddress, 
		       &newScreenAddress, (textColumns * textRows * 2));
  
  // Make sure we got a proper new virtual address
  if (status < 0)
    return (status);

  screenAddress = newScreenAddress;

  // Initialize the console text output driver
  textModeDriver.initialize((void *) screenAddress, textColumns, textRows,
			    DEFAULTFOREGROUND, DEFAULTBACKGROUND);

  // Get a new kernelStream to be our console text output stream
  consoleOutput.s = kernelStreamNew(TEXTSTREAMSIZE, itemsize_char);

  // Success?
  if (consoleOutput.s == NULL)
    {
      // Don't bother making a kernelError here, since there's nowhere to
      // output the message to.
      return (status = ERR_NOTINITIALIZED);
    }

  // For convenience, we save the pointer to the stream's functions
  // so we don't always have to cast it from a void * to a
  // kernelStreamFunctions *
  consoleOutput.sFn = (streamFunctions *) consoleOutput.s->functions;

  // We assign the text console driver to be the output driver for now.
  consoleOutput.outputDriver = &textModeDriver;

  // Set the foreground/background colours
  consoleOutput.foregroundColour = DEFAULTFOREGROUND;
  consoleOutput.backgroundColour = DEFAULTBACKGROUND;
  

  // Repeat the above procedure for our console input stream
  consoleInput.s = kernelStreamNew(TEXTSTREAMSIZE, itemsize_char);

  // Success?
  if (consoleInput.s == NULL)
    {
      kernelError(kernel_error,
		  "Unable to create the console text input stream");
      return (status = ERR_NOTINITIALIZED);
    }

  consoleInput.sFn = (streamFunctions *) consoleInput.s->functions;

  // There's no output driver for an input stream
  consoleInput.outputDriver = NULL;

  // Make note that we've been initialized
  initialized = 1;

  // Return success
  return (status = 0);
}


kernelTextStream *kernelTextStreamGetConsoleInput(void)
{
  // Returns a pointer to the console input stream

  // Don't do anything unless we've been initialized
  if (!initialized)
    return (NULL);

  return (&consoleInput);
}


kernelTextStream *kernelTextStreamGetConsoleOutput(void)
{
  // Returns a pointer to the console output stream

  // Don't do anything unless we've been initialized
  if (!initialized)
    return (NULL);

  return (&consoleOutput);
}


int kernelTextStreamGetMode(void)
{
  // Don't do anything unless we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  // Returns the type of the screen output
  return (outputMode);
}


int kernelTextStreamSetMode(int newMode)
{
  // Sets the mode (text or graphic) of the screen output.  Returns 0
  // on success, negative otherwise.

  int status = 0;

  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check to make sure it's a valid mode
  if ((newMode != VIDEOTEXTMODE) && (newMode != VIDEOGRAPHICMODE))
    return (status = ERR_INVALID);

  // It's OK, so set it
  outputMode = newMode;

  // Assign the correct driver to the console text stream
  if (outputMode == VIDEOTEXTMODE)
    {
      consoleOutput.outputDriver = &textModeDriver;
    }

  // Return success
  return (status = 0);
}


int kernelTextStreamGetColumns(void)
{
  // Don't do anything unless we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  // Yup.  Returns the number of columns
  return (textColumns);
}


int kernelTextStreamGetRows(void)
{
  // Don't do anything unless we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  // Yup.  Returns the number of rows
  return (textRows);
}


int kernelTextStreamGetForeground(void)
{
  int status = 0;
  kernelTextStream *outputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return (status = ERR_INVALID);

  // Returns the foreground colour of the screen output
  return (outputStream->foregroundColour);
}


int kernelTextStreamSetForeground(int newColour)
{
  // Sets the foreground colour of the screen output.  Returns 0 
  // if successful, negative otherwise.

  int status = 0;
  kernelTextStream *outputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return (status = ERR_INVALID);

  // The colours are as follows:
  // 0  = black
  // 1  = blue
  // 2  = green
  // 3  = cyan
  // 4  = red
  // 5  = magenta
  // 6  = brown
  // 7  = light grey
  // 8  = dark grey
  // 9  = light blue
  // 10 = light green
  // 11 = light cyan
  // 12 = light red
  // 13 = light magenta
  // 14 = yellow
  // 15 = white

  // Check to make sure it's a valid colour
  if ((newColour < 0) || (newColour > 15))
    return (status = ERR_INVALID);

  // It's OK, so set it
  outputStream->foregroundColour = newColour;

  // Set it in the text output driver
  outputStream->outputDriver->setForeground(newColour);

  // Return success
  return (status = 0);
}


int kernelTextStreamGetBackground(void)
{
  int status = 0;
  kernelTextStream *outputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return (status = ERR_INVALID);

  // Returns the background colour of the screen output
  return (outputStream->backgroundColour);
}


int kernelTextStreamSetBackground(int newColour)
{
  // Sets the background colour of the screen output.  Returns 0 
  // if successful, negative otherwise.  Just like SetForegound, above.

  int status = 0;
  kernelTextStream *outputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return (status = ERR_INVALID);

  // Check to make sure it's a valid colour
  if ((newColour < 0) || (newColour > 15))
    return (status = ERR_INVALID);

  // It's OK, so set it
  outputStream->backgroundColour = newColour;

  // Set it in the text output driver
  outputStream->outputDriver->setBackground(newColour);

  // Return success
  return (status = 0);
}


int kernelTextPutc(int ascii)
{
  // Determines the current target of character output, then makes calls
  // to output the character.  Returns 0 if successful, negative otherwise.

  int status = 0;
  kernelTextStream *outputStream = NULL;
  char theChar[2];


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return (status = ERR_INVALID);

  theChar[0] = (char) ascii;
  theChar[1] = '\0';

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return (status = ERR_NOTIMPLEMENTED);

  // Call the text stream output driver routine with the characters
  // we were passed
  outputStream->outputDriver->print(theChar);

  // Return success
  return (status = 0);
}


int kernelTextPrint(const char *format, ...)
{
  // Determines the current target of character output, then makes calls
  // to output the text (without a newline).  Returns 0 if successful, 
  // negative otherwise.

  int status = 0;
  va_list list;
  char output[MAXSTRINGLENGTH];
  kernelTextStream *outputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return (status = ERR_INVALID);

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return (status = ERR_NOTIMPLEMENTED);

  // Initialize the argument list
  va_start(list, format);

  // Expand the format string into an output string
  _expand_format_string(output, format, list);

  va_end(list);

  // We will call the text stream output driver routine with the 
  // characters we were passed
  outputStream->outputDriver->print(output);

  // Return success
  return (status = 0);
}


int kernelTextPrintLine(const char *format, ...)
{
  // Determines the current target of character output, then makes calls
  // to output the text (with a newline).  Returns 0 if successful, 
  // negative otherwise.

  int status = 0;
  va_list list;
  char output[MAXSTRINGLENGTH];
  kernelTextStream *outputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return (status = ERR_INVALID);

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return (status = ERR_NOTIMPLEMENTED);

  // Initialize the argument list
  va_start(list, format);

  // Expand the format string into an output string
  _expand_format_string(output, format, list);

  va_end(list);

  // We will call the text stream output driver routine with the 
  // characters we were passed
  outputStream->outputDriver->print(output);

  // Print the newline too
  outputStream->outputDriver->print("\n");

  // Return success
  return (status = 0);
}


void kernelTextNewline(void)
{
  // This routine executes a newline

  kernelTextStream *outputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return;

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return;

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return;

  // Call the text stream output driver routine to print the newline
  outputStream->outputDriver->print("\n");

  return;
}


void kernelTextBackSpace(void)
{
  // This routine executes a backspace (or delete)

  int cursorPosition = 0;
  kernelTextStream *outputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return;

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return;

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return;

  // We will call the text stream output driver routines to make the
  // backspace appear
      
  // Move the cursor back one position
  cursorPosition = outputStream->outputDriver->getCursorAddress();
  cursorPosition --;
  outputStream->outputDriver->setCursorAddress(cursorPosition);

  // Erase the character
  outputStream->outputDriver->print(" \0");

  // Move the cursor back again
  outputStream->outputDriver->setCursorAddress(cursorPosition);

  return;
}


void kernelTextTab(void)
{
  // This routine executes a hoizontal tab

  kernelTextStream *outputStream = NULL;
  int tabChars = 0;
  char spaces[DEFAULT_TAB + 1];
  int count;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return;

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return;

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return;

  // Figure out how many characters the tab should be
  tabChars = (DEFAULT_TAB - (outputStream->outputDriver->getCursorAddress() %
			     DEFAULT_TAB));

  if (tabChars == 0)
    tabChars = DEFAULT_TAB;

  // Fill up the spaces buffer with the appropriate number of spaces
  for (count = 0; count < tabChars; count ++)
    spaces[count] = ' ';

  spaces[tabChars] = NULL;

  // Call the text stream output driver to print the spaces
  outputStream->outputDriver->print(spaces);

  return;
}


void kernelTextCursorUp(void)
{
  // This routine moves the cursor up

  kernelTextStream *outputStream = NULL;
  int cursorPosition;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return;

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return;

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return;

  // We will call the text stream output driver routines to make the
  // cursor move up one row (which is actually moving it backwards
  // by "textColumns" spaces)
  cursorPosition = outputStream->outputDriver->getCursorAddress();

  if ((cursorPosition - textColumns) >= 0)
    cursorPosition -= textColumns;

  outputStream->outputDriver->setCursorAddress(cursorPosition);

  return;
}


void kernelTextCursorDown(void)
{
  // This routine executes a does a cursor-down.

  kernelTextStream *outputStream = NULL;
  int cursorPosition;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return;

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return;

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return;

  // We will call the text stream output driver routines to make the
  // cursor move down one row (which is actually moving it forwards
  // by "textColumns" spaces)
  cursorPosition = outputStream->outputDriver->getCursorAddress();

  if ((cursorPosition + textColumns) < (textRows * textColumns))
    cursorPosition += textColumns;

  outputStream->outputDriver->setCursorAddress(cursorPosition);

  return;
}


void kernelTextCursorLeft(void)
{
  // This routine executes a cursor left

  kernelTextStream *outputStream = NULL;
  int cursorPosition;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return;

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return;

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return;

  // We will call the text stream output driver routines to make the
  // cursor move left
  cursorPosition = outputStream->outputDriver->getCursorAddress();

  if ((cursorPosition - 1) >= 0)
    cursorPosition -= 1;

  outputStream->outputDriver->setCursorAddress(cursorPosition);

  return;
}


void kernelTextCursorRight(void)
{
  // This routine executes a cursor right

  kernelTextStream *outputStream = NULL;
  int cursorPosition;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return;

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return;

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return;

  // We will call the text stream output driver routines to make the
  // cursor move right.
  cursorPosition = outputStream->outputDriver->getCursorAddress();

  if ((cursorPosition + 1) < (textRows * textColumns))
    cursorPosition += 1;

  outputStream->outputDriver->setCursorAddress(cursorPosition);

  return;
}


int kernelTextGetNumColumns(void)
{
  // Returns total number of screen columns
  return (textColumns);
}


int kernelTextGetNumRows(void)
{
  // Returns total number of screen rows
  return (textRows);
}


int kernelTextGetColumn(void)
{
  // Returns the current cursor column (zero-based)

  kernelTextStream *outputStream = NULL;
  int cursorPosition;

  // Don't do anything unless we've been initialized
  if (!initialized)
    return (0);

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return (0);

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return (0);

  // We will call the text stream output driver to get the current
  // cursor address
  cursorPosition = outputStream->outputDriver->getCursorAddress();

  // Calculate the column, which is position % numcolumns
  return (cursorPosition % textColumns);
}


void kernelTextSetColumn(int newColumn)
{
  // Sets the current cursor column (zero-based), leaving it in the same
  // row as before

  kernelTextStream *outputStream = NULL;
  int cursorPosition = 0;
  int row = 0;

  // Don't do anything unless we've been initialized
  if (!initialized)
    return;

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return;

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return;

  // We will call the text stream output driver to get the current
  // cursor address
  cursorPosition = outputStream->outputDriver->getCursorAddress();

  // Calculate the row, which is position / numcolumns
  row = (cursorPosition / textColumns);

  // Calculate the new cursor position
  cursorPosition = (row * textColumns);
  cursorPosition += newColumn;

  // Set the new cursor position
  outputStream->outputDriver->setCursorAddress(cursorPosition);

  return;
}


int kernelTextGetRow(void)
{
  // Returns the current cursor column (zero-based)

  kernelTextStream *outputStream = NULL;
  int cursorPosition;

  // Don't do anything unless we've been initialized
  if (!initialized)
    return (0);

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return (0);

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return (0);

  // We will call the text stream output driver to get the current
  // cursor address
  cursorPosition = outputStream->outputDriver->getCursorAddress();

  // Calculate the row, which is position / numcolumns
  return (cursorPosition / textColumns);
}


void kernelTextSetRow(int newRow)
{
  // Sets the current cursor row (zero-based), leaving it in the same
  // column as before

  kernelTextStream *outputStream = NULL;
  int cursorPosition = 0;
  int column = 0;

  // Don't do anything unless we've been initialized
  if (!initialized)
    return;

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return;

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return;

  // We will call the text stream output driver to get the current
  // cursor address
  cursorPosition = outputStream->outputDriver->getCursorAddress();

  // Calculate the column, which is position % numcolumns
  column = (cursorPosition % textColumns);

  // Calculate the new cursor position
  cursorPosition = (newRow * textColumns);
  cursorPosition += column;

  // Set the new cursor position
  outputStream->outputDriver->setCursorAddress(cursorPosition);

  return;
}


void kernelTextClearScreen(void)
{
  // This routine clears the screen

  kernelTextStream *outputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return;

  // Get the text output stream for the current process
  outputStream = kernelMultitaskerGetTextOutput();

  if (outputStream == NULL)
    return;

  if (outputMode == VIDEOGRAPHICMODE)
    // Not implemented currently
    return;

  // Call the text stream output driver routine to clear the screen
  outputStream->outputDriver->clearScreen();
}


int kernelTextInputCount(void)
{
  // Returns the number of characters that are currently waiting in the
  // input stream.

  int numberChars = 0;
  kernelTextStream *inputStream = NULL;
  

  // Don't do anything unless we've been initialized
  if (!initialized)
    return (numberChars = ERR_NOTINITIALIZED);

  // Get the text input stream for the current process
  inputStream = kernelMultitaskerGetTextInput();

  if (inputStream == NULL)
    return (numberChars = ERR_INVALID);

  // Get the number of characters in the stream
  numberChars = inputStream->s->count;

  // Return the value from the call
  return (numberChars);
}


int kernelTextInputGetc(char *returnChar)
{
  // Returns a single character from the keyboard buffer.

  int status = 0;
  kernelTextStream *inputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure returnChar isn't NULL
  if (returnChar == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the text input stream for the current process
  inputStream = kernelMultitaskerGetTextInput();

  if (inputStream == NULL)
    return (status = ERR_INVALID);

  // Call the 'pop' function for this stream
  status = inputStream->sFn->pop(inputStream->s, returnChar);

  // Return the status from the call
  return (status);
}


int kernelTextInputPeek(char *returnChar)
{
  // Returns a single character from the keyboard buffer.

  int status = 0;
  kernelTextStream *inputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure returnChar isn't NULL
  if (returnChar == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the text input stream for the current process
  inputStream = kernelMultitaskerGetTextInput();

  if (inputStream == NULL)
    return (status = ERR_INVALID);

  // Call the 'pop' function for this stream
  status = inputStream->sFn->pop(inputStream->s, returnChar);

  if (status)
    return (status);

  // Push the character back into the stream
  status = inputStream->sFn->push(inputStream->s, *returnChar);

  // Return the status from the call
  return (status);
}


int kernelTextInputReadN(int numberRequested, char *returnChars)
{
  // Gets the requested number of characters from the keyboard buffer, 
  // and puts them in the string supplied.

  int status = 0;
  kernelTextStream *inputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure returnChars isn't NULL
  if (returnChars == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the text input stream for the current process
  inputStream = kernelMultitaskerGetTextInput();

  if (inputStream == NULL)
    return (status = ERR_INVALID);

  // Call the 'popN' function for this stream
  status = inputStream->sFn->popN(inputStream->s, numberRequested,
				       returnChars);

  // Return the status from the call
  return (status);
}


int kernelTextInputReadAll(char *returnChars)
{
  // Takes a pointer to an initialized character array, and fills it
  // with all of the characters present in the buffer.

  int status = 0;
  kernelTextStream *inputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure returnChars isn't NULL
  if (returnChars == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the text input stream for the current process
  inputStream = kernelMultitaskerGetTextInput();

  if (inputStream == NULL)
    return (status = ERR_INVALID);

  // Get all of the characters in the stream.  Call the 'popN' function
  // for this stream
  status = inputStream->sFn->popN(inputStream->s, inputStream->s->count,
				       returnChars);

  // Return the status from the call
  return (status);
}


int kernelTextInputAppend(int ascii)
{
  // Adds a single character to the text input stream.

  int status = 0;
  kernelTextStream *inputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text input stream for the current process
  inputStream = kernelMultitaskerGetTextInput();

  if (inputStream == NULL)
    return (status = ERR_INVALID);

  // Call the 'append' function for this stream
  status = inputStream->sFn->append(inputStream->s,
					 (unsigned char) ascii);

  // Return the status from the call
  return (status);
}


int kernelTextInputAppendN(int numberRequested, char *addCharacters)
{
  // Adds the requested number of characters to the text input stream.  

  int status = 0;
  kernelTextStream *inputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure addCharacters isn't NULL
  if (addCharacters == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the text input stream for the current process
  inputStream = kernelMultitaskerGetTextInput();

  if (inputStream == NULL)
    return (status = ERR_INVALID);

  // Call the 'appendN' function for this stream
  status = inputStream->sFn->appendN(inputStream->s,
					  numberRequested, addCharacters);

  // Return the status from the call
  return (status);
}


int kernelTextInputRemove(void)
{
  // Removes a single character from the keyboard buffer.

  int status = 0;
  kernelTextStream *inputStream = NULL;
  char junk = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text input stream for the current process
  inputStream = kernelMultitaskerGetTextInput();

  if (inputStream == NULL)
    return (status = ERR_INVALID);

  // Call the 'pop' function for this stream, and discard the char we
  // get back.
  status = inputStream->sFn->pop(inputStream->s, &junk);

  // Return the status from the call
  return (status);
}


int kernelTextInputRemoveN(int numberRequested)
{
  // Removes the requested number of characters from the keyboard buffer.  

  int status = 0;
  kernelTextStream *inputStream = NULL;
  char junk[TEXTSTREAMSIZE];


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text input stream for the current process
  inputStream = kernelMultitaskerGetTextInput();

  if (inputStream == NULL)
    return (status = ERR_INVALID);

  // Call the 'popN' function for this stream, and discard the chars we
  // get back.
  status = inputStream->sFn->popN(inputStream->s, numberRequested,
				       junk);

  // Return the status from the call
  return (status);
}


int kernelTextInputRemoveAll(void)
{
  // Removes all data from the keyboard buffer.

  int status = 0;
  kernelTextStream *inputStream = NULL;


  // Don't do anything unless we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get the text input stream for the current process
  inputStream = kernelMultitaskerGetTextInput();

  if (inputStream == NULL)
    return (status = ERR_INVALID);

  // Call the 'clear' function for this stream
  status = inputStream->sFn->clear(inputStream->s);

  // Return the status from the call
  return (status);
}
