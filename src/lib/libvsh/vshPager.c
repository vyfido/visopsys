//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  vshPager.c
//

// This contains some useful functions written for the shell

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>
#include <sys/ascii.h>
#include <sys/text.h>
#include <sys/vsh.h>


_X_ int vshPageBuffer(const char *buffer, unsigned len, const char *prompt)
{
	// Desc: Print the contents of the buffer to standard output, one screenfull at a time.  'prompt' is shown when pausing at the end of a screenfull; if NULL, the default "--More--(%d%%)" is used.  To page forward to the next screenfull, press the [SPACE] key.  To quit, press the [Q] key.  To advance by a single line, press any other key.

	int status = 0;
	int screenColumns = 0;
	int screenRows = 0;
	int charEntered = 0;
	int charsSoFar = 0;
	int cursorPos1, cursorPos2;
	textAttrs attrs;
	char more[32];
	unsigned count1;
	int count2;

	// Check params
	if (!buffer || !len)
		return (status = ERR_NULLPARAMETER);

	if (!prompt)
		prompt = "--More--(%d%%)";

	// Initialize stack data
	memset(&attrs, 0, sizeof(textAttrs));
	attrs.flags = TEXT_ATTRS_REVERSE;

	// Get screen parameters
	screenColumns = textGetNumColumns();
	screenRows = textGetNumRows();

	// Print the buffer, one screen at a time
	for (count1 = 0; count1 < len; count1 ++)
	{
		// Are we at the end of a screenful of data?
		if (charsSoFar >= (screenColumns * (screenRows - 1)))
		{
			// Print the prompt
			snprintf(more, 32, prompt, ((count1 * 100) / len));
			textPrintAttrs(&attrs, more);

			// Wait for user input
			textInputSetEcho(0);
			charEntered = getchar();
			textInputSetEcho(1);

			// Erase the prompt
			cursorPos1 = textGetColumn();
			for (count2 = 0; count2 < cursorPos1; count2++)
				textBackSpace();

			// Did the user want to quit or anything?
			if (charEntered == (int) 'q')
				break;

			// Another screenful?
			else if (charEntered == (int) ' ')
				charsSoFar = 0;

			// Another lineful
			else
				charsSoFar -= screenColumns;

			// Continue, fall through
		}

		// Look out for tab characters
		if (buffer[count1] == ASCII_TAB)
		{
			// We need to keep track of how many whitespace characters get
			// 'printed'
			cursorPos1 = textGetColumn();

			textTab();

			cursorPos2 = textGetColumn();

			// Did we wrap to the next line?
			if (cursorPos2 >= cursorPos1)
			{
				// No wrap
				charsSoFar += (cursorPos2 - cursorPos1);
			}
			else
			{
				// We wrapped
				charsSoFar += (screenColumns - (cursorPos1 + 1)) +
					(cursorPos2 + 1);
			}
		}

		// Look out for newline characters
		else if (buffer[count1] == ASCII_LF)
		{
			// We need to keep track of how many whitespace characters get
			// 'printed'
			cursorPos1 = textGetColumn();

			textNewline();

			charsSoFar += (screenColumns - cursorPos1);
		}

		else
		{
			textPutc(buffer[count1]);
			charsSoFar += 1;
		}
	}

	return (status = 0);
}


_X_ int vshPageFile(const char *fileName, const char *prompt)
{
	// Desc: Print the contents of the file specified by 'fileName', which must be an absolute pathname, beginning with '/', to standard output, one screenfull at a time.  See vshPageBuffer().

	int status = 0;
	file theFile;
	char *fileBuffer = NULL;

	// Check params
	if (!fileName)
		return (status = ERR_NULLPARAMETER);

	// Initialize stack data
	memset(&theFile, 0, sizeof(file));

	// Call the "find file" function to see if we can get the file
	status = fileFind(fileName, &theFile);
	if (status < 0)
		return (status);

	// Make sure the file isn't empty.  We don't want to try reading
	// data from a nonexistent place on the disk.
	if (!theFile.size)
		// It is empty, so skip it
		return (status = 0);

	// The file exists and is non-empty.  That's all we care about (we
	// don't care at this point, for example, whether it's a file or a
	// directory.  Read it into memory and print it on the screen.

	// Allocate a buffer to store the file contents in
	fileBuffer = malloc((theFile.blocks * theFile.blockSize) + 1);
	if (!fileBuffer)
		return (status = ERR_MEMORY);

	status = fileOpen(fileName, OPENMODE_READ, &theFile);
	if (status < 0)
	{
		free(fileBuffer);
		return (status);
	}

	status = fileRead(&theFile, 0, theFile.blocks, fileBuffer);
	if (status < 0)
	{
		free(fileBuffer);
		return (status);
	}

	status = vshPageBuffer(fileBuffer, theFile.size, prompt);

	// Free the memory
	free(fileBuffer);

	return (status);
}

