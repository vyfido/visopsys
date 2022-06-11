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
//  netsniff.c
//

// This is a network packet sniffer

/* This is the text that appears when a user requests help about this program
<help>

 -- netsniff --

A network packet sniffer.

Usage:
  netsniff [-T] [device_name]

This command will display network traffic to/from a named network device,
if specified, otherwise all devices.

Options:
-T  : Force text mode operation

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <locale.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/font.h>
#include <sys/errors.h>
#include <sys/network.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/vis.h>
#include <sys/vsh.h>
#include <sys/window.h>

#define _(string) gettext(string)

#define WINDOW_TITLE	_("Packet Sniffer")

// Transport layer protocols that currently seem too obscure to be defined in
// <sys/network.h>.  Move them there as needed.
#define NETWORK_TRANSPROTOCOL_GGP			3
#define NETWORK_TRANSPROTOCOL_ST			5
#define NETWORK_TRANSPROTOCOL_EGP			8
#define NETWORK_TRANSPROTOCOL_IGP			9
#define NETWORK_TRANSPROTOCOL_NVP			11
#define NETWORK_TRANSPROTOCOL_HMP			20
#define NETWORK_TRANSPROTOCOL_ISOTP4		29
#define NETWORK_TRANSPROTOCOL_NETBLT		30
#define NETWORK_TRANSPROTOCOL_GSR			47
#define NETWORK_TRANSPROTOCOL_DSR			48
#define NETWORK_TRANSPROTOCOL_ESP			50
#define NETWORK_TRANSPROTOCOL_AH			51
#define NETWORK_TRANSPROTOCOL_NARP			54
#define NETWORK_TRANSPROTOCOL_IP6_ICMP		58
#define NETWORK_TRANSPROTOCOL_IP6_NONXT		59
#define NETWORK_TRANSPROTOCOL_IP6_OPTS		60
#define NETWORK_TRANSPROTOCOL_EIGRP			88
#define NETWORK_TRANSPROTOCOL_OSPFIGP		89
#define NETWORK_TRANSPROTOCOL_ETHERIP		97
#define NETWORK_TRANSPROTOCOL_ENCAP			98
#define NETWORK_TRANSPROTOCOL_PIM			103
#define NETWORK_TRANSPROTOCOL_IPCOMP		108
#define NETWORK_TRANSPROTOCOL_VRRP			112
#define NETWORK_TRANSPROTOCOL_L2TP			115
#define NETWORK_TRANSPROTOCOL_FC			133
#define NETWORK_TRANSPROTOCOL_RSVPE2EIG		134
#define NETWORK_TRANSPROTOCOL_MOBHDR		135
#define NETWORK_TRANSPROTOCOL_MPLSINIP		137
#define NETWORK_TRANSPROTOCOL_MANET			138
#define NETWORK_TRANSPROTOCOL_HIP			139
#define NETWORK_TRANSPROTOCOL_SHIM6			140
#define NETWORK_TRANSPROTOCOL_WESP			141
#define NETWORK_TRANSPROTOCOL_ROHC			142


typedef struct {
	char name[NETWORK_DEVICE_MAX_NAMELENGTH + 1];
	int linkProtocol;
	objectKey input;
	objectKey output;

} deviceHook;

typedef struct {
	deviceHook *hook;
	unsigned char *packet;
	unsigned packetLen;
	listItemParameters itemParams;

} packetInfo;

static int graphics = 0;
static deviceHook *hooks = NULL;
static int numHooks = 0;
static objectKey window = NULL;
static objectKey packetList = NULL;
static int guiThreadPid = 0;
linkedList packetInfoList;
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
		fprintf(stderr, "\n%s\n", output);
}


static void refreshWindow(void)
{
	// We got a 'window refresh' event (probably because of a language
	// switch), so we need to update things

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("netsniff");

	// Re-get the character set
	if (getenv(ENV_CHARSET))
		windowSetCharSet(window, getenv(ENV_CHARSET));

	// Refresh the window title
	windowSetTitle(window, WINDOW_TITLE);

	// Re-layout the window (not necessary if no components have changed)
	//windowLayout(window);
}


static packetInfo *findPacketInfo(int packetNumber)
{
	// Find a packetInfo based on the packet number

	packetInfo *info = NULL;
	linkedListItem *iter = NULL;

	info = linkedListIterStart(&packetInfoList, &iter);

	while (info && (packetNumber > 0))
	{
		info = linkedListIterNext(&packetInfoList, &iter);
		packetNumber -= 1;
	}

	if (!packetNumber)
		return (info);
	else
		return (NULL);
}


static unsigned packetLenEthernet(unsigned char *buffer)
{
	networkEthernetHeader *header = (networkEthernetHeader *) buffer;
	unsigned headerType = ntohs(header->type);
	networkIp4Header *ip4Header = (networkIp4Header *)(buffer +
		sizeof(networkEthernetHeader));
	networkIp6Header *ip6Header = (networkIp6Header *)(buffer +
		sizeof(networkEthernetHeader));

	// Check for IEEE 802.3
	if (headerType <= NETWORK_ETHERTYPE_IEEE802_3)
	{
		// It's the length field
		return (headerType);
	}

	switch (headerType)
	{
		case NETWORK_ETHERTYPE_IP4:
			return (sizeof(networkEthernetHeader) +
				ntohs(ip4Header->totalLength));

		case NETWORK_ETHERTYPE_ARP:
			return (sizeof(networkArpPacket));

		case NETWORK_ETHERTYPE_IP6:
			return (sizeof(networkEthernetHeader) + sizeof(networkIp6Header) +
				ntohs(ip6Header->payloadLen));

		default:
			fprintf(stderr, "%s %x\n", _("Unsupported ethernet packet type"),
				ntohs(headerType));
			return (0);
	}
}


static const char *ipProto2String(unsigned char protocol)
{
	switch (protocol)
	{
		case NETWORK_TRANSPROTOCOL_ICMP:
			return ("ICMP");

		case NETWORK_TRANSPROTOCOL_IGMP:
			return ("IGMP");

		case NETWORK_TRANSPROTOCOL_IP4ENCAP:
			return ("IPv4 encap");

		case NETWORK_TRANSPROTOCOL_TCP:
			return ("TCP");

		case NETWORK_TRANSPROTOCOL_UDP:
			return ("UDP");

		case NETWORK_TRANSPROTOCOL_RDP:
			return ("RDP");

		case NETWORK_TRANSPROTOCOL_IRTP:
			return ("IRTP");

		case NETWORK_TRANSPROTOCOL_DCCP:
			return ("DCCP");

		case NETWORK_TRANSPROTOCOL_IP6ENCAP:
			return ("IPv6 encap");

		case NETWORK_TRANSPROTOCOL_RSVP:
			return ("RSVP");

		case NETWORK_TRANSPROTOCOL_SCTP:
			return ("SCTP");

		case NETWORK_TRANSPROTOCOL_UDPLITE:
			return ("UDPLITE");

		default:
			return (_("unknown"));
	}
}


static void printIcmp(unsigned char *buffer)
{
	networkIcmpHeader *header = (networkIcmpHeader *) buffer;
	const char *type = NULL;
	networkPingPacket *ping = (networkPingPacket *) buffer;

	switch (header->type)
	{
		case NETWORK_ICMP_ECHOREPLY:
			type = "ECHOREPLY";
			break;

		case NETWORK_ICMP_DESTUNREACHABLE:
			type = "DESTUNREACHABLE";
			break;

		case NETWORK_ICMP_SOURCEQUENCH:
			type = "SOURCEQUENCH";
			break;

		case NETWORK_ICMP_REDIRECT:
			type = "REDIRECT";
			break;

		case NETWORK_ICMP_ECHO:
			type = "ECHO";
			break;

		case NETWORK_ICMP_TIMEEXCEEDED:
			type = "TIMEEXCEEDED";
			break;

		case NETWORK_ICMP_PARAMPROBLEM:
			type = "PARAMPROBLEM";
			break;

		case NETWORK_ICMP_TIMESTAMP:
			type = "TIMESTAMP";
			break;

		case NETWORK_ICMP_TIMESTAMPREPLY:
			type = "TIMESTAMPREPLY";
			break;

		case NETWORK_ICMP_INFOREQUEST:
			type = "INFOREQUEST";
			break;

		case NETWORK_ICMP_INFOREPLY:
			type = "INFOREPLY";
			break;

		default:
			type = _("unknown");
			break;
	}

	printf("ICMP %s (%d) code=%d ", type, header->type, header->code);

	switch (header->type)
	{
		case NETWORK_ICMP_ECHOREPLY:
		case NETWORK_ICMP_ECHO:
			printf("id=%d seq=%d", ntohs(ping->identifier),
				ntohs(ping->sequenceNum));
			break;

		default:
			break;
	}

	printf("\n");
}


static const char *ipPort2String(unsigned short port)
{
	switch (port)
	{
		case NETWORK_PORT_FTPDATA:
			return ("FTP data");

		case NETWORK_PORT_FTP:
			return ("FTP");

		case NETWORK_PORT_SSH:
			return ("SSH");

		case NETWORK_PORT_TELNET:
			return ("telnet");

		case NETWORK_PORT_SMTP:
			return ("SMTP");

		case NETWORK_PORT_DNS:
			return ("DNS");

		case NETWORK_PORT_BOOTPSERVER:
			return (_("DHCP server"));

		case NETWORK_PORT_BOOTPCLIENT:
			return (_("DHCP client"));

		case NETWORK_PORT_HTTP:
			return ("HTTP");

		case NETWORK_PORT_POP3:
			return ("POP3");

		case NETWORK_PORT_NTP:
			return ("NTP");

		case NETWORK_PORT_IMAP3:
			return ("IMAP");

		case NETWORK_PORT_LDAP:
			return ("LDAP");

		case NETWORK_PORT_HTTPS:
			return (_("secure HTTP"));

		case NETWORK_PORT_FTPSDATA:
			return (_("secure FTP data"));

		case NETWORK_PORT_FTPS:
			return (_("secure FTP"));

		case NETWORK_PORT_TELNETS:
			return (_("secure telnet"));

		case NETWORK_PORT_IMAPS:
			return (_("secure IMAP"));

		case NETWORK_PORT_POP3S:
			return (_("secure POP3"));

		default:
			return (NULL);
	}
}


static void printTcp(unsigned char *buffer, unsigned len)
{
	networkTcpHeader *header = (networkTcpHeader *) buffer;
	const char *srcPort = NULL;
	char srcPortString[80];
	const char *destPort = NULL;
	char destPortString[80];
	unsigned seq = 0;
	unsigned short flags = 0;
	unsigned dataLen = 0;

	srcPortString[0] = '\0';
	srcPort = ipPort2String(ntohs(header->srcPort));
	if (srcPort)
		snprintf(srcPortString, 80, " (%s)", srcPort);

	destPortString[0] = '\0';
	destPort = ipPort2String(ntohs(header->destPort));
	if (destPort)
		snprintf(destPortString, 80, " (%s)", destPort);

	seq = ntohl(header->sequenceNum);
	flags = ((header->dataOffsetFlags & 0x3F00) >> 8);
	dataLen = (len - sizeof(networkTcpHeader));

	printf("TCP srcPort=%u%s destPort=%u%s seq=%u-%u ack=%u\n"
		"   flags:%s%s%s%s%s%s win=%u len=%u dataLen=%u\n",
		ntohs(header->srcPort), srcPortString,
		ntohs(header->destPort), destPortString, seq,
		(dataLen? (seq + (dataLen - 1)) : seq), ntohl(header->ackNum),
		((flags & NETWORK_TCPFLAG_URG)? "URG," : ""),
		((flags & NETWORK_TCPFLAG_ACK)? "ACK," : ""),
		((flags & NETWORK_TCPFLAG_PSH)? "PSH," : ""),
		((flags & NETWORK_TCPFLAG_RST)? "RST," : ""),
		((flags & NETWORK_TCPFLAG_SYN)? "SYN," : ""),
		((flags & NETWORK_TCPFLAG_FIN)? "FIN" : ""), ntohs(header->window),
		len, dataLen);
}


static void printUdp(unsigned char *buffer)
{
	networkUdpHeader *header = (networkUdpHeader *) buffer;
	const char *srcPort = NULL;
	char srcPortString[80];
	const char *destPort = NULL;
	char destPortString[80];

	srcPortString[0] = '\0';
	srcPort = ipPort2String(ntohs(header->srcPort));
	if (srcPort)
		snprintf(srcPortString, 80, " (%s)", srcPort);

	destPortString[0] = '\0';
	destPort = ipPort2String(ntohs(header->destPort));
	if (destPort)
		snprintf(destPortString, 80, " (%s)", destPort);

	printf("UDP srcPort=%u%s destPort=%u%s len=%u\n", ntohs(header->srcPort),
		srcPortString, ntohs(header->destPort), destPortString,
		ntohs(header->length));
}


static void printIp4(unsigned char *buffer)
{
	networkIp4Header *header = (networkIp4Header *) buffer;
	unsigned char version = 0;
	char srcString[INET_ADDRSTRLEN];
	char destString[INET_ADDRSTRLEN];
	unsigned len = 0;

	version = (header->versionHeaderLen >> 4);
	inet_ntop(AF_INET, &header->srcAddress, srcString, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &header->destAddress, destString, INET_ADDRSTRLEN);
	len = ntohs(header->totalLength);

	printf("IPv%d proto=%s (%u) %s -> %s\n   tos=%u id=%u flags=%x "
		"ttl=%u len=%u\n", version, ipProto2String(header->protocol),
		header->protocol, srcString, destString, header->typeOfService,
		ntohs(header->identification), (ntohs(header->flagsFragOffset) >> 13),
		header->timeToLive, len);

	buffer += sizeof(networkIp4Header);
	len -= sizeof(networkIp4Header);

	// Print based on the network protocol
	switch (header->protocol)
	{
		case NETWORK_TRANSPROTOCOL_ICMP:
			printIcmp(buffer);
			break;

		case NETWORK_TRANSPROTOCOL_TCP:
			printTcp(buffer, len);
			break;

		case NETWORK_TRANSPROTOCOL_UDP:
			printUdp(buffer);
			break;
	}
}


static void printLoop(unsigned char *buffer, unsigned packetLen)
{
	// Assume IP for the time being
	printf("Loop type=IP len=%u\n", packetLen);
	printIp4(buffer);
}


static const char *ethernetType2String(unsigned short type)
{
	if (type <= NETWORK_ETHERTYPE_IEEE802_3)
		return ("802.3");

	switch (type)
	{
		case NETWORK_ETHERTYPE_IP4:
			return ("IPv4");

		case NETWORK_ETHERTYPE_ARP:
			return ("ARP");

		case NETWORK_ETHERTYPE_IP6:
			return ("IPv6");

		default:
			return (_("unknown"));
	}
}


static void printArp(unsigned char *buffer)
{
	networkArpHeader *arp = (networkArpHeader *) buffer;
	char srcString[INET_ADDRSTRLEN];
	char destString[INET_ADDRSTRLEN];
	unsigned short opCode = 0;

	inet_ntop(AF_INET, &arp->srcLogicalAddress, srcString, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &arp->destLogicalAddress, destString, INET_ADDRSTRLEN);
	opCode = ntohs(arp->opCode);

	switch (opCode)
	{
		case NETWORK_ARPOP_REQUEST:
			printf("ARP %02x:%02x:%02x:%02x:%02x:%02x (%s) %s %s\n",
				arp->srcHardwareAddress[0], arp->srcHardwareAddress[1],
				arp->srcHardwareAddress[2], arp->srcHardwareAddress[3],
				arp->srcHardwareAddress[4], arp->srcHardwareAddress[5],
				srcString, _("request"), destString);
			break;

		case NETWORK_ARPOP_REPLY:
			printf("ARP %s %s is %02x:%02x:%02x:%02x:%02x:%02x\n",
				_("reply"), srcString, arp->srcHardwareAddress[0],
				arp->srcHardwareAddress[1], arp->srcHardwareAddress[2],
				arp->srcHardwareAddress[3], arp->srcHardwareAddress[4],
				arp->srcHardwareAddress[5]);
			break;
	}
}


static void printIp6(unsigned char *buffer)
{
	networkIp6Header *header = (networkIp6Header *) buffer;
	unsigned char version = 0;
	char srcString[INET6_ADDRSTRLEN];
	char destString[INET6_ADDRSTRLEN];
	unsigned len = 0;

	version = (header->versionClassLo >> 4);
	inet_ntop(AF_INET6, &header->srcAddress, srcString, INET6_ADDRSTRLEN);
	inet_ntop(AF_INET6, &header->destAddress, destString, INET6_ADDRSTRLEN);
	len = (sizeof(networkIp6Header) + ntohs(header->payloadLen));

	printf("IPv%d nexthdr=%s (%u) %s ->\n   %s hops=%u len=%u\n", version,
		ipProto2String(header->nextHeader), header->nextHeader, srcString,
		destString, header->hopLimit, len);

	buffer += sizeof(networkIp6Header);
	len -= sizeof(networkIp6Header);

	switch (header->nextHeader)
	{
		case NETWORK_TRANSPROTOCOL_ICMP:
			printIcmp(buffer);
			break;

		case NETWORK_TRANSPROTOCOL_TCP:
			printTcp(buffer, len);
			break;

		case NETWORK_TRANSPROTOCOL_UDP:
			printUdp(buffer);
			break;
	}
}


static void printEthernet(unsigned char *buffer, unsigned packetLen)
{
	networkEthernetHeader *header = (networkEthernetHeader *) buffer;
	unsigned short type = 0;

	type = ntohs(header->type);

	printf("Ethernet %s=%s (%x) %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:"
		"%02x:%02x:%02x:%02x len=%u\n", _("type"), ethernetType2String(type),
		type, header->source[0], header->source[1], header->source[2],
		header->source[3], header->source[4], header->source[5],
		header->dest[0], header->dest[1], header->dest[2],
		header->dest[3], header->dest[4], header->dest[5], packetLen);

	// Check for IEEE 802.3
	if (type <= NETWORK_ETHERTYPE_IEEE802_3)
		return;

	buffer += sizeof(networkEthernetHeader);

	// Print based on the network protocol
	switch (type)
	{
		case NETWORK_ETHERTYPE_IP4:
			printIp4(buffer);
			break;

		case NETWORK_ETHERTYPE_ARP:
			printArp(buffer);
			break;

		case NETWORK_ETHERTYPE_IP6:
			printIp6(buffer);
			break;
	}
}


static void print(deviceHook *hook, unsigned char *packet, unsigned packetLen)
{
	// Determine the link-level packet length
	switch (hook->linkProtocol)
	{
		case NETWORK_LINKPROTOCOL_ETHERNET:
			packetLen = packetLenEthernet(packet);
			break;

		default:
			break;
	}

	// If we're confused, skip it
	if (!packetLen)
		return;

	// Print based on the link protocol
	switch (hook->linkProtocol)
	{
		case NETWORK_LINKPROTOCOL_LOOP:
			printLoop(packet, packetLen);
			break;

		case NETWORK_LINKPROTOCOL_ETHERNET:
			printEthernet(packet, packetLen);
			break;

		default:
			break;
	}

	printf("\n");
}


static void printHex(unsigned char *packet, unsigned packetLen)
{
	unsigned offset = 0;
	int printBytes = 0;
	char lineBuff[160];
	int count;

	while (offset < packetLen)
	{
		sprintf(lineBuff, "%08x  ", offset);

		printBytes = min(16, (packetLen - offset));

		for (count = 0; count < 16; count ++)
		{
			if (count < printBytes)
			{
				sprintf((lineBuff + strlen(lineBuff)), "%02x ",
					packet[offset + count]);
			}
			else
			{
				strcat(lineBuff, "   ");
			}

			if ((count == 7) || (count == 15))
				strcat(lineBuff, " ");
		}

		strcat(lineBuff, "|");
		for (count = 0; count < 16; count ++)
		{
			if ((count < printBytes) && (packet[offset + count] >= 32) &&
				(packet[offset + count] <= 126))
			{
				sprintf((lineBuff + strlen(lineBuff)), "%c",
					packet[offset + count]);
			}
			else
			{
				strcat(lineBuff, ".");
			}
		}
		strcat(lineBuff, "|");

		printf("%s\n", lineBuff);
		offset += printBytes;
	}

	printf("\n");
}


static void eventHandler(objectKey key, windowEvent *event)
{
	int packetNumber = 0;
	packetInfo *info = NULL;

	// Check for window events
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

	else if ((key == packetList) && (event->type & WINDOW_EVENT_SELECTION) &&
		((event->type & WINDOW_EVENT_MOUSE_DOWN) ||
		(event->type & WINDOW_EVENT_KEY_DOWN)))
	{
		windowComponentGetSelected(packetList, &packetNumber);
		if (packetNumber < 0)
			return;

		info = findPacketInfo(packetNumber);
		if (!info)
			return;

		print(info->hook, info->packet, info->packetLen);
		printHex(info->packet, info->packetLen);
	}
}


static int constructWindow(void)
{
	// If we are in graphics mode, make a window rather than operating on the
	// command line

	int status = 0;
	objectKey textArea = NULL;
	componentParameters params;
	listItemParameters dummyParams;

	// Create a new window
	window = windowNew(multitaskerGetCurrentProcessId(), WINDOW_TITLE);
	if (!window)
		return (status = ERR_NOCREATE);

	// Create the list of packets
	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 5;
	params.padTop = 5;
	params.padBottom = 5;
	params.orientationX = orient_center;
	params.orientationY = orient_middle;
	memset(&dummyParams, 0, sizeof(listItemParameters));
	memset(dummyParams.text, ' ', WINDOW_MAX_LABEL_LENGTH);
	packetList = windowNewList(window, windowlist_textonly, 20 /* rows */,
		1 /* columns */, 0 /* selectMultiple */, &dummyParams,
		1 /* numItems */, &params);
	windowRegisterEventHandler(packetList, &eventHandler);
	windowComponentFocus(packetList);

	// Put a text area in the window
	params.gridX += 1;
	params.padRight = 5;
	params.flags |= COMP_PARAMS_FLAG_STICKYFOCUS;
	params.font = fontGet(FONT_FAMILY_LIBMONO, FONT_STYLEFLAG_FIXED, 8, NULL);

	textArea = windowNewTextArea(window, 80 /* columns */, 50 /* rows */,
		200 /* bufferLines */, &params);

	// Use the text area for all our input and output
	windowSetTextOutput(textArea);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);

	// Go live
	windowSetVisible(window, 1);

	return (status = 0);
}


static void interrupt(int sig)
{
	// This is our interrupt signal handler
	if (sig == SIGINT)
		stop = 1;
}


static void sniffStream(deviceHook *hook, int input)
{
	int status = 0;
	unsigned packetLen = 0;
	unsigned char packet[NETWORK_PACKET_MAX_LENGTH];
	packetInfo *info = NULL;
	struct tm currTime;

	packetLen = networkDeviceSniff((input? hook->input : hook->output),
		packet, sizeof(packet));

	if (packetLen)
	{
		if (graphics)
		{
			// Get memory for our packet info structure
			info = calloc(1, sizeof(packetInfo));
			if (!info)
			{
				perror("calloc");
				return;
			}

			// Get memory for the packet data
			info->packet = calloc(packetLen, 1);
			if (!info->packet)
			{
				perror("calloc");
				free(info);
				return;
			}

			// Copy the packet data and info
			info->hook = hook;
			memcpy(info->packet, packet, packetLen);
			info->packetLen = packetLen;

			// Set up the list item parameters and append it to the list
			// component

			if (rtcDateTime(&currTime) >= 0)
			{
				vshPrintTime(info->itemParams.text, &currTime);
				strcat(info->itemParams.text, ": ");
			}

			sprintf((info->itemParams.text + strlen(info->itemParams.text)),
				"%s: %s: %d %s", hook->name, (input? _("input") :
				_("output")), info->packetLen, _("bytes"));

			if (!packetInfoList.numItems)
			{
				status = windowComponentSetData(packetList, &info->itemParams,
					1 /* size */, 1 /* render */);
			}
			else
			{
				status = windowComponentAppendData(packetList,
					&info->itemParams, 1 /* size */, 1 /* render */);
			}
			if (status < 0)
			{
				errno = status;
				perror("windowComponentAppendData");
				free(info->packet);
				free(info);
				return;
			}

			// Add it to the linked list
			status = linkedListAddBack(&packetInfoList, info);
			if (status < 0)
			{
				errno = status;
				perror("linkedListAddBack");
				free(info->packet);
				free(info);
				return;
			}
		}
		else
		{
			if (input)
				printf("%s ", _("Input"));
			else
				printf("%s ", _("Output"));

			print(hook, packet, packetLen);
		}
	}
}


static void sniff(void)
{
	int count;

	// Loop until we're told to stop
	while (!stop && (!graphics || multitaskerProcessIsAlive(guiThreadPid)))
	{
		for (count = 0; count < numHooks; count ++)
		{
			if (hooks[count].input)
				sniffStream(&hooks[count], 1 /* input */);

			if (hooks[count].output)
				sniffStream(&hooks[count], 0 /* output */);
		}

		// Give back the rest of this timeslice
		multitaskerYield();
	}
}


static void clearPacketInfoList(void)
{
	packetInfo *info = NULL;
	linkedListItem *iter = NULL;

	info = linkedListIterStart(&packetInfoList, &iter);

	while (info)
	{
		if (info->packet)
			free(info->packet);

		free(info);

		info = linkedListIterNext(&packetInfoList, &iter);
	}

	linkedListClear(&packetInfoList);
}


int main(int argc, char *argv[])
{
	int status = 0;
	char opt;
	char *deviceName = NULL;
	networkDevice dev;
	int hookedDevices = 0, hookedInput = 0, hookedOutput = 0;
	int count;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("netsniff");

	memset(&packetInfoList, 0, sizeof(packetInfoList));

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

	// Is the last argument a non-option?
	if ((argc > 1) && (argv[argc - 1][0] != '-'))
	{
		// The user requested a specific device
		deviceName = argv[argc - 1];
		numHooks = 1;
	}
	else
	{
		// The user didn't request a specific device, so hook them all
		numHooks = networkDeviceGetCount();
		if (numHooks <= 0)
		{
			error("%s", _("Can't get the count of network devices"));
			return (numHooks);
		}
	}

	hooks = calloc(numHooks, sizeof(deviceHook));
	if (!hooks)
	{
		error("%s", _("Couldn't allocate memory"));
		return (status = ERR_MEMORY);
	}

	// Set up the names
	if (deviceName)
	{
		strncpy(hooks[0].name, deviceName, NETWORK_DEVICE_MAX_NAMELENGTH);
	}
	else
	{
		for (count = 0; count < numHooks; count ++)
			sprintf(hooks[count].name, "net%d", count);
	}

	// Get the link protocols of each device
	for (count = 0; count < numHooks; count ++)
	{
		// Assume ethernet to start with
		hooks[count].linkProtocol = NETWORK_LINKPROTOCOL_ETHERNET;

		status = networkDeviceGet(hooks[count].name, &dev);
		if (status < 0)
		{
			error(_("Can't get info for device %s"), hooks[count].name);
			hooks[count].name[0] = '\0';
			continue;
		}

		hooks[count].linkProtocol = dev.linkProtocol;
		hookedDevices += 1;
	}

	// Hook the devices
	for (count = 0; count < numHooks; count ++)
	{
		if (hooks[count].name[0])
		{
			status = networkDeviceHook(hooks[count].name,
				&hooks[count].input, 1 /* input */);
			if (status < 0)
				hooks[count].input = NULL;
			else
				hookedInput += 1;

			status = networkDeviceHook(hooks[count].name,
				&hooks[count].output, 0 /* output */);
			if (status < 0)
				hooks[count].output = NULL;
			else
				hookedOutput += 1;
		}
	}

	if (!hookedInput && !hookedOutput)
	{
		// Nothing to do
		status = 0;
		goto out;
	}

	if (graphics)
	{
		// Make our window
		status = constructWindow();
		if (status < 0)
			goto out;

		// Run the GUI
		guiThreadPid = windowGuiThread();
	}
	else
	{
		printf(_("Hooked %d devices, %d input streams, %d output streams\n"),
			hookedDevices, hookedInput, hookedOutput);

		// Set up the signal handler for catching CTRL-C interrupt
		if (signal(SIGINT, &interrupt) == SIG_ERR)
		{
			error("%s", _("Error setting signal handler"));
			status = ERR_NOTINITIALIZED;
			goto out;
		}
	}

	// This is our main loop
	sniff();

	status = 0;

out:
	if (graphics)
	{
		clearPacketInfoList();
	}
	else
	{
		// To terminate the signal handler
		signal(0, SIG_DFL);
	}

	// Unhook the devices
	for (count = 0; count < numHooks; count ++)
	{
		if (hooks[count].input)
		{
			networkDeviceUnhook(hooks[count].name, hooks[count].input,
				1 /* input */);
		}
		if (hooks[count].output)
		{
			networkDeviceUnhook(hooks[count].name, hooks[count].output,
				0 /* output */);
		}
	}

	free(hooks);
	return (status);
}

