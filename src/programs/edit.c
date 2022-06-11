//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  edit.c
//

// This is a simple text editor

/* This is the text that appears when a user requests help about this program
<help>

 -- edit --

Simple, interactive text editor.

Usage:
  edit [-T] [file]

(Only available in graphics mode)

Options:
-T  : Force text mode operation

</help>
*/

#include <libintl.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/ascii.h>
#include <sys/disk.h>
#include <sys/env.h>
#include <sys/errors.h>
#include <sys/file.h>
#include <sys/font.h>
#include <sys/text.h>
#include <sys/window.h>

#define _(string) gettext(string)
#define gettext_noop(string) (string)

#define WINDOW_TITLE		_("Edit")
#define FILE_MENU			_("File")
#define OPEN				gettext_noop("Open")
#define SAVE				gettext_noop("Save")
#define QUIT				gettext_noop("Quit")
#define UNTITLED_FILENAME	_("Untitled")
#define FILENAMEQUESTION	_("Please enter the name of the file to edit:")
#define DISCARDQUESTION		_("File has been modified.  Discard changes?")

#define TAB_CHARS(column)	(TEXT_DEFAULT_TAB - (column % TEXT_DEFAULT_TAB))

// Information about a line of text on the screen
typedef struct {
	unsigned filePos;
	int length;
	int screenLength;

} screenLineInfo;

// Information about the screen and what's being shown
static struct {
	int columns;
	int rows;
	screenLineInfo *lines;
	int numLines;

} screen;

// Information about the file
static struct {
	char *tempName;
	char *name;
	fileStream strm;
	unsigned size;
	char *buffer;
	unsigned bufferSize;
	unsigned numLines;
	int readOnly;
	int modified;

} editFile;

// Editing state
static struct {
	int screenLine;
	int screenColumn;
	unsigned fileLine;
	unsigned firstLineFilePos;
	unsigned lastLineFilePos;
	unsigned cursorFilePos;

} state;

static int processId = 0;
static int stop = 0;

// GUI stuff
static int graphics = 0;
static objectKey window = NULL;
static objectKey fileMenu = NULL;
static objectKey font = NULL;
static objectKey textArea = NULL;
static objectKey statusLabel = NULL;

#define FILEMENU_OPEN 0
#define FILEMENU_SAVE 1
#define FILEMENU_QUIT 2
windowMenuContents fileMenuContents = {
	3,
	{
		{ OPEN, NULL },
		{ SAVE, NULL },
		{ QUIT, NULL }
	}
};


__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
	// Generic error message code for either text or graphics modes

	va_list list;
	char output[MAXSTRINGLENGTH + 1];

	va_start(list, format);
	vsnprintf(output, MAXSTRINGLENGTH, format, list);
	va_end(list);

	if (graphics)
	{
		windowNewErrorDialog(window, _("Error"), output);
	}
	else
	{
	}
}


static void initMenuContents(void)
{
	strncpy(fileMenuContents.items[FILEMENU_OPEN].text, gettext(OPEN),
		WINDOW_MAX_LABEL_LENGTH);
	strncpy(fileMenuContents.items[FILEMENU_SAVE].text, gettext(SAVE),
		WINDOW_MAX_LABEL_LENGTH);
	strncpy(fileMenuContents.items[FILEMENU_QUIT].text, gettext(QUIT),
		WINDOW_MAX_LABEL_LENGTH);
}


static void processLine(unsigned filePos, screenLineInfo *line)
{
	// Given a file position and a screen line structure, scan the buffer and
	// fill in the length fields

	char *character = NULL;
	int charBytes = 0;

	memset(line, 0, sizeof(screenLineInfo));

	line->filePos = filePos;

	for (line->length = 0; ((line->filePos + line->length) < editFile.size);
		line->length ++)
	{
		character = (editFile.buffer + line->filePos + line->length);

		// Look out for tab characters
		if (*character == ASCII_TAB)
		{
			line->screenLength += TAB_CHARS(line->screenLength);
		}
		else
		{
			if (*character == ASCII_LF)
			{
				line->length += 1;
				break;
			}
			else
			{
				charBytes = mblen(character, (editFile.size - (line->filePos +
					line->length)));

				line->screenLength += 1;

				if (charBytes > 1)
					line->length += (charBytes - 1);
			}
		}
	}
}


static void printLine(screenLineInfo *line)
{
	// Given a screen line, print it on the screen at the current cursor
	// position

	int screenLength = 0;
	int screenPos = 0;
	char *character = NULL;
	int charBytes = 0;
	int count;

	processLine(line->filePos, line);

	textSetColumn(0);

	// This, until we have horizontal scrolling and things get more
	// complicated
	screenLength = min(line->screenLength, screen.columns);

	for (count = 0; screenPos < screenLength; count ++)
	{
		character = (editFile.buffer + line->filePos + count);

		// Look out for tab characters
		if (*character == ASCII_TAB)
		{
			textTab();

			screenPos += TAB_CHARS(screenPos);
		}
		else
		{
			charBytes = textPutMbc(character);

			screenPos += 1;

			if (charBytes > 1)
				count += (charBytes - 1);
		}
	}

	// Clear out the remainder of the line on screen
	while (screenPos++ < screen.columns)
		textPutc(' ');
}


static void showScreen(unsigned filePos)
{
	// Show the screen at the requested file position

	int row = 0, column = 0;
	screenLineInfo *line = NULL;

	row = textGetRow();
	column = textGetColumn();

	textSetRow(0);

	memset(screen.lines, 0, (screen.rows * sizeof(screenLineInfo)));

	// First screen line file position starts at filePos
	screen.lines[0].filePos = state.firstLineFilePos = filePos;

	for (screen.numLines = 0; screen.numLines < screen.rows; )
	{
		line = &screen.lines[screen.numLines];

		// In case this is the last line
		state.lastLineFilePos = line->filePos;

		printLine(line);

		// If we're doing more lines, record the next file position
		if (screen.numLines < (screen.rows - 1))
		{
			screen.lines[screen.numLines + 1].filePos = (line->filePos +
				line->length);
		}

		screen.numLines += 1;

		if (!editFile.size || (line->filePos >= (editFile.size - 1)))
			break;
	}

	textSetRow(row);
	textSetColumn(column);
}


static unsigned columnBufferOffset(screenLineInfo *line, int column,
	int *midTab)
{
	// Return the buffer offset of the on-screen column of the supplied line

	char *buffPtr = (editFile.buffer + line->filePos);
	char *endPtr = (buffPtr + line->length);
	int charBytes = 0;
	int count;

	if (column > line->screenLength)
		column = line->screenLength;

	*midTab = 0;

	// Find the buffer position that corresponds to the screen column
	for (count = 0; count < column; )
	{
		if (*buffPtr == ASCII_TAB)
		{
			count += TAB_CHARS(count);

			// If the requested column is inside a TAB character
			if (count > column)
			{
				*midTab = 1;
				break;
			}
		}
		else
		{
			count += 1;

			charBytes = mblen(buffPtr, (endPtr - buffPtr));
			if (charBytes > 1)
				buffPtr += (charBytes - 1);
		}

		buffPtr += 1;
	}

	return (buffPtr - editFile.buffer);
}


static void updateStatus(void)
{
	// Update the status line

	int column = textGetColumn();
	int row = textGetRow();
	char statusMessage[MAXSTRINGLENGTH + 1];
	textAttrs attrs;

	memset(&attrs, 0, sizeof(textAttrs));
	attrs.flags = TEXT_ATTRS_REVERSE;

	if (!strncmp(editFile.name, UNTITLED_FILENAME, MAX_PATH_NAME_LENGTH))
	{
		sprintf(statusMessage, "%s%s  %u/%u", UNTITLED_FILENAME,
			(editFile.modified? _(" (modified)") : ""), (state.fileLine + 1),
			editFile.numLines);
	}
	else
	{
		sprintf(statusMessage, "%s%s  %u/%u", editFile.strm.f.name,
			(editFile.modified? _(" (modified)") : ""), (state.fileLine + 1),
			editFile.numLines);
	}

	if (graphics)
	{
		windowComponentSetData(statusLabel, statusMessage,
			strlen(statusMessage), 1 /* redraw */);
	}
	else
	{
		// Extend to the end of the line
		while (textGetColumn() < (screen.columns - 1))
			strcat(statusMessage, " ");

		// Put the cursor on the last line
		textSetColumn(0);
		textSetRow(screen.rows - 1);

		textPrintAttrs(&attrs, statusMessage);

		// Put the cursor back where it belongs
		textSetColumn(column);
		textSetRow(row);
	}
}


static void setCursorPosition(int row, int column, int tabForward)
{
	// Set the on-screen cursor position.  'tabForward' means if the column is
	// in a TAB character, go forward to the column of the next character.

	screenLineInfo *line = NULL;
	char *buffPtr = NULL;
	int midTab = 0;

	if (row >= screen.numLines)
		row = (screen.numLines - 1);

	line = &screen.lines[row];

	if (column > line->screenLength)
		column = line->screenLength;

	// Until we have horizontal scrolling
	if (column >= screen.columns)
		column = (screen.columns - 1);

	buffPtr = (editFile.buffer + columnBufferOffset(line, column, &midTab));

	while (midTab)
	{
		column += (tabForward? 1 : -1);
		buffPtr = (editFile.buffer + columnBufferOffset(line, column,
			&midTab));
	}

	// (Again) until we have horizontal scrolling
	if (column >= screen.columns)
		column = (screen.columns - 1);

	textSetRow(row);
	state.fileLine += (row - state.screenLine);
	state.screenLine = row;

	textSetColumn(column);
	state.screenColumn = column;

	state.cursorFilePos = (buffPtr - editFile.buffer);

	updateStatus();
}


static void refreshWindow(void)
{
	// We got a 'window refresh' event (probably because of a language
	// switch), so we need to update things

	const char *charSet = NULL;

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("edit");

	// Re-get the character set
	charSet = getenv(ENV_CHARSET);

	if (charSet)
		windowSetCharSet(window, charSet);

	// Refresh all the menu contents
	initMenuContents();

	// Refresh the 'file' menu
	windowMenuUpdate(fileMenu, FILE_MENU, charSet, &fileMenuContents,
		NULL /* params */);

	// Refresh the window title
	windowSetTitle(window, WINDOW_TITLE);

	// Re-layout the window
	windowLayout(window);
}


static int askDiscardChanges(void)
{
	int response = 0;

	if (graphics)
	{
		response = windowNewChoiceDialog(window, _("Discard changes?"),
			DISCARDQUESTION, (char *[]){ _("Discard"), _("Cancel") },
			2 /* numChoices */, 1 /* defaultChoice */);

		if (!response)
			return (1);
		else
			return (0);
	}
	else
	{
		return (0);
	}
}


static void quit(void)
{
	if (!editFile.modified || askDiscardChanges())
		stop = 1;
}


static int askFileName(char *fileName)
{
	int status = 0;
	char pwd[MAX_PATH_NAME_LENGTH + 1];

	multitaskerGetCurrentDirectory(pwd, MAX_PATH_NAME_LENGTH);

	if (graphics)
	{
		// Prompt for a file name
		status = windowNewFileDialog(window, _("Enter filename"),
			FILENAMEQUESTION, pwd, fileName, MAX_PATH_NAME_LENGTH, fileT,
			0 /* no thumbnails */);
		return (status);
	}
	else
	{
		return (status = 0);
	}
}


static int doLoadFile(const char *fileName)
{
	// This is the 'inner' load file function, where a name must be supplied.
	// If the file doesn't exist, the function will try to create it.

	int status = 0;
	disk theDisk;
	file tmpFile;
	int openFlags = OPENMODE_READWRITE;

	memset(&tmpFile, 0, sizeof(file));

	// Find out whether the file is on a read-only filesystem
	if (!fileGetDisk(fileName, &theDisk))
		editFile.readOnly = theDisk.readOnly;

	// Call the "find file" function to see if we can get the file
	status = fileFind(fileName, &tmpFile);

	if (status >= 0)
	{
		if (editFile.readOnly)
			openFlags = OPENMODE_READ;
	}

	if ((status < 0) || !tmpFile.size)
	{
		// The file either doesn't exist or is zero-length

		// If the file doesn't exist, and the filesystem is read-only, quit
		// here
		if ((status < 0) && editFile.readOnly)
			return (status = ERR_NOWRITE);

		if (status < 0)
			// The file doesn't exist; try to create one
			openFlags |= OPENMODE_CREATE;

		status = fileStreamOpen(fileName, openFlags, &editFile.strm);
		if (status < 0)
			return (status);

		// Use a default initial buffer size of one file block
		editFile.bufferSize = editFile.strm.f.blockSize;
		editFile.buffer = malloc(editFile.bufferSize);
		if (!editFile.buffer)
			return (status = ERR_MEMORY);
	}
	else
	{
		// The file exists and has data in it

		status = fileStreamOpen(fileName, openFlags, &editFile.strm);
		if (status < 0)
			return (status);

		// Allocate a buffer to store the file contents in
		editFile.bufferSize = (editFile.strm.f.blocks *
			editFile.strm.f.blockSize);
		editFile.buffer = malloc(editFile.bufferSize);
		if (!editFile.buffer)
			return (status = ERR_MEMORY);

		status = fileStreamRead(&editFile.strm, editFile.strm.f.size,
			editFile.buffer);
		if (status < 0)
			return (status);
	}

	strncpy(editFile.name, fileName, MAX_PATH_NAME_LENGTH);
	return (status = 0);
}


static void countLines(void)
{
	// Sets the 'numLines' variable to the number of lines (of unlimited
	// length) in the file

	unsigned count;

	editFile.numLines = 0;
	for (count = 0; count < editFile.size; count ++)
	{
		if (editFile.buffer[count] == '\n')
			editFile.numLines += 1;
	}

	if (!editFile.numLines)
		editFile.numLines = 1;
}


static int loadFile(const char *fileName)
{
	// This is the 'outer' load file function, where a name need not be
	// supplied.  If not, the function will try to open a temporary 'untitled'
	// file, or if that fails, query the user for a name.

	int status = 0;
	disk rootDisk;

	if (editFile.buffer)
		free(editFile.buffer);

	memset(&editFile, 0, sizeof(editFile));

	// Did the user specify a file name?
	if (fileName)
	{
		// Yes.  Do the load.
		status = doLoadFile(fileName);
		if (status < 0)
			return (status);
	}
	else
	{
		// No.  Try to open a new temporary file to use as an 'untitled' file
		// that we will prompt for a file name later when it gets saved.

		if (!fileGetDisk("/", &rootDisk) && !rootDisk.readOnly &&
			(fileStreamGetTemp(&editFile.strm) >= 0))
		{
			// Use a default initial buffer size of one file block
			editFile.bufferSize = editFile.strm.f.blockSize;
			editFile.buffer = malloc(editFile.bufferSize);
			if (!editFile.buffer)
				return (status = ERR_MEMORY);

			strncpy(editFile.name, UNTITLED_FILENAME, MAX_PATH_NAME_LENGTH);

			// Try to remember the name of the temp file, so we can delete it
			// later if it doesn't get saved

			editFile.tempName = malloc(MAX_PATH_NAME_LENGTH + 1);
			if (!editFile.tempName)
				return (status = ERR_MEMORY);

			if (fileGetFullPath(&editFile.strm.f, editFile.tempName,
				MAX_PATH_NAME_LENGTH) < 0)
			{
				free(editFile.tempName);
				editFile.tempName = NULL;
			}
		}
		else
		{
			// Couldn't open a temporary file.  We might be running from a
			// read-only filesystem, for example.  Prompt for some file to
			// open, otherwise there's no point really.
			status = askFileName(editFile.name);
			if (status != 1)
			{
				if (status < 0)
					return (status);
				else
					return (status = ERR_CANCELLED);
			}

			// Do the load
			status = doLoadFile(editFile.name);
			if (status < 0)
				return (status);
		}
	}

	editFile.size = editFile.strm.f.size;
	memset(&state, 0, sizeof(state));
	countLines();
	textScreenClear();
	showScreen(0 /* File position */);
	setCursorPosition(0, 0, 0 /* TAB backwards */);

	if (graphics)
	{
		if (editFile.readOnly)
		{
			windowComponentSetEnabled(
				fileMenuContents.items[FILEMENU_SAVE].key, 0);
		}

		windowComponentFocus(textArea);
	}

	return (status = 0);
}


static void openFileThread(void)
{
	int status = 0;
	char *fileName = NULL;

	fileName = malloc(MAX_PATH_NAME_LENGTH + 1);
	if (!fileName)
		goto out;

	status = askFileName(fileName);
	if (status != 1)
	{
		if (status >= 0)
			status = ERR_CANCELLED;
		goto out;
	}

	status = loadFile(fileName);
	if (status < 0)
	{
		if (status == ERR_NOWRITE)
			error("%s", _("Couldn't create file in a read-only filesystem"));
		else if (status != ERR_CANCELLED)
			error(_("Error %d loading file"), status);
	}

out:
	if (fileName)
		free(fileName);

	multitaskerTerminate(status);
}


static int saveFile(void)
{
	int status = 0;
	fileStream tmpFileStream;

	if (!strncmp(editFile.name, UNTITLED_FILENAME, MAX_PATH_NAME_LENGTH))
	{
		if (graphics)
		{
			// Prompt for a file name
			status = askFileName(editFile.name);
			if (status != 1)
			{
				if (status < 0)
					return (status);
				else
					return (status = ERR_CANCELLED);
			}
		}

		// Open the file (truncate if necessary)
		status = fileStreamOpen(editFile.name, (OPENMODE_CREATE |
			OPENMODE_TRUNCATE | OPENMODE_READWRITE), &tmpFileStream);
		if (status < 0)
			return (status);

		// Close the temp file and swap the info
		fileStreamClose(&editFile.strm);
		if (editFile.tempName)
		{
			fileDelete(editFile.tempName);
			free(editFile.tempName);
			editFile.tempName = NULL;
		}

		memcpy(&editFile.strm, &tmpFileStream, sizeof(fileStream));
	}

	status = fileStreamSeek(&editFile.strm, 0);
	if (status < 0)
		return (status);

	status = fileStreamWrite(&editFile.strm, editFile.size, editFile.buffer);
	if (status < 0)
		return (status);

	status = fileStreamFlush(&editFile.strm);
	if (status < 0)
		return (status);

	editFile.modified = 0;
	updateStatus();

	if (graphics)
		windowComponentFocus(textArea);

	return (status = 0);
}


static void eventHandler(objectKey key, windowEvent *event)
{
	int status = 0;
	screenLineInfo *newScreenLines = NULL;

	// Check for window events
	if (key == window)
	{
		// Check for window refresh
		if (event->type == WINDOW_EVENT_WINDOW_REFRESH)
			refreshWindow();

		// Check for window resize
		else if (event->type == WINDOW_EVENT_WINDOW_RESIZE)
		{
			newScreenLines = realloc(screen.lines, (textGetNumRows() *
				sizeof(screenLineInfo)));

			if (newScreenLines)
			{
				screen.columns = textGetNumColumns();
				screen.rows = textGetNumRows();
				screen.lines = newScreenLines;
				textScreenClear();
				showScreen(state.firstLineFilePos);
			}
		}

		// Check for the window being closed
		else if (event->type == WINDOW_EVENT_WINDOW_CLOSE)
			quit();
	}

	// Look for file menu events

	else if (key == fileMenuContents.items[FILEMENU_OPEN].key)
	{
		if (event->type & WINDOW_EVENT_SELECTION)
		{
			if (!editFile.modified || askDiscardChanges())
			{
				if (multitaskerSpawn(&openFileThread, "open file",
					0 /* no args */, NULL /* no args */, 1 /* run */) < 0)
				{
					error("%s", _("Unable to launch file dialog"));
				}
			}
		}
	}

	else if (key == fileMenuContents.items[FILEMENU_SAVE].key)
	{
		if (event->type & WINDOW_EVENT_SELECTION)
		{
			status = saveFile();
			if (status < 0)
			{
				if (status == ERR_NOWRITE)
				{
					error("%s", _("Couldn't save file in a read-only "
						"filesystem"));
				}
				else if (status != ERR_CANCELLED)
				{
					error(_("Error %d saving file"), status);
				}
			}
		}
	}

	else if (key == fileMenuContents.items[FILEMENU_QUIT].key)
	{
		if (event->type & WINDOW_EVENT_SELECTION)
			quit();
	}

	// Look for cursor movements caused by clicking in the text area

	else if (key == textArea)
	{
	 	if (event->type & WINDOW_EVENT_CURSOR_MOVE)
		{
			// The user clicked to move the cursor.
			setCursorPosition(textGetRow(), textGetColumn(),
				0 /* TAB backwards */);
		}
	}
}


static void handleMenuEvents(windowMenuContents *contents)
{
	int count;

	for (count = 0; count < contents->numItems; count ++)
		windowRegisterEventHandler(contents->items[count].key, &eventHandler);
}


static void constructWindow(void)
{
	int rows = 25;
	componentParameters params;

	// Create a new window
	window = windowNew(processId, WINDOW_TITLE);

	memset(&params, 0, sizeof(componentParameters));

	// Create the top menu bar
	objectKey menuBar = windowNewMenuBar(window, &params);

	initMenuContents();

	// Create the top 'file' menu
	fileMenu = windowNewMenu(window, menuBar, FILE_MENU, &fileMenuContents,
		&params);
	handleMenuEvents(&fileMenuContents);

	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 1;
	params.padRight = 1;
	params.padTop = 1;
	params.orientationX = orient_center;
	params.orientationY = orient_middle;

	// Set up the font for our main text area
	font = fontGet(FONT_FAMILY_LIBMONO, FONT_STYLEFLAG_FIXED, 10, NULL);
	if (!font)
		// We'll be using the system font we guess.  The system font can
		// comfortably show more rows.
		rows = 40;

	// Put a text area in the window
	params.flags |= (COMP_PARAMS_FLAG_STICKYFOCUS |
		COMP_PARAMS_FLAG_CLICKABLECURSOR);
	params.font = font;
	textArea = windowNewTextArea(window, 80 /* columns */, rows,
		0 /* bufferLines */, &params);
	windowRegisterEventHandler(textArea, &eventHandler);
	windowComponentFocus(textArea);

	// Use the text area for all our input and output
	windowSetTextOutput(textArea);

	// Put a status label below the text area
	params.flags &= ~COMP_PARAMS_FLAG_STICKYFOCUS;
	params.gridY += 1;
	params.padBottom = 1;
	params.font = fontGet(FONT_FAMILY_ARIAL, FONT_STYLEFLAG_BOLD, 10, NULL);
	statusLabel = windowNewTextLabel(window, "", &params);
	windowComponentSetWidth(statusLabel, windowComponentGetWidth(textArea));

	// Go live
	windowSetVisible(window, 1);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);

	// Run the GUI as a thread
	windowGuiThread();
}


static unsigned previousLineStart(unsigned filePos)
{
	unsigned lineStart = 0;

	if (!filePos)
		return (lineStart = 0);

	lineStart = (filePos - 1);

	// Watch for the start of the buffer
	if (!lineStart)
		return (lineStart);

	// Lines that end with a newline (most)
	if (editFile.buffer[lineStart] == '\n')
		lineStart -= 1;

	// Watch for the start of the buffer
	if (!lineStart)
		return (lineStart);

	for ( ; editFile.buffer[lineStart] != '\n'; lineStart --)
	{
		// Watch for the start of the buffer
		if (!lineStart)
			return (lineStart);
	}

	lineStart += 1;

	return (lineStart);
}


static unsigned nextLineStart(unsigned filePos)
{
	unsigned lineStart = 0;

	if (filePos >= editFile.size)
		return (lineStart = (editFile.size - 1));

	lineStart = filePos;

	// Determine where the current line ends
	while (lineStart < (editFile.size - 1))
	{
		if (editFile.buffer[lineStart++] == '\n')
			break;
	}

	return (lineStart);
}


static unsigned lineNumStart(unsigned lineNum)
{
	unsigned lineStart = screen.lines[state.screenLine].filePos;
	int lines = 0;

	if (lineNum >= editFile.numLines)
		lineNum = (editFile.numLines - 1);

	lines = (lineNum - state.fileLine);

	while (lines)
	{
		if (lines > 0)
		{
			lineStart = nextLineStart(lineStart);
			lines -= 1;
		}
		else
		{
			lineStart = previousLineStart(lineStart);
			lines += 1;
		}
	}

	return (lineStart);
}


static void pageUp(void)
{
	int scrollLines = 0;
	unsigned cursorLineFilePos = 0;

	// Are we already on the first line?
	if (!state.fileLine)
		return;

	// How many lines to scroll?

	// Maximum possible
	scrollLines = (state.fileLine - state.screenLine);

	// Constrain to one screenfull
	scrollLines = min(scrollLines, screen.rows);

	if (scrollLines)
	{
		// Scroll the screen
		cursorLineFilePos = lineNumStart((state.fileLine - state.screenLine) -
			scrollLines);

		showScreen(cursorLineFilePos);

		state.fileLine -= scrollLines;

		setCursorPosition(state.screenLine, state.screenColumn,
			0 /* TAB backwards */);
	}

	if (scrollLines < screen.rows)
	{
		// Move the cursor to the first line
		setCursorPosition(0, state.screenColumn, 0 /* TAB backwards */);
	}
}


static void pageDown(void)
{
	int scrollLines = 0;
	unsigned cursorLineFilePos = 0;

	// Are we already on the last line?
	if (state.fileLine >= (editFile.numLines - 1))
		return;

	// How many lines to scroll?

	// Maximum possible
	scrollLines = ((editFile.numLines - state.fileLine) -
		(screen.numLines - state.screenLine));

	// Constrain to one screenfull
	scrollLines = min(scrollLines, screen.rows);

	if (scrollLines)
	{
		// Scroll the screen
		cursorLineFilePos = lineNumStart((state.fileLine - state.screenLine) +
			scrollLines);

		showScreen(cursorLineFilePos);

		state.fileLine += scrollLines;

		setCursorPosition(state.screenLine, state.screenColumn,
			0 /* TAB backwards */);
	}

	if (scrollLines < screen.rows)
	{
		// Move the cursor to the last line
		setCursorPosition((screen.numLines - 1), state.screenColumn,
			0 /* TAB backwards */);
	}
}


static void cursorUp(void)
{
	unsigned cursorLineFilePos = 0;

	if (!state.fileLine)
		return;

	cursorLineFilePos =
		previousLineStart(screen.lines[state.screenLine].filePos);

	// Do we need to scroll the screen up?
	if (cursorLineFilePos < state.firstLineFilePos)
	{
		showScreen(cursorLineFilePos);

		state.fileLine -= 1;

		setCursorPosition(state.screenLine, state.screenColumn,
			0 /* TAB backwards */);
	}
	else
	{
		setCursorPosition((state.screenLine - 1), state.screenColumn,
			0 /* TAB backwards */);
	}
}


static void cursorDown(void)
{
	unsigned cursorLineFilePos = 0;

	if (state.fileLine >= (editFile.numLines - 1))
		return;

	cursorLineFilePos = nextLineStart(screen.lines[state.screenLine].filePos);

	// Do we need to scroll the screen down?
	if (cursorLineFilePos > state.lastLineFilePos)
	{
		showScreen(screen.lines[1].filePos);

		state.fileLine += 1;

		setCursorPosition(state.screenLine, state.screenColumn,
			0 /* TAB backwards */);
	}
	else
	{
		setCursorPosition((state.screenLine + 1), state.screenColumn,
			0 /* TAB backwards */);
	}
}


static void cursorLeft(void)
{
	if (state.screenColumn)
	{
		setCursorPosition(state.screenLine, (state.screenColumn - 1),
			0 /* TAB backwards */);
	}
	else if (state.screenLine)
	{
		cursorUp();
		setCursorPosition(state.screenLine,
			screen.lines[state.screenLine].screenLength,
			0 /* TAB backwards */);
	}
}


static void cursorRight(void)
{
	if (state.screenColumn < screen.lines[state.screenLine].screenLength)
	{
		setCursorPosition(state.screenLine, (state.screenColumn + 1),
			1 /* TAB forwards */);
	}
	else if (state.screenLine < (screen.numLines - 1))
	{
		cursorDown();
		setCursorPosition(state.screenLine, 0, 0 /* TAB backwards */);
	}
}


static int expandBuffer(unsigned length)
{
	// Expand the buffer by at least 'length' characters

	int status = 0;
	unsigned tmpBufferSize = 0;
	char *tmpBuffer = NULL;

	// Allocate more buffer, rounded up to the nearest block size of the file
	tmpBufferSize = (editFile.bufferSize + (((length +
		(editFile.strm.f.blockSize - 1)) / editFile.strm.f.blockSize) *
		editFile.strm.f.blockSize));

	tmpBuffer = realloc(editFile.buffer, tmpBufferSize);
	if (!tmpBuffer)
		return (status = ERR_MEMORY);

	editFile.buffer = tmpBuffer;
	editFile.bufferSize = tmpBufferSize;

	return (status = 0);
}


static void shiftBuffer(unsigned filePos, int shiftBy)
{
	// Shift the contents of the buffer at 'filePos' by 'shiftBy' bytes
	// (positive or negative)

	unsigned shiftBytes = 0;

	shiftBytes = (editFile.size - filePos);

	if (shiftBy && shiftBytes)
	{
		memmove(((editFile.buffer + filePos) + shiftBy), (editFile.buffer +
			filePos), shiftBytes);
	}
}


static int insertChars(char *string, int length)
{
	// Insert characters at the current position

	int status = 0;
	screenLineInfo *line = &screen.lines[state.screenLine];
	int row = 0, column = 0;
	int count;

	// Do we need a bigger buffer?
	if ((editFile.size + length) >= editFile.bufferSize)
	{
		status = expandBuffer(length);
		if (status < 0)
			return (status);
	}

	row = textGetRow();
	column = textGetColumn();

	if (state.cursorFilePos < editFile.size)
	{
		// Shift data that occurs later in the buffer
		shiftBuffer(state.cursorFilePos, length);
	}

	editFile.size += length;
	editFile.modified = 1;

	// Copy the data
	memcpy((editFile.buffer + state.cursorFilePos), string, length);

	printLine(line);

	// We need to adjust the recorded file positions of all lines that follow
	// on the screen
	for (count = (state.screenLine + 1); count < screen.numLines; count ++)
		screen.lines[count].filePos += length;

	textSetRow(row);
	textSetColumn(column);

	return (status = 0);
}


static void deleteChars(int length)
{
	// Delete characters at the current position

	screenLineInfo *line = &screen.lines[state.screenLine];
	int row = 0, column = 0;
	int count;

	row = textGetRow();
	column = textGetColumn();

	if (state.cursorFilePos < (editFile.size - 1))
	{
		// Shift data that occurs later in the buffer
		shiftBuffer((state.cursorFilePos + length), -length);
	}

	editFile.size -= length;
	editFile.modified = 1;

	// Clear trailing data
	memset((editFile.buffer + (editFile.size - length)), 0, length);

	printLine(line);

	// We need to adjust the recorded file positions of all lines that follow
	// on the screen
	for (count = (state.screenLine + 1); count < screen.numLines; count ++)
		screen.lines[count].filePos -= length;

	textSetRow(row);
	textSetColumn(column);
}


static void backspace(void)
{
	int oldRow = state.screenLine;

	if (oldRow || state.screenColumn)
	{
		cursorLeft();
		deleteChars(mblen((editFile.buffer + state.cursorFilePos),
			(editFile.size - state.cursorFilePos)));

		// Were we at the beginning of a line?
		if (state.screenLine != oldRow)
		{
			editFile.numLines -= 1;
			showScreen(state.firstLineFilePos);
			updateStatus();
		}
	}
}


static void end(void)
{
	setCursorPosition(state.screenLine,
		screen.lines[state.screenLine].screenLength, 1 /* TAB forwards */);
}


static void home(void)
{
	setCursorPosition(state.screenLine, 0, 0 /* TAB backwards */);
}


static void delete(void)
{
	int endLine = (state.screenColumn >=
		(screen.lines[state.screenLine].screenLength - 1));

	deleteChars(mblen((editFile.buffer + state.cursorFilePos),
		(editFile.size - state.cursorFilePos)));

	// Were we at the end of a line?
	if (endLine)
	{
		editFile.numLines -= 1;
		showScreen(state.firstLineFilePos);
		updateStatus();
	}
}


static void enter(void)
{
	if (insertChars("\n", 1 /* length */) < 0)
		return;

	editFile.numLines += 1;

	if (editFile.numLines < (unsigned) screen.numLines)
		state.firstLineFilePos = previousLineStart(state.firstLineFilePos);

	showScreen(state.firstLineFilePos);

	cursorDown();

	setCursorPosition(state.screenLine, 0, 0 /* TAB backwards */);
}


static int edit(void)
{
	// This function is the base from which we do all the editing

	int status = 0;
	unsigned character = 0;
	char mbChars[MB_LEN_MAX + 1];
	int mbLen = 0;

	while (!stop)
	{
		if (!textInputCount())
		{
			multitaskerYield();
			continue;
		}

		textInputGetc(&character);

		switch (character)
		{
			case ASCII_PAGEUP:
			{
				// PAGE UP key
				pageUp();
				break;
			}

			case ASCII_PAGEDOWN:
			{
				// PAGE DOWN key
				pageDown();
				break;
			}

			case ASCII_CRSRUP:
			{
				// UP cursor key
				cursorUp();
				break;
			}

			case ASCII_CRSRDOWN:
			{
				// DOWN cursor key
				cursorDown();
				break;
			}

			case ASCII_CRSRLEFT:
			{
				// LEFT cursor key
				cursorLeft();
				break;
			}

			case ASCII_CRSRRIGHT:
			{
				// RIGHT cursor key
				cursorRight();
				break;
			}

			case ASCII_BACKSPACE:
			{
				// BACKSPACE key
				backspace();
				break;
			}

			case ASCII_END:
			{
				// END key
				end();
				break;
			}

			case ASCII_HOME:
			{
				// HOME key
				home();
				break;
			}

			case ASCII_DEL:
			{
				// DEL key
				delete();
				break;
			}

			case ASCII_ENTER:
			{
				// ENTER key
				enter();
				break;
			}

			default:
			{
				// Typing anything else
				mbLen = wctomb(mbChars, character);
				if (mbLen > 0)
				{
					status = insertChars(mbChars, mbLen);
					if (status < 0)
						break;
				}

				setCursorPosition(state.screenLine, (state.screenColumn + 1),
					(character == ASCII_TAB) /* TAB direction */);

				break;
			}
		}
	}

	status = fileStreamClose(&editFile.strm);

	if (editFile.tempName)
	{
		fileDelete(editFile.tempName);
		free(editFile.tempName);
		editFile.tempName = NULL;
	}

	return (status);
}


int main(int argc, char *argv[])
{
	int status = 0;
	char opt;
	char *fileName = NULL;
	textScreen saveScreen;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("edit");

	processId = multitaskerGetCurrentProcessId();

	// Are graphics enabled?
	graphics = graphicsAreEnabled();

	// For the moment, only operate in graphics mode
	if (!graphics)
	{
		fprintf(stderr, _("\nThe \"%s\" command only works in graphics "
			"mode\n"), (argc? argv[0] : ""));
		return (status = ERR_NOTINITIALIZED);
	}

	// Check options
	while (strchr("T?", (opt = getopt(argc, argv, "T"))))
	{
		switch (opt)
		{
			case 'T':
				// Force text mode
				graphics = 0;
				break;

			default:
				error(_("Unknown option '%c'"), optopt);
				return (status = ERR_INVALID);
		}
	}

	if (optind < argc)
		fileName = argv[optind];

	if (graphics)
		constructWindow();
	else
		// Save the current screen
		textScreenSave(&saveScreen);

	// Clear global data
	memset(&screen, 0, sizeof(screen));
	memset(&editFile, 0, sizeof(editFile));
	memset(&state, 0, sizeof(state));

	// Get screen parameters
	screen.columns = textGetNumColumns();
	screen.rows = textGetNumRows();
	if (!graphics)
		// Save one for the status line
		screen.rows -= 1;

	textEnableScroll(0);
	textInputSetEcho(0);

	screen.lines = malloc(screen.rows * sizeof(screenLineInfo));
	editFile.name = malloc(MAX_PATH_NAME_LENGTH + 1);
	if (!screen.lines || !editFile.name)
	{
		status = ERR_MEMORY;
		goto out;
	}

	status = loadFile(fileName);
	if (status < 0)
	{
		if (status == ERR_NOWRITE)
			error("%s", _("Couldn't create file in a read-only filesystem"));
		else if (status != ERR_CANCELLED)
			error(_("Error %d loading file"), status);
		goto out;
	}

	// Go
	status = edit();

out:
	textInputSetEcho(1);
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
		textScreenRestore(&saveScreen);

		if (saveScreen.data)
			memoryRelease(saveScreen.data);
	}

	if (editFile.buffer)
		free(editFile.buffer);
	if (editFile.name)
		free (editFile.name);
	if (screen.lines)
		free(screen.lines);

	// Return success
	return (status);
}

