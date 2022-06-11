//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  kernelNetworkDevice.c
//

// This file contains routines for abstracting and managing network adapter
// devices, as well as for any network activity below the IP/ICMP/etc network
// protocol layer -- for example, ARP Address Resolution Protocol.  In other
// words, this is the portion of the link layer that is not a hardware driver,
// but which does all the interfacing with the hardware drivers.

#include "kernelNetworkDevice.h"
#include "kernelNetworkStream.h"
#include "kernelProcessorX86.h"
#include "kernelInterrupt.h"
#include "kernelPic.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include "kernelLog.h"
#include <string.h>
#include <stdio.h>

#include "kernelText.h"

// An array of pointers to all network devices.
static kernelDevice *devices[NETWORK_MAX_ADAPTERS];
static int numDevices = 0;
static networkAddress ethernetBroadcastAddress = {
  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0 }
};


/*
static void printAddress(networkAddress *address, int addressSize, int hex)
{
  int count;
  for (count = 0; count < addressSize; count ++)
    {
      if (hex)
	kernelTextPrint("%02x%c", address->bytes[count],
			((count < (addressSize - 1))? ':' : '\0'));
      else
	kernelTextPrint("%d%c", address->bytes[count],
			((count < (addressSize - 1))? ':' : '\0'));
    }
}


static void debugArp(kernelArpPacket *arpPacket)
{
  kernelTextPrintLine("ARP: hardAddrSpc=%x protAddrSpc=%x "
		      "hardAddrLen=%d, protAddrLen=%d opCode=%d",
		      kernelProcessorSwap16(arpPacket->hardwareAddressSpace),
		      kernelProcessorSwap16(arpPacket->protocolAddressSpace),
		      arpPacket->hardwareAddrLen,
		      arpPacket->protocolAddrLen,
		      kernelProcessorSwap16(arpPacket->opCode));
  kernelTextPrint("ARP: srcHardAddr=");
  printAddress((networkAddress *) &(arpPacket->srcHardwareAddress),
	       NETWORK_ADDRLENGTH_ETHERNET, 1);
  kernelTextPrint(" srcLogAddr=");
  printAddress((networkAddress *) &(arpPacket->srcLogicalAddress),
	       NETWORK_ADDRLENGTH_IP, 0);
  kernelTextPrint("\nARP: dstHardAddr=");
  printAddress((networkAddress *) &(arpPacket->destHardwareAddress),
	       NETWORK_ADDRLENGTH_ETHERNET, 1);
  kernelTextPrint(" dstLogAddr=");
  printAddress((networkAddress *) &(arpPacket->destLogicalAddress),
	       NETWORK_ADDRLENGTH_IP, 0);
  kernelTextPrintLine("");
}
*/


static int searchArpCache(kernelNetworkDevice *adapter,
			  networkAddress *logicalAddress)
{
  // Search the adapter's ARP cache for an entry corresponding to the supplied
  // logical address, and if found, copy the physical address into the supplied
  // pointer.

  int status = 0;
  int count;

  for (count = 0; count < adapter->numArpCaches; count ++)
    if (networkAddressesEqual(logicalAddress,
			      &(adapter->arpCache[count].logicalAddress),
			      NETWORK_ADDRLENGTH_IP))
      return (count);

  // If we fall through, not found
  return (status = ERR_NOSUCHENTRY);
}


static int sendArp(kernelNetworkDevice *adapter,
		   networkAddress *destLogicalAddress,
		   networkAddress *destPhysicalAddress, int opCode,
		   int immediate)
{
  // Yes, send an ARP request.

  int status = 0;
  kernelNetworkPacket packet;
  kernelArpPacket *arpPacket = NULL;

  // Get memory for our ARP packet
  arpPacket = kernelMalloc(sizeof(kernelArpPacket));
  if (arpPacket == NULL)
    return (status = ERR_MEMORY);

  if ((opCode == NETWORK_ARPOP_REPLY) && destPhysicalAddress)
    // Destination is the supplied physical address
    kernelMemCopy(destPhysicalAddress, &(arpPacket->header.dest),
		  NETWORK_ADDRLENGTH_ETHERNET);
  else
    // Destination is the ethernet broadcast address FF:FF:FF:FF:FF:FF
    kernelMemCopy(&ethernetBroadcastAddress, &(arpPacket->header.dest),
		  NETWORK_ADDRLENGTH_ETHERNET);

  // Source is the adapter hardware address
  kernelMemCopy((void *) &(adapter->device.hardwareAddress),
		&(arpPacket->header.source), NETWORK_ADDRLENGTH_ETHERNET);

  // Ethernet type is ARP
  arpPacket->header.type = kernelProcessorSwap16(NETWORK_ETHERTYPE_ARP);
  // Hardware address space is ethernet=1
  arpPacket->hardwareAddressSpace =
    kernelProcessorSwap16(NETWORK_ARPHARDWARE_ETHERNET);
  // Protocol address space is IP=0x0800
  arpPacket->protocolAddressSpace =
    kernelProcessorSwap16(NETWORK_ETHERTYPE_IP);
  // Hardware address length is 6
  arpPacket->hardwareAddrLen = NETWORK_ADDRLENGTH_ETHERNET;
  // Protocol address length is 4 for IP
  arpPacket->protocolAddrLen = NETWORK_ADDRLENGTH_IP;
  // Operation code.  Request or reply.
  arpPacket->opCode = kernelProcessorSwap16(opCode);

  // Our source hardware address
  kernelMemCopy((void *) &(adapter->device.hardwareAddress),
		&(arpPacket->srcHardwareAddress), NETWORK_ADDRLENGTH_ETHERNET);
  // Our source logical address
  kernelMemCopy((void *) &(adapter->device.hostAddress),
		&(arpPacket->srcLogicalAddress), NETWORK_ADDRLENGTH_IP);
  // Our desired logical address
  kernelMemCopy(destLogicalAddress, &(arpPacket->destLogicalAddress),
		NETWORK_ADDRLENGTH_IP);

  if ((opCode == NETWORK_ARPOP_REPLY) && destPhysicalAddress)
    // The target's hardware address
    kernelMemCopy(destPhysicalAddress, &(arpPacket->destHardwareAddress),
		  NETWORK_ADDRLENGTH_ETHERNET);

  //debugArp(arpPacket);

  // Try to lock the adapter's input stream and queue the reply, rather than
  // tying up the adapter inside an interrupt handler.
  if (immediate || kernelLockGet(&(adapter->outputStreamLock)) < 0)
    {
      // Can't lock.  Send it now.
      status = kernelNetworkDeviceSend((char *) adapter->device.name,
				       (unsigned char *) arpPacket,
				       sizeof(kernelArpPacket));
      kernelFree(arpPacket);
    }
  else
    {
      // Set up the simplified packet structure for it.  The network thread
      // only uses the packet memory and length
      kernelMemClear(&packet, sizeof(kernelNetworkPacket));
      packet.memory = arpPacket;
      packet.length = sizeof(kernelArpPacket);
      status =
	kernelNetworkPacketStreamWrite(&(adapter->outputStream), &packet);

      // ARP packet memory will be released by the network thread

      kernelLockRelease(&(adapter->outputStreamLock));
    }

  return (status);
}


static void addArpCache(kernelNetworkDevice *adapter,
			networkAddress *logicalAddress,
			networkAddress *physicalAddress)
{
  // Add the supplied entry to our ARP cache
  
  // We always put the most recent entry at the start of the list.  If the
  // list grows to its maximum size, the oldest entries fall off the bottom.

  // Shift all down
  kernelMemCopy((void *) &(adapter->arpCache[0]),
		(void *) &(adapter->arpCache[1]),
		((NETWORK_ARPCACHE_SIZE - 1) * sizeof(kernelArpCacheItem)));

  adapter->arpCache[0].logicalAddress.quad = logicalAddress->quad;
  adapter->arpCache[0].physicalAddress.quad = physicalAddress->quad;

  //kernelTextPrint("Added ARP address ");
  //printAddress((networkAddress *) &(adapter->arpCache[0].logicalAddress),
  //       NETWORK_ADDRLENGTH_IP, 0);
  //kernelTextPrint(" = ");
  //printAddress((networkAddress *) &(adapter->arpCache[0].physicalAddress),
  //       NETWORK_ADDRLENGTH_ETHERNET, 1);
  //kernelTextPrintLine("");

  if (adapter->numArpCaches < NETWORK_ARPCACHE_SIZE)
    adapter->numArpCaches += 1;
}


static void receiveArp(kernelNetworkDevice *adapter,
		       kernelArpPacket *arpPacket)
{
  // This gets called anytime we receive an ARP packet (request or reply)

  int arpPosition = 0;

  // Make sure it's ethernet ARP
  if (kernelProcessorSwap16(arpPacket->hardwareAddressSpace) !=
      NETWORK_ARPHARDWARE_ETHERNET)
    return;

  // See if we have the source feller in our table
  arpPosition = searchArpCache(adapter, (networkAddress *)
			       &(arpPacket->srcLogicalAddress));
  if (arpPosition >= 0)
    // Update him in our cache
    kernelMemCopy(&(arpPacket->srcHardwareAddress),
		  (void *) &(adapter->arpCache[arpPosition].physicalAddress),
		  NETWORK_ADDRLENGTH_ETHERNET);
  else
    // Add him to our cache.  Perhaps we shouldn't do this unless the ARP
    // packet is for us, but we suppose for the moment it can't hurt too
    // badly to have a few extras in our table.
    addArpCache(adapter, (networkAddress *) &(arpPacket->srcLogicalAddress),
		(networkAddress *) &(arpPacket->srcHardwareAddress));

  //debugArp(arpPacket);

  // If this isn't for me, ignore it.
  if (!networkAddressesEqual(&(adapter->device.hostAddress), (networkAddress *)
			     &(arpPacket->destLogicalAddress),
			     NETWORK_ADDRLENGTH_IP))
    return;

  if (kernelProcessorSwap16(arpPacket->opCode) == NETWORK_ARPOP_REQUEST)
    {
      // Someone is asking for us.  Send a reply, but it should be queued
      // instead of immediate.
      sendArp(adapter, (networkAddress *) &(arpPacket->srcLogicalAddress),
	      (networkAddress *) &(arpPacket->srcHardwareAddress),
	      NETWORK_ARPOP_REPLY, 0);
    }
}


static int readData(kernelDevice *dev)
{
  int status = 0;
  unsigned char *buffer = NULL;
  networkEthernetHeader *header = NULL;
  kernelNetworkDeviceOps *ops = dev->driver->ops;
  unsigned bufferLength = 0;
  kernelNetworkDevice *adapter = NULL;
  kernelNetworkPacket packet;
  unsigned ipHeaderLen = 0;

  adapter = dev->data;
  adapter->device.recvPackets += 1;

  buffer = kernelMalloc(NETWORK_PACKET_MAX_LENGTH);
  if (buffer == NULL)
    return (status = ERR_MEMORY);

  header = (networkEthernetHeader *) buffer;

  if (ops->driverReadData)
    bufferLength = ops->driverReadData(dev->data, buffer);

  // If there's no data, or the adapter is not initialized, or
  // the packet is not IP or ARP, we are finished
  if (!bufferLength ||
      !(adapter->device.flags & NETWORK_ADAPTERFLAG_INITIALIZED) ||
      ((kernelProcessorSwap16(header->type) != NETWORK_ETHERTYPE_IP) &&
       (kernelProcessorSwap16(header->type) != NETWORK_ETHERTYPE_ARP)))
    {
      kernelFree(buffer);
      return (status = 0);
    }

  /*
  kernelTextPrint("Receive %d: type=%x %02x:%02x:%02x:%02x:%02x:%02x "
		  "-> %02x:%02x:%02x:%02x:%02x:%02x ",
		  adapter->recvPackets,
		  kernelProcessorSwap16(header->type),
		  header->source[0], header->source[1],
		  header->source[2], header->source[3],
		  header->source[4], header->source[5],
		  header->dest[0], header->dest[1], header->dest[2],
		  header->dest[3], header->dest[4], header->dest[5]);
  kernelTextPrintLine("Messagesize %d: ", bufferLength);
  */

  if (kernelProcessorSwap16(header->type) == NETWORK_ETHERTYPE_ARP)
    {
      receiveArp(adapter, (kernelArpPacket *) buffer);
      kernelFree(buffer);
    }

  else
    {
      // Put the packet into the adapter's packet input stream
      
      // Clear the packet structure
      kernelMemClear(&packet, sizeof(kernelNetworkPacket));
      
      // Set the packet headers and the data pointer into the the buffer,
      // plus the data length
      packet.linkProtocol = NETWORK_LINKPROTOCOL_ETHERNET;
      packet.netProtocol = NETWORK_NETPROTOCOL_IP;
      packet.memory = buffer;
      packet.length = bufferLength;
      packet.linkHeader = packet.memory;
      packet.netHeader =
	((void *) packet.linkHeader + sizeof(networkEthernetHeader));
      ipHeaderLen = ((((networkIpHeader *) packet.netHeader)
		      ->versionHeaderLen & 0x0F) << 2);
      packet.transHeader = (packet.netHeader + ipHeaderLen);
      packet.data = packet.transHeader;
      packet.dataLength =
	(packet.length - (sizeof(networkEthernetHeader) + ipHeaderLen));

      // Try to get a lock on the input stream.
      status = kernelLockGet(&(adapter->inputStreamLock));
      if (status < 0)
	{
	  // It would be good if we had a collection of 'deferred packets'
	  // for cases like this, so we can try to insert them next time,
	  // since by doing this we actually drop the packet
	  kernelError(kernel_error, "Couldn't lock input stream; packet "
		      "dropped");
	  adapter->device.recvDropped += 1;
	  return (status);
	}

      // Insert it into the input packet stream
      kernelNetworkPacketStreamWrite(&(adapter->inputStream), &packet);

      kernelLockRelease(&(adapter->inputStreamLock));
    }

  return (status = 0);
}


static void networkInterrupt(void)
{
  // This is the network interrupt handler.  It calls the network driver
  // for the device in order to actually service the interrupt

  void *address = NULL;
  int interruptNum = 0;
  kernelDevice *dev = NULL;
  kernelNetworkDevice *adapter = NULL;
  kernelNetworkDeviceOps *ops = NULL;
  int count;

  kernelProcessorIsrEnter(address);
  kernelProcessingInterrupt = 1;

  // Which interrupt number is active?
  interruptNum = kernelPicGetActive();

  // Find the device that uses this interrupt
  for (count = 0; count < numDevices; count ++)
    if (((kernelNetworkDevice *) devices[count]->data)->device.interruptNum ==
	interruptNum)
      {
	dev = devices[count];
	break;
      }

  if (dev == NULL)
    // Eek.  Don't know where this came from.
    return;

  adapter = dev->data;
  ops = dev->driver->ops;

  // Try to get a lock, though it might fail since we are are inside an
  // interrupt
  kernelLockGet(&(adapter->adapterLock));

  if (ops->driverInterruptHandler)
    // Call the driver routine.
    ops->driverInterruptHandler(adapter);

  // Read the data from all queued packets
  if (0) { while (adapter->device.recvQueued && (readData(dev) >= 0)); }
  while (adapter->device.recvQueued)
    readData(dev);

  kernelLockRelease(&(adapter->adapterLock));

  kernelPicEndOfInterrupt(interruptNum);
  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit(address);
}


static kernelDevice *findDeviceByName(const char *adapterName)
{
  // Find the named adapter

  kernelNetworkDevice *adapter = NULL;
  int count;

  for (count = 0; count < numDevices; count ++)
    {
      adapter = devices[count]->data;
      if (!strcmp((char *) adapter->device.name, adapterName))
	return (devices[count]);
    }
  
  // Not found
  return (NULL);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelNetworkDeviceRegister(kernelDevice *dev)
{
  // This function is called by the network drivers' detection routines
  // to tell us about a new adapter device.

  int status = 0;
  kernelNetworkDevice *adapter = NULL;

  if (dev == NULL)
    {
      kernelError(kernel_error, "The network device is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  if ((dev->data == NULL) || (dev->driver == NULL) ||
      (dev->driver->ops == NULL))
    {
      kernelError(kernel_error, "The network device, driver or ops are NULL");
      return (status = ERR_NULLPARAMETER);
    }

  adapter = dev->data;
  sprintf((char *) adapter->device.name, "net%d", numDevices);

  // Register our interrupt handler for this device
  status =
    kernelInterruptHook(adapter->device.interruptNum, &networkInterrupt);
  if (status < 0)
    return (status);

  devices[numDevices++] = dev;

  // Turn on the interrupt
  kernelPicMask(adapter->device.interruptNum, 1);

  // Register the adapter with the upper-level kernelNetwork functions
  status = kernelNetworkRegister(adapter);
  if (status < 0)
    return (status);

  kernelLog("Added network adapter %s (%02x:%02x:%02x:%02x:%02x:%02x) "
	    "link=%s", adapter->device.name,
	    adapter->device.hardwareAddress.bytes[0],
	    adapter->device.hardwareAddress.bytes[1],
	    adapter->device.hardwareAddress.bytes[2],
	    adapter->device.hardwareAddress.bytes[3],
	    adapter->device.hardwareAddress.bytes[4],
	    adapter->device.hardwareAddress.bytes[5],
	    ((adapter->device.flags & NETWORK_ADAPTERFLAG_LINK)?
	     "UP" : "DOWN"));

  return (status = 0);
}


int kernelNetworkDeviceSetFlags(const char *adapterName, unsigned flags,
				int onOff)
{
  // Changes any user-settable flags associated with a network device

  int status = 0;
  kernelDevice *dev = NULL;
  kernelNetworkDevice *adapter = NULL;
  kernelNetworkDeviceOps *ops = NULL;

  // Check params
  if (adapterName == NULL)
    {
      kernelError(kernel_error, "The adapter name is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Find the adapter by name
  dev = findDeviceByName(adapterName);
  if (dev == NULL)
    {
      kernelError(kernel_error, "No such network adapter \"%s\"", adapterName);
      return (status = ERR_NOSUCHENTRY);
    }

  adapter = dev->data;
  ops = dev->driver->ops;

  // Lock the adapter
  status = kernelLockGet(&(adapter->adapterLock));
  if (status < 0)
    return (status);

  if (ops->driverSetFlags)
    // Call the driver flag-setting routine.
    status = ops->driverSetFlags(adapter, flags, onOff);

  // Release the lock
  kernelLockRelease(&(adapter->adapterLock));

  return (status);
}


int kernelNetworkDeviceGetAddress(const char *adapterName,
				  networkAddress *logicalAddress,
				  networkAddress *physicalAddress)
{
  // This function attempts to use the named network adapter to determine
  // the physical address of the host with the supplied logical address.
  // The Address Resolution Protocol (ARP) is used for this.

  int status = 0;
  kernelDevice *dev = NULL;
  kernelNetworkDevice *adapter = NULL;
  int arpPosition = 0;
  int count;

  // TODO (important): Cache ARP requests with the adapter structure, so we
  // don't have to do this for every bloody packet, right?  Right.

  // Check params
  if ((adapterName == NULL) || (logicalAddress == NULL) ||
      (physicalAddress == NULL))
    {
      kernelError(kernel_error, "Device name or address is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Find the adapter by name
  dev = findDeviceByName(adapterName);
  if (dev == NULL)
    {
      kernelError(kernel_error, "No such network adapter \"%s\"", adapterName);
      return (status = ERR_NOSUCHENTRY);
    }

  adapter = dev->data;

  // Try up to 5 attempts to get an address.  This is arbitrary.  Is it right?
  // From network activity, it looks like Linux tries approx 6 times, when we
  // don't reply to it; once per second.
  for (count = 0; count < 5; count ++)
    {
      // Is the address in the adapter's ARP cache?
      arpPosition = searchArpCache(adapter, logicalAddress);
      if (arpPosition >= 0)
	{
	  // Found it.
	  physicalAddress->quad =
	    adapter->arpCache[arpPosition].physicalAddress.quad;
	  //kernelTextPrintLine("Found ARP cache request");
	  return (status = 0);
	}

      // Construct and send our ethernet packet with the ARP request
      // (not queued; immediately)
      status =
	sendArp(adapter, logicalAddress, NULL, NETWORK_ARPOP_REQUEST, 1);
      if (status < 0)
	return (status);

      // Expect a quick reply the first time
      if (count == 0)
	kernelMultitaskerYield();
      else
	// Delay briefly.  About 1/2 second
	kernelMultitaskerWait(10);
    }

  // If we fall through, we didn't find it.
  return (status = ERR_NOSUCHENTRY);
}


int kernelNetworkDeviceSend(const char *adapterName, unsigned char *buffer,
			    unsigned bufferLength)
{
  // Send a prepared packet using the named network adapter

  int status = 0;
  kernelDevice *dev = NULL;
  kernelNetworkDevice *adapter = NULL;
  kernelNetworkDeviceOps *ops = NULL;
  networkEthernetHeader *header = NULL;

  // Check params
  if ((adapterName == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "The adapter name or buffer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Find the adapter by name
  dev = findDeviceByName(adapterName);
  if (dev == NULL)
    {
      kernelError(kernel_error, "No such network adapter \"%s\"", adapterName);
      return (status = ERR_NOSUCHENTRY);
    }

  if (bufferLength == 0)
    // Nothing to do?  Hum.
    return (status = 0);

  adapter = dev->data;
  ops = dev->driver->ops;

  // Lock the adapter
  status = kernelLockGet(&(adapter->adapterLock));
  if (status < 0)
    return (status);

  if (ops->driverWriteData)
    // Call the driver transmit routine.
    status = ops->driverWriteData(adapter, buffer, bufferLength);

  // Wait until all packets are transmitted before returning, since the memory
  // is needed by the adapter
  while (adapter->device.transQueued && !kernelProcessingInterrupt)
    kernelMultitaskerYield();

  // Release the lock
  kernelLockRelease(&(adapter->adapterLock));

  if (status >= 0)
    {
      adapter->device.transPackets += 1;
      header = (networkEthernetHeader *) buffer;

      /*
      kernelTextPrint("SEND %d: %02x:%02x:%02x:%02x:%02x:%02x -> "
		      "%02x:%02x:%02x:%02x:%02x:%02x ",
		      adapter->transPackets, header->source[0],
		      header->source[1], header->source[2],
		      header->source[3], header->source[4],
		      header->source[5], header->dest[0],
		      header->dest[1], header->dest[2],
		      header->dest[3], header->dest[4], header->dest[5]);
      kernelTextPrintLine("Messagesize %d: ", bufferLength);
      */
    }

  return (status);
}


int kernelNetworkDeviceGetCount(void)
{
  // Returns the count of network devices
  return (numDevices);
}


int kernelNetworkDeviceGet(const char *name, networkDevice *dev)
{
  // Returns the user-space portion of the requested (by name) network
  // device.
  
  kernelDevice *kernelDev = NULL;
  kernelNetworkDevice *adapter = NULL;

  // Check params
  if ((name == NULL) || (dev == NULL))
    return (ERR_NULLPARAMETER);
  
  // Find the adapter by name
  kernelDev = findDeviceByName(name);
  if (kernelDev == NULL)
    return (ERR_NOSUCHENTRY);

  adapter = kernelDev->data;

  kernelMemCopy((networkDevice *) &(adapter->device), dev,
		sizeof(networkDevice));
  return (0);
}
