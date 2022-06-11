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
//  kernelLanceDriver.c
//

// Driver for LANCE ethernet network adapters.  Based in part on a driver
// contributed by Jonas Zaddach: See the files in the directory
// contrib/jonas-net/src/kernel/

#include "kernelDriver.h" // Contains my prototypes
#include "kernelLanceDriver.h"
#include "kernelNetworkDevice.h"
#include "kernelBus.h"
#include "kernelPciDriver.h"
#include "kernelProcessorX86.h"
#include "kernelMalloc.h"
#include "kernelParameters.h"
#include "kernelPage.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include <string.h>


static void reset(lanceDevice *lance)
{
  unsigned tmp;
	
  // 32-bit reset, by doing a 32-bit read from the 32-bit reset port.
  kernelProcessorInPort32((lance->ioAddress + LANCE_PORTOFFSET32_RESET), tmp);

  // Then 16-bit reset, by doing a 16-bit read from the 16-bit reset port,
  // so the chip is reset and in 16-bit mode.  
  kernelProcessorInPort16((lance->ioAddress + LANCE_PORTOFFSET16_RESET), tmp);
  // The NE2100 LANCE card needs an extra write access to follow
  kernelProcessorOutPort16((lance->ioAddress + LANCE_PORTOFFSET16_RESET), tmp);
}


static unsigned readCSR(lanceDevice *lance, int idx)
{
  // Read the indexed 16-bit control status register (CSR)

  unsigned short data = 0;

  kernelProcessorOutPort16((lance->ioAddress + LANCE_PORTOFFSET16_RAP), idx);
  kernelProcessorInPort16((lance->ioAddress + LANCE_PORTOFFSET_RDP), data);
  return (data);
}


static void writeCSR(lanceDevice *lance, int idx, unsigned data)
{
  // Write the indexed 16-bit control status register (CSR)

  // 16-bit only
  data &= 0xFFFF;

  kernelProcessorOutPort16((lance->ioAddress + LANCE_PORTOFFSET16_RAP), idx);
  kernelProcessorOutPort16((lance->ioAddress + LANCE_PORTOFFSET_RDP), data);
  return;
}


static void modifyCSR(lanceDevice *lance, int idx, unsigned data, opType op)
{
  // Read the indexed 16-bit control status register (CSR), then do logical
  // AND or OR with the supplied data, and write it back.

  unsigned short contents = 0;
  
  // 16-bit only
  data &= 0xFFFF;

  kernelProcessorOutPort16((lance->ioAddress + LANCE_PORTOFFSET16_RAP), idx);
  kernelProcessorInPort16((lance->ioAddress + LANCE_PORTOFFSET_RDP), contents);

  if (op == op_or)
    contents |= data;
  else if (op == op_and)
    contents &= data;

  kernelProcessorOutPort16((lance->ioAddress + LANCE_PORTOFFSET_RDP),
			   contents);
  return;
}


static unsigned readBCR(lanceDevice *lance, int idx)
{
  // Read the indexed 16-bit bus configuration register (BCR)

  unsigned short data = 0;

  kernelProcessorOutPort16((lance->ioAddress + LANCE_PORTOFFSET16_RAP), idx);
  kernelProcessorInPort16((lance->ioAddress + LANCE_PORTOFFSET16_BDP), data);
  return (data);
}


static void writeBCR(lanceDevice *lance, int idx, unsigned data)
{
  // Write the indexed 16-bit bus configuration register (BCR)

  kernelProcessorOutPort16((lance->ioAddress + LANCE_PORTOFFSET16_RAP), idx);
  kernelProcessorOutPort16((lance->ioAddress + LANCE_PORTOFFSET16_BDP), data);
  return;
}


static void driverInterruptHandler(kernelNetworkDevice *adapter)
{
  // This is the 'body' of the interrupt handler for lance devices.  Called
  // from the netorkInterrupt() function in kernelNetwork.c

  unsigned csr0 = 0;
  lanceDevice *lance = NULL;
  int head = 0;
  int *tail = NULL;
  unsigned short flags;

  // Check params
  if (adapter == NULL)
    return;

  lance = adapter->data;

  // Get the contents of the status register
  csr0 = readCSR(lance, LANCE_CSR_STATUS);

  // Disable lance interrupts and clear interrupt status
  csr0 &= ~LANCE_CSR_STATUS_IENA;
  writeCSR(lance, LANCE_CSR_STATUS, csr0);

  // Check for collision errors
  if (csr0 & LANCE_CSR_STATUS_CERR)
    adapter->device.collisions += 1;

  // Why the interrupt, bub?
  if (csr0 & LANCE_CSR_STATUS_RINT)
    {
      // Received

      // If there were general errors in reception, update the error
      // statistics
      if (csr0 & LANCE_CSR_STATUS_ERR)
	{
	  adapter->device.recvErrors += 1;
	  if (csr0 & LANCE_CSR_STATUS_MISS)
	    adapter->device.recvOverruns += 1;
	}

      // Count the number of queued receive packets
      head = lance->recvRing.head;
      while ((adapter->device.recvQueued < adapter->device.recvQueueLen) &&
	     !(lance->recvRing.desc.recv[head].flags & LANCE_DESCFLAG_OWN))
	{
	  flags = lance->recvRing.desc.recv[head].flags;

	  // Check for receive errors with this packet
	  if (flags & LANCE_DESCFLAG_ERR)
	    {
	      adapter->device.recvErrors += 1;
	      if ((flags & LANCE_DESCFLAG_RECV_FRAM) ||
		  (flags & LANCE_DESCFLAG_RECV_OFLO) ||
		  (flags & LANCE_DESCFLAG_RECV_CRC))
		adapter->device.recvDropped += 1;
	    }

	  // Increase the count of packets queued for receiving
	  adapter->device.recvQueued += 1;

	  // Move to the next receive descriptor
	  head += 1;
	  if (head >= adapter->device.recvQueueLen)
	    head = 0;

	  if (head == lance->recvRing.head)
	    // We wrapped all the way around.
	    break;
	}
    }

  if (csr0 & LANCE_CSR_STATUS_TINT)
    {
      // Transmitted

      // If there were general errors in tranmission, update the error
      // statistics
      if (csr0 & LANCE_CSR_STATUS_ERR)
	{
	  adapter->device.transErrors += 1;
	  if (csr0 & LANCE_CSR_STATUS_MISS)
	    adapter->device.transOverruns += 1;
	}

      // Loop for each transmitted packet
      tail = &(lance->transRing.tail);
      while (adapter->device.transQueued &&
	     !(lance->transRing.desc.trans[*tail].flags & LANCE_DESCFLAG_OWN))
	{
	  flags = lance->transRing.desc.trans[*tail].flags;

	  // Check for transmit errors with this packet
	  if (flags & LANCE_DESCFLAG_ERR)
	    {
	      adapter->device.transErrors += 1;
	      if ((flags & LANCE_DESCFLAG_TRANS_UFLO) ||
		  (flags & LANCE_DESCFLAG_TRANS_LCOL) ||
		  (flags & LANCE_DESCFLAG_TRANS_LCAR) ||
		  (flags & LANCE_DESCFLAG_TRANS_RTRY))
		adapter->device.transDropped += 1;
	    }

	  // Reduce the counter of packets queued for transmission
	  adapter->device.transQueued -= 1;

	  // Move to the next transmit descriptor
	  *tail += 1;
	  if (*tail >= adapter->device.transQueueLen)
	    *tail = 0;
	}
    }

  // Reenable lance interrupts
  modifyCSR(lance, LANCE_CSR_STATUS, LANCE_CSR_STATUS_IENA, op_or);

  return;
}


static int driverSetFlags(kernelNetworkDevice *adapter, unsigned flags,
			  int onOff)
{
  // Changes any user-settable flags associated with the adapter

  int status = 0;
  lanceDevice *lance = NULL;

  // Check params
  if (adapter == NULL)
    return (status = ERR_NULLPARAMETER);

  lance = adapter->data;

  // Change any flags that are settable for this NIC.  Ignore any that aren't
  // supported.

  if (flags & NETWORK_ADAPTERFLAG_AUTOSTRIP)
    {
      if (onOff)
	{
	  modifyCSR(lance, LANCE_CSR_FEAT, LANCE_CSR_FEAT_ASTRPRCV, op_or);
	  adapter->device.flags |= NETWORK_ADAPTERFLAG_AUTOSTRIP;
	}
      else
	{
	  modifyCSR(lance, LANCE_CSR_FEAT, ~LANCE_CSR_FEAT_ASTRPRCV, op_and);
	  adapter->device.flags &= ~NETWORK_ADAPTERFLAG_AUTOSTRIP;
	}
    }

  if (flags & NETWORK_ADAPTERFLAG_AUTOPAD)
    {
      if (onOff)
	{
	  modifyCSR(lance, LANCE_CSR_FEAT, LANCE_CSR_FEAT_APADXMT, op_or);
	  adapter->device.flags |= NETWORK_ADAPTERFLAG_AUTOPAD;
	}
      else
	{
	  modifyCSR(lance, LANCE_CSR_FEAT, ~LANCE_CSR_FEAT_APADXMT, op_and);
	  adapter->device.flags &= ~NETWORK_ADAPTERFLAG_AUTOPAD;
	}
    }

  if (flags & NETWORK_ADAPTERFLAG_AUTOCRC)
    {
    }

  return (status = 0);
}


static unsigned driverReadData(kernelNetworkDevice *adapter,
			       unsigned char *buffer)
{
  // This routine copies 1 network packet's worth data from our ring buffer
  // to the supplied frame pointer, if any are currently queued.  Decrements
  // the count of queued packets, and returns the number of bytes copied into
  // the frame pointer.

  unsigned messageLength = 0;
  lanceDevice *lance = NULL;
  int *head = NULL;
  lanceRecvDesc16 *recvDesc = NULL;

  // Check params
  if ((adapter == NULL) || (buffer == NULL))
    return (messageLength = 0);

  lance = adapter->data;
  head = &(lance->recvRing.head);
  recvDesc = lance->recvRing.desc.recv;

  if (adapter->device.recvQueued &&
      !(recvDesc[*head].flags & LANCE_DESCFLAG_OWN))
    {
      messageLength = (unsigned) recvDesc[*head].messageSize;

      kernelMemCopy(lance->recvRing.buffers[*head], buffer, messageLength);

      adapter->device.recvQueued -= 1;

      // Return ownership to the controller
      recvDesc[*head].flags |= LANCE_DESCFLAG_OWN;
      
      *head += 1;
      if (*head >= adapter->device.recvQueueLen)
	*head = 0;
    }

  return (messageLength);
}


static int driverWriteData(kernelNetworkDevice *adapter,
			   unsigned char *buffer, unsigned bufferSize)
{
  // This routine writes network packet data

  int status = 0;
  lanceDevice *lance = NULL;
  lanceTransDesc16 *transDesc = NULL;
  int *head = NULL;
  void *bufferPhysical = NULL;

  // Check params
  if ((adapter == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  lance = adapter->data;
  transDesc = lance->transRing.desc.trans;
  head = &(lance->transRing.head);

  // Make sure we've got room for another packet
  if (adapter->device.transQueued >= adapter->device.transQueueLen)
    return (status = ERR_NOFREE);

  if (!(transDesc[*head].flags & LANCE_DESCFLAG_OWN))
    {
      // Get the physical address of the buffer.
      bufferPhysical = kernelPageGetPhysical(KERNELPROCID, buffer);
      if (bufferPhysical == NULL)
	{
	  kernelError(kernel_error, "Unable to get memory physical address.");
	  return (status = ERR_MEMORY);
	}

      transDesc[*head].buffAddrLow =
	(unsigned short) ((unsigned) bufferPhysical & 0xFFFF);
      transDesc[*head].buffAddrHigh =
	(unsigned short) (((unsigned) bufferPhysical & 0x00FF0000) >> 16);
      transDesc[*head].bufferSize =
	(unsigned short) (0xF000 | (((short) -bufferSize) & 0x0FFF));
      transDesc[*head].transFlags = 0;

      adapter->device.transQueued += 1;
      
      // Set the start packet and end packet bits, and give the descriptor
      // to the controller for transmitting.
      transDesc[*head].flags =
	(LANCE_DESCFLAG_OWN | LANCE_DESCFLAG_STP | LANCE_DESCFLAG_ENP);
      
      *head += 1;
      if (*head >= adapter->device.transQueueLen)
	*head = 0;
    }

  return (status = 0);
}


static int driverDetect(void *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.  Also issues the
  // appropriate commands to the netork adapter to initialize it.

  int status = 0;
  kernelDevice *pciDevice = NULL;
  kernelBusOps *ops = NULL;
  kernelBusTarget *busTargets = NULL;
  int numBusTargets = 0;
  pciDeviceInfo pciDevInfo;
  kernelDevice *dev = NULL;
  kernelNetworkDevice *adapter = NULL;
  lanceDevice *lance = NULL;
  unsigned ioSpaceSize = 0;
  void *receiveBuffer = NULL;
  void *receiveBufferPhysical = NULL;
  void *recvRingPhysical = NULL;
  void *recvRingVirtual = NULL;
  void *transRingPhysical = NULL;
  void *transRingVirtual = NULL;
  void *initPhysical = NULL;
  lanceInitBlock16 *initBlock = NULL;
  int deviceCount, count, shift;

  // Get the PCI bus device
  status = kernelDeviceFindType(kernelDeviceGetClass(DEVICECLASS_BUS),
				kernelDeviceGetClass(DEVICESUBCLASS_BUS_PCI),
				&pciDevice, 1);
  if (status <= 0)
    return (status);

  ops = (kernelBusOps *) pciDevice->driver->ops;

  // Search the PCI bus(es) for devices
  numBusTargets = ops->driverGetTargets(&busTargets);
  if (busTargets == NULL)
    return (status = ERR_NODATA);

  // Search the bus targets for ethernet adapters
  for (deviceCount = 0; deviceCount < numBusTargets; deviceCount ++)
    {
      // If it's not an ethernet device, skip it
      if ((busTargets[deviceCount].class == NULL) ||
	  (busTargets[deviceCount].class->class != DEVICECLASS_NETWORK) ||
	  (busTargets[deviceCount].subClass == NULL) ||
	  (busTargets[deviceCount].subClass->class !=
	   DEVICESUBCLASS_NETWORK_ETHERNET))
	continue;

      // Get the PCI device header
      status =
	ops->driverGetTargetInfo(busTargets[deviceCount].target, &pciDevInfo);
      if (status < 0)
	continue;

      // Check for our vendor and device IDs
      if ((pciDevInfo.device.vendorID != LANCE_VENDOR_ID) ||
	  (pciDevInfo.device.deviceID != LANCE_DEVICE_ID))
	continue;

      // Make sure it's a non-bridge header
      if (pciDevInfo.device.headerType != PCI_HEADERTYPE_NORMAL)
	continue;

      // After this point, we know we have a supported device.

      // Enable the device on the PCI bus as a bus msater
      if ((ops->driverDeviceEnable(busTargets[deviceCount].target, 1) < 0) ||
	  (ops->driverSetMaster(busTargets[deviceCount].target, 1) < 0))
	continue;

      // Allocate memory for the device
      dev = kernelMalloc(sizeof(kernelDevice));
      adapter = kernelMalloc(sizeof(kernelNetworkDevice));
      lance = kernelMalloc(sizeof(lanceDevice));
      adapter->data = lance;

      // Check the first 2 base addresses for I/O and memory addresses.
      // For the time being, we are only implementing I/O mapping, as opposed
      // to memory sharing.  Therefore we expect the first base address
      // register to contain an I/O address, which is signified by bit 0
      // being set.
      if (!(pciDevInfo.device.nonBridge.baseAddress[0] & 1))
	{
	  kernelError(kernel_error, "Unknown adapter I/O address");
	  kernelFree(dev);
	  continue;
	}

      lance->ioAddress =
	(void *) (pciDevInfo.device.nonBridge.baseAddress[0] & 0xFFFFFFFC);

      // Determine the I/O space size.  Write all 1s to the register.
      ops->driverWriteRegister(busTargets[deviceCount].target,
			       PCI_CONFREG_BASEADDRESS0, 32, 0xFFFFFFFF);

      shift = 2;
      lance->ioSpaceSize = 4;
      ioSpaceSize = ops->driverReadRegister(busTargets[deviceCount].target,
					    PCI_CONFREG_BASEADDRESS0, 32);
      while (!((ioSpaceSize >> shift++) & 1))
	lance->ioSpaceSize *= 2;

      // Restore the register we clobbered.
      ops->driverWriteRegister(busTargets[deviceCount].target,
			       PCI_CONFREG_BASEADDRESS0, 32,
			       pciDevInfo.device.nonBridge.baseAddress[0]);

      adapter->device.flags =
	(NETWORK_ADAPTERFLAG_AUTOPAD | NETWORK_ADAPTERFLAG_AUTOSTRIP |
	 NETWORK_ADAPTERFLAG_AUTOCRC);
      adapter->device.linkProtocol = NETWORK_LINKPROTOCOL_ETHERNET;
      adapter->device.interruptNum = pciDevInfo.device.nonBridge.interruptLine;
      adapter->device.recvQueueLen = LANCE_NUM_RINGBUFFERS;
      adapter->device.transQueueLen = LANCE_NUM_RINGBUFFERS;

      // Reset it
      reset(lance);

      // Stop the adapter
      writeCSR(lance, LANCE_CSR_STATUS, LANCE_CSR_STATUS_STOP);

      // Get the ethernet address
      for (count = 0; count < 6; count ++)
	kernelProcessorInPort8((lance->ioAddress + count),
			       adapter->device.hardwareAddress.bytes[count]);

      // Get chip version and set the model name
      lance->chipVersion = ((readCSR(lance, LANCE_CSR_MODEL0) & 0x0FFF) << 4);
      lance->chipVersion |=
	((readCSR(lance, LANCE_CSR_MODEL1) & 0xF000) >> 12);

      switch (lance->chipVersion)
	{
	case 0x2420:
	  dev->device.model = "AMD PCnet/PCI 79C970"; // PCI 
	  break;
	case 0x2621:
	  dev->device.model = "AMD PCnet/PCI II 79C970A"; // PCI 
	  break;
	case 0x2623:
	  dev->device.model = "AMD PCnet/FAST 79C971"; // PCI 
	  break;
	case 0x2624:
	  dev->device.model = "AMD PCnet/FAST+ 79C972"; // PCI 
	  break;
	case 0x2625:
	  dev->device.model = "AMD PCnet/FAST III 79C973"; // PCI 
	  break;
	case 0x2626:
	  dev->device.model = "AMD PCnet/Home 79C978"; // PCI 
	  break;
	case 0x2627:
	  dev->device.model = "AMD PCnet/FAST III 79C975"; // PCI 
	  break;
	case 0x2628:
	  dev->device.model = "AMD PCnet/PRO 79C976";
	  break;
	default:
	  dev->device.model = "AMD LANCE";
	  break;
	}

      // Get space for the buffers
      receiveBufferPhysical =
	kernelMemoryGetPhysical((LANCE_NUM_RINGBUFFERS *
				 LANCE_RINGBUFFER_SIZE),
				0, "lance receive buffers");

      kernelPageMapToFree(KERNELPROCID, receiveBufferPhysical, &receiveBuffer,
			  (LANCE_NUM_RINGBUFFERS * LANCE_RINGBUFFER_SIZE));

      kernelMemClear(receiveBuffer,
		     (LANCE_NUM_RINGBUFFERS * LANCE_RINGBUFFER_SIZE));

      // Set up the receive ring descriptors
      lance->recvRing.head = 0;
      lance->recvRing.tail = 0;
      recvRingPhysical = kernelMemoryGetPhysical((LANCE_NUM_RINGBUFFERS *
						  sizeof(lanceRecvDesc16)), 0,
						 "lance ring descriptors");
      kernelPageMapToFree(KERNELPROCID, recvRingPhysical, &recvRingVirtual,
			  (LANCE_NUM_RINGBUFFERS * sizeof(lanceRecvDesc16)));
      kernelMemClear(recvRingVirtual, (LANCE_NUM_RINGBUFFERS *
				       sizeof(lanceRecvDesc16)));
      lance->recvRing.desc.recv = recvRingVirtual; 
      for (count = 0; count < LANCE_NUM_RINGBUFFERS; count ++)
	{
	  lance->recvRing.desc.recv[count].buffAddrLow = (unsigned short)
	    ((unsigned) receiveBufferPhysical & 0xFFFF);
	  lance->recvRing.desc.recv[count].buffAddrHigh = (unsigned short)
	    (((unsigned) receiveBufferPhysical & 0x00FF0000) >> 16);
	  lance->recvRing.desc.recv[count].flags = LANCE_DESCFLAG_OWN;
	  lance->recvRing.desc.recv[count].bufferSize = (unsigned short)
	    (0xF000 | (((short) -LANCE_RINGBUFFER_SIZE) & 0x0FFF));
	  receiveBufferPhysical += LANCE_RINGBUFFER_SIZE;
	  lance->recvRing.buffers[count] = receiveBuffer;
	  receiveBuffer += LANCE_RINGBUFFER_SIZE;
	}

      // Set up the transmit ring descriptors
      lance->transRing.head = 0;
      lance->transRing.tail = 0;
      transRingPhysical =
	kernelMemoryGetPhysical((LANCE_NUM_RINGBUFFERS *
				 sizeof(lanceTransDesc16)), 0,
				"lance ring descriptors");
      kernelPageMapToFree(KERNELPROCID, transRingPhysical, &transRingVirtual,
			  (LANCE_NUM_RINGBUFFERS * sizeof(lanceTransDesc16)));
      kernelMemClear(transRingVirtual, (LANCE_NUM_RINGBUFFERS *
					sizeof(lanceTransDesc16)));
      lance->transRing.desc.trans = transRingVirtual; 

      // Set up the initialization registers.
      
      // Set the software style as 0 == 16-bit LANCE
      writeCSR(lance, LANCE_CSR_STYLE, 0);

      // Set up the initialization block
      initPhysical = kernelMemoryGetPhysical(sizeof(lanceInitBlock16), 0,
					     "lance init block");
      kernelPageMapToFree(KERNELPROCID, initPhysical, (void **) &initBlock,
			  sizeof(lanceInitBlock16));
      kernelMemClear(initBlock, sizeof(lanceInitBlock16));
      // Mode zero is 'normal' mode
      initBlock->mode = 0;//LANCE_CSR_MODE_PROM;
      for (count = 0; count < 6; count ++)
	initBlock->physAddr[count] =
	  adapter->device.hardwareAddress.bytes[count];
      // Accept all multicast packets for now.
      for (count = 0; count < 4; count ++)
	initBlock->addressFilter[count] = 0xFFFF;
      initBlock->recvDescLow = ((unsigned) recvRingPhysical & 0xFFFF);
      initBlock->recvDescHigh = ((unsigned) recvRingPhysical >> 16);
      initBlock->recvRingLen = (LANCE_NUM_RINGBUFFERS_CODE << 5);
      initBlock->transDescLow = ((unsigned) transRingPhysical & 0xFFFF);
      initBlock->transDescHigh = ((unsigned) transRingPhysical >> 16);
      initBlock->transRingLen = (LANCE_NUM_RINGBUFFERS_CODE << 5);

      // Interrupt mask and deferral control: enable everything except
      // initialization interrupt
      writeCSR(lance, LANCE_CSR_IMASK, LANCE_CSR_IMASK_IDONM);

      // Test and features control register.  Turn on 'DMA plus', auto
      // transmit padding, and auto receive stripping
      modifyCSR(lance, LANCE_CSR_FEAT, (LANCE_CSR_FEAT_DMAPLUS |
					LANCE_CSR_FEAT_APADXMT |
					LANCE_CSR_FEAT_ASTRPRCV), op_or);

      // Turn on burst-mode reading and writing
      writeBCR(lance, LANCE_BCR_BURST,
	       (readBCR(lance, LANCE_BCR_BURST) | 0x0260));

      // Load init block address registers
      writeCSR(lance, LANCE_CSR_IADR0, ((unsigned) initPhysical & 0xFFFF));
      writeCSR(lance, LANCE_CSR_IADR1, ((unsigned) initPhysical >> 16));

      // Start the init
      writeCSR(lance, LANCE_CSR_STATUS, LANCE_CSR_STATUS_INIT);

      // Wait until done
      while (!(readCSR(lance, LANCE_CSR_STATUS) & LANCE_CSR_STATUS_IDON));

      kernelMemoryReleasePhysical(initPhysical);

      // Start it and enable device interrupts
      writeCSR(lance, LANCE_CSR_STATUS,
	       (LANCE_CSR_STATUS_STRT | LANCE_CSR_STATUS_IENA));

      // Set link status
      if (readBCR(lance, LANCE_BCR_LINK) & LANCE_BCR_LINK_LEDOUT)
	adapter->device.flags |= NETWORK_ADAPTERFLAG_LINK;

      dev->device.class = kernelDeviceGetClass(DEVICECLASS_NETWORK);
      dev->device.subClass =
	kernelDeviceGetClass(DEVICESUBCLASS_NETWORK_ETHERNET);
      dev->driver = driver;
      dev->data = (void *) adapter;

      // Register the network adapter device
      status = kernelNetworkDeviceRegister(dev);
      if (status < 0)
	{
	  kernelFree(dev);
	  return (status);
	}

      status = kernelDeviceAdd(pciDevice, dev);
      if (status < 0)
	{
	  kernelFree(dev);
	  return (status);
	}
    }

  return (status = 0);
}


static kernelNetworkDeviceOps networkOps = {
  driverInterruptHandler,
  driverSetFlags,
  driverReadData,
  driverWriteData,
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelLanceDriverRegister(void *driverData)
{
   // Device driver registration.

  kernelDriver *driver = (kernelDriver *) driverData;

  driver->driverDetect = driverDetect;
  driver->ops = &networkOps;

  return;
}
