//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  telnet.c
//

// This is the UNIX-style command for telnetting to another network host

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/network.h>
#include <sys/telnet.h>

static objectKey connection = NULL;
static int stop = 0;


static void usage(char *name)
{
  printf("usage:\n%s <address | hostname>\n", name);
  return;
}


static void interrupt(int sig)
{
  // This is our interrupt signal handler.
  if (sig == SIGINT)
    stop = 1;

  // To terminate the signal handler
  signal(0, SIG_DFL);
}


static void printCommand(unsigned char commandCode)
{
  switch (commandCode)
    {
    case TELNET_COMMAND_SE:
      printf("SE\n");
      break;
    case TELNET_COMMAND_NOP:
      printf("NOP\n");
      break;
    case TELNET_COMMAND_DM:
      printf("Data Mark\n");
      break;
    case TELNET_COMMAND_BRK:
      printf("Break\n");
      break;
    case TELNET_COMMAND_IP:
      printf("Interrupt Process\n");
      break;
    case TELNET_COMMAND_AO:
      printf("Abort output\n");
      break;
    case TELNET_COMMAND_AYT:
      printf("Are You There\n");
      break;
    case TELNET_COMMAND_EC:
      printf("Erase character\n");
      break;
    case TELNET_COMMAND_EL:
      printf("Erase Line\n");
      break;
    case TELNET_COMMAND_GA:
      printf("Go ahead\n");
      break;
    case TELNET_COMMAND_GSB:
      printf("SB\n");
      break;
    case TELNET_COMMAND_WILL:
      printf("WILL ");
      break;
    case TELNET_COMMAND_WONT:
      printf("WON'T ");
      break;
    case TELNET_COMMAND_DO:
      printf("DO ");
      break;
    case TELNET_COMMAND_DONT:
      printf("DON'T ");
      break;
    default:
      printf("unknown code %d\n", commandCode);
      break;
    }
}


static void printOption(unsigned char optionCode)
{
  switch (optionCode)
    {
    case TELNET_OPTION_ECHO:
      printf("echo\n");
      break;
    case TELNET_OPTION_SUPGA:
      printf("suppress go ahead\n");
      break;
    case TELNET_OPTION_STATUS:
      printf("status\n");
      break;
    case TELNET_OPTION_TMARK:
      printf("timing mark\n");
      break;
    case TELNET_OPTION_TTYPE:
      printf("terminal type\n");
      break;
    case TELNET_OPTION_WINSZ:
      printf("window size\n");
      break;
    case TELNET_OPTION_TSPEED:
      printf("terminal speed\n");
      break;
    case TELNET_OPTION_REMFC:
      printf("remote flow control\n");
      break;
    case TELNET_OPTION_LMODE:
      printf("linemode\n");
      break;
    case TELNET_OPTION_ENVAR:
      printf("environment variables\n");
      break;
    case TELNET_OPTION_ENVOPT:
      printf("environment variables\n");
      break;
    default:
      printf("unknown option %d\n", optionCode);
      break;
    }
}


static int sendCommand(unsigned char commandCode, unsigned char optionCode)
{
  unsigned char buffer[4];
  int bytes = 0;
  
  buffer[bytes++] = TELNET_COMMAND_IAC;
  buffer[bytes++] = commandCode;

  switch (commandCode)
    {
    case TELNET_COMMAND_WILL:
    case TELNET_COMMAND_WONT:
    case TELNET_COMMAND_DO:
    case TELNET_COMMAND_DONT:
      buffer[bytes++] = optionCode;
      break;
    }

  return (networkWrite(connection, buffer, ++bytes));
}


int main(int argc, char *argv[])
{
  int status = 0;
  int length = 0;
  networkAddress address = { { 0, 0, 0, 0, 0, 0 } };
  networkFilter filter;
  unsigned char *buffer = NULL;
  int count, bytes;

  if (argc != 2)
    {
      usage(argv[0]);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Parse the supplied network address into our networkAddress structure.

  length = strlen(argv[1]);

  // Replace dots ('.') with NULLS
  for (count = 0; count < length; count ++)
    if (argv[1][count] == '.')
      argv[1][count] = '\0';

  // Get the decimal value of up to 4 numbers
  for (count = 0; count < 4; count ++)
    {
      status = atoi(argv[1]);
      if (status < 0)
	{
	  usage(argv[0]);
	  return (status = ERR_INVALID);
	}

      address.bytes[count] = status;
      argv[1] += (strlen(argv[1]) + 1);
    }

  buffer = malloc(NETWORK_PACKET_MAX_LENGTH);
  if (buffer == NULL)
    {
      errno = ERR_MEMORY;
      perror(argv[0]);
      return (status);
    }

  // Clear out our filter and ask for the network the headers we want
  bzero(&filter, sizeof(networkFilter));
  filter.transProtocol = NETWORK_TRANSPROTOCOL_TCP;
  filter.localPort = 12468;
  filter.remotePort = 23;

  printf("Telnet %d.%d.%d.%d\n", address.bytes[0], address.bytes[1],
	 address.bytes[2], address.bytes[3]);

  // Open a connection on the adapter
  connection = networkOpen(NETWORK_MODE_READWRITE, &address, &filter);
  if (connection == NULL)
    {
      errno = ERR_IO;
      perror(argv[0]);
      free(buffer);
      return (status = errno);
    }

  // Set up the signal handler for catching CTRL-C interrupt
  if (signal(SIGINT, &interrupt) == SIG_ERR)
    {
      errno = ERR_NOTINITIALIZED;
      perror(argv[0]);
      networkClose(connection);
      free(buffer);
      return (status);
    }

  //if (0)
    {
      // Send some commands
      //sendCommand(TELNET_COMMAND_DO, TELNET_OPTION_SUPGA);
      //sendCommand(TELNET_COMMAND_WILL, TELNET_OPTION_TTYPE);

      while (!stop)
	{
	  bytes = networkCount(connection);
	  if (bytes < 0)
	    break;

	  if (bytes)
	    {
	      bytes =
		networkRead(connection, buffer, NETWORK_PACKET_MAX_LENGTH);
	      if (bytes < 0)
		break;

	      for (count = 0 ; count < bytes; count ++)
		{
		  if ((unsigned char) buffer[count] == TELNET_COMMAND_IAC)
		    {
		      count ++;
		      printCommand(buffer[count]);

		      switch ((unsigned char) buffer[count])
			{
			case TELNET_COMMAND_WILL:
			case TELNET_COMMAND_WONT:
			case TELNET_COMMAND_DO:
			case TELNET_COMMAND_DONT:
			  count ++;
			  printOption(buffer[count]);
			  printCommand(TELNET_COMMAND_WONT);
			  printOption(buffer[count]);
			  sendCommand(TELNET_COMMAND_WONT, buffer[count]);
			  break;

			default:
			  break;
			}
		    }
		  else
		    printf("%c", buffer[count]);
		}
	    }

	  multitaskerYield();
	}
    }

  free(buffer);

  status = networkClose(connection);
  if (status < 0)
    {
      errno = status;
      perror(argv[0]);
      return (status);
    }

  return (status = 0);
}
