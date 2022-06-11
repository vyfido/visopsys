//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  ping.c
//

// This is the UNIX-style command for pinging another network host

/* This is the text that appears when a user requests help about this program
<help>

 -- ping --

'Ping' a host on the network.

Usage:
  ping [-T] <address | hostname>

This command will send 'echo request' (ping) packets across the network to
the requested host, and show information about any response acquired from
that host.  The most common usage of this command is to test network
connectivity.

Options:
-T              : Force text mode operation

</help>
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/network.h>

static int graphics = 0;
static char pingWhom[80];
static objectKey window = NULL;
static objectKey textArea = NULL;
static objectKey stopButton = NULL;
static objectKey connection = NULL;
static int threadPid = 0;
static int stop = 0;
static unsigned char *pingData = NULL;
static int pingPacketSize = 88;
static int packetsReceived = 0;


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
    printf("\n%s\n", output);
}


static void usage(char *name)
{
  error("usage:\n%s [-T] <address | hostname>\n", name);
  return;
}


static void quit(int status)
{
  stop = 1;

  // To terminate the signal handler
  signal(0, SIG_DFL);

  if (graphics)
    {
      windowGuiStop();

      if (window)
	windowDestroy(window);
    }

  exit(status);
}


static void interrupt(int sig)
{
  // This is our interrupt signal handler.
  if (sig == SIGINT)
    quit(0);
}


static void responseThread(void)
{
  // This thread is launched to read response packets from our network
  // connection.

  int bytes = 0;
  unsigned char *buffer = NULL;
  networkIpHeader *ipHeader = NULL;
  networkAddress *srcAddress = NULL;
  networkPingPacket *pingPacket = NULL;

  buffer = malloc(NETWORK_PACKET_MAX_LENGTH);
  if (buffer == NULL)
    {
      errno = ERR_MEMORY;
      multitaskerTerminate(errno);
    }

  ipHeader = (networkIpHeader *) buffer;
  srcAddress = (networkAddress *) &(ipHeader->srcAddress);
  pingPacket = ((void *) ipHeader + sizeof(networkIpHeader));

  while (!stop)
    {
      if (networkCount(connection) >= pingPacketSize)
	{
	  bytes = networkRead(connection, buffer, NETWORK_PACKET_MAX_LENGTH);
	  if (bytes > 0)
	    {
	      // Byte-swap any things we need
	      swab(&(ipHeader->totalLength), &(ipHeader->totalLength),
		   sizeof(unsigned short));
	      swab(&(pingPacket->sequenceNum), &(pingPacket->sequenceNum),
		   sizeof(unsigned short));

	      printf("%d bytes from %d.%d.%d.%d: icmp_seq=%d ttl=%d time=X "
		     "ms\n", ipHeader->totalLength,  srcAddress->bytes[0],
		     srcAddress->bytes[1], srcAddress->bytes[2],
		     srcAddress->bytes[3], pingPacket->sequenceNum,
		     ipHeader->timeToLive);

	      packetsReceived += 1;
	    }
	}

      multitaskerYield();
    }

  multitaskerTerminate(0);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  // Check for the window being closed by a GUI event.
  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    quit(0);
  else if ((key == stopButton) && (event->type == EVENT_MOUSE_LEFTUP))
    stop = 1;
  return;
}


static void constructWindow(void)
{
  componentParameters params;

  // Create a new window
  window = windowNew(multitaskerGetCurrentProcessId(), "Ping");
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));

  // A text label to show whom we're pinging
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;
  windowNewTextLabel(window, pingWhom, &params);

  if (fontLoad("/system/fonts/xterm-normal-10.bmp", "xterm-normal-10",
	       &(params.font), 1) < 0)
    params.font = NULL;

  // Create a text area to show our ping activity
  params.gridY = 1;
  textArea = windowNewTextArea(window, 60, 5, 50, &params);
  windowSetTextOutput(textArea);
  textSetCursor(0);
  textInputSetEcho(0);
  
  // Create a 'Stop' button
  params.gridY = 2;
  params.padBottom = 5;
  params.orientationX = orient_center;
  params.useDefaultBackground = 1;
  params.fixedWidth = 1;
  params.font = NULL;
  stopButton = windowNewButton(window, "Stop", NULL, &params);
  windowComponentFocus(stopButton);
  windowRegisterEventHandler(stopButton, &eventHandler);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char opt;
  int length = 0;
  char addressBuffer[18];
  networkAddress address = { { 0, 0, 0, 0, 0, 0 } };
  networkFilter filter;
  int packetsSent = 0;
  unsigned currentTime = rtcUptimeSeconds();
  unsigned tmpTime = 0;
  int count;

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  while (strchr("T", (opt = getopt(argc, argv, "T"))))
    {
      // Force text mode?
      if (opt == 'T')
	graphics = 0;
    }

  // Make sure networking is enabled
  if (!networkInitialized())
    {
      error("Networking is not currently enabled");
      return (status = ERR_NOTINITIALIZED);
    }

  if ((argc < 2) && !graphics)
    {
      usage(argv[0]);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // If we are in graphics mode, we will create an interactive window
  // which prompts for a destination to ping.
  if (graphics)
    {
      if (argc < 2)
	{
	  status = windowNewPromptDialog(window, "Enter Address", "Enter the "
					 "network address to ping:", 1, 18,
					 addressBuffer);
	  if (status <= 0)
	    quit(status);

	  argv[argc - 1] = addressBuffer;
	}
    }

  // Parse the supplied network address into our networkAddress structure.

  length = strlen(argv[argc - 1]);

  // Replace dots ('.') with NULLS
  for (count = 0; count < length; count ++)
    if (argv[argc - 1][count] == '.')
      argv[argc - 1][count] = '\0';

  // Get the decimal value of up to 4 numbers
  for (count = 0; count < 4; count ++)
    {
      status = atoi(argv[argc - 1]);
      if (status < 0)
	{
	  usage(argv[0]);
	  quit(status = ERR_INVALID);
	}

      address.bytes[count] = status;
      argv[argc - 1] += (strlen(argv[argc - 1]) + 1);
    }

  // Get memory for our ping data buffer
  pingData = malloc(NETWORK_PING_DATASIZE);
  if (pingData == NULL)
    {
      error("Memory allocation error");
      quit(errno = ERR_MEMORY);
    }

  // Fill out our ping data.  56 ASCII characters: 'A' through 'x'
  for (count = 0; count < NETWORK_PING_DATASIZE; count ++)
    pingData[count] = (char) (count + 65);

  // Clear out our filter and ask for the network the headers we want
  bzero(&filter, sizeof(networkFilter));
  filter.headers = NETWORK_HEADERS_NET;
  filter.transProtocol = NETWORK_TRANSPROTOCOL_ICMP;
  filter.subProtocol = NETWORK_ICMP_ECHOREPLY;

  // Open a raw network-level connection on the adapter in order to receive
  // the ping replies
  connection = networkOpen(NETWORK_MODE_READWRITE, &address, &filter);
  if (connection == NULL)
    {
      error("Error opening network connection");
      quit(errno = ERR_IO);
    }

  sprintf(pingWhom, "Ping %d.%d.%d.%d %d bytes of data\n", address.bytes[0],
	  address.bytes[1], address.bytes[2], address.bytes[3],
	  NETWORK_PING_DATASIZE);

  if (graphics)
    {
      constructWindow();
      windowGuiThread();
    }
  else
    {
      // Set up the signal handler for catching CTRL-C interrupt
      if (signal(SIGINT, &interrupt) == SIG_ERR)
	{
	  error("Error setting signal handler");
	  quit(errno = ERR_NOTINITIALIZED);
	}

      printf("%s\n", pingWhom);
    }

  // Launch our thread to read response packets from the connection we just
  // opened
  threadPid =
    multitaskerSpawn(responseThread, "ping receive thread", 0, NULL);
  if (threadPid < 0)
    {
      error("Error starting response thread");
      quit(errno = threadPid);
    }

  for (count = 0; !stop ; count ++)
    {
      status = networkPing(connection, count, pingData, NETWORK_PING_DATASIZE);
      if (status < 0)
	{
	  error("Error pinging host");
	  quit(errno = status);
	}

      packetsSent += 1;

      // Wait about 1 second
      while (((tmpTime = rtcUptimeSeconds()) <= currentTime) && !stop)
	multitaskerYield();

      currentTime = tmpTime;
    }

  networkClose(connection);

  // Wait for the receive thread to finish.
  while (multitaskerProcessIsAlive(threadPid));

  printf("\n--- %d.%d.%d.%d ping statistics ---\n", address.bytes[0],
	 address.bytes[1], address.bytes[2], address.bytes[3]);
  printf("%d packets transmitted, %d received, %d%% packet loss, time Xms\n",
	 packetsSent, packetsReceived,
	 (packetsSent?
	  (((packetsSent - packetsReceived) * 100) / packetsSent) : 0));

  quit(0);
  // Compiler happy
  return (0);
}
