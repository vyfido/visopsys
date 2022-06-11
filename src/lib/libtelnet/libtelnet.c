//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  libtelnet.c
//

// This is the library for using the Telnet protocol

#include "libtelnet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/network.h>
#include <sys/telnet.h>

// Default supported host options
static telnetHostOptions defaultHostOptions = {
	{ 0 },
	{
		{ TELNET_OPTION_ECHO, 0, NULL },
		{ TELNET_OPTION_SUPGA, 0, NULL },
		{ TELNET_OPTION_STATUS, 0, NULL },
		{ TELNET_OPTION_TMARK, 0, NULL },
		{ TELNET_OPTION_TTYPE, 0, NULL }
	}
};

// Strings for all known command codes
static struct {
	unsigned char code;
	const char *string;

} cmdStrings[] = {
	{ TELNET_COMMAND_SE,	"Subnegotiation End" },
	{ TELNET_COMMAND_NOP,	"NO-OP" },
	{ TELNET_COMMAND_DM,	"Data Mark" },
	{ TELNET_COMMAND_BRK,	"Break" },
	{ TELNET_COMMAND_IP,	"Interrupt Process" },
	{ TELNET_COMMAND_AO,	"Abort Output" },
	{ TELNET_COMMAND_AYT,	"Are You There" },
	{ TELNET_COMMAND_EC,	"Erase Character" },
	{ TELNET_COMMAND_EL,	"Erase Line" },
	{ TELNET_COMMAND_GA,	"Go Ahead" },
	{ TELNET_COMMAND_SB,	"Subnegotiation Begin" },
	{ TELNET_COMMAND_WILL,	"WILL" },
	{ TELNET_COMMAND_WONT,	"WON'T" },
	{ TELNET_COMMAND_DO,	"DO" },
	{ TELNET_COMMAND_DONT,	"DON'T" },
	{ 0, NULL }
};

// Strings for all known option codes
static struct {
	unsigned char code;
	const char *string;

} optStrings[] = {
	{ TELNET_OPTION_BINTRANS,	"binary transmission" },
	{ TELNET_OPTION_ECHO,		"echo" },
	{ TELNET_OPTION_RECONCT,	"reconnection" },
	{ TELNET_OPTION_SUPGA,		"suppress go ahead" },
	{ TELNET_OPTION_APPMSN,		"approx message size negotiation" },
	{ TELNET_OPTION_STATUS,		"status" },
	{ TELNET_OPTION_TMARK,		"timing mark" },
	{ TELNET_OPTION_RCTRECH,	"remote controlled trans/echo" },
	{ TELNET_OPTION_OLINEW,		"output line width" },
	{ TELNET_OPTION_OPGSIZE,	"output page size" },
	{ TELNET_OPTION_OCRDISP,	"output CR disposition" },
	{ TELNET_OPTION_OHTSTOPS,	"output HT stops" },
	{ TELNET_OPTION_OHTDISP,	"output HT disposition" },
	{ TELNET_OPTION_OFFDISP,	"output FF disposition" },
	{ TELNET_OPTION_OVTSTOPS,	"output VT stops" },
	{ TELNET_OPTION_OVTDISP,	"output VT disposition" },
	{ TELNET_OPTION_OLFDISP,	"output LF disposition" },
	{ TELNET_OPTION_EXTASCII,	"extended ASCII" },
	{ TELNET_OPTION_LOGOUT,		"logout" },
	{ TELNET_OPTION_BYTEMACRO,	"byte macro" },
	{ TELNET_OPTION_DATAETERM,	"data entry terminal" },
	{ TELNET_OPTION_SUPDUP,		"SUPDUP" },
	{ TELNET_OPTION_SUPDUPO,	"SUPDUP output" },
	{ TELNET_OPTION_SENDLOC,	"send location" },
	{ TELNET_OPTION_TTYPE,		"terminal type" },
	{ TELNET_OPTION_EOR,		"end of record" },
	{ TELNET_OPTION_TACACSUID,	"TACACS user identification" },
	{ TELNET_OPTION_OMARKING,	"output marking" },
	{ TELNET_OPTION_TLOCNUM,	"terminal location number" },
	{ TELNET_OPTION_TNET3270,	"telnet 3270 regime" },
	{ TELNET_OPTION_X3PAD,		"X.3 PAD" },
	{ TELNET_OPTION_WINSZ,		"window size" },
	{ TELNET_OPTION_TSPEED,		"terminal speed" },
	{ TELNET_OPTION_REMFC,		"remote flow control" },
	{ TELNET_OPTION_LMODE,		"linemode" },
	{ TELNET_OPTION_XDISPLOC,	"X display location" },
	{ TELNET_OPTION_ENVAR,		"environment variables" },
	{ TELNET_OPTION_AUTHOPT,	"authentication option" },
	{ TELNET_OPTION_ENCOPT,		"encryption option" },
	{ TELNET_OPTION_ENVOPT,		"environment option" },
	{ TELNET_OPTION_TN3270E,	"TN3270E" },
	{ TELNET_OPTION_XAUTH,		"XAUTH" },
	{ TELNET_OPTION_CHARSET,	"CHARSET" },
	{ TELNET_OPTION_TELRSP,		"telnet remote serial port" },
	{ TELNET_OPTION_COMPCTRL,	"com port control option" },
	{ TELNET_OPTION_TELSLE,		"telnet suppress local echo" },
	{ TELNET_OPTION_TELSTLS,	"telnet start TLS" },
	{ TELNET_OPTION_KERMIT,		"KERMIT" },
	{ TELNET_OPTION_SENDURL,	"SEND-URL" },
	{ TELNET_OPTION_FORWARDX,	"FORWARD_X" },
	{ TELNET_OPTION_PRAGMALOG,	"TELOPT PRAGMA LOGON" },
	{ TELNET_OPTION_SSPILOGON,	"TELOPT SSPI LOGON" },
	{ TELNET_OPTION_PRAGHEART,	"TELOPT PRAGMA HEARTBEAT" },
	{ 0, NULL }
};

#ifdef DEBUG
	#define DEBUG_OUTMAX	160
	int debugLibTelnet = 0;
	static char debugOutput[DEBUG_OUTMAX];

	static void DEBUGMSG(const char *message, ...)
	{
		va_list list;

		if (debugLibTelnet)
		{
			va_start(list, message);
			vsnprintf(debugOutput, DEBUG_OUTMAX, message, list);
			printf("%s", debugOutput);
		}
	}
#else
	#define DEBUGMSG(message, arg...) do { } while (0)
#endif


static telnetOption *findOption(telnetHostOptions *host, unsigned char code)
{
	// Try to locate the option struct in the given host-specific list

	int count;

	for (count = 0; count < LIBTELNET_NUM_SUPP_OPTS; count ++)
	{
		if (host->option[count].code == code)
			return (&host->option[count]);
	}

	// Not found
	return (NULL);
}


static int subnegStatus(telnetState *telnet, unsigned char *buffer,
	unsigned bufferLen)
{
	// Send or receive status information about the state of the options.
	// Returns the number of bytes occupied by the remainder of the command
	// in the buffer

	unsigned char isSend = 0;
	unsigned char output[6 + (4 * LIBTELNET_NUM_SUPP_OPTS)];
	int optBytes = 0;
	unsigned count1 = 0, count2 = 0;

	// Check params
	if (!bufferLen)
		return (0);

	// Are we sending or receiving?
	if (bufferLen - count1)
		isSend = buffer[count1++];

	switch (isSend)
	{
		case 0: // IS
		{
			DEBUGMSG("IS\n");
			for ( ; count1 < (bufferLen - 1); count1 += 2)
			{
				if ((buffer[count1] == TELNET_COMMAND_IAC) &&
					(buffer[count1 + 1] == TELNET_COMMAND_SE))
				{
					break;
				}

				// Should probably do something with these.  Change our local
				// view of the options?  Error-check them?  Request changes?
				DEBUGMSG("\t\t%s %s\n", telnetCommandString(buffer[count1]),
					telnetOptionString(buffer[count1 + 1]));
			}

			break;
		}

		case 1: // SEND
		{
			if ((buffer[count1] == TELNET_COMMAND_IAC) &&
				(buffer[count1 + 1] == TELNET_COMMAND_SE))
			{
				count1 += 2;
				output[0] = TELNET_COMMAND_IAC;
				output[1] = TELNET_COMMAND_SB;
				output[2] = TELNET_OPTION_STATUS;
				output[3] = 0; // IS
				DEBUGMSG("SEND\n");
				for (count2 = 0; count2 < LIBTELNET_NUM_SUPP_OPTS; count2 ++)
				{
					if (telnet->options.local.option[count2].flags &
						LIBTELNET_OPTION_SET)
					{
						if (telnet->options.local.option[count2].value)
							output[4 + optBytes] = TELNET_COMMAND_WILL;
						else
							output[4 + optBytes] = TELNET_COMMAND_WONT;
						output[5 + optBytes] =
							telnet->options.local.option[count2].code;
						DEBUGMSG("\t\t%s %s\n",
							telnetCommandString(output[4 + optBytes]),
							telnetOptionString(output[5 + optBytes]));
						optBytes += 2;
					}
					if (telnet->options.remote.option[count2].flags &
						LIBTELNET_OPTION_SET)
					{
						if (telnet->options.remote.option[count2].value)
							output[4 + optBytes] = TELNET_COMMAND_DO;
						else
							output[4 + optBytes] = TELNET_COMMAND_DONT;
						output[5 + optBytes] =
							telnet->options.remote.option[count2].code;
						DEBUGMSG("\t\t%s %s\n",
							telnetCommandString(output[4 + optBytes]),
							telnetOptionString(output[5 + optBytes]));
						optBytes += 2;
					}
				}
				output[4 + optBytes] = TELNET_COMMAND_IAC;
				output[5 + optBytes] = TELNET_COMMAND_SE;
				if (telnet->writeData)
					telnet->writeData(telnet, output, (6 + optBytes));
			}

			break;
		}

		default:
		{
			// Invalid
			break;
		}
	}

	return (count1);
}


static int subnegTerminalType(telnetState *telnet, unsigned char *buffer,
	unsigned bufferLen)
{
	// Send or receive information about supported terminal types.  Returns
	// the number of bytes occupied by the remainder of the command in the
	// buffer

	unsigned char isSend = 0;
	int nameLen = 0;
	unsigned char output[TELNET_TTYPE_MAXLEN + 7];
	unsigned count = 0;

	// Check params
	if (!bufferLen)
		return (0);

	// Are we sending or receiving?
	if (bufferLen - count)
		isSend = buffer[count++];

	switch (isSend)
	{
		case 0: // IS
		{
			memset(telnet->options.remote.terminalType, 0,
				(TELNET_TTYPE_MAXLEN + 1));
			while ((count < (bufferLen - 1)) && (nameLen <=
				TELNET_TTYPE_MAXLEN))
			{
				if ((buffer[count] == TELNET_COMMAND_IAC) &&
					(buffer[count + 1] == TELNET_COMMAND_SE))
				{
					count += 2;
					break;
				}

				telnet->options.remote.terminalType[nameLen++] =
					buffer[count++];
			}

			telnet->options.remote.terminalType[nameLen] = '\0';
			DEBUGMSG("IS %s\n", telnet->options.remote.terminalType);
			break;
		}

		case 1: // SEND
		{
			if ((buffer[count] == TELNET_COMMAND_IAC) &&
				(buffer[count + 1] == TELNET_COMMAND_SE))
			{
				count += 2;
				output[0] = TELNET_COMMAND_IAC;
				output[1] = TELNET_COMMAND_SB;
				output[2] = TELNET_OPTION_TTYPE;
				output[3] = 0; // IS
				nameLen = max(TELNET_TTYPE_MAXLEN,
					strlen(telnet->options.local.terminalType));
				strncpy((char *) &output[4],
					telnet->options.local.terminalType, TELNET_TTYPE_MAXLEN);
				output[4 + nameLen] = TELNET_COMMAND_IAC;
				output[5 + nameLen] = TELNET_COMMAND_SE;

				DEBUGMSG("SEND %s\n", telnet->options.local.terminalType);
				if (telnet->writeData)
					telnet->writeData(telnet, output, (6 + nameLen));
			}

			break;
		}

		default:
		{
			// Invalid
			break;
		}
	}

	return (count);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int telnetInit(int sockFd, telnetState *telnet)
{
	// Given an open network connection and a pointer to a Telnet 'state'
	// structure, initialize everything.

	int status = 0;

	// Check params
	if ((sockFd <= 0) || !telnet)
		return (status = -1);

	memset(telnet, 0, sizeof(telnetState));
	telnet->sockFd = sockFd;
	telnet->sentGoAhead = 1;
	telnet->lastContact = time(NULL);
	telnet->alive = 1;

	telnet->inBuffer = malloc(NETWORK_PACKET_MAX_LENGTH);
	telnet->outBuffer = malloc(NETWORK_PACKET_MAX_LENGTH);
	if (!telnet->inBuffer || !telnet->outBuffer)
	{
		telnetFini(telnet);
		return (status = -1);
	}

	// Set up the initial (supported) options
	memcpy(&telnet->options.local, &defaultHostOptions,
		sizeof(telnetHostOptions));
	memcpy(&telnet->options.remote, &defaultHostOptions,
		sizeof(telnetHostOptions));

	strcpy(telnet->options.local.terminalType, TELNET_TTYPE_NVT);
	strcpy(telnet->options.remote.terminalType, TELNET_TTYPE_UNKNOWN);

	return (status = 0);
}


const char *telnetCommandString(unsigned char code)
{
	// Return a human-readable string name for any known command code

	int count;

	for (count = 0; cmdStrings[count].string; count ++)
	{
		if (cmdStrings[count].code == code)
			return (cmdStrings[count].string);
	}

	// Not found
	return (NULL);
}


const char *telnetOptionString(unsigned char code)
{
	// Return a human-readable string name for any known option code

	int count;

	for (count = 0; optStrings[count].string; count ++)
	{
		if (optStrings[count].code == code)
			return (optStrings[count].string);
	}

	// Not found
	return (NULL);
}


int telnetOptionIsSet(telnetHostOptions *host, unsigned char code)
{
	// Returns 1 if the option is supported, and has been set for the given
	// host, otherwise 0

	int status = 0;
	telnetOption *option = NULL;

	// Check params
	if (!host)
		return (status = 0);

	option = findOption(host, code);
	if (!option)
		return (status = 0);

	return (status = (option->flags & LIBTELNET_OPTION_SET)? 1 : 0);
}


int telnetOptionWait(telnetHostOptions *host, unsigned char code)
{
	// Sets a flag on the option, so we know when we're receiving a response
	// to an option request

	int status = 0;
	telnetOption *option = NULL;

	// Check params
	if (!host)
		return (status = -1);

	option = findOption(host, code);
	if (!option)
		return (status = -1);

	option->flags |= LIBTELNET_OPTION_WAIT;

	return (status = 0);
}


int telnetOptionIsWaiting(telnetHostOptions *host, unsigned char code)
{
	// Returns 1 if the option is waiting for a request-reply from the remote
	// host, otherwise 0.  Clears the waiting flag.

	int status = 0;
	telnetOption *option = NULL;

	// Check params
	if (!host)
		return (status = 0);

	option = findOption(host, code);
	if (!option)
		return (status = 0);

	status = ((option->flags & LIBTELNET_OPTION_WAIT)? 1 : 0);

	option->flags &= ~LIBTELNET_OPTION_WAIT;

	return (status);
}


int telnetOptionGet(telnetHostOptions *host, unsigned char code)
{
	// If an option is supported, get it (as an int)

	int status = 0;
	telnetOption *option = NULL;

	// Check params
	if (!host)
		return (status = -1);

	option = findOption(host, code);
	if (!option)
		return (status = -1);

	return ((int)((long) option->value));
}


int telnetOptionSet(telnetHostOptions *host, unsigned char code, int value)
{
	// If an option is supported, set it (as an int)

	int status = 0;
	telnetOption *option = NULL;

	// Check params
	if (!host)
		return (status = -1);

	option = findOption(host, code);
	if (!option)
		return (status = -1);

	option->value = (void *)((long) value);
	option->flags |= LIBTELNET_OPTION_SET;

	return (status = 0);
}


int telnetSendCommand(telnetState *telnet, unsigned char command,
	unsigned char option)
{
	// Sends the supplied command, along with an option (depending on whether
	// the command uses one)

	unsigned char buffer[3];
	int bytes = 0;

	// Check params
	if (!telnet)
		return (-1);

	DEBUGMSG("Sending: %s", telnetCommandString(command));

	// Check pointers
	if (!telnet->writeData)
		return (-1);

	buffer[bytes++] = TELNET_COMMAND_IAC;
	buffer[bytes++] = command;

	switch (command)
	{
		case TELNET_COMMAND_WILL:
		case TELNET_COMMAND_WONT:
		case TELNET_COMMAND_DO:
		case TELNET_COMMAND_DONT:
			DEBUGMSG(" %s", telnetOptionString(option));
			buffer[bytes++] = option;
			break;
	}

	DEBUGMSG("\n");

	return (telnet->writeData(telnet, buffer, bytes));
}


int telnetSendResponse(telnetState *telnet, unsigned char command,
	unsigned char option, int positive)
{
	// Send the response to a simple WILL/WON'T/DO/DON'T { command, option }
	// command, with the appropriate positive or negative acknowledgement

	unsigned char response = 0;

	// Check params
	if (!telnet)
		return (-1);

	switch (command)
	{
		case TELNET_COMMAND_WILL:
			if (positive)
				response = TELNET_COMMAND_DO;
			else
				response = TELNET_COMMAND_DONT;
			break;

		case TELNET_COMMAND_WONT:
			if (positive)
				response = TELNET_COMMAND_DONT;
			else
				response = TELNET_COMMAND_DO;
			break;

		case TELNET_COMMAND_DO:
			if (positive)
				response = TELNET_COMMAND_WILL;
			else
				response = TELNET_COMMAND_WONT;
			break;

		case TELNET_COMMAND_DONT:
			if (positive)
				response = TELNET_COMMAND_WONT;
			else
				response = TELNET_COMMAND_WILL;
			break;

		default:
			fprintf(stderr, "Command not understood\n");
			break;
	}

	return (telnetSendCommand(telnet, response, option));
}


int telnetProcessCommand(telnetState *telnet, unsigned char *buffer,
	unsigned bufferLen)
{
	// Given a pointer to the start of a command in the buffer, perform the
	// appropriate actions to handle it.  Returns the number of bytes occupied
	// by the command in the buffer (e.g. 2 for simple commands without
	// options, 3 for simple commands with options, ...)

	unsigned char command = 0;
	unsigned char option = 0;
	unsigned char subCommand = 0;
	int doReply = 0, reply = 0;
	int count = 0;

	// Check params
	if (!telnet || !buffer || !bufferLen)
		return (-1);

	command = buffer[++count];

	DEBUGMSG("Received: %s", telnetCommandString(command));

	// For commands that have an option, read it
	switch (command)
	{
		case TELNET_COMMAND_WILL:
		case TELNET_COMMAND_WONT:
		case TELNET_COMMAND_DO:
		case TELNET_COMMAND_DONT:
			if (bufferLen - count)
				option = buffer[++count];
			DEBUGMSG(" %s", telnetOptionString(option));
			break;
	}

	DEBUGMSG("\n");

	switch (command)
	{
		case TELNET_COMMAND_NOP:
		{
			break;
		}

		case TELNET_COMMAND_DM:
		case TELNET_COMMAND_BRK:
		case TELNET_COMMAND_IP:
		case TELNET_COMMAND_AO:
		case TELNET_COMMAND_AYT:
		case TELNET_COMMAND_EC:
		case TELNET_COMMAND_EL:
		case TELNET_COMMAND_GA:
		{
			// Not sure what we'll do with these, yet
			break;
		}

		case TELNET_COMMAND_SB:
		{
			// Subnegotiation of a supported command
			if (bufferLen - count)
			{
				subCommand = buffer[++count];
				DEBUGMSG("\t%s ", telnetOptionString(subCommand));

				if (bufferLen - count)
					count += 1;
			}

			switch (subCommand)
			{
				case TELNET_OPTION_STATUS:
					count += subnegStatus(telnet, &buffer[count], (bufferLen -
						count));
					break;

				case TELNET_OPTION_TTYPE:
					count += subnegTerminalType(telnet, &buffer[count],
						(bufferLen - count));
					break;

				default:
					// Should never happen.  Ignore this command.
					break;
			}

			break;
		}

		case TELNET_COMMAND_WILL:
		{
			switch (option)
			{
				case TELNET_OPTION_ECHO:
				{
					// The sender wants to echo
					if (!telnetOptionGet(&telnet->options.remote, option))
					{
						telnetOptionSet(&telnet->options.local, option, 0);
						telnetOptionSet(&telnet->options.remote, option, 1);
						// Don't need to print our output
						telnet->localEcho = 0;
						doReply = 1; reply = 1;
					}

					break;
				}

				case TELNET_OPTION_SUPGA:
				case TELNET_OPTION_STATUS:
				case TELNET_OPTION_TMARK:
				case TELNET_OPTION_TTYPE:
				{
					// Allow the sender to suppress go-ahead, provide status,
					// timing marks, terminal type, etc.
					if (!telnetOptionGet(&telnet->options.remote, option))
					{
						telnetOptionSet(&telnet->options.remote, option, 1);
						doReply = 1; reply = 1;
					}

					break;
				}

				default:
				{
					// Not a supported option, send a negative response
					doReply = 1; reply = 0;
					break;
				}
			}

			if (doReply && !telnetOptionIsWaiting(&telnet->options.remote,
				option))
			{
				telnetSendResponse(telnet, command, option, reply);
			}

			break;
		}

		case TELNET_COMMAND_WONT:
		{
			switch (option)
			{
				case TELNET_OPTION_ECHO:
				{
					// The sender doesn't want to echo
					if (!telnetOptionIsSet(&telnet->options.remote, option) ||
						telnetOptionGet(&telnet->options.remote, option))
					{
						telnetOptionSet(&telnet->options.remote, option, 0);
						// Need to print our output
						telnet->localEcho = 1;
						doReply = 1; reply = 1;
					}

					break;
				}

				case TELNET_OPTION_SUPGA:
				case TELNET_OPTION_STATUS:
				case TELNET_OPTION_TMARK:
				case TELNET_OPTION_TTYPE:
				{
					// The sender doesn't want to suppress go-ahead, provide
					// status, timing marks, terminal type, etc.
					if (!telnetOptionIsSet(&telnet->options.remote, option) ||
						telnetOptionGet(&telnet->options.remote, option))
					{
						telnetOptionSet(&telnet->options.remote, option, 0);
						doReply = 1; reply = 1;
					}

					break;
				}

				default:
				{
					// Not a supported option, send a negative response
					doReply = 1; reply = 0;
					break;
				}
			}

			if (doReply && !telnetOptionIsWaiting(&telnet->options.remote,
				option))
			{
				telnetSendResponse(telnet, command, option, reply);
			}

			break;
		}

		case TELNET_COMMAND_DO:
		{
			switch (option)
			{
				case TELNET_OPTION_ECHO:
				{
					// The sender wants us to echo
					if (!telnetOptionGet(&telnet->options.local, option))
					{
						telnetOptionSet(&telnet->options.local, option, 1);
						telnetOptionSet(&telnet->options.remote, option, 0);
						// Remote echo must be off - need to print our output
						telnet->localEcho = 1;
						doReply = 1; reply = 1;
					}

					break;
				}

				case TELNET_OPTION_SUPGA:
				case TELNET_OPTION_STATUS:
				case TELNET_OPTION_TTYPE:
				{
					// The sender wants us to suppress go-ahead, provide
					// status, our terminal type, etc.
					if (!telnetOptionGet(&telnet->options.local, option))
					{
						telnetOptionSet(&telnet->options.local, option, 1);
						doReply = 1; reply = 1;
					}

					break;
				}

				case TELNET_OPTION_TMARK:
				{
					// Always reply positively to a timing mark request, even
					// if the option is already set
					telnetOptionSet(&telnet->options.local, option, 1);
					doReply = 1; reply = 1;
					break;
				}

				default:
				{
					// Not a supported option, send a negative response
					doReply = 1; reply = 0;
					break;
				}
			}

			if (doReply && !telnetOptionIsWaiting(&telnet->options.local,
				option))
			{
				telnetSendResponse(telnet, command, option, reply);
			}

			break;
		}

		case TELNET_COMMAND_DONT:
		{
			switch (option)
			{
				case TELNET_OPTION_ECHO:
				case TELNET_OPTION_SUPGA:
				case TELNET_OPTION_STATUS:
				case TELNET_OPTION_TMARK:
				case TELNET_OPTION_TTYPE:
				{
					// The sender doesn't want us to echo (doesn't necessarily
					// affect remote echo), suppress go-ahead, provide status,
					// timing marks, our terminal type, etc.
					if (!telnetOptionIsSet(&telnet->options.local, option) ||
						telnetOptionGet(&telnet->options.local, option))
					{
						telnetOptionSet(&telnet->options.local, option, 0);
						doReply = 1; reply = 1;
					}

					break;
				}

				default:
				{
					// Not a supported option, send a negative response
					doReply = 1; reply = 0;
					break;
				}
			}

			if (doReply && !telnetOptionIsWaiting(&telnet->options.local,
				option))
			{
				telnetSendResponse(telnet, command, option, reply);
			}

			break;
		}

		default:
		{
			fprintf(stderr, "Command: %d option:%d not understood\n", command,
				option);
			telnetSendResponse(telnet, command, option, 0);
			break;
		}
	}

	return (count);
}


int telnetReadData(telnetState *telnet)
{
	// Reads data from the network, processes commands in the data, handles
	// local echo, and leaves data bytes in the input buffer, returning the
	// number of data bytes.

	int status = 0;
	int totalBytes = 0;
	time_t tm = 0;
	int dataBytes = 0;
	int echoBytes = 0;
	int count;

	// Check params
	if (!telnet)
		return (status = -1);

	// Check pointers
	if (!telnet->readData || !telnet->writeData || !telnet->inBuffer ||
		!telnet->outBuffer)
	{
		return (status = -1);
	}

	tm = time(NULL);

	totalBytes = telnet->readData(telnet, telnet->inBuffer,
		(NETWORK_PACKET_MAX_LENGTH - 1));
	if (totalBytes <= 0)
	{
		// See whether we think the connection is still alive
		if (tm > (telnet->lastContact + 2))
		{
			status = telnetPing(telnet);
			if (status < 0)
			{
				telnet->alive = 0;
				return (status);
			}

			// We won't call it dead until a ping write fails
			telnet->lastContact = tm;
		}

		return (status = totalBytes);
	}

	// Record the time of last contact
	telnet->lastContact = tm;

	for (count = 0 ; count < totalBytes; count ++)
	{
		if (!telnet->inBuffer[count])
		{
			continue;
		}

		else if (telnet->inBuffer[count] == TELNET_COMMAND_IAC)
		{
			count += telnetProcessCommand(telnet, &telnet->inBuffer[count],
				(totalBytes - count));
		}

		else
		{
			telnet->inBuffer[dataBytes++] = telnet->inBuffer[count];

			if (telnetOptionGet(&telnet->options.local, TELNET_OPTION_ECHO))
			{
				// Del should be echoed as a backspace, followed by a space,
				// and then another backspace
				if (telnet->inBuffer[count] == 0x7F)
				{
					telnet->outBuffer[echoBytes++] = '\b';
					if (echoBytes < (NETWORK_PACKET_MAX_LENGTH - 1))
						telnet->outBuffer[echoBytes++] = ' ';
					if (echoBytes < (NETWORK_PACKET_MAX_LENGTH - 1))
						telnet->outBuffer[echoBytes++] = '\b';
				}
				else
				{
					telnet->outBuffer[echoBytes++] = telnet->inBuffer[count];

					// Carriage return should be echoed as a carriage return
					// followed by a newline
					if ((telnet->inBuffer[count] == '\r') &&
						(echoBytes < (NETWORK_PACKET_MAX_LENGTH - 1)))
					{
						telnet->outBuffer[echoBytes++] = '\n';
					}
				}
			}
		}
	}

	if (dataBytes)
		// No 'go ahead' since the last data received
		telnet->sentGoAhead = 0;

	// Echo back?
	if (telnetOptionGet(&telnet->options.local, TELNET_OPTION_ECHO))
		telnet->writeData(telnet, telnet->outBuffer, echoBytes);

	telnet->inBuffer[dataBytes] = '\0';
	return (status = dataBytes);
}


int telnetWriteData(telnetState *telnet, unsigned bytes)
{
	// Simply writes data to the network, but also performs command requests
	// if they've not been negotiated (typically at the beginning of the
	// connection, before any data has been sent).

	int status = 0;
	int count;

	// Check params
	if (!telnet)
		return (status = -1);

	// Check pointers
	if (!telnet->writeData || !telnet->outBuffer)
		return (status = -1);

	if (!telnet->sentRequests)
	{
		// See whether any options we support have not been negotiated yet
		for (count = 0; count < LIBTELNET_NUM_SUPP_OPTS; count ++)
		{
			if (!(telnet->options.local.option[count].flags &
					LIBTELNET_OPTION_SET) &&
				!(telnet->options.local.option[count].flags &
					LIBTELNET_OPTION_WAIT))
			{
				telnet->options.local.option[count].flags |=
					LIBTELNET_OPTION_WAIT;
				status = telnetSendCommand(telnet, TELNET_COMMAND_WILL,
					telnet->options.local.option[count].code);
				if (status < 0)
					break;
			}

			if (!(telnet->options.remote.option[count].flags &
					LIBTELNET_OPTION_SET) &&
				!(telnet->options.remote.option[count].flags &
					LIBTELNET_OPTION_WAIT))
			{
				telnet->options.remote.option[count].flags |=
					LIBTELNET_OPTION_WAIT;
				status = telnetSendCommand(telnet, TELNET_COMMAND_DO,
					telnet->options.remote.option[count].code);
				if (status < 0)
					break;
			}
		}

		telnet->sentRequests = 1;
	}

	status = telnet->writeData(telnet, telnet->outBuffer, bytes);

	return (status);
}


int telnetPing(telnetState *telnet)
{
	// Send a 'timing mark' request to probe whether the connection is still
	// alive.

	telnetOptionWait(&telnet->options.remote, TELNET_OPTION_TMARK);

	return (telnetSendCommand(telnet, TELNET_COMMAND_DO,
		TELNET_OPTION_TMARK));
}


void telnetFini(telnetState *telnet)
{
	// Does any required shutdown and deallocates things

	// Check params
	if (!telnet)
		return;

	if (telnet->inBuffer)
	{
		free(telnet->inBuffer);
		telnet->inBuffer = NULL;
	}

	if (telnet->outBuffer)
	{
		free(telnet->outBuffer);
		telnet->outBuffer = NULL;
	}
}

