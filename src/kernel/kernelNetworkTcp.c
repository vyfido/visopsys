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
//  kernelNetworkTcp.c
//

#include "kernelNetworkTcp.h"
#include "kernelCpu.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelLock.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelNetwork.h"
#include "kernelRandom.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#define SEQ_SEND(connection, num) \
	(connection->tcp.sendInit? (num - connection->tcp.sendInit) : 0)
#define SEQ_RECV(connection, num) \
	(connection->tcp.recvInit? (num - connection->tcp.recvInit) : 0)


static void changeTcpState(kernelNetworkConnection *connection,
	networkTcpState state)
{
#if defined(DEBUG)
	const char *stateNames[11] = {
		"tcp_closed", "tcp_listen", "tcp_syn_sent", "tcp_syn_received",
		"tcp_established", "tcp_close_wait", "tcp_last_ack", "tcp_fin_wait1",
		"tcp_closing", "tcp_fin_wait2", "tcp_time_wait"
	};
	const char *stateName = NULL;

	switch (state)
	{
		case tcp_closed:
		case tcp_listen:
		case tcp_syn_sent:
		case tcp_syn_received:
		case tcp_established:
		case tcp_close_wait:
		case tcp_last_ack:
		case tcp_fin_wait1:
		case tcp_closing:
		case tcp_fin_wait2:
		case tcp_time_wait:
			stateName = stateNames[state];
			break;
		default:
			stateName = "(unknown)";
			break;
	}

	kernelDebug(debug_net, "TCP state=%s", stateName);
#endif

	connection->tcp.state = state;
}


static void addAck(kernelNetworkConnection *connection,
	kernelNetworkPacket *packet, unsigned ackNum)
{
	// Adds the specified ack to a packet

	networkTcpHeader *tcpHeader = NULL;
	unsigned flags = 0;

	tcpHeader = (networkTcpHeader *)(packet->memory +
			packet->transHeaderOffset);
	flags = networkGetTcpHdrFlags(tcpHeader);

	flags |= NETWORK_TCPFLAG_ACK;

	networkSetTcpHdrFlags(tcpHeader, flags);

	tcpHeader->ackNum = htonl(ackNum);

	// Update the last ack sent value
	connection->tcp.recvAcked = ackNum;
}


static int sendEmpty(kernelNetworkConnection *connection,
	unsigned short flags, unsigned ackNum)
{
	// Send an empty packet with given flags and ack number

	int status = 0;
	kernelNetworkPacket *packet = NULL;
	networkTcpHeader *tcpHeader = NULL;

	packet = kernelNetworkPacketGet();
	if (!packet)
		return (status = ERR_MEMORY);

	status = kernelNetworkSetupSendPacket(connection, packet);
	if (status < 0)
	{
		kernelNetworkPacketRelease(packet);
		return (status);
	}

	// No data in the packet
	packet->length = packet->dataOffset;
	packet->dataLength = 0;

	tcpHeader = (networkTcpHeader *)(packet->memory +
		packet->transHeaderOffset);
	networkSetTcpHdrFlags(tcpHeader, flags);

	if (flags & NETWORK_TCPFLAG_ACK)
		addAck(connection, packet, ackNum);

	kernelNetworkFinalizeSendPacket(connection, packet,
		0 /* not a re-transmit */, 0 /* not 'last packet' */);

	kernelDebug(debug_net, "TCP send empty packet %u ",
		SEQ_SEND(connection, ntohl(tcpHeader->sequenceNum)));

	status = kernelNetworkSendPacket(connection->netDev, packet,
		0 /* not immediate, can queue */);

	return (status);
}


static void purgeRetransQueue(kernelNetworkConnection *connection)
{
	// Eliminate anything that remains in the TCP re-transmission queue

	connection->tcp.retransQueueLen = 0;
	memset((void *) connection->tcp.retransQueue, 0,
		(NETWORK_TCP_MAX_RETRANSQUEUE * sizeof(kernelNetworkTcpQueuePacket)));
}


static void purgeWaitQueue(kernelNetworkConnection *connection)
{
	// Eliminate anything that remains in the TCP wait queue

	connection->tcp.waitQueueLen = 0;
	memset((void *) connection->tcp.waitQueue, 0,
		(NETWORK_TCP_MAX_WAITQUEUE * sizeof(kernelNetworkTcpQueuePacket)));
}


static unsigned short tcpChecksum(unsigned checksum,
	networkTcpHeader *tcpHeader, unsigned srcAddress, unsigned destAddress,
	unsigned char protocol, unsigned short tcpLength)
{
	// Calculate the TCP checksum for the supplied packet.  This is done
	// as a 1's complement sum of:
	//
	// "the 16 bit one's complement of the one's complement sum of all 16
	// bit words in the header and text.  If a segment contains an odd number
	// of header and text octets to be checksummed, the last octet is padded
	// on the right with zeros to form a 16 bit word for checksum purposes.
	// The pad is not transmitted as part of the segment.  While computing
	// the checksum, the checksum field itself is replaced with zeros.
	//
	// The checksum also covers a 96 bit pseudo header conceptually
	// prefixed to the TCP header:
	//
	//		 0      7 8     15 16    23 24    31
	//		+--------+--------+--------+--------+
	//		|           Source Address          |
	//		+--------+--------+--------+--------+
	//		|         Destination Address       |
	//		+--------+--------+--------+--------+
	//		|  zero  |  PTCL  |    TCP Length   |
	//		+--------+--------+--------+--------+
	//
	// The TCP Length is the TCP header length plus the data length in
	// octets (this is not an explicitly transmitted quantity, but is
	// computed), and it does not count the 12 octets of the pseudo header."

	unsigned words = 0;
	unsigned short *wordPtr = NULL;
	unsigned count;

	struct {
		unsigned srcAddress;
		unsigned destAddress;
		unsigned char zero;
		unsigned char protocol;
		unsigned short tcpLength;

	} __attribute__((packed)) pseudoHeader = {
		srcAddress, destAddress, 0, protocol, htons(tcpLength)
	};

	kernelDebug(debug_net, "TCP checksum src=%08x dest=%08x proto=%u len=%u",
		srcAddress, destAddress, protocol, tcpLength);

	// First the pseudo-header
	words = (sizeof(pseudoHeader) >> 1);
	wordPtr = (unsigned short *) &pseudoHeader;
	for (count = 0; count < words; count ++)
		checksum += ntohs(wordPtr[count]);

	// The TCP header and data
	words = (tcpLength >> 1);
	wordPtr = (unsigned short *) tcpHeader;
	for (count = 0; count < words; count ++)
	{
		if (count != 8)
		{
			// Skip the checksum field itself
			checksum += ntohs(wordPtr[count]);
		}
	}

	if (tcpLength & 1)
		checksum += ntohs(wordPtr[words] & 0x00FF);

	checksum = ~((checksum & 0xFFFF) + (checksum >> 16));

	kernelDebug(debug_net, "TCP checksum %04x", (unsigned short) checksum);

	return ((unsigned short) checksum);
}


#ifdef DEBUG
static void debugPacket(kernelNetworkConnection *connection,
	kernelNetworkPacket *packet, int received)
{
	networkIp4Header *ip4Header = NULL;
	networkTcpHeader *tcpHeader = NULL;
	unsigned short flags = 0, window = 0;
	unsigned sequenceNum = 0, ackNum = 0;

	ip4Header = (networkIp4Header *)(packet->memory +
		packet->netHeaderOffset);
	tcpHeader = (networkTcpHeader *)(packet->memory +
		packet->transHeaderOffset);

	flags = networkGetTcpHdrFlags(tcpHeader);

	sequenceNum = ntohl(tcpHeader->sequenceNum);
	if (connection)
	{
		sequenceNum = (received? SEQ_RECV(connection, sequenceNum) :
			SEQ_SEND(connection, sequenceNum));
	}

	ackNum = ntohl(tcpHeader->ackNum);
	if (connection)
	{
		ackNum = (received? SEQ_SEND(connection, ackNum) :
			SEQ_RECV(connection, ackNum));
	}

	window = ntohs(tcpHeader->window);

	kernelDebug(debug_net, "TCP packet from %u.%u.%u.%u:%u to "
		"%u.%u.%u.%u:%u flags:%s%s%s%s%s%s", (ip4Header->srcAddress & 0xFF),
		((ip4Header->srcAddress >> 8) & 0xFF),
		((ip4Header->srcAddress >> 16) & 0xFF), (ip4Header->srcAddress >> 24),
		ntohs(tcpHeader->srcPort), (ip4Header->destAddress & 0xFF),
		((ip4Header->destAddress >> 8) & 0xFF),
		((ip4Header->destAddress >> 16) & 0xFF),
		(ip4Header->destAddress >> 24), ntohs(tcpHeader->destPort),
		((flags & NETWORK_TCPFLAG_URG)? "URG," : ""),
		((flags & NETWORK_TCPFLAG_ACK)? "ACK," : ""),
		((flags & NETWORK_TCPFLAG_PSH)? "PSH," : ""),
		((flags & NETWORK_TCPFLAG_RST)? "RST," : ""),
		((flags & NETWORK_TCPFLAG_SYN)? "SYN," : ""),
		((flags & NETWORK_TCPFLAG_FIN)? "FIN" : ""));

	if (flags & NETWORK_TCPFLAG_ACK)
	{
		kernelDebug(debug_net, "TCP packet seq %u-%u with ACK %u",
			sequenceNum, (packet->dataLength? (sequenceNum +
			(packet->dataLength - 1)) : sequenceNum), ackNum);
	}
	else
	{
		kernelDebug(debug_net, "TCP packet seq %u-%u without ACK",
			sequenceNum, (packet->dataLength? (sequenceNum +
			(packet->dataLength - 1)) : sequenceNum));
	}

	kernelDebug(debug_net, "TCP packet length=%u dataLength=%u window=%u",
		ntohs(ip4Header->totalLength), packet->dataLength, window);
}
#else
	#define debugPacket(connection, packet, received) do {} while (0)
#endif


static int checkFlags(kernelNetworkConnection *connection,
	unsigned short flags, unsigned sequenceNum __attribute__((unused)),
	unsigned ackNum)
{
	// Checks the validity of the flags in the packet, given the current
	// state of the connection

	int status = 0;

	// Does this packet contain a SYN?
	if (flags & NETWORK_TCPFLAG_SYN)
	{
		// Ensure that the SYN is expected
		if (connection->tcp.state >= tcp_syn_received)
		{
			// Reset?
			kernelDebugError("Unexpected SYN in packet %u",
				SEQ_RECV(connection, sequenceNum));
			return (status = ERR_INVALID);
		}
	}

	// Does this packet contain an ACK?
	if (flags & NETWORK_TCPFLAG_ACK)
	{
		// Ensure that the ACK is within the expected range
		if (ackNum < connection->tcp.sendUnAcked)
		{
			kernelDebugError("Duplicate ACK %u in packet %u",
				SEQ_SEND(connection, ackNum), SEQ_RECV(connection,
				sequenceNum));
		}
		else if ((ackNum < connection->tcp.sendUnAcked) ||
			(ackNum > connection->tcp.sendNext))
		{
			// Reset?
			kernelDebugError("Invalid ACK %u in packet %u",
				SEQ_SEND(connection, ackNum), SEQ_RECV(connection,
				sequenceNum));
			return (status = ERR_INVALID);
		}
	}
	else if (connection->tcp.state >= tcp_syn_sent)
	{
		// We are expecting an ACK in this state.  Reset?
		kernelDebugError("Missing ACK from packet %u", SEQ_RECV(connection,
			sequenceNum));
		return (status = ERR_INVALID);
	}

	// Does this packet contain a FIN?
	if (flags & NETWORK_TCPFLAG_FIN)
	{
		// Ensure that the FIN is expected
		if (connection->tcp.state < tcp_established)
		{
			// Reset?
			kernelDebugError("Unexpected FIN in packet %u",
				SEQ_RECV(connection, sequenceNum));
			return (status = ERR_INVALID);
		}
	}

	return (status = 0);
}


static void removeRetrans(kernelNetworkConnection *connection,
	unsigned ackNum)
{
	// Removed any ACKed packets from the TCP re-transmission queue

	kernelNetworkTcpQueuePacket *retrans = NULL;
	int count1, count2;

	for (count1 = 0; count1 < connection->tcp.retransQueueLen; count1 ++)
	{
		retrans = (kernelNetworkTcpQueuePacket *)
			&connection->tcp.retransQueue[count1];

		if ((retrans->sequence + retrans->dataLen) <= ackNum)
		{
			kernelDebug(debug_net, "TCP removing %u-%u from re-transmission "
				"queue", SEQ_SEND(connection, retrans->sequence),
				(SEQ_SEND(connection, retrans->sequence) +
					(retrans->dataLen - 1)));

			if (!retrans->reTransmitted)
			{
				// Calculate the running round-trip time with a = 0.5 (medium
				// smoothness):
				//	new RTT = (a * old RTT) + ((1 - a) * newest measurement)
				connection->tcp.roundTripTime =
					((connection->tcp.roundTripTime >> 1) +
					((kernelCpuGetMs() - retrans->packet->timeSent) >> 1));

				// Reset the back-off timer
				connection->tcp.backoff = 0;
			}

			kernelDebug(debug_net, "TCP round trip time now %u, backoff %u",
				connection->tcp.roundTripTime, connection->tcp.backoff);

			kernelNetworkPacketRelease(retrans->packet);

			if (count1 < (connection->tcp.retransQueueLen - 1))
			{
				for (count2 = count1;
					count2 < (connection->tcp.retransQueueLen - 1); count2 ++)
				{
					memmove((void *) &connection->tcp.retransQueue[count2],
						(void *) &connection->tcp.retransQueue[count2 + 1],
						sizeof(kernelNetworkTcpQueuePacket));
				}
			}

			connection->tcp.retransQueueLen -= 1;
			count1 -= 1;
		}
		else
		{
			kernelDebug(debug_net, "TCP re-transmission %u-%u still queued",
				SEQ_SEND(connection, retrans->sequence), (SEQ_SEND(connection,
				retrans->sequence) + (retrans->dataLen - 1)));
		}
	}
}


static void addRetrans(kernelNetworkConnection *connection,
	unsigned sequence, unsigned dataLen, kernelNetworkPacket *packet)
{
	// Add a packet to the TCP re-transmission queue

	kernelNetworkTcpQueuePacket *retrans = NULL;

	while (connection->tcp.retransQueueLen >= NETWORK_TCP_MAX_RETRANSQUEUE)
		kernelMultitaskerYield();

	retrans = (kernelNetworkTcpQueuePacket *)
		&connection->tcp.retransQueue[connection->tcp.retransQueueLen];

	retrans->sequence = sequence;
	retrans->dataLen = dataLen;
	retrans->packet = packet;
	retrans->reTransmitted = 0;

	connection->tcp.retransQueueLen += 1;

	kernelNetworkPacketHold(retrans->packet);
}


static void removeWait(kernelNetworkConnection *connection, unsigned sequence)
{
	// Removed any processed packets from the TCP wait queue

	kernelNetworkTcpQueuePacket *wait = NULL;
	int count1, count2;

	for (count1 = 0; count1 < connection->tcp.waitQueueLen; count1 ++)
	{
		wait = (kernelNetworkTcpQueuePacket *)
			&connection->tcp.waitQueue[count1];

		if (wait->sequence == sequence)
		{
			kernelDebug(debug_net, "TCP removing %u-%u from wait queue",
				SEQ_RECV(connection, wait->sequence), (SEQ_RECV(connection,
				wait->sequence) + (wait->dataLen - 1)));

			kernelNetworkPacketRelease(wait->packet);

			if (count1 < (connection->tcp.waitQueueLen - 1))
			{
				for (count2 = count1;
					count2 < (connection->tcp.waitQueueLen - 1); count2 ++)
				{
					memmove((void *) &connection->tcp.waitQueue[count2],
						(void *) &connection->tcp.waitQueue[count2 + 1],
						sizeof(kernelNetworkTcpQueuePacket));
				}
			}

			connection->tcp.waitQueueLen -= 1;
			break;
		}
	}
}


static void addWait(kernelNetworkConnection *connection, unsigned sequence,
	unsigned dataLen, kernelNetworkPacket *packet)
{
	// Add a packet to the TCP wait queue, for later reprocessing by
	// kernelNetworkTcpProcessPacket()

	kernelNetworkTcpQueuePacket *wait = NULL;

	if (connection->tcp.waitQueueLen >= NETWORK_TCP_MAX_WAITQUEUE)
	{
		// No room to save this; discard it
		return;
	}

	// If it's already in the queue, remove it.  This new one might have
	// more up-to-date ACKs or window size.
	removeWait(connection, sequence);

	kernelDebug(debug_net, "TCP add %u-%u to wait queue", SEQ_RECV(connection,
		sequence), (SEQ_RECV(connection, sequence) + (dataLen - 1)));

	wait = (kernelNetworkTcpQueuePacket *)
		&connection->tcp.waitQueue[connection->tcp.waitQueueLen];

	wait->sequence = sequence;
	wait->dataLen = dataLen;
	wait->packet = packet;

	connection->tcp.waitQueueLen += 1;

	kernelNetworkPacketHold(wait->packet);
}


static void processWaitQueue(kernelNetworkConnection *connection)
{
	// Loop through the TCP wait queue, and for any packets are due for
	// reception, process them

	kernelNetworkTcpQueuePacket *wait = NULL;
	int count;

	if (kernelLockGet(&connection->tcp.lock) < 0)
		return;

	for (count = 0; count < connection->tcp.waitQueueLen; count ++)
	{
		wait = (kernelNetworkTcpQueuePacket *)
			&connection->tcp.waitQueue[count];

		if (wait->sequence <= (connection->tcp.recvLast + 1))
		{
			if (wait->sequence == (connection->tcp.recvLast + 1))
			{
				kernelDebug(debug_net, "TCP re-process %u-%u",
					SEQ_RECV(connection, wait->sequence),
					(SEQ_RECV(connection, wait->sequence) +
					wait->dataLen - 1));

				if (kernelNetworkTcpProcessPacket(connection, wait->packet,
					1 /* reprocess */) >= 0)
				{
					kernelNetworkDeliverData(connection, wait->packet);
				}
			}

			removeWait(connection, wait->sequence);
		}
	}

	kernelLockRelease(&connection->tcp.lock);
}


static void processRetransQueue(kernelNetworkConnection *connection)
{
	// Loop through the TCP re-transmission queue, and for any un-ACKed
	// packets that have reached or exceeded the re-transmission timeout,
	// re-send them

	kernelNetworkTcpQueuePacket *retrans = NULL;
	int count;

	for (count = 0; count < connection->tcp.retransQueueLen; count ++)
	{
		retrans = (kernelNetworkTcpQueuePacket *)
			&connection->tcp.retransQueue[count];

		if (retrans->packet->timeout <= kernelCpuGetMs())
		{
			// Update the backoff timer
			if (!connection->tcp.backoff)
			{
				// Use double the round-trip time, with a minimum of 200ms
				connection->tcp.backoff =
					max((connection->tcp.roundTripTime * 2), 200);
			}
			else
			{
				// Use double the old backoff time, with a maximum of 4000ms
				connection->tcp.backoff = min((connection->tcp.backoff * 2),
					4000 /* ms */);
			}

			kernelDebug(debug_net, "TCP re-transmit %u-%u with backoff=%u",
				SEQ_SEND(connection, retrans->sequence), (SEQ_SEND(connection,
				retrans->sequence) + retrans->dataLen - 1),
				connection->tcp.backoff);

			// Note that it was re-transmitted
			retrans->reTransmitted += 1;

			// Set a new time stamp and timeout on it
			retrans->packet->timeSent = kernelCpuGetMs();
			retrans->packet->timeout = (retrans->packet->timeSent +
				connection->tcp.backoff);

			// Add an ACK to it
			addAck(connection, retrans->packet,
				(connection->tcp.recvLast + 1));

			// Finalize checksums, etc
			kernelNetworkFinalizeSendPacket(connection, retrans->packet,
				1 /* re-transmit */, 0 /* not 'last packet' */);

			kernelNetworkSendPacket(connection->netDev, retrans->packet,
				0 /* not immediate, can queue */);

			// Only do the oldest timed-out packet per call
			break;
		}
	}
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for internal use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelNetworkTcpOpenConnection(kernelNetworkConnection *connection)
{
	// Given an initialized connection structure, with the device, address,
	// and ports assigned, try to set up the TCP connection with the
	// destination host

	int status = 0;
	uquad_t synTimeout = 0;
	int retries = 0;

	kernelDebug(debug_net, "TCP open connection");

	while (connection->tcp.state != tcp_established)
	{
		if (kernelLockGet(&connection->tcp.lock) < 0)
		{
			kernelMultitaskerYield();
			continue;
		}

		// Get random numbers for any intitial TCP sequence numbers
		connection->tcp.sendInit = kernelRandomUnformatted();
		connection->tcp.sendNext = connection->tcp.sendInit;
		connection->tcp.sendUnAcked = connection->tcp.sendNext;

		// Send the initial SYN packet
		kernelDebug(debug_net, "TCP send SYN packet %u", SEQ_SEND(connection,
			connection->tcp.sendNext));
		changeTcpState(connection, tcp_syn_sent);

		status = sendEmpty(connection, NETWORK_TCPFLAG_SYN, 0 /* no ACK */);

		if (status < 0)
		{
			// Go back
			changeTcpState(connection, tcp_closed);
		}

		kernelLockRelease(&connection->tcp.lock);

		if (status < 0)
			return (status);

		// Wait for the connection to become established

		retries += 1;
		synTimeout = (kernelCpuGetMs() + NETWORK_TCP_SYN_TIMEOUT_MS);

		while ((connection->tcp.state != tcp_established) &&
			(kernelCpuGetMs() < synTimeout))
		{
			// If there was an old connection hanging about, or the connection
			// was refused, the kernelNetworkTcpProcessPacket() function will
			// reset the connection and return it to the closed state, so we
			// should resend
			if (connection->tcp.state == tcp_closed)
				break;

			kernelMultitaskerYield();
		}

		if (retries >= NETWORK_TCP_SYN_RETRIES)
		{
			kernelError(kernel_error, "Connection retries exceeded");
			return (status = ERR_NOCONNECTION);
		}
	}

	kernelDebug(debug_net, "TCP opened connection");

	return (status = 0);
}


int kernelNetworkTcpCloseConnection(kernelNetworkConnection *connection)
{
	// Given a connection structure, try to shut down the TCP connection with
	// the destination host

	int status = 0;

	kernelDebug(debug_net, "TCP close connection");

	if ((connection->tcp.state != tcp_closed) &&
		(connection->tcp.state <= tcp_established))
	{
		while (kernelLockGet(&connection->tcp.lock) < 0)
			kernelMultitaskerYield();

		// Clear the re-transmission queue
		purgeRetransQueue(connection);

		// Clear the wait queue
		purgeWaitQueue(connection);

		// Send the FIN packet

		kernelDebug(debug_net, "TCP send FIN packet");
		changeTcpState(connection, tcp_fin_wait1);

		// Should probably only add the ACK here if one is needed
		status = sendEmpty(connection, (NETWORK_TCPFLAG_FIN |
			NETWORK_TCPFLAG_ACK), (connection->tcp.recvLast + 1));

		kernelLockRelease(&connection->tcp.lock);

		if (status < 0)
			return (status);
	}
	else
	{
		// Seems like the closure is already pending/in progress
	}

	// Wait for the connection to be closed
	while (connection->tcp.state != tcp_closed)
		kernelMultitaskerYield();

	kernelDebug(debug_net, "TCP closed connection");

	return (status = 0);
}


int kernelNetworkTcpSetupReceivedPacket(kernelNetworkPacket *packet)
{
	// This takes a semi-raw 'received' TCP packet, as from the network
	// device's packet input stream,  and tries to interpret the rest and
	// set up the remainder of the packet's fields

	int status = 0;
	networkIp4Header *ip4Header = NULL;
	networkTcpHeader *tcpHeader = NULL;

	kernelDebug(debug_net, "TCP setup received packet");

	ip4Header = (networkIp4Header *)(packet->memory +
		packet->netHeaderOffset);
	tcpHeader = (networkTcpHeader *)(packet->memory +
		packet->transHeaderOffset);

	// Check the checksum
	if (tcpChecksum(ntohs(tcpHeader->checksum), tcpHeader,
		ip4Header->srcAddress, ip4Header->destAddress, ip4Header->protocol,
		(packet->length - packet->transHeaderOffset)))
	{
		kernelError(kernel_error, "TCP header checksum mismatch");
		return (status = ERR_INVALID);
	}

	// Source and destination ports
	packet->srcPort = ntohs(tcpHeader->srcPort);
	packet->destPort = ntohs(tcpHeader->destPort);

	// Update the data pointer and length
	packet->dataOffset += networkGetTcpHdrSize(tcpHeader);
	packet->dataLength -= networkGetTcpHdrSize(tcpHeader);

	return (status = 0);
}


int kernelNetworkTcpProcessPacket(kernelNetworkConnection *connection,
	kernelNetworkPacket *packet, int reprocess)
{
	// Checks the validity of the packet for the connection, does any required
	// TCP state transitions, and sends (non-data) SYNs, ACKs, and FINs as
	// appropriate

	int status = 0;
	networkIp4Header *ip4Header = NULL;
	networkTcpHeader *tcpHeader = NULL;
	unsigned short flags = 0, window = 0;
	unsigned sequenceNum = 0, ackNum = 0;

	kernelDebug(debug_net, "TCP process packet");

	ip4Header = (networkIp4Header *)(packet->memory +
		packet->netHeaderOffset);
	tcpHeader = (networkTcpHeader *)(packet->memory +
		packet->transHeaderOffset);

	while (kernelLockGet(&connection->tcp.lock) < 0)
		kernelMultitaskerYield();

	flags = networkGetTcpHdrFlags(tcpHeader);
	sequenceNum = ntohl(tcpHeader->sequenceNum);
	ackNum = ntohl(tcpHeader->ackNum);
	window = ntohs(tcpHeader->window);

	debugPacket(connection, packet, 1 /* received */);

	// Make sure the packet flags seem legit
	status = checkFlags(connection, flags, sequenceNum, ackNum);
	if (status < 0)
	{
		kernelLockRelease(&connection->tcp.lock);
		return (status = ERR_BADDATA);
	}

	if (!reprocess)
	{
		// Update the other end's window size
		connection->tcp.recvWindow = window;
	}

	// Do general flag processing

	// Does this packet contain an ACK?
	if (flags & NETWORK_TCPFLAG_ACK)
	{
		if (ackNum > connection->tcp.sendUnAcked)
		{
			connection->tcp.sendUnAcked = ackNum;

			kernelDebug(debug_net, "TCP received ACK packet %u for %u "
				"(tcp.sendNext=%u)", SEQ_RECV(connection, sequenceNum),
				SEQ_SEND(connection, ackNum), SEQ_SEND(connection,
				connection->tcp.sendNext));

			// Remove any ACKed data waiting in the re-transmission queue
			removeRetrans(connection, ackNum);
		}
	}

	// Does this packet contain a RST?
	if (flags & NETWORK_TCPFLAG_RST)
	{
		kernelDebug(debug_net, "TCP received RST packet %u",
			SEQ_RECV(connection, sequenceNum));
	}

	// Does this packet contain a SYN?
	if (flags & NETWORK_TCPFLAG_SYN)
	{
		kernelDebug(debug_net, "TCP received SYN packet %u",
			SEQ_RECV(connection, sequenceNum));
	}

	// Does this packet contain a FIN?
	if (flags & NETWORK_TCPFLAG_FIN)
	{
		kernelDebug(debug_net, "TCP received FIN packet %u",
			SEQ_RECV(connection, sequenceNum));
	}

	// Check and update the sequence value
	if (!connection->tcp.recvInit &&
		(connection->tcp.state < tcp_syn_received))
	{
		// This is the first packet we're receiving
		connection->tcp.recvInit = sequenceNum;
		connection->tcp.recvLast = connection->tcp.recvInit;
	}
	else if (packet->dataLength)
	{
		if (sequenceNum > (connection->tcp.recvLast + 1))
		{
			// This seems like an out-of-order packet from the future.  We
			// will try to add it to our wait queue for future processing.
			kernelDebug(debug_net, "TCP future segment: %u > %u",
				SEQ_RECV(connection, sequenceNum), (SEQ_RECV(connection,
				connection->tcp.recvLast) + 1));

			addWait(connection, sequenceNum, packet->dataLength, packet);

			kernelLockRelease(&connection->tcp.lock);
			return (status = ERR_RANGE);
		}
		else if (sequenceNum <= connection->tcp.recvLast)
		{
			// Probably a duplicate transmission, due to a lost ACK.  Just
			// re-ACK and discard it.

			kernelDebug(debug_net, "TCP duplicate segment: %u <= %u",
				SEQ_RECV(connection, sequenceNum), SEQ_RECV(connection,
				connection->tcp.recvLast));

			sendEmpty(connection, NETWORK_TCPFLAG_ACK,
				(connection->tcp.recvLast + 1));

			kernelLockRelease(&connection->tcp.lock);
			return (status = ERR_RANGE);
		}

		connection->tcp.recvLast += packet->dataLength;
	}

	// State machine changes and responses.  Do the 'established' case first,
	// since it's the most common state.

	if (connection->tcp.state == tcp_established)
	{
		if (flags & NETWORK_TCPFLAG_FIN)
		{
			// The other end is closing the connection
			changeTcpState(connection, tcp_close_wait);

			// Send ACK, incrementing for the FIN
			sendEmpty(connection, NETWORK_TCPFLAG_ACK, (sequenceNum + 1));

			// Wait for application close?
			changeTcpState(connection, tcp_last_ack);

			// Send FIN
			sendEmpty(connection, (NETWORK_TCPFLAG_FIN | NETWORK_TCPFLAG_ACK),
				(sequenceNum + 1));
		}
	}

	else if (connection->tcp.state == tcp_listen)
	{
		if (flags & NETWORK_TCPFLAG_SYN)
		{
			// Are we accepting connections from any address?  Else, does this
			// packet claim to be from the address we're expecting?
			if ((!connection->address.dword[0] ||
				(connection->address.dword[0] == ip4Header->srcAddress)) &&
				(!(connection->filter.flags &
					NETWORK_FILTERFLAG_REMOTEPORT) ||
				(connection->filter.remotePort == ntohs(tcpHeader->srcPort))))
			{
				changeTcpState(connection, tcp_syn_received);

				// Get random numbers for any intitial TCP sequence numbers
				connection->tcp.sendInit = kernelRandomUnformatted();
				connection->tcp.sendNext = connection->tcp.sendInit;
				connection->tcp.sendUnAcked = connection->tcp.sendNext;

				// Make sure the address and remote port are assigned to the
				// connection
				connection->address.dword[0] = ip4Header->srcAddress;
				connection->filter.flags |= NETWORK_FILTERFLAG_REMOTEPORT;
				connection->filter.remotePort = ntohs(tcpHeader->srcPort);

				// Send SYN-ACK, incrementing for the SYN
				status = sendEmpty(connection, (NETWORK_TCPFLAG_SYN |
					NETWORK_TCPFLAG_ACK), (sequenceNum + 1));
				if (status < 0)
				{
					// Go back
					changeTcpState(connection, tcp_listen);
				}
			}
			else
			{
				// Refuse this connection.  Send ACK-RST, incrementing for the
				// SYN, and stay in the listening state.
				sendEmpty(connection, (NETWORK_TCPFLAG_ACK |
					NETWORK_TCPFLAG_RST), (sequenceNum + 1));
			}
		}
	}

	else if (connection->tcp.state == tcp_syn_sent)
	{
		if (flags & NETWORK_TCPFLAG_SYN)
		{
			// We might expect SYN-ACK in the same packet as is customary,
			// however this requires only that our sent SYN has been ACKed
			if (connection->tcp.sendUnAcked == connection->tcp.sendNext)
			{
				changeTcpState(connection, tcp_established);

				// Send ACK, incrementing for the SYN
				status = sendEmpty(connection, NETWORK_TCPFLAG_ACK,
					(sequenceNum + 1));
				if (status < 0)
				{
					// Go back
					changeTcpState(connection, tcp_syn_sent);
				}
			}

			else if (!(flags & NETWORK_TCPFLAG_ACK))
			{
				// This may be a case of simultaneous connections.  Transition
				// to the SYN-received state, and send an ACK

				changeTcpState(connection, tcp_syn_received);

				// Send ACK, incrementing for the SYN
				status = sendEmpty(connection, NETWORK_TCPFLAG_ACK,
					(sequenceNum + 1));
				if (status < 0)
				{
					// Go back
					changeTcpState(connection, tcp_syn_sent);
				}
			}
		}
		else if (flags & NETWORK_TCPFLAG_RST)
		{
			// The connection is being refused
			changeTcpState(connection, tcp_closed);
		}
	}

	else if (connection->tcp.state == tcp_syn_received)
	{
		if ((flags & NETWORK_TCPFLAG_ACK) && (ackNum ==
			connection->tcp.sendNext))
		{
			// Connection established
			changeTcpState(connection, tcp_established);
		}
	}

	else if (connection->tcp.state == tcp_close_wait)
	{
		// Not expecting any packets in this state
	}

	else if (connection->tcp.state == tcp_last_ack)
	{
		if ((flags & NETWORK_TCPFLAG_ACK) && (ackNum ==
			connection->tcp.sendNext))
		{
			// Connection closed
			changeTcpState(connection, tcp_closed);
		}
	}

	else if (connection->tcp.state == tcp_fin_wait1)
	{
		if (flags & NETWORK_TCPFLAG_FIN)
		{
			if ((flags & NETWORK_TCPFLAG_ACK) && (ackNum ==
				connection->tcp.sendNext))
			{
				changeTcpState(connection, tcp_time_wait);
			}
			else
			{
				// Simultaneous close
				changeTcpState(connection, tcp_closing);
			}

			// Send ACK, incrementing for the FIN
			sendEmpty(connection, NETWORK_TCPFLAG_ACK, (sequenceNum + 1));
		}
		else if ((flags & NETWORK_TCPFLAG_ACK) && (ackNum ==
			connection->tcp.sendNext))
		{
			changeTcpState(connection, tcp_fin_wait2);
		}
	}

	else if (connection->tcp.state == tcp_closing)
	{
		if ((flags & NETWORK_TCPFLAG_ACK) && (ackNum ==
			connection->tcp.sendNext))
		{
			connection->tcp.timeWaitTime = kernelCpuGetMs();
			changeTcpState(connection, tcp_time_wait);
		}
	}

	else if (connection->tcp.state == tcp_fin_wait2)
	{
		if (flags & NETWORK_TCPFLAG_FIN)
		{
			connection->tcp.timeWaitTime = kernelCpuGetMs();
			changeTcpState(connection, tcp_time_wait);

			// Send ACK, incrementing for the FIN
			sendEmpty(connection, NETWORK_TCPFLAG_ACK, (sequenceNum + 1));
		}
	}

	else if (connection->tcp.state == tcp_time_wait)
	{
		// Not expecting any packets in this state
	}

	kernelLockRelease(&connection->tcp.lock);
	return (status = 0);
}


void kernelNetworkTcpPrependHeader(kernelNetworkPacket *packet)
{
	networkTcpHeader *header = NULL;

	kernelDebug(debug_net, "TCP prepend header");

	header = (networkTcpHeader *)(packet->memory + packet->dataOffset);

	header->srcPort = htons(packet->srcPort);
	header->destPort = htons(packet->destPort);

	// We have to defer the checksum and other things until later

	networkSetTcpHdrSize(header, sizeof(networkTcpHeader));

	// Adjust the packet structure
	packet->transHeaderOffset = packet->dataOffset;
	packet->dataOffset += sizeof(networkTcpHeader);
	packet->dataLength -= sizeof(networkTcpHeader);
}


void kernelNetworkTcpFinalizeSendPacket(kernelNetworkConnection *connection,
	kernelNetworkPacket *packet, int retransmit, int last)
{
	// This does any required finalizing and checksumming of a packet before
	// it is to be sent

	networkIp4Header *ip4Header = NULL;
	networkTcpHeader *tcpHeader = NULL;
	int flags = 0;

	kernelDebug(debug_net, "TCP finalize send packet");

	ip4Header = (networkIp4Header *)(packet->memory +
		packet->netHeaderOffset);
	tcpHeader = (networkTcpHeader *)(packet->memory +
		packet->transHeaderOffset);

	while (kernelLockGet(&connection->tcp.lock) < 0)
		kernelMultitaskerYield();

	if (!retransmit)
		tcpHeader->sequenceNum = htonl(connection->tcp.sendNext);

	// Update the window size (number of bytes we're ready to accept)
	tcpHeader->window = htons(min(0xFFFF, (NETWORK_DATASTREAM_LENGTH -
		connection->inputStream.count)));

	// Get the flags from the header
	flags = networkGetTcpHdrFlags(tcpHeader);

	// If it's the last packet, add a push flag
	if (last)
	{
		flags |= NETWORK_TCPFLAG_PSH;
		networkSetTcpHdrFlags(tcpHeader, flags);
	}

	// Now we can do the checksum
	tcpHeader->checksum = htons(tcpChecksum(0 /* initial */, tcpHeader,
		ip4Header->srcAddress, ip4Header->destAddress, ip4Header->protocol,
		(packet->length - packet->transHeaderOffset)));

	if (flags & (NETWORK_TCPFLAG_SYN | NETWORK_TCPFLAG_FIN))
	{
		// If this is a SYN or FIN packet, advance the sequence number by 1
		connection->tcp.sendNext += 1;
	}
	else if (!retransmit)
	{
		// Otherwise, if it's not a retransmission, advance the sequence
		// number by the packet data length
		connection->tcp.sendNext += packet->dataLength;
	}

	kernelLockRelease(&connection->tcp.lock);

	debugPacket(connection, packet, 0 /* sent */);
}


void kernelNetworkTcpSendState(kernelNetworkConnection *connection,
	kernelNetworkPacket *packet)
{
	while (kernelLockGet(&connection->tcp.lock) < 0)
		kernelMultitaskerYield();

	kernelDebug(debug_net, "TCP sending %u-%u with ACK %u, send window=%u",
		SEQ_SEND(connection, connection->tcp.sendNext),
		(SEQ_SEND(connection, connection->tcp.sendNext) + packet->dataLength -
		1), (SEQ_RECV(connection, connection->tcp.recvLast) + 1),
		(connection->tcp.recvWindow - (SEQ_SEND(connection,
		connection->tcp.sendNext) - SEQ_SEND(connection,
		connection->tcp.sendUnAcked))));

	// Make sure we're within the recipient's receive window
	if (((connection->tcp.sendNext - connection->tcp.sendUnAcked) +
		packet->dataLength) > connection->tcp.recvWindow)
	{
		kernelDebug(debug_net, "TCP wait for window to slide");

		kernelLockRelease(&connection->tcp.lock);

		while (((connection->tcp.sendNext - connection->tcp.sendUnAcked) +
			packet->dataLength) > connection->tcp.recvWindow)
		{
			kernelMultitaskerYield();
		}

		while (kernelLockGet(&connection->tcp.lock) < 0)
			kernelMultitaskerYield();

		kernelDebug(debug_net, "TCP window OK");
	}

	// Remember the time we sent it (for TCP re-transmission)
	packet->timeSent = kernelCpuGetMs();

	// Set the timeout.  Timeout is any backoff timer that's in effect, else
	// the greater of either twice the average round-trip time, or 100ms.
	packet->timeout = packet->timeSent;
	if (connection->tcp.backoff)
		packet->timeout += connection->tcp.backoff;
	else
		packet->timeout += max((connection->tcp.roundTripTime << 1), 100);

	kernelDebug(debug_net, "TCP packet timeout %u", (packet->timeout -
		packet->timeSent));

	// Add it to the re-transmission queue
	addRetrans(connection, connection->tcp.sendNext, packet->dataLength,
		packet);

	// Add an ACK to it
	addAck(connection, packet, (connection->tcp.recvLast + 1));

	kernelLockRelease(&connection->tcp.lock);
}


void kernelNetworkTcpThreadCall(kernelNetworkConnection *connection)
{
	// Do processing in the context of the network thread.  Time-based
	// connection processing, such as sending acks, doing TCP re-
	// transmissions, killing dead or time-wait TCP connections, etc.

	if (kernelLockGet(&connection->tcp.lock) < 0)
		return;

	if (connection->tcp.state >= tcp_established)
	{
		// If we have un-ACKed received data, ACK it now
		if (connection->tcp.recvAcked <= connection->tcp.recvLast)
		{
			kernelDebug(debug_net, "TCP network thread ACKing, "
				"tcp.recvAcked=%u <= tcp.recvLast=%u",
				SEQ_RECV(connection, connection->tcp.recvAcked),
				SEQ_RECV(connection, connection->tcp.recvLast));
			sendEmpty(connection, NETWORK_TCPFLAG_ACK,
				(connection->tcp.recvLast + 1));
		}

		// If there are any deliverable packets saved in the wait queue for
		// this connection, deliver them now
		processWaitQueue(connection);

		// If we have un-ACKed sent data, process the re-transmit queue
		if (connection->tcp.sendUnAcked < connection->tcp.sendNext)
			processRetransQueue(connection);
	}

	// If we have any closed connections in the time-wait state, see whether
	// they can be closed
	if ((connection->tcp.state == tcp_time_wait) &&
		(kernelCpuGetMs() >= (connection->tcp.timeWaitTime + 5000)))
	{
		changeTcpState(connection, tcp_closed);
	}

	kernelLockRelease(&connection->tcp.lock);
}

