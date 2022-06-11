//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  edit.c
//

// This is a simple text editor

/* This is the text that appears when a user requests help about this program
<help>

 -- edit --

Simple, interactive text editor.

Usage:
  edit <file>

(Only available in graphics mode)

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/api.h>
#include <sys/cdefs.h>
#include <sys/text.h>

typedef struct {
  unsigned filePos;
  int length;
  int screenStartRow;
  int screenEndRow;
  int screenLength;
  int screenRows;

} screenLineInfo;

static int processId = 0;
static int screenColumns = 0;
static int screenRows = 0;
static int foregroundColor = 0;
static int backgroundColor = 0;
static file theFile;
static unsigned fileSize = 0;
static char *buffer = NULL;
static unsigned bufferSize = 0;
static screenLineInfo *screenLines = NULL;
static int numScreenLines = 0;
static unsigned firstLineFilePos = 0;
static unsigned lastLineFilePos = 0;
static unsigned cursorLineFilePos = 0;
static int cursorColumn = 0;
static unsigned line = 0;
static unsigned screenLine = 0;
static unsigned numLines = 0;
static int readOnly = 1;
static int modified = 0;
static char *discardQuestion  = "File has been modified.  Discard changes?";
static char *fileNameQuestion = "Please enter the name of the file to edit:";

// GUI stuff
static int graphics = 0;
static objectKey window = NULL;
static objectKey menuOpen = NULL;
static objectKey menuSave = NULL;
static objectKey menuQuit = NULL;
static objectKey font = NULL;
static objectKey textArea = NULL;
static objectKey statusLabel = NULL;


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <file>\n", name);
  return;
}


static void error(const char *format, ...)
{
  // Generic error message code for either text or graphics modes
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  _expandFormatString(output, format, list);
  va_end(list);

  if (graphics)
    windowNewErrorDialog(window, "Error", output);
  else
    {
    }
}


static void updateStatus(void)
{
  // Update the status line

  int column = textGetColumn();
  int row = textGetRow();
  char statusMessage[MAXSTRINGLENGTH];

  sprintf(statusMessage, "%s%s  %u/%u", theFile.name,
	  (modified? " (modified)" : ""), line, numLines);

  if (graphics)
    {
      windowComponentSetData(statusLabel, statusMessage,
			     strlen(statusMessage));
    }
  else
    {
      // Put the cursor on the last line and switch colors
      textSetForeground(backgroundColor);
      textSetBackground(foregroundColor);
      textSetColumn(0);
      textSetRow(screenRows);

      printf(statusMessage);
      // Extend to the end of the line
      while (textGetColumn() < (screenColumns - 1))
	putchar(' ');

      // Restore colors and put the cursor back where it belongs
      textSetForeground(foregroundColor);
      textSetBackground(backgroundColor);
      textSetColumn(column);
      textSetRow(row);
    }
}


static void countLines(void)
{
  // Sets the 'numLines' variable to the number of lines in the file
  
  unsigned count;

  numLines = 0;
  for (count = 0; count < fileSize; count ++)
    {
      if (buffer[count] == '\n')
	numLines += 1;
    }

  if (!numLines)
    numLines = 1;
}


static void printLine(int lineNum)
{
  // Given a screen line number, print it on the screen at the current cursor
  // position and update its length fields in the array

  char character;
  int maxScreenLength = 0;
  int count1, count2;

  screenLines[lineNum].length = 0;
  screenLines[lineNum].screenLength = 0;

  maxScreenLength =
    ((screenRows - screenLines[lineNum].screenStartRow) * screenColumns);

  for (count1 = 0; (screenLines[lineNum].screenLength < maxScreenLength);
       count1 ++)
    {
      character = buffer[screenLines[lineNum].filePos + count1];

      // Look out for tab characters
      if (character == (char) 9)
	{
	  textTab();
	  
	  screenLines[lineNum].screenLength +=
	    (TEXT_DEFAULT_TAB - (screenLines[lineNum].screenLength %
				 TEXT_DEFAULT_TAB));
	}

      else
	{
	  if (character == (char) 10)
	    {
	      for (count2 = 0; count2 < (screenColumns -
					 (screenLines[lineNum].screenLength %
					  screenColumns)); count2 ++)
		textPutc(' ');

	      screenLines[lineNum].screenLength += 1;
	      break;
	    }
	  else
	    {
	      textPutc(character);
	      screenLines[lineNum].screenLength += 1;
	    }
	}

      screenLines[lineNum].length += 1;
    }

  screenLines[lineNum].screenEndRow = (textGetRow() - 1);

  screenLines[lineNum].screenRows =
    ((screenLines[lineNum].screenEndRow -
      screenLines[lineNum].screenStartRow) + 1);
}


static void showScreen(void)
{
  // Show the screen at the current file position

  textScreenClear();
  bzero(screenLines, (screenRows * sizeof(screenLineInfo)));

  screenLines[0].filePos = firstLineFilePos;

  for (numScreenLines = 0; numScreenLines < screenRows; )
    {
      lastLineFilePos = screenLines[numScreenLines].filePos;

      screenLines[numScreenLines].screenStartRow = textGetRow();

      printLine(numScreenLines);

      if (screenLines[numScreenLines].screenEndRow >= (screenRows - 1))
	break;

      if (numScreenLines < (screenRows - 1))
	screenLines[numScreenLines + 1].filePos =
	  (screenLines[numScreenLines].filePos +
	   screenLines[numScreenLines].length + 1);

      if (screenLines[numScreenLines].filePos >= fileSize)
	break;

      numScreenLines += 1;
    }

  updateStatus();
}


static void setCursorColumn(int column)
{
  int screenColumn = 0;
  int count;

  if (column > screenLines[screenLine].length)
    column = screenLines[screenLine].length;

  for (count = 0; count < column; count ++)
    {
      if (buffer[screenLines[screenLine].filePos + count] == '\t')
	screenColumn += (TEXT_DEFAULT_TAB - (screenColumn % TEXT_DEFAULT_TAB));
      else
	screenColumn += 1;
    }

  textSetRow(screenLines[screenLine].screenStartRow +
	     (screenColumn / screenColumns));
  textSetColumn(screenColumn % screenColumns);

  cursorColumn = column;
}


static int loadFile(const char *fileName)
{
  int status = 0;
  disk theDisk;
  int openFlags = OPENMODE_READWRITE;

  // Initialize the file structure
  bzero(&theFile, sizeof(file));

  if (buffer)
    free(buffer);

  // Call the "find file" routine to see if we can get the file
  status = fileFind(fileName, &theFile);

  if (status >= 0)
    {
      // Find out whether we are currently running on a read-only filesystem
      if (!fileGetDisk(fileName, &theDisk))
	readOnly = theDisk.readOnly;

      if (readOnly)
	openFlags = OPENMODE_READ;
    }

  if ((status < 0) || (theFile.size == 0))
    {
      // The file either doesn't exist or is zero-length.

      if (status < 0)
	// The file doesn't exist; try to create one
	openFlags |= OPENMODE_CREATE;

      status = fileOpen(fileName, openFlags, &theFile);
      if (status < 0)
	return (status);

      // Use a default initial buffer size of one file block
      bufferSize = theFile.blockSize;
      buffer = malloc(bufferSize);
      if (buffer == NULL)
	return (status = ERR_MEMORY);
    }
  else
    {
      // The file exists and has data in it

      status = fileOpen(fileName, openFlags, &theFile);
      if (status < 0)
	return (status);

      // Allocate a buffer to store the file contents in
      bufferSize = (theFile.blocks * theFile.blockSize);
      buffer = malloc(bufferSize);
      if (buffer == NULL)
	return (status = ERR_MEMORY);

      status = fileRead(&theFile, 0, theFile.blocks, buffer);
      if (status < 0)
	return (status);
    }

  fileSize = theFile.size;
  firstLineFilePos = 0;
  lastLineFilePos = 0;
  cursorLineFilePos = 0;
  cursorColumn = 0;
  line = 0;
  screenLine = 0;
  numLines = 0;
  modified = 0;

  countLines();
  showScreen();
  setCursorColumn(0);

  if (graphics)
    {
      if (readOnly)
	windowComponentSetEnabled(menuSave, 0);

      windowComponentFocus(textArea);
    }

  return (status = 0);
}


static int saveFile(void)
{
  int status = 0;
  int blocks =
    ((fileSize / theFile.blockSize) + ((fileSize % theFile.blockSize)? 1 : 0));

  status = fileWrite(&theFile, 0, blocks, buffer);
  if (status < 0)
    return (status);

  modified = 0;
  updateStatus();

  if (graphics)
    windowComponentFocus(textArea);

  return (status = 0);
}


static unsigned previousLineStart(unsigned filePos)
{
  unsigned lineStart = 0;

  if (filePos == 0)
    return (lineStart = 0);

  lineStart = (filePos - 1);

  // Watch for the start of the buffer
  if (!lineStart)
    return (lineStart);

  // Lines that end with a newline (most)
  if (buffer[lineStart] == '\n')
    lineStart -= 1;

  // Watch for the start of the buffer
  if (!lineStart)
    return (lineStart);

  for ( ; buffer[lineStart] != '\n'; lineStart --)
    // Watch for the start of the buffer
    if (!lineStart)
      return (lineStart);

  lineStart += 1;

  return (lineStart);
}


static unsigned nextLineStart(unsigned filePos)
{
  unsigned lineStart = 0;

  if (filePos >= fileSize)
    return (lineStart = (fileSize - 1));

  lineStart = filePos;

  // Determine where the current line ends
  while (lineStart < (fileSize - 1))
    {
      if (buffer[lineStart] == '\n')
	{
	  lineStart += 1;
	  break;
	}
      else
	lineStart += 1;
    }

  return (lineStart);
}


static void cursorUp(void)
{
  if (!line)
    return;

  cursorLineFilePos = previousLineStart(cursorLineFilePos);

  // Do we need to scroll the screen up?
  if (cursorLineFilePos < firstLineFilePos)
    {
      firstLineFilePos = cursorLineFilePos;
      showScreen();
    }
  else
    textSetRow(screenLines[--screenLine].screenStartRow);

  setCursorColumn(cursorColumn);

  line -= 1;
  return;
}


static void cursorDown(void)
{
  if (line >= numLines)
    return;

  cursorLineFilePos = nextLineStart(cursorLineFilePos);

  if (cursorLineFilePos > lastLineFilePos)
    {
      // Do we need to scroll the screen down?
      firstLineFilePos = nextLineStart(firstLineFilePos);
      showScreen();
    }
  else
    textSetRow(screenLines[++screenLine].screenStartRow);

  setCursorColumn(cursorColumn);

  line += 1;
  return;
}


static void cursorLeft(void)
{
  if (cursorColumn)
    setCursorColumn(cursorColumn - 1);
  else
    {
      cursorUp();
      setCursorColumn(screenLines[screenLine].length);
    }

  return;
}


static void cursorRight(void)
{
  if (cursorColumn < screenLines[screenLine].length)
    setCursorColumn(cursorColumn + 1);
  else
    {
      cursorDown();
      setCursorColumn(0);
    }

  return;
}


static int expandBuffer(unsigned length)
{
  // Expand the buffer by at least 'length' characters

  int status = 0;
  unsigned tmpBufferSize = 0;
  char *tmpBuffer = NULL;

  // Allocate more buffer, rounded up to the nearest block size of the file
  tmpBufferSize = (bufferSize + (((length / theFile.blockSize) +
				  ((length % theFile.blockSize)? 1 : 0)) *
				 theFile.blockSize));
  tmpBuffer = realloc(buffer, tmpBufferSize);
  if (tmpBuffer == NULL)
    return (status = ERR_MEMORY);
  buffer = tmpBuffer;
  bufferSize = tmpBufferSize;
  
  return (status = 0);
}


static void shiftBuffer(unsigned filePos, int shiftBy)
{
  // Shift the contents of the buffer at 'filePos' by 'shiftBy' characters
  // (positive or negative)

  unsigned shiftChars = 0;
  unsigned count;

  shiftChars = (fileSize - filePos);

  if (shiftChars)
    {
      if (shiftBy > 0)
	{
	  for (count = 1; count <= shiftChars; count ++)
	    buffer[fileSize + (shiftBy - count)] =
	      buffer[filePos + (shiftChars - count)];
	}
      else if (shiftBy < 0)
	{
	  filePos -= 1;
	  shiftBy *= -1;
	  for (count = 0; count < shiftChars; count ++)
	    buffer[filePos + count] = buffer[filePos + shiftBy + count];
	}
    }
}


static int insertChars(char *string, unsigned length)
{
  // Insert characters at the current position.

  int status = 0;
  int oldRows = 0;
  int count;

  // Do we need a bigger buffer?
  if ((fileSize + length) >= bufferSize)
    {
      status = expandBuffer(length);
      if (status < 0)
	return (status);
    }

  if ((screenLines[screenLine].filePos + cursorColumn) < (fileSize - 1))
    // Shift data that occurs later in the buffer.
    shiftBuffer((screenLines[screenLine].filePos + cursorColumn), length);

  // Copy the data
  strncpy((buffer + screenLines[screenLine].filePos + cursorColumn), string,
	  length);

  // We need to adjust the recorded file positions of all lines that follow
  // on the screen
  for (count = (screenLine + 1); count < numScreenLines; count ++)
    screenLines[count].filePos += length;

  fileSize += length;
  modified = 1;

  textSetRow(screenLines[screenLine].screenStartRow);
  textSetColumn(0);
  oldRows = screenLines[screenLine].screenRows;
  printLine(screenLine);
  
  // If the line now occupies more screen lines, redraw the lines below it.
  if (screenLines[screenLine].screenRows != oldRows)
    {
      for (count = (screenLine + 1); count < numScreenLines; count ++)
	{
	  screenLines[count].screenStartRow = textGetRow();
	  printLine(count);
	}
    }

  return (status = 0);
}


static void deleteChars(unsigned length)
{
  // Delete characters at the current position.

  int oldRows = 0;
  int count;

  if ((screenLines[screenLine].filePos + cursorColumn) < (fileSize - 1))
    // Shift data that occurs later in the buffer.
    shiftBuffer((screenLines[screenLine].filePos + cursorColumn + 1), -length);

  // Clear data
  bzero((buffer + (fileSize - length)), length);

  // We need to adjust the recorded file positions of all lines that follow
  // on the screen
  for (count = (screenLine + 1); count < numScreenLines; count ++)
    screenLines[count].filePos -= length;

  fileSize -= length;
  modified = 1;

  textSetRow(screenLines[screenLine].screenStartRow);
  textSetColumn(0);
  oldRows = screenLines[screenLine].screenRows;
  printLine(screenLine);

  // If the line now occupies fewer screen lines, redraw the lines below it.
  if (screenLines[screenLine].screenRows != oldRows)
    {
      for (count = (screenLine + 1); count < numScreenLines; count ++)
	{
	  screenLines[count].screenStartRow = textGetRow();
	  printLine(count);
	}
    }

  return;
}


static int edit(void)
{
  // This routine is the base from which we do all the editing.

  int status = 0;
  char character = '\0';
  int oldRow = 0;
  int endLine = 0;
  
  while (1)
    {
      textInputGetc(&character);

      switch (character)
	{
	case (char) 17:
	  // UP cursor key
	  cursorUp();
	  break;

	case (char) 20:
	  // DOWN cursor key
	  cursorDown();
	  break;

	case (char) 18:
	  // LEFT cursor key
	  cursorLeft();
	  break;

	case (char) 19:
	  // RIGHT cursor key
	  cursorRight();
	  break;

	case (char) 8:
	  // BACKSPACE key
	  oldRow = screenLines[screenLine].screenStartRow;
	  cursorLeft();
	  deleteChars(1);
	  // If we were at the beginning of a line...
	  if (screenLines[screenLine].screenStartRow != oldRow)
	    {
	      numLines -= 1;
	      showScreen();
	    }
	  setCursorColumn(cursorColumn);
	  break;

	case (char) 127:
	  // DEL key
	  endLine = (cursorColumn >= screenLines[screenLine].length);
	  deleteChars(1);
	  // If we were at the end of a line...
	  if (endLine)
	    {
	      numLines -= 1;
	      showScreen();
	    }
	  setCursorColumn(cursorColumn);
	  break;

	case (char) 10:
	  // ENTER key
	  status = insertChars(&character, 1);
	  if (status < 0)
	    break;
	  numLines += 1;
	  showScreen();
	  setCursorColumn(0);
	  cursorDown();
	  break;

	default:
	  // Typing anything else.  Is it printable?
	  status = insertChars(&character, 1);
	  if (status < 0)
	    break;
	  setCursorColumn(cursorColumn + 1);
	  break;
	}

      updateStatus();
    }

  return (status);
}


static int askDiscardChanges(void)
{
  int response = 0;

  if (graphics)
    {
      response =
	windowNewChoiceDialog(window, "Discard changes?", discardQuestion,
			      (char *[]) { "Discard", "Cancel" }, 2, 1);
      if (response == 0)
	return (1);
      else
	return (0);
    }
  else
    {
      return (0);
    }
}


static int askFileName(char *fileName)
{
  int status = 0;

  if (graphics)
    {
      // Prompt for a file name
      status =
	windowNewFileDialog(window, "Enter filename", fileNameQuestion, "/",
			    fileName, MAX_PATH_NAME_LENGTH);
      return (status);
    }
  else
    {
      return (status = 0);
    }
}


static void openFileThread(void)
{
  int status = 0;
  char *fileName = NULL;

  fileName = malloc(MAX_PATH_NAME_LENGTH);
  if (fileName == NULL)
    goto out;

  status = askFileName(fileName);
  if (status != 1)
    goto out;

  status = loadFile(fileName);
  if (status < 0)
    error("Error %d loading file", status);

 out:
  if (fileName)
    free(fileName);

  multitaskerTerminate(status);
}


static int calcCursorColumn(int screenColumn)
{
  // Given an on-screen column, calculate the real character column i.e. as
  // we would pass to setCursorColumn(), above.

  int column = 0;
  int count;

  for (count = 0; column < screenColumn; count ++)
    {
      if (buffer[screenLines[screenLine].filePos + count] == '\t')
	column += (TEXT_DEFAULT_TAB - (column % TEXT_DEFAULT_TAB));
      else
	column += 1;
    }

  return (count);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  int status = 0;
  int oldScreenLine = 0;
  int newColumn = 0;
  int newRow = 0;
  int count;

  // Look for window close
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == menuQuit) && (event->type & EVENT_SELECTION)))
    {
      if (!modified || askDiscardChanges())
	{
	  windowGuiStop();
	  multitaskerKillProcess(processId, 0 /* no force */);
	}
      return;
    }

  // Look for window resize
  else if ((key == window) && (event->type == EVENT_WINDOW_RESIZE))
    {
      screenColumns = textGetNumColumns();
      screenRows = textGetNumRows();
      showScreen();
    }

  // Look for menu events

  else if ((key == menuSave) && (event->type & EVENT_SELECTION))
    {
      status = saveFile();
      if (status < 0)
	error("Error %d saving file", status);
    }

  else if ((key == menuOpen) && (event->type & EVENT_SELECTION))
    {
      if (!modified || askDiscardChanges())
	{
	  if (multitaskerSpawn(&openFileThread, "open file", 0, NULL) < 0)
	    error("Unable to launch file dialog");
	}
    }

  // Look for cursor movements caused by clicking in the text area

  else if ((key == textArea) && (event->type & EVENT_CURSOR_MOVE))
    {
      // The user clicked to move the cursor, which is a pain.  We need to
      // try to figure out the new screen line.

      oldScreenLine = screenLine;
      newRow = textGetRow();

      for (count = 0; count < numScreenLines; count ++)
	{
	  if ((newRow >= screenLines[count].screenStartRow) &&
	      (newRow <= screenLines[count].screenEndRow))
	    {
	      screenLine = count;
	      cursorLineFilePos = screenLines[count].filePos;
	      line += (screenLine - oldScreenLine);
	      newColumn = (((newRow - screenLines[count].screenStartRow) *
			    screenColumns) + textGetColumn());
	      setCursorColumn(calcCursorColumn(newColumn));
	      updateStatus();
	      break;
	    }
	}
    }
}


static void constructWindow(void)
{
  int rows = 25;
  componentParameters params;

  // Create a new window
  window = windowNew(processId, "Edit");

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 1;
  params.padRight = 1;
  params.padTop = 1;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;

  // Create the top 'file' menu
  objectKey menuBar = windowNewMenuBar(window, &params);
  objectKey menu = windowNewMenu(menuBar, "File", &params);
  menuOpen = windowNewMenuItem(menu, "Open", &params);
  windowRegisterEventHandler(menuOpen, &eventHandler);
  menuSave = windowNewMenuItem(menu, "Save", &params);
  windowRegisterEventHandler(menuSave, &eventHandler);
  menuQuit = windowNewMenuItem(menu, "Quit", &params);
  windowRegisterEventHandler(menuQuit, &eventHandler);

  // Set up the font for our main text area
  fontGetDefault(&font);
  if (fontLoad("/system/fonts/xterm-normal-10.bmp", "xterm-normal-10",
	       &font, 1) < 0)
    // We'll be using the system font we guess.  The system font can
    // comfortably show more rows
    rows = 40;
  
  // Put a text area in the window
  params.flags |=
    (WINDOW_COMPFLAG_STICKYFOCUS | WINDOW_COMPFLAG_CLICKABLECURSOR);
  params.gridY += 1;
  params.font = font;
  textArea = windowNewTextArea(window, 80, rows, 0, &params);
  windowRegisterEventHandler(textArea, &eventHandler);

  // Use the text area for all our input and output
  windowSetTextOutput(textArea);

  // Put a status label below the text area
  params.flags &= ~WINDOW_COMPFLAG_STICKYFOCUS;
  params.gridY += 1;
  params.padBottom = 1;
  fontLoad("/system/fonts/arial-bold-10.bmp", "arial-bold-10",
	   &(params.font), 1);
  statusLabel = windowNewTextLabel(window, "", &params);
  windowComponentSetWidth(statusLabel, windowComponentGetWidth(textArea));

  // Go live.
  windowSetVisible(window, 1);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  // Run the GUI as a thread
  windowGuiThread();
}


int main(int argc, char *argv[])
{
  int status = 0;
  textScreen screen;
  char *shortFileName = NULL;
  char *fullFileName = NULL;

  processId = multitaskerGetCurrentProcessId();

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  // For the moment, only operate in graphics mode
  if (!graphics)
    {
      printf("\nThe \"%s\" command only works in graphics mode\n",
	     (argc? argv[0] : ""));
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  if (getopt(argc, argv, "T") == 'T')
    // Force text mode
    graphics = 0;

  if (graphics)
    constructWindow();
  else
    // Save the current screen
    textScreenSave(&screen);

  // Get screen parameters
  screenColumns = textGetNumColumns();
  screenRows = textGetNumRows();
  if (!graphics)
    // Save one for the status line
    screenRows -= 1;
  foregroundColor = textGetForeground();
  backgroundColor = textGetBackground();

  // Clear it
  textScreenClear();
  textEnableScroll(0);

  screenLines = malloc(screenRows * sizeof(screenLineInfo));
  shortFileName = malloc(MAX_PATH_NAME_LENGTH);
  fullFileName = malloc(MAX_PATH_NAME_LENGTH);
  if ((screenLines == NULL) || (shortFileName == NULL) ||
      (fullFileName == NULL))
    {
      errno = status = ERR_MEMORY;
      perror(argv[0]);
      goto out;
    }

  if (argc < 2)
    {
      if (graphics)
	{
	  // Prompt for a file name
	  status = askFileName(shortFileName);

	  if (status != 1)
	    {
	      if (status != 0)
		{
		  errno = status;
		  perror(argv[0]);
		}

	      goto out;
	    }
	}
      else
	{
	  // In text mode we need a filename as an argument
	  usage(argv[0]);      
	  status = errno = ERR_ARGUMENTCOUNT;
	  goto out;
	}
    }
  else
    strcpy(shortFileName, argv[argc - 1]);

  // Make sure the file name is complete
  vshMakeAbsolutePath(shortFileName, fullFileName);

  status = loadFile(fullFileName);
  if (status < 0)
    {
      errno = status;
      perror(argv[0]);
      goto out;
    }

  // Go
  status = edit();
  if (status < 0)
    errno = status;

 out:

  textEnableScroll(1);

  if (graphics)
    {
      // Stop our GUI thread
      windowGuiStop();

      // Destroy the window
      windowDestroy(window);
    }
  else
    {
      textScreenRestore(&screen);

      if (screen.data)
	memoryRelease(screen.data);
    }

  if (screenLines)
    free(screenLines);
  if (shortFileName)
    free(shortFileName);
  if (fullFileName)
    free (fullFileName);
  if (buffer)
    free(buffer);

  // Return success
  return (status);
}
