//
//  Visopsys
//  Copyright (C) 1998-2015 J. Andrew McLaughlin
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
//  vshCursorMenu.c
//

// This contains some useful functions written for the shell

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/ascii.h>
#include <sys/vsh.h>


_X_ int vshCursorMenu(const char *prompt, char *items[], int numItems,
	int defaultSelection)
{
	// Desc: This will create a pretty cursor-changeable text menu with the supplied 'prompt' string at the stop.  Returns the integer (zero-based) selected item number, or else negative on error or no selection.

	int itemWidth = 0;
	char *buffer = NULL;
	int selected = defaultSelection;
	textAttrs attrs;
	char c = '\0';
	int count1, count2;

	// Check params
	if ((prompt == NULL) || (items == NULL))
		return (errno = ERR_NULLPARAMETER);

	memset(&attrs, 0, sizeof(textAttrs));

	// Get the width of the widest item and set our item width
	for (count1 = 0; count1 < numItems; count1 ++)
	{
		if ((int) strlen(items[count1]) > itemWidth)
			itemWidth = strlen(items[count1]);
	}

	itemWidth = min(itemWidth, textGetNumColumns());

	buffer = malloc(itemWidth + 1);
	if (buffer == NULL)
		return (errno = ERR_MEMORY);

	// Print prompt message
	printf("\n%s\n", prompt);

	// Now, print 'numItems' newlines before calculating the current row so
	// that we don't get messed up if the screen scrolls
	for (count1 = 0; count1 < (numItems + 3); count1 ++)
		printf("\n");

	int row = (textGetRow() - (numItems + 3));

	while (1)
	{
		textSetColumn(0);
		textSetRow(row);
		textSetCursor(0);

		for (count1 = 0; count1 < numItems; count1 ++)
		{
			printf(" ");

			sprintf(buffer, " %s ", items[count1]);
			for (count2 = 0;
				 count2 < (itemWidth - (int) strlen(items[count1])); count2 ++)
			{
				strcat(buffer, " ");
			}

			if (selected == count1)
				attrs.flags = TEXT_ATTRS_REVERSE;
			else
				attrs.flags = 0;

			textPrintAttrs(&attrs, buffer);
			printf("\n");
		}

		printf("\n  [Cursor up/down to change, Enter to select, 'Q' to quit]\n");
		textInputSetEcho(0);
		c = getchar();
		textInputSetEcho(1);

		switch (c)
		{
			case (char) ASCII_CRSRUP:
				// Cursor up.
				if (selected > 0)
					selected -= 1;
				break;

			case (char) ASCII_CRSRDOWN:
				// Cursor down.
				if (selected < (numItems - 1))
					selected += 1;
				break;

			case (char) ASCII_ENTER:
				// Enter
				textSetCursor(1);
				free(buffer);
				return (selected);

			case 'Q':
			case 'q':
				// Cancel
				textSetCursor(1);
				free(buffer);
				return (errno = ERR_CANCELLED);
		}
	}
}

