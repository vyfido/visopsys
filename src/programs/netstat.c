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
//  netstat.c
//

// This command prints information about all current network connections

/* This is the text that appears when a user requests help about this program
<help>

 -- netstat --

Prints information about all current network connections.

Usage:
  netstat [-T]

This command will show information about the system's network connections.

In graphics mode, the program is interactive and the user can view network
connection status and perform tasks visually.

Options:
-T  : Force text mode operation

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/font.h>
#include <sys/network.h>

#define _(string) gettext(string)

#define WINDOW_TITLE			_("Network Connections")
#define DEVICE_STRING			_("Device")
#define PROCESS_STRING			_("Process")
#define ADDRESS_STRING			_("Address")
#define MODE_STRING				_("Mode")
#define STATE_STRING			_("State")

#define COL_PROC	11
#define COL_ADDR	22
#define COL_MODE	44
#define COL_STATE	56

static const char *tcpStateNames[11] = {
	"tcp_closed", "tcp_listen", "tcp_syn_sent", "tcp_syn_received",
	"tcp_established", "tcp_close_wait", "tcp_last_ack", "tcp_fin_wait1",
	"tcp_closing", "tcp_fin_wait2", "tcp_time_wait"
};

static int graphics = 0;
static int numConnections = 0;
static networkConnection *connection = NULL;
static listItemParameters *connectionListParams = NULL;
static objectKey window = NULL;
static objectKey listHeaderLabel = NULL;
static objectKey connectionList = NULL;
static int stop = 0;


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
		windowNewErrorDialog(NULL, _("Error"), output);
	else
		printf("\n%s\n", output);
}


static int getConnections(void)
{
	int status = 0;
	int newNumConnections = 0;
	networkConnection *newConnection = NULL;

	// Free any old list of connections
	numConnections = 0;
	if (connection)
	{
		free(connection);
		connection = NULL;
	}

	// Call the kernel to give us the number of connections
	newNumConnections = networkConnectionGetCount();
	if (newNumConnections < 0)
	{
		errno = status;
		perror("networkConnectionGetCount");
		return (status);
	}

	if (!newNumConnections)
		return (status = 0);

	newConnection = calloc(newNumConnections, sizeof(networkConnection));
	if (!newConnection)
	{
		perror("calloc");
		return (status = errno);
	}

	// Call the kernel to get the list of connections
	status = networkConnectionGetAll(newConnection, (newNumConnections *
		sizeof(networkConnection)));
	if (status < 0)
	{
		errno = status;
		perror("networkConnectionGetAll");
		free(newConnection);
		return (status);
	}

	connection = newConnection;
	numConnections = newNumConnections;

	return (status = 0);
}


static void makeConnectionString(networkConnection *conn, char *lineBuffer,
	int bufferSize)
{
	const char *modeName = NULL;
	char tmp[80];

	memset(lineBuffer, ' ', bufferSize);

	sprintf(tmp, "%s", conn->netDev);
	memcpy(lineBuffer, tmp, strlen(tmp));

	sprintf(tmp, "  %d", conn->processId);
	memcpy((lineBuffer + (COL_PROC - 2)), tmp, strlen(tmp));

	sprintf(tmp, "  %d.%d.%d.%d:%d", conn->address.byte[0],
		conn->address.byte[1], conn->address.byte[2], conn->address.byte[3],
		conn->filter.remotePort);
	memcpy((lineBuffer + (COL_ADDR - 2)), tmp, strlen(tmp));

	switch (conn->mode)
	{
		case NETWORK_MODE_LISTEN:
			modeName = _("listen");
			break;
		case NETWORK_MODE_READ:
			modeName = _("read");
			break;
		case NETWORK_MODE_WRITE:
			modeName = _("write");
			break;
		case NETWORK_MODE_READWRITE:
			modeName = _("read/write");
			break;
		default:
			modeName = _("unknown");
			break;
	}

	sprintf(tmp, "  %s", modeName);
	memcpy((lineBuffer + (COL_MODE - 2)), tmp, strlen(tmp));

	if ((conn->filter.flags & NETWORK_FILTERFLAG_TRANSPROTOCOL) &&
		(conn->filter.transProtocol == NETWORK_TRANSPROTOCOL_TCP))
	{
		sprintf((lineBuffer + (COL_STATE - 2)), "  %s",
			tcpStateNames[conn->tcpState]);
	}
	else
	{
		lineBuffer[COL_STATE - 2] = '\0';
	}
}


static int getUpdate(void)
{
	int status = 0;
	int numParams = 0;
	listItemParameters *newConnectionListParams = NULL;
	int count;

	status = getConnections();
	if (status < 0)
		return (status);

	if (numConnections)
		numParams = numConnections;
	else
		numParams = 1;

	newConnectionListParams = calloc(numParams, sizeof(listItemParameters));
	if (!newConnectionListParams)
	{
		perror("calloc");
		return (status = errno);
	}

	if (numConnections)
	{
		for (count = 0; count < numConnections; count ++)
		{
			makeConnectionString(&connection[count],
				newConnectionListParams[count].text, WINDOW_MAX_LABEL_LENGTH);
		}
	}

	if (connectionListParams)
		free(connectionListParams);

	connectionListParams = newConnectionListParams;

	return (status = 0);
}


static void makeListHeader(char *lineBuffer, int bufferSize)
{
	// Create the label of column headers for the connection list
	memset(lineBuffer, ' ', bufferSize);
	memcpy(lineBuffer, DEVICE_STRING, strlen(DEVICE_STRING));
	memcpy((lineBuffer + COL_PROC), PROCESS_STRING, strlen(PROCESS_STRING));
	memcpy((lineBuffer + COL_ADDR), ADDRESS_STRING, strlen(ADDRESS_STRING));
	memcpy((lineBuffer + COL_MODE), MODE_STRING, strlen(MODE_STRING));
	sprintf((lineBuffer + COL_STATE), "%s", STATE_STRING);
}


static void refreshWindow(void)
{
	// We got a 'window refresh' event (probably because of a language
	// switch), so we need to update things

	char tmp[80];

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("netstat");

	// Re-get the character set
	if (getenv(ENV_CHARSET))
		windowSetCharSet(window, getenv(ENV_CHARSET));

	// Refresh the connection list header label
	makeListHeader(tmp, sizeof(tmp));
	windowComponentSetData(listHeaderLabel, tmp, strlen(tmp), 1 /* redraw */);

	// Refresh the window title
	windowSetTitle(window, WINDOW_TITLE);

	// Re-layout the window
	windowLayout(window);
}


static void eventHandler(objectKey key, windowEvent *event)
{
	// Check for window events.
	if (key == window)
	{
		// Check for window refresh
		if (event->type == WINDOW_EVENT_WINDOW_REFRESH)
		{
			refreshWindow();
		}

		// Check for the window being closed
		else if (event->type == WINDOW_EVENT_WINDOW_CLOSE)
		{
			stop = 1;
			windowGuiStop();
			windowDestroy(window);
		}
	}
}


static int constructWindow(void)
{
	// If we are in graphics mode, make a window rather than operating on the
	// command line.

	int status = 0;
	componentParameters params;
	char tmp[80];

	// Create a new window
	window = windowNew(multitaskerGetCurrentProcessId(), WINDOW_TITLE);
	if (!window)
		return (status = ERR_NOCREATE);

	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 5;
	params.padRight = 5;
	params.padTop = 5;
	params.orientationX = orient_left;
	params.orientationY = orient_top;
	params.flags = COMP_PARAMS_FLAG_FIXEDHEIGHT;
	params.font = fontGet(FONT_FAMILY_LIBMONO, FONT_STYLEFLAG_FIXED, 8, NULL);

	// Create the label of column headers for the list below
	makeListHeader(tmp, sizeof(tmp));
	listHeaderLabel = windowNewTextLabel(window, tmp, &params);

	// Create the list of connections
	params.gridY += 1;
	params.padBottom = 5;
	connectionList = windowNewList(window, windowlist_textonly, 20 /* rows */,
		1 /* columns */, 0 /* selectMultiple */, connectionListParams,
		(numConnections? numConnections : 1), &params);
	windowComponentFocus(connectionList);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);

	windowSetVisible(window, 1);

	return (status = 0);
}


static void printConnections(void)
{
	char tmp[80];
	int count;

	if (!numConnections)
	{
		printf("%s\n", _("No connections."));
		return;
	}

	makeListHeader(tmp, sizeof(tmp));
	printf("%s\n", tmp);

	for (count = 0; count < numConnections; count ++)
	{
		// Print connection info
		makeConnectionString(&connection[count], tmp, sizeof(tmp));
		printf("%s\n", tmp);
	}
}


int main(int argc, char *argv[])
{
	int status = 0;
	char opt;
	int guiThreadPid = 0;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("netstat");

	// Are graphics enabled?
	graphics = graphicsAreEnabled();

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

	if (graphics)
	{
		// Get the list of connections and make list item parameters for them
		status = getUpdate();
		if (status < 0)
			return (status);

		// Make our window
		status = constructWindow();
		if (status < 0)
			return (status);

		// Run the GUI
		guiThreadPid = windowGuiThread();

		while (!stop && multitaskerProcessIsAlive(guiThreadPid))
		{
			if (getUpdate() < 0)
				break;

			windowComponentSetData(connectionList, connectionListParams,
				numConnections, 1 /* redraw */);

			sleep(1);
		}
	}
	else
	{
		// Get the list of connections
		status = getConnections();
		if (status < 0)
			return (status);

		// Print them out
		printConnections();
	}

	status = 0;

	if (connectionListParams)
		free(connectionListParams);

	if (connection)
		free(connection);

	return (status);
}

