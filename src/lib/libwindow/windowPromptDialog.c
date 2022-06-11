//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  windowPromptDialog.c
//

// This contains functions for user programs to operate GUI components.

#include <libintl.h>
#include <string.h>
#include <sys/api.h>
#include <sys/ascii.h>
#include <sys/errors.h>
#include <sys/window.h>

#define _(string) gettext(string)

typedef enum {
	promptDialog, passwordDialog
} dialogType;


extern int libwindow_initialized;
extern void libwindowInitialize(void);


static int dialog(dialogType type, objectKey parentWindow, const char *title,
	const char *message, int rows, int columns, char *buffer)
{
	// This will make a simple dialog with either a text field, password field,
	// or text area depending on the requested type and the number of rows
	// requested

	int status = 0;
	objectKey dialogWindow = NULL;
	componentParameters params;
	objectKey field = NULL;
	objectKey okButton = NULL;
	objectKey cancelButton = NULL;
	windowEvent event;

	if (!libwindow_initialized)
		libwindowInitialize();

	// Check params.  It's okay for parentWindow to be NULL.
	if ((title == NULL) || (message == NULL) || (buffer == NULL))
		return (status = ERR_NULLPARAMETER);

	buffer[0] = '\0';

	// Create the dialog.
	if (parentWindow)
		dialogWindow = windowNewDialog(parentWindow, title);
	else
		dialogWindow = windowNew(multitaskerGetCurrentProcessId(), title);
	if (dialogWindow == NULL)
		return (status = ERR_NOCREATE);

	bzero(&params, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 5;
	params.padRight = 5;
	params.padTop = 5;
	params.orientationX = orient_center;
	params.orientationY = orient_top;

	params.gridWidth = 2;
	windowNewTextLabel(dialogWindow, message, &params);

	params.gridY = 1;
	params.padTop = 5;
	if (type == passwordDialog)
		field = windowNewPasswordField(dialogWindow, columns, &params);
	else
	{
		if (rows <= 1)
			field = windowNewTextField(dialogWindow, columns, &params);
		else
			field = windowNewTextArea(dialogWindow, columns, rows, 0, &params);
	}
	windowComponentFocus(field);

	// Create the OK button
	params.gridY = 2;
	params.gridWidth = 1;
	params.padBottom = 5;
	params.orientationX = orient_right;
	params.flags = WINDOW_COMPFLAG_FIXEDWIDTH;
	okButton = windowNewButton(dialogWindow, _("OK"), NULL, &params);

	// Create the Cancel button
	params.gridX = 1;
	params.padLeft = 0;
	params.padRight = 5;
	params.orientationX = orient_left;
	cancelButton = windowNewButton(dialogWindow, _("Cancel"), NULL, &params);

	if (parentWindow)
		windowCenterDialog(parentWindow, dialogWindow);
	windowSetVisible(dialogWindow, 1);

	while (1)
	{
		// Check for the OK button
		status = windowComponentEventGet(okButton, &event);
		if (status < 0)
			break;
		else if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
		{
			status = windowComponentGetData(field, buffer, (rows * columns));
			if (status < 0)
				break;
			status = strlen(buffer);
				break;
		}

		// Check for the Cancel button
		status = windowComponentEventGet(cancelButton, &event);
		if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP)))
		{
			status = 0;
			break;
		}

		// Check for window close events
		status = windowComponentEventGet(dialogWindow, &event);
		if ((status < 0) || ((status > 0) && (event.type == EVENT_WINDOW_CLOSE)))
		{
			status = 0;
			break;
		}

		// Check for keyboard events
		status = windowComponentEventGet(field, &event);
		if (status < 0)
			break;
		else if ((event.type == EVENT_KEY_DOWN) &&
			(event.key == (unsigned char) ASCII_ENTER))
		{
			status = windowComponentGetData(field, buffer, (rows * columns));
			if (status < 0)
				break;
			status = strlen(buffer);
				break;
		}

		// Done
		multitaskerYield();
	}

	windowDestroy(dialogWindow);
	return (status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ int windowNewPromptDialog(objectKey parentWindow, const char *title, const char *message, int rows, int columns, char *buffer)
{
	// Desc: Create a 'prompt' dialog box, with the parent window 'parentWindow', and the given titlebar text and main message.  The dialog will have a single text field for the user to enter data.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').
	return (dialog(promptDialog, parentWindow, title, message, rows, columns,
		buffer));
}


_X_ int windowNewPasswordDialog(objectKey parentWindow, const char *title, const char *message, int columns, char *buffer)
{
	// Desc: Create a 'password' dialog box, with the parent window 'parentWindow', and the given titlebar text and main message.  The dialog will have a single password field.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').
	return (dialog(passwordDialog, parentWindow, title, message, 1, columns,
		buffer));
}

