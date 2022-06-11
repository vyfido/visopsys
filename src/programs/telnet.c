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
//  telnet.c
//

// This is the UNIX-style command for opening a telnet connection to another
// network host

/* This is the text that appears when a user requests help about this program
<help>

 -- telnet --

Connect to a remote host using the telnet protocol.

Usage:
  telnet [-T] <address | hostname>

Options:
-T  : Force text mode operation

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <netdb.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/api.h>
#include <sys/ascii.h>
#include <sys/env.h>
#include <sys/font.h>
#include <sys/network.h>
#include <sys/socket.h>
#include <sys/telnet.h>
#include <sys/window.h>

#define _(string) gettext(string)

#define WINDOW_TITLE	_("Telnet")

static int graphics = 0;
static objectKey window = NULL;
static int closeWindow = 0;
static int sendInterrupt = 0;
static int closeConnection = 0;
static telnetState telnet;


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
		windowNewErrorDialog(window, _("Error"), output);
	else
		fprintf(stderr, "\n%s\n", output);
}


static void usage(char *name)
{
	error(_("usage:\n%s [-T] <address | hostname>"), name);
}


static void refreshWindow(void)
{
	// We got a 'window refresh' event (probably because of a language
	// switch), so we need to update things

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("telnet");

	// Re-get the character set
	if (getenv(ENV_CHARSET))
		windowSetCharSet(window, getenv(ENV_CHARSET));

	// Refresh the window title
	windowSetTitle(window, WINDOW_TITLE);

	// Re-layout the window (not necessary if no components have changed)
	//windowLayout(window);
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
			closeWindow = 1;
		}
	}
}


static void constructWindow(void)
{
	int rows = 25;
	objectKey textArea = NULL;
	componentParameters params;

	// Create a new window
	window = windowNew(multitaskerGetCurrentProcessId(), WINDOW_TITLE);
	if (!window)
		return;

	// Put a text area in the window
	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 1;
	params.padRight = 1;
	params.padTop = 1;
	params.padBottom = 1;
	params.orientationX = orient_center;
	params.orientationY = orient_middle;
	params.flags |= COMP_PARAMS_FLAG_STICKYFOCUS;
	params.font = fontGet(FONT_FAMILY_XTERM, FONT_STYLEFLAG_FIXED, 10, NULL);
	if (!params.font)
		// The default/system fonts can comfortably show more rows
		rows = 40;

	textArea = windowNewTextArea(window, 80 /* columns */, rows,
		200 /* bufferLines */, &params);
	windowComponentFocus(textArea);

	// Use the text area for all our input and output
	windowSetTextOutput(textArea);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);

	// Go live
	windowSetVisible(window, 1);

	// Run the GUI as a thread
	windowGuiThread();
}


static void interrupt(int sig)
{
	if (sig == SIGINT)
		// Send an interrupt process
		sendInterrupt = 1;
}


__attribute__((format(printf, 2, 3)))
static objectKey info(int button, const char *format, ...)
{
	// Generic info message code for either text or graphics modes

	objectKey dialogWindow = NULL;
	va_list list;
	char output[MAXSTRINGLENGTH + 1];

	va_start(list, format);
	vsnprintf(output, MAXSTRINGLENGTH, format, list);
	va_end(list);

	if (graphics)
	{
		if (button)
			windowNewInfoDialog(window, _("Info"), output);
		else
			dialogWindow = windowNewBannerDialog(window, _("Info"), output);
	}
	else
	{
		fprintf(stdout, "\n%s\n", output);
	}

	return (dialogWindow);
}


static int doConnect(const char *remoteHost)
{
	// Get the address to connect to

	int status = 0;
	struct addrinfo hints;
	struct addrinfo *addrInfo = NULL;
	char addrString[INET6_ADDRSTRLEN];
	int port = 0;
	objectKey dialogWindow = NULL;
	int sockFd = 0;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_protocol = NETWORK_TRANSPROTOCOL_TCP;

	status = getaddrinfo(remoteHost, NETWORK_PORTNAME_TELNET, &hints,
		&addrInfo);
	if (status || ((addrInfo->ai_family != AF_INET) &&
		(addrInfo->ai_family != AF_INET6)))
	{
		// Seems like an invalid address or hostname
		error("%s", _("Invalid address or unknown host name"));
		return (status = ERR_HOSTUNKNOWN);
	}

	if (addrInfo->ai_family == AF_INET)
	{
		struct sockaddr_in *addr =
			(struct sockaddr_in *) addrInfo->ai_addr;
		inet_ntop(AF_INET, &addr->sin_addr, addrString, INET_ADDRSTRLEN);
		port = ntohs(addr->sin_port);
	}
	else
	{
		struct sockaddr_in6 *addr6 =
			(struct sockaddr_in6 *) addrInfo->ai_addr;
		inet_ntop(AF_INET6, &addr6->sin6_addr, addrString, INET6_ADDRSTRLEN);
		port = ntohs(addr6->sin6_port);
	}

	dialogWindow = info(0 /* no button */, _("Trying %s..."), addrString);

	sockFd = socket(addrInfo->ai_family, SOCK_STREAM, addrInfo->ai_protocol);
	if (sockFd < 0)
	{
		status = errno;
		perror("socket");
		error("%s", _("socket() creation failed"));
		goto out;
	}

	status = connect(sockFd, addrInfo->ai_addr, addrInfo->ai_addrlen);
	if (status < 0)
	{
		status = errno;
		perror("connect");
		error("%s", _("connect() failed"));
		goto out;
	}

	printf(_("Connected to %s port %u\n"), addrString, port);
	printf("%s", _("Escape character is CTRL-]\n"));

	status = sockFd;

out:
	if (dialogWindow)
		windowDestroy(dialogWindow);

	// Free the addrinfo list
	if (addrInfo)
		freeaddrinfo(addrInfo);

	return (status);
}


static int readData(telnetState *ts, unsigned char *buffer, unsigned buffLen)
{
	int status = 0;
	struct pollfd pfds = { ts->sockFd, POLLIN, 0 };
	ssize_t received = 0;

	memset(buffer, 0, buffLen);

	// See whether there's any data currently waiting
	status = poll(&pfds, 1, 1 /* ms */);
	if (status <= 0)
		return (status = -1);

	received = recv(ts->sockFd, buffer, buffLen, 0);
	if (received <= 0)
		// If poll() returns >0 and this returns 0, seems like a hint that
		// we've disconnected.
		return (status = -1);

	return (status = (int) received);
}


static int writeData(telnetState *ts, unsigned char *buffer, unsigned buffLen)
{
	ssize_t status = 0;
	ssize_t sent = 0;

	while ((unsigned) sent < buffLen)
	{
		status = send(ts->sockFd, (buffer + sent), (buffLen - sent), 0);
		if (status < 0)
			return (status);

		sent += status;
	}

	return (0);
}


static int readKeyboard(telnetState *ts, unsigned char *buffer,
	unsigned buffLen)
{
	int status = 0;
	struct pollfd pfds = { 0 /* stdin */, POLLIN, 0 };
	unsigned numRead = 0;
	int data = 0;

	memset(buffer, 0, buffLen);

	// See whether there's any data currently waiting
	status = poll(&pfds, 1, 1 /* ms */);
	if (status <= 0)
		return (status = -1);

	while (numRead < buffLen)
	{
		data = getchar();

		if (data == '\n')
		{
			// User Enter should be output as carriage return followed by NULL
			buffer[numRead++] = '\r';
			if (numRead < buffLen)
				buffer[numRead++] = '\0';
		}
		else if (data == ASCII_GS)
		{
			// Escape character
			closeConnection = 1;
			break;
		}
		else if (data == ASCII_BS)
		{
			// Backspace character
			telnetSendCommand(&telnet, TELNET_COMMAND_EC, 0);
			break;
		}
		else
		{
			buffer[numRead++] = (unsigned char) data;
		}

		if (!telnetOptionGet(&ts->options.remote, TELNET_OPTION_ECHO))
			printf("%c", data);
		else
			break;
	}

	buffer[numRead] = '\0';

	return (status = numRead);
}


__attribute__((noreturn))
static void quit(int status)
{
	// Restore automatic local keyboard echo
	textInputSetEcho(1);

	// To terminate the signal handler
	signal(0, SIG_DFL);

	if (graphics)
	{
		// Stop our GUI thread
		windowGuiStop();

		// Destroy the window
		windowDestroy(window);
		window = NULL;
	}

	// Close the socket
	if (telnet.sockFd > 0)
	{
		close(telnet.sockFd);
		telnet.sockFd = 0;
	}

	// Free memory
	telnetFini(&telnet);

	exit(status);
}


int main(int argc, char *argv[])
{
	int status = 0;
	char opt;
	const char *remoteHost = NULL;
	char addressBuffer[18];
	int sockFd = 0;
	int bytes = 0;
	int count;

	memset(&telnet, 0, sizeof(telnetState));

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("telnet");

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
				usage(argv[0]);
				return (status = ERR_INVALID);
		}
	}

	// Make sure networking is enabled
	if (!networkEnabled())
	{
		error("%s", _("Networking is not currently enabled"));
		return (status = ERR_NOTINITIALIZED);
	}

	if (argc >= 2)
		remoteHost = argv[argc - 1];

	// If we are in graphics mode, and no destination has been specified, we
	// will create an interactive window which prompts for it.
	if (graphics)
	{
		if (!remoteHost)
		{
			status = windowNewPromptDialog(window, _("Enter Address"),
				_("Enter the remote address:"), 1, sizeof(addressBuffer),
				addressBuffer);
			if (status <= 0)
				return(status);

			remoteHost = addressBuffer;
		}
	}
	else
	{
		if (!remoteHost)
		{
			usage(argv[0]);
			return (status = ERR_ARGUMENTCOUNT);
		}
	}

	if (graphics)
		constructWindow();

	// Set up the signal handler for catching CTRL-C interrupt
	if (signal(SIGINT, &interrupt) == SIG_ERR)
	{
		error("%s", _("Error setting signal handler"));
		status = ERR_NOTINITIALIZED;
		goto out;
	}

	// Connect
	sockFd = doConnect(remoteHost);
	if (sockFd < 0)
	{
		status = sockFd;
		goto out;
	}

	// Initialize the telnet library
	status = telnetInit(sockFd, &telnet);
	if (status < 0)
	{
		error("%s", _("telnetInit() failed"));
		goto out;
	}

	telnet.readData = &readData;
	telnet.writeData = &writeData;

	// Turn off automatic local keyboard echo
	textInputSetEcho(0);

	while (telnet.alive && !closeConnection && !closeWindow)
	{
		// Read
		while (1)
		{
			// Check for user CTRL-C
			if (sendInterrupt)
			{
				telnetSendCommand(&telnet, TELNET_COMMAND_IP, 0);
				sendInterrupt = 0;
			}

			bytes = telnetReadData(&telnet);
			if (bytes <= 0)
				break;

			for (count = 0; count < bytes; count ++)
			{
				if (telnet.inBuffer[count] == '\a')
				{
					// Bell.  Do a beep?
					continue;
				}
				else if (telnet.inBuffer[count] == '\r')
				{
					// Carriage return.  Ignore.
					continue;
				}
				else if (telnet.inBuffer[count] == '\b')
				{
					// Backspace
					textBackSpace();
					continue;
				}

				printf("%c", telnet.inBuffer[count]);
			}
		}

		// Write
		while (1)
		{
			bytes = readKeyboard(&telnet, telnet.outBuffer,
				NETWORK_PACKET_MAX_LENGTH);
			if (bytes <= 0)
			{
				if (!telnet.sentGoAhead &&
					!telnetOptionGet(&telnet.options.local,
						TELNET_OPTION_SUPGA))
				{
					// Send 'go ahead' to signal that we're finished
					telnetSendCommand(&telnet, TELNET_COMMAND_GA, 0);
					telnet.sentGoAhead = 1;
				}

				break;
			}

			status = telnetWriteData(&telnet,
				strlen((char *) telnet.outBuffer));
			if (status < 0)
				goto closed;
		}

		// Yield the rest of this timeslice
		sched_yield();
	}

closed:
	if (!closeWindow)
		info(1 /* button */, "%s", _("Connection closed"));

	status = 0;

out:
	quit(status);
}

