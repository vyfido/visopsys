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
//  telnet.h
//

// This file contains definitions and structures for using the telnet protocol
// and libtelnet library in Visopsys.

#ifndef _TELNET_H
#define _TELNET_H

// Telnet commands
#define TELNET_COMMAND_SE		240	// End of subnegotiation parameters.
#define TELNET_COMMAND_NOP		241	// No operation
#define TELNET_COMMAND_DM		242	// Data mark
#define TELNET_COMMAND_BRK		243	// Break
#define TELNET_COMMAND_IP		244	// Suspend, interrupt or abort the process
#define TELNET_COMMAND_AO		245	// Abort output
#define TELNET_COMMAND_AYT		246	// Are you there
#define TELNET_COMMAND_EC		247	// Erase character
#define TELNET_COMMAND_EL		248	// Erase line
#define TELNET_COMMAND_GA		249	// Go ahead
#define TELNET_COMMAND_SB		250	// Subnegotiation of the indicated option
#define TELNET_COMMAND_WILL		251	// Indicates the desire to begin performing,
									// or confirmation that you are now
									// performing, the indicated option.
#define TELNET_COMMAND_WONT		252	// Indicates the refusal to perform, or
									// continue performing, the indicated option
#define TELNET_COMMAND_DO		253	// Indicates the request that the other
									// party perform, or confirmation that you
									// are expecting the other party to perform,
									// the indicated option.
#define TELNET_COMMAND_DONT		254	// Indicates the demand that the other party
									// stop performing, or confirmation that you
									// are no longer expecting the other party
									// to perform, the indicated option.
#define TELNET_COMMAND_IAC		255	// Interpret as command

// Telnet option codes
#define TELNET_OPTION_BINTRANS	0	// binary transmission: RFC 856
#define TELNET_OPTION_ECHO		1	// echo: RFC 857
#define TELNET_OPTION_RECONCT	2	// reconnection
#define TELNET_OPTION_SUPGA		3	// suppress go ahead: RFC 858
#define TELNET_OPTION_APPMSN	4	// approx message size negotiation
#define TELNET_OPTION_STATUS	5	// status: RFC 859
#define TELNET_OPTION_TMARK		6	// timing mark: RFC 860
#define TELNET_OPTION_RCTRECH	7	// remote controlled trans/echo: RFC 726
#define TELNET_OPTION_OLINEW	8	// output line width
#define TELNET_OPTION_OPGSIZE	9	// output page size
#define TELNET_OPTION_OCRDISP	10	// output CR disposition: RFC 652
#define TELNET_OPTION_OHTSTOPS	11	// output HT stops: RFC 653
#define TELNET_OPTION_OHTDISP	12	// output HT disposition: RFC 654
#define TELNET_OPTION_OFFDISP	13	// output FF disposition: RFC 655
#define TELNET_OPTION_OVTSTOPS	14	// output VT stops: RFC 656
#define TELNET_OPTION_OVTDISP	15	// output VT disposition: RFC 657
#define TELNET_OPTION_OLFDISP	16	// output LF disposition: RFC 658
#define TELNET_OPTION_EXTASCII	17	// extended ASCII: RFC 698
#define TELNET_OPTION_LOGOUT	18	// logout: RFC 727
#define TELNET_OPTION_BYTEMACRO	19	// byte macro: RFC 735
#define TELNET_OPTION_DATAETERM	20	// data entry terminal: RFC 1043, 732
#define TELNET_OPTION_SUPDUP	21	// SUPDUP: RFC 736, 734
#define TELNET_OPTION_SUPDUPO	22	// SUPDUP output:  RFC 749
#define TELNET_OPTION_SENDLOC	23	// send location: RFC 779
#define TELNET_OPTION_TTYPE		24	// terminal type: RFC 1091
#define TELNET_OPTION_EOR		25	// end of record:  RFC 885
#define TELNET_OPTION_TACACSUID	26	// TACACS user identification: RFC 927
#define TELNET_OPTION_OMARKING	27	// output marking: RFC 933
#define TELNET_OPTION_TLOCNUM	28	// terminal location number: RFC 946
#define TELNET_OPTION_TNET3270	29	// telnet 3270 regime: RFC 1041
#define TELNET_OPTION_X3PAD		30	// X.3 PAD: RFC 1053
#define TELNET_OPTION_WINSZ		31	// window size: RFC 1073
#define TELNET_OPTION_TSPEED	32	// terminal speed: RFC 1079
#define TELNET_OPTION_REMFC		33	// remote flow control: RFC 1372
#define TELNET_OPTION_LMODE		34	// linemode: RFC 1184
#define TELNET_OPTION_XDISPLOC	35	// X display location: RFC 1096
#define TELNET_OPTION_ENVAR		36	// environment variables: RFC 1408
#define TELNET_OPTION_AUTHOPT	37	// authentication option: RFC 2941
#define TELNET_OPTION_ENCOPT	38	// encryption option: RFC 2946
#define TELNET_OPTION_ENVOPT	39	// environment option: RFC 1572
#define TELNET_OPTION_TN3270E	40	// TN3270E: RFC 2355
#define TELNET_OPTION_XAUTH		41	// XAUTH
#define TELNET_OPTION_CHARSET	42	// CHARSET: RFC 2066
#define TELNET_OPTION_TELRSP	43	// telnet remote serial port (RSP)
#define TELNET_OPTION_COMPCTRL	44	// com port control option: RFC 2217
#define TELNET_OPTION_TELSLE	45	// telnet suppress local echo
#define TELNET_OPTION_TELSTLS	46	// telnet start TLS
#define TELNET_OPTION_KERMIT	47	// KERMIT: RFC 2840
#define TELNET_OPTION_SENDURL	48	// SEND-URL
#define TELNET_OPTION_FORWARDX	49	// FORWARD_X
#define TELNET_OPTION_PRAGMALOG	138	// TELOPT PRAGMA LOGON
#define TELNET_OPTION_SSPILOGON	139	// TELOPT SSPI LOGON
#define TELNET_OPTION_PRAGHEART	140	// TELOPT PRAGMA HEARTBEAT

// Some telnet terminal type names
#define TELNET_TTYPE_MAXLEN		40 // max 40 chars per RFC 1091
#define TELNET_TTYPE_NVT		"NETWORK-VIRTUAL-TERMINAL"
#define TELNET_TTYPE_VT52		"DEC-VT52"
#define TELNET_TTYPE_VT100		"DEC-VT100"
#define TELNET_TTYPE_VT220		"DEC-VT220"
#define TELNET_TTYPE_UNKNOWN	"UNKNOWN"

// Option flags for libtelnet
#define LIBTELNET_OPTION_WAIT	2
#define LIBTELNET_OPTION_SET	1

// Generic struct for describing an option
typedef struct {
	unsigned char code;
	unsigned char flags;
	void *value;

} telnetOption;

// The current number of supported options in libtelnet
#define LIBTELNET_NUM_SUPP_OPTS	5

// An array of supported options for a host
typedef struct {
	char terminalType[TELNET_TTYPE_MAXLEN + 1];
	telnetOption option[LIBTELNET_NUM_SUPP_OPTS];

} telnetHostOptions;

// Arrays of supported options for both ends of a connection
typedef struct {
	telnetHostOptions local;
	telnetHostOptions remote;

} telnetOptions;

// Everything needed to maintain a Telnet connection
typedef struct _telnetState {
	int sockFd;
	int sentRequests;
	int sentGoAhead;
	unsigned lastContact;
	int alive;
	int localEcho;
	unsigned char *inBuffer;
	unsigned char *outBuffer;
	telnetOptions options;

	// Callback function pointers for network I/O
	int (*readData)(struct _telnetState *, unsigned char *, unsigned);
	int (*writeData)(struct _telnetState *, unsigned char *, unsigned);

	// Callback function pointers for terminal control

} telnetState;

// Functions exported from the libtelnet library
int telnetInit(int, telnetState *);
const char *telnetCommandString(unsigned char);
const char *telnetOptionString(unsigned char);
int telnetOptionIsSet(telnetHostOptions *, unsigned char);
int telnetOptionWait(telnetHostOptions *, unsigned char);
int telnetOptionIsWaiting(telnetHostOptions *, unsigned char);
int telnetOptionGet(telnetHostOptions *, unsigned char);
int telnetOptionSet(telnetHostOptions *, unsigned char, int);
int telnetSendCommand(telnetState *, unsigned char, unsigned char);
int telnetSendResponse(telnetState *, unsigned char, unsigned char, int);
int telnetProcessCommand(telnetState *, unsigned char *, unsigned);
int telnetReadData(telnetState *);
int telnetWriteData(telnetState *, unsigned);
int telnetPing(telnetState *);
void telnetFini(telnetState *);

#endif

