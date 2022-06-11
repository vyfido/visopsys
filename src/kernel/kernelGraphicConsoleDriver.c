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
//  kernelGraphicConsoleDriver.c
//

// This is the graphic console screen driver.  Manipulates character images
// using the kernelGraphic functions.

#include "kernelError.h"
#include "kernelFont.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelWindow.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


static inline void updateComponent(kernelTextArea *area)
{
	kernelWindowComponent *component = (kernelWindowComponent *)
		area->windowComponent;

	if (component && component->update)
		component->update(area->windowComponent);
}


static int buffer2Char(kernelTextArea *area, char *dest,
	const unsigned char * volatile src, int index, int maxChars)
{
	// Pack padded multibyte characters from the buffer into a char array

	int charLen = 0;
	int destLen = 0;
	int count1, count2;

	src += (index * area->bytesPerChar);

	for (count1 = 0; count1 < maxChars; count1 ++)
	{
		charLen = mblen((char *) src, MB_LEN_MAX);
		if (charLen >= 0)
		{
			if (!charLen)
				break;

			for (count2 = 0; count2 < charLen; count2 ++)
				dest[count2] = src[count2];

			dest += charLen;
			destLen += charLen;
		}

		src += area->bytesPerChar;
	}

	*dest = '\0';
	return (destLen);
}


static int char2Buffer(kernelTextArea *area, unsigned char * volatile dest,
	int index, const char *src, int maxChars)
{
	// Unpack multibyte characters from a char array into the buffer

	int charLen = 0;
	int srcLen = 0;
	int count1, count2;

	dest += (index * area->bytesPerChar);

	for (count1 = 0; count1 < maxChars; count1 ++)
	{
		charLen = mblen((char *) src, MB_LEN_MAX);
		if (charLen >= 0)
		{
			if (!charLen)
				break;

			for (count2 = 0; count2 < charLen; count2 ++)
				dest[count2] = src[count2];

			dest += area->bytesPerChar;
		}

		src += charLen;
		srcLen += charLen;
	}

	return (srcLen);
}


static void scrollBuffer(kernelTextArea *area, int lines)
{
	// Scrolls back everything in the area's buffer

	int dataLength = (lines * area->columns * area->bytesPerChar);

	// Increasing the stored scrollback lines?
	if ((area->rows + area->scrollBackLines) < area->maxBufferLines)
	{
		area->scrollBackLines += min(lines, (area->maxBufferLines -
			(area->rows + area->scrollBackLines)));

		updateComponent(area);
	}

	memcpy(TEXTAREA_FIRSTSCROLLBACK(area),
		(TEXTAREA_FIRSTSCROLLBACK(area) + dataLength),
		((area->rows + area->scrollBackLines) * (area->columns *
		area->bytesPerChar)));
}


static void setCursor(kernelTextArea *area, int onOff)
{
	// Draws or erases the cursor at the current position

	graphicBuffer *buffer = ((kernelWindowComponent *)
		area->windowComponent)->buffer;
	char string[MB_LEN_MAX + 1];

	buffer2Char(area, string, area->visibleData, TEXTAREA_CURSORPOS(area), 1);

	if (onOff)
	{
		kernelGraphicDrawRect(buffer, (color *) &area->foreground,
			draw_normal,
			(area->xCoord + (area->cursorColumn * area->font->glyphWidth)),
			(area->yCoord + (area->cursorRow * area->font->glyphHeight)),
			area->font->glyphWidth, area->font->glyphHeight, 1, 1);
		kernelGraphicDrawText(buffer, (color *) &area->background,
			(color *) &area->foreground, area->font, area->charSet, string,
			draw_normal,
			(area->xCoord + (area->cursorColumn * area->font->glyphWidth)),
			(area->yCoord + (area->cursorRow * area->font->glyphHeight)));
	}
	else
	{
		// Clear out the position and redraw the character
		kernelGraphicClearArea(buffer, (color *) &area->background,
			(area->xCoord + (area->cursorColumn * area->font->glyphWidth)),
			(area->yCoord + (area->cursorRow * area->font->glyphHeight)),
			area->font->glyphWidth, area->font->glyphHeight);
		kernelGraphicDrawText(buffer, (color *) &area->foreground,
			(color *) &area->background, area->font, area->charSet, string,
			draw_normal,
			(area->xCoord + (area->cursorColumn * area->font->glyphWidth)),
			(area->yCoord + (area->cursorRow * area->font->glyphHeight)));
	}

	// Tell the window manager to update the graphic buffer
	kernelWindowUpdateBuffer(buffer,
		(area->xCoord + (area->cursorColumn * area->font->glyphWidth)),
		(area->yCoord + (area->cursorRow * area->font->glyphHeight)),
		area->font->glyphWidth, area->font->glyphHeight);

	area->cursorState = onOff;

	return;
}


static int scrollLine(kernelTextArea *area)
{
	// Scrolls the text by 1 line in the text area provided

	kernelWindowComponent *component = area->windowComponent;
	kernelWindowTextArea *windowTextArea = component->data;
	graphicBuffer *buffer = component->buffer;
	int maxWidth = 0;

	if (windowTextArea)
		maxWidth = windowTextArea->areaWidth;
	else if (component->width)
		maxWidth = component->width;
	else
		maxWidth = buffer->width;

	if (buffer->height > area->font->glyphHeight)
	{
		// Copy everything up by one line
		kernelGraphicCopyArea(buffer, area->xCoord, (area->yCoord +
			area->font->glyphHeight), maxWidth, ((area->rows - 1) *
			area->font->glyphHeight), area->xCoord, area->yCoord);
	}

	// Erase the last line
	kernelGraphicClearArea(buffer, (color *) &area->background, area->xCoord,
		(area->yCoord + ((area->rows - 1) * area->font->glyphHeight)),
		maxWidth, area->font->glyphHeight);

	// Tell the window manager to update the whole graphic buffer
	kernelWindowUpdateBuffer(buffer, area->xCoord, area->yCoord, maxWidth,
		(area->rows * area->font->glyphHeight));

	// Move the buffer up by one
	scrollBuffer(area, 1);

	// Clear out the bottom row
	memset(TEXTAREA_LASTVISIBLE(area), 0, (area->columns *
		area->bytesPerChar));

	// Copy our buffer data to the visible area
	memcpy(area->visibleData, TEXTAREA_FIRSTVISIBLE(area),
		(area->rows * area->columns * area->bytesPerChar));

	// The cursor position is now 1 row up from where it was
	area->cursorRow -= 1;

	return (0);
}


static int getCursorAddress(kernelTextArea *area)
{
	// Returns the cursor address as an integer
	return ((area->cursorRow * area->columns) + area->cursorColumn);
}


static int screenDraw(kernelTextArea *area)
{
	// Draws the text area as currently specified

	graphicBuffer *buffer = ((kernelWindowComponent *)
		area->windowComponent)->buffer;
	char *lineBuffer = NULL;
	int rowIndex = 0;
	int count;

	lineBuffer = kernelMalloc((area->columns * area->bytesPerChar) + 1);
	if (!lineBuffer)
		return (ERR_MEMORY);

	// Clear the area
	kernelGraphicClearArea(buffer, (color *) &area->background, area->xCoord,
		area->yCoord, (area->columns * area->font->glyphWidth),
		(area->rows * area->font->glyphHeight));

	// Copy from the buffer to the visible area, minus any scrollback lines
	rowIndex = -(area->scrolledBackLines * area->columns);
	for (count = 0; count < area->rows; count ++)
	{
		buffer2Char(area, lineBuffer, TEXTAREA_FIRSTVISIBLE(area), rowIndex,
			area->columns);

		kernelGraphicDrawText(buffer, (color *) &area->foreground,
			(color *) &area->background, area->font, area->charSet,
			lineBuffer, draw_normal, area->xCoord, (area->yCoord + (count *
				area->font->glyphHeight)));

		rowIndex += area->columns;
	}

	kernelFree(lineBuffer);

	// Tell the window manager to update the whole area buffer
	kernelWindowUpdateBuffer(buffer, area->xCoord, area->yCoord,
		(area->columns * area->font->glyphWidth),
		(area->rows * area->font->glyphHeight));

	// If we aren't scrolled back, show the cursor again
	if (area->cursorState && !(area->scrolledBackLines))
		setCursor(area, 1);

	return (0);
}


static int setCursorAddress(kernelTextArea *area, int row, int col)
{
	// Moves the cursor

	int cursorState = area->cursorState;
	char *line = NULL;
	int count;

	// If we are currently scrolled back, this puts us back to normal
	if (area->scrolledBackLines)
	{
		area->scrolledBackLines = 0;
		screenDraw(area);

		updateComponent(area);
	}

	if (cursorState)
		setCursor(area, 0);

	area->cursorRow = row;
	area->cursorColumn = col;

	if (col)
	{
		// If any of the preceding spots have NULLS in them, fill those with
		// spaces instead
		line = ((char *) TEXTAREA_FIRSTVISIBLE(area) +
			(TEXTAREA_CURSORPOS(area) * area->bytesPerChar));
		for (count = ((col - 1) * area->bytesPerChar); count >= 0;
			count -= area->bytesPerChar)
		{
			if (!line[count])
				line[count] = L' ';
		}
	}

	if (cursorState)
		setCursor(area, 1);

	return (0);
}


static int print(kernelTextArea *area, const char *input, textAttrs *attrs)
{
	// Prints input to the text area

	int status = 0;
	graphicBuffer *buffer = ((kernelWindowComponent *)
		area->windowComponent)->buffer;
	char *lineBuffer = NULL;
	int cursorState = area->cursorState;
	color *foreground = (color *) &area->foreground;
	color *background = (color *) &area->background;
	int inputLen = 0;
	int inputCount = 0;
	int bufferCount = 0;
	int charLen = 0;
	int newLine = 0;
	int printed = 0;
	int count;

	lineBuffer = kernelMalloc((area->columns * area->bytesPerChar) + 1);
	if (!lineBuffer)
		return (status = ERR_MEMORY);

	// See whether we're printing with special attributes
	if (attrs)
	{
		if (attrs->flags & TEXT_ATTRS_FOREGROUND)
			foreground = &attrs->foreground;
		if (attrs->flags & TEXT_ATTRS_BACKGROUND)
			background = &attrs->background;
		if (attrs->flags & TEXT_ATTRS_REVERSE)
		{
			color *tmpColor = foreground;
			foreground = background;
			background = tmpColor;
		}
	}

	// If we are currently scrolled back, this puts us back to normal
	if (area->scrolledBackLines)
	{
		area->scrolledBackLines = 0;
		screenDraw(area);

		updateComponent(area);
	}

	if (cursorState)
		// Turn off the cursor
		setCursor(area, 0);

	// How long is the input string?
	inputLen = strlen(input);

	// Loop through the input string, adding characters to our line buffer.
	// If we reach the end of a line or encounter a newline character, do
	// a newline.
	for (inputCount = 0; inputCount < inputLen; )
	{
		newLine = 0;

		if ((unsigned char) input[inputCount] < CHARSET_IDENT_CODES)
		{
			// Skip the range of unprintable characters in the ASCII set
			if (!isprint((int) input[inputCount]))
			{
				inputCount += 1;
				continue;
			}

			if (input[inputCount] == '\t')
			{
				charLen = ((TEXT_DEFAULT_TAB - ((area->cursorColumn +
					printed) % TEXT_DEFAULT_TAB)) - 1);

				for (count = 0; count < charLen; count ++)
					lineBuffer[bufferCount++] = ' ';

				inputCount += 1;
				printed += charLen;
			}
			else
			{
				if (input[inputCount] == '\n')
					newLine = 1;

				charLen = 1;
				lineBuffer[bufferCount++] = input[inputCount++];
				printed += 1;
			}
		}
		else
		{
			// How many bytes is the next character?
			charLen = mblen((input + inputCount), (inputLen - inputCount));
			if (charLen < 1)
			{
				// Skip this
				inputCount += 1;
				continue;
			}

			// Add this character to the lineBuffer
			for (count = 0; count < charLen; count ++)
				lineBuffer[bufferCount++] = input[inputCount++];

			printed += 1;
		}

		// Is this the completion of the line?
		if (newLine || (inputCount >= (inputLen - 1)) ||
			((area->cursorColumn + printed) >= area->columns))
		{
			lineBuffer[bufferCount] = '\0';

			// Add it to our buffers
			char2Buffer(area, TEXTAREA_FIRSTVISIBLE(area),
				TEXTAREA_CURSORPOS(area), lineBuffer, printed);

			if (area->hidden)
			{
				for (count = 0; count < printed; count ++)
					lineBuffer[count] = '*';
				lineBuffer[printed] = '\0';
				bufferCount = printed;
			}

			char2Buffer(area, area->visibleData, TEXTAREA_CURSORPOS(area),
				lineBuffer, printed);

			// Draw it
			kernelGraphicDrawText(buffer, foreground, background, area->font,
				area->charSet, lineBuffer, draw_normal,
				(area->xCoord + (area->cursorColumn *
					area->font->glyphWidth)),
				(area->yCoord + (area->cursorRow * area->font->glyphHeight)));

			kernelWindowUpdateBuffer(buffer, (area->xCoord +
					(area->cursorColumn * area->font->glyphWidth)),
				(area->yCoord + (area->cursorRow * area->font->glyphHeight)),
				(printed * area->font->glyphWidth), area->font->glyphHeight);

			if (newLine || ((area->cursorColumn + printed) >= area->columns))
			{
				// Will this cause a scroll?
				if (area->cursorRow >= (area->rows - 1))
				{
					if (!area->noScroll)
					{
						scrollLine(area);
						area->cursorRow += 1;
					}
				}
				else
				{
					area->cursorRow += 1;
				}

				area->cursorColumn = 0;
			}
			else
			{
				area->cursorColumn += printed;
			}

			bufferCount = 0;
			printed = 0;
		}
	}

	kernelFree(lineBuffer);

	if (cursorState)
		// Turn on the cursor
		setCursor(area, 1);

	return (status = 0);
}


static int delete(kernelTextArea *area)
{
	// Erase the character at the current position

	graphicBuffer *buffer = ((kernelWindowComponent *)
		area->windowComponent)->buffer;
	int cursorState = area->cursorState;
	int position = TEXTAREA_CURSORPOS(area);

	// If we are currently scrolled back, this puts us back to normal
	if (area->scrolledBackLines)
	{
		area->scrolledBackLines = 0;
		screenDraw(area);

		updateComponent(area);
	}

	if (cursorState)
		// Turn off the cursor
		setCursor(area, 0);

	// Delete the character in our buffers
	*(TEXTAREA_FIRSTVISIBLE(area) + (position * area->bytesPerChar)) = L'\0';
	*(area->visibleData + (position * area->bytesPerChar)) = L'\0';

	kernelWindowUpdateBuffer(buffer, (area->xCoord + (area->cursorColumn *
		area->font->glyphWidth)), (area->yCoord + (area->cursorRow *
		area->font->glyphHeight)), area->font->glyphWidth,
		area->font->glyphHeight);

	if (cursorState)
		// Turn on the cursor
		setCursor(area, 1);

	return (0);
}


static int screenClear(kernelTextArea *area)
{
	// Clears the text area

	graphicBuffer *buffer = ((kernelWindowComponent *)
		area->windowComponent)->buffer;

	// Clear the area
	kernelGraphicClearArea(buffer, (color *) &area->background,
		area->xCoord, area->yCoord, (area->columns * area->font->glyphWidth),
		(area->rows * area->font->glyphHeight));

	// Tell the window manager to update the whole area buffer
	kernelWindowUpdateBuffer(buffer, area->xCoord, area->yCoord,
		(area->columns * area->font->glyphWidth),
		(area->rows * area->font->glyphHeight));

	// Empty all the data
	memset(TEXTAREA_FIRSTVISIBLE(area), 0, (area->columns * area->rows *
		area->bytesPerChar));

	// Copy to the visible area
	memcpy(area->visibleData, TEXTAREA_FIRSTVISIBLE(area),
		(area->rows * area->columns * area->bytesPerChar));

	// Cursor to the top right
	area->cursorColumn = 0;
	area->cursorRow = 0;

	if (area->cursorState)
		// Turn on the cursor
		setCursor(area, 1);

	updateComponent(area);

	return (0);
}


static int screenSave(kernelTextArea *area, textScreen *screen)
{
	// This function saves the current contents of the screen

	// Get memory for a new save area
	screen->data = kernelMemoryGet((area->columns * area->rows *
		area->bytesPerChar), "text screen data");
	if (!screen->data)
		return (ERR_MEMORY);

	memcpy(screen->data, TEXTAREA_FIRSTVISIBLE(area), (area->rows *
		area->columns * area->bytesPerChar));

	screen->column = area->cursorColumn;
	screen->row = area->cursorRow;

	return (0);
}


static int screenRestore(kernelTextArea *area, textScreen *screen)
{
	// This function restores the saved contents of the screen

	if (screen->data)
	{
		memcpy(TEXTAREA_FIRSTVISIBLE(area), screen->data, (area->rows *
			area->columns * area->bytesPerChar));

		// Copy to the visible area
		memcpy(area->visibleData, screen->data, (area->rows * area->columns *
			area->bytesPerChar));
	}

	area->cursorColumn = screen->column;
	area->cursorRow = screen->row;

	screenDraw(area);

	updateComponent(area);

	return (0);
}


static kernelTextOutputDriver graphicModeDriver = {
	setCursor,
	getCursorAddress,
	setCursorAddress,
	NULL,	// setForeground
	NULL,	// setBackground
	print,
	delete,
	screenDraw,
	screenClear,
	screenSave,
	screenRestore
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelGraphicConsoleInitialize(void)
{
	// Called before the first use of the text console

	// Register our driver
	return (kernelSoftwareDriverRegister(graphicConsoleDriver,
		&graphicModeDriver));
}

