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
//  kernelUsbHubDriver.c
//

// Driver for USB hubs.

#include "kernelDriver.h" // Contains my prototypes
#include "kernelBus.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelUsbDriver.h"


#ifdef DEBUG
static void debugHubDesc(volatile usbHubDesc *hubDesc)
{
  kernelDebug(debug_usb, "Debug hub descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%x\n"
	      "    numPorts=%d\n"
	      "    hubAttrs=%02x\n"
	      "    pwrOn2PwrGood=%d\n"
	      "    maxPower=%d", hubDesc->descLength, hubDesc->descType,
	      hubDesc->numPorts, hubDesc->hubAttrs, hubDesc->pwrOn2PwrGood,
	      hubDesc->maxPower);
}
#else
  #define debugHubDesc(hubDesc) do { } while (0)
#endif // DEBUG


static int getHubStatus(usbHub *hub, usbHubStatus *hubStatus)
{
  // Fills in the usbHubStructure with data returned by the hub.

  usbTransaction usbTrans;

  kernelDebug(debug_usb, "Get hub status for address %d",
	      hub->usbDev->address);

  kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
  usbTrans.type = usbxfer_control;
  usbTrans.address = hub->usbDev->address;
  usbTrans.control.requestType =
    (USB_DEVREQTYPE_DEV2HOST | USB_DEVREQTYPE_CLASS);
  usbTrans.control.request = USB_GET_STATUS;
  usbTrans.length = sizeof(usbHubStatus);
  usbTrans.buffer = hubStatus;
  usbTrans.pid = USB_PID_IN;

  // Write the command
  return (kernelBusWrite(bus_usb, hub->target, sizeof(usbTransaction),
			 (void *) &usbTrans));
}


static int getPortStatus(usbHub *hub, unsigned char port,
			 usbPortStatus *portStatus)
{
  // Fills in the usbPortStatus for the requested port with data returned
  // by the hub.

  usbTransaction usbTrans;

  kernelDebug(debug_usb, "Get port status for address %d port %d",
	      hub->usbDev->address, port);

  kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
  usbTrans.type = usbxfer_control;
  usbTrans.address = hub->usbDev->address;
  usbTrans.control.requestType =
    (USB_DEVREQTYPE_DEV2HOST | USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_OTHER);
  usbTrans.control.request = USB_GET_STATUS;
  usbTrans.control.index = port;
  usbTrans.length = sizeof(usbPortStatus);
  usbTrans.buffer = portStatus;
  usbTrans.pid = USB_PID_IN;

  // Write the command
  return (kernelBusWrite(bus_usb, hub->target, sizeof(usbTransaction),
			 (void *) &usbTrans));
}


static int setPortFeature(usbHub *hub, unsigned char port,
			  unsigned char feature)
{
  // Sends a 'set feature' request to the hub for the requested port.

  usbTransaction usbTrans;

  kernelDebug(debug_usb, "Set port feature %d for address %d port %d", feature,
	      hub->usbDev->address, port);

  kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
  usbTrans.type = usbxfer_control;
  usbTrans.address = hub->usbDev->address;
  usbTrans.control.requestType =
    (USB_DEVREQTYPE_HOST2DEV | USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_OTHER);
  usbTrans.control.request = USB_SET_FEATURE;
  usbTrans.control.value = feature;
  usbTrans.control.index = port;
  usbTrans.pid = USB_PID_OUT;

  // Write the command
  return (kernelBusWrite(bus_usb, hub->target, sizeof(usbTransaction),
			 (void *) &usbTrans));
}


static int clearHubFeature(usbHub *hub, unsigned char feature)
{
  // Sends a 'clear feature' request to the hub.

  kernelDebug(debug_usb, "Clear hub feature %d for address %d", feature,
	      hub->usbDev->address);

  return (kernelUsbControlTransfer(hub->usbDev, USB_CLEAR_FEATURE, feature,
				   0, 0, NULL, NULL));
}


static int clearPortFeature(usbHub *hub, unsigned char port,
			    unsigned char feature)
{
  // Sends a 'clear feature' request to the hub for the requested port.

  usbTransaction usbTrans;

  kernelDebug(debug_usb, "Clear port feature %d for address %d port %d",
	      feature, hub->usbDev->address, port);

  kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
  usbTrans.type = usbxfer_control;
  usbTrans.address = hub->usbDev->address;
  usbTrans.control.requestType =
    (USB_DEVREQTYPE_HOST2DEV | USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_OTHER);
  usbTrans.control.request = USB_CLEAR_FEATURE;
  usbTrans.control.value = feature;
  usbTrans.control.index = port;
  usbTrans.pid = USB_PID_OUT;

  // Write the command
  return (kernelBusWrite(bus_usb, hub->target, sizeof(usbTransaction),
			 (void *) &usbTrans));
}


static void threadCall(usbHub *hub)
{
  // This function gets called periodically by the USB thread, to give us
  // an opportunity to detect connections/disconnections, or whatever else
  // we want.

  int lowSpeed = 0;
  int count;

  // Look for port status changes
  for (count = 0; count < hub->hubDesc.numPorts; count ++)
    {
      if (hub->portStatus[count].change & USB_HUBPORTSTAT_CONN)
	{
	  // A device connected or disconnected
	  kernelDebug(debug_usb, "USB hub CONNECTION change (=%d) on port %d",
		      (hub->portStatus[count].status & USB_HUBPORTSTAT_CONN),
		      count);

	  // A device connected or disconnected
	  if (hub->portStatus[count].status & USB_HUBPORTSTAT_CONN)
	    {
	      // A device connected.  Set the reset feature on the port.
	      // The next interrupt should occur when the reset is
	      // finished.
	      setPortFeature(hub, (count + 1), USB_HUBFEAT_PORTRESET);
	    }
	  else
	    {
	      // A device disconnected.
	      kernelUsbDevDisconnect(hub->controller, hub, count);
	      kernelDebug(debug_usb, "Port %d is disconnected", count);
	    }

	  hub->portStatus[count].change &= ~USB_HUBPORTSTAT_CONN;
	}

      if (hub->portStatus[count].change & USB_HUBPORTSTAT_ENABLE)
	{
	  kernelDebug(debug_usb, "USB hub ENABLED change (=%d) on port %d",
		      (hub->portStatus[count].status & USB_HUBPORTSTAT_ENABLE),
		      count);
	  hub->portStatus[count].change &= ~USB_HUBPORTSTAT_ENABLE;
	}

      if (hub->portStatus[count].change & USB_HUBPORTSTAT_RESET)
	{
	  // A reset has finished.
	  kernelDebug(debug_usb, "USB hub RESET change (=%d) on port %d",
		      (hub->portStatus[count].status & USB_HUBPORTSTAT_RESET),
		      count);

	  if (!(hub->portStatus[count].status & USB_HUBPORTSTAT_RESET) &&
	      (hub->portStatus[count].status & USB_HUBPORTSTAT_ENABLE))
	    {
	      // A port reset/enable has completed.
	  
	      lowSpeed = ((hub->portStatus[count].status &
			   USB_HUBPORTSTAT_LOWSPEED)? 1 : 0);

	      if (kernelUsbDevConnect(hub->controller, hub, count, lowSpeed)
		  < 0)
		kernelError(kernel_error, "Error enumerating new USB device");

	      kernelDebug(debug_usb, "Port %d is connected", count);
	    }

	  hub->portStatus[count].change &= ~USB_HUBPORTSTAT_RESET;
	}
    }
}


static int getHubDescriptor(usbHub *hub)
{
  usbTransaction usbTrans;

  kernelDebug(debug_usb, "Get hub descriptor for target %d", hub->target);

  // Set up the USB transaction to send the 'get descriptor' command
  kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
  usbTrans.type = usbxfer_control;
  usbTrans.address = hub->usbDev->address;
  usbTrans.control.requestType =
    (USB_DEVREQTYPE_DEV2HOST | USB_DEVREQTYPE_CLASS);
  usbTrans.control.request = USB_GET_DESCRIPTOR;
  usbTrans.control.value = (USB_DESCTYPE_HUB << 8);
  usbTrans.length = 8;
  usbTrans.buffer = (void *) &hub->hubDesc;
  usbTrans.pid = USB_PID_IN;

  // Write the command
  return (kernelBusWrite(bus_usb, hub->target, sizeof(usbTransaction),
			 (void *) &usbTrans));
}


static void interrupt(usbDevice *usbDev, void *buffer,
		      unsigned length __attribute((unused)))
{
  // This is called when the hub wants to report a change, on the hub or
  // else on one of the ports.

  usbHub *hub = usbDev->data;
  unsigned char *bitmap = buffer;
  usbPortStatus portStatus;
  int count;

  kernelDebug(debug_usb, "USB hub interrupt %u bytes", length);

  if (bitmap[0] & 0x01)
    {
      if (getHubStatus(hub, (usbHubStatus *) &hub->hubStatus) < 0)
	return;

      kernelDebug(debug_usb, "USB hub status=%04x change=%04x",
		  hub->hubStatus.status, hub->hubStatus.change);

      if (hub->hubStatus.change & USB_HUBSTAT_LOCPOWER)
	clearHubFeature(hub, USB_HUBFEAT_HUBLOCPOWER_CH);

      else if (hub->hubStatus.change & USB_HUBSTAT_OVERCURR)
	clearHubFeature(hub, USB_HUBFEAT_HUBOVERCURR_CH);
    }

  else
    {
      for (count = 1; count <= hub->hubDesc.numPorts; count ++)
	if ((bitmap[count / 8] >> (count % 8)) & 0x01)
	  {
	    if (getPortStatus(hub, count, &portStatus) < 0)
	      return;

	    kernelDebug(debug_usb, "USB port %d status=%04x change=%04x",
			count, portStatus.status, portStatus.change);

	    // Record the current status of the port
	    hub->portStatus[count - 1].status = portStatus.status;

	    // Accumulate the status change info
	    hub->portStatus[count - 1].change |= portStatus.change;

	    if (portStatus.change & USB_HUBPORTSTAT_CONN)
	      clearPortFeature(hub, count, USB_HUBFEAT_PORTCONN_CH);

	    if (portStatus.change & USB_HUBPORTSTAT_ENABLE)
	      clearPortFeature(hub, count, USB_HUBFEAT_PORTENABLE_CH);

	    if (portStatus.change & USB_HUBPORTSTAT_SUSPEND)
	      clearPortFeature(hub, count, USB_HUBFEAT_PORTSUSPEND_CH);

	    if (portStatus.change & USB_HUBPORTSTAT_OVERCURR)
	      clearPortFeature(hub, count, USB_HUBFEAT_PORTOVERCURR_CH);

	    if (portStatus.change & USB_HUBPORTSTAT_RESET)
	      clearPortFeature(hub, count, USB_HUBFEAT_PORTRESET_CH);
	  }
    }

  return;
}


static int detectTarget(void *parent, int target, void *driver)
{
  int status = 0;
  usbHub *hub = NULL;
  int count;

  // Get a hub structure
  hub = kernelMalloc(sizeof(usbHub));
  if (hub == NULL)
    return (status = ERR_MEMORY);

  hub->usbDev = kernelUsbGetDevice(target);
  if (hub->usbDev == NULL)
    {
      status = ERR_NODATA;
      goto out;
    }

  hub->controller = hub->usbDev->controller;
  hub->target = target;
  hub->usbDev->data = (void *) hub;

  // If the USB class is 0x09, and the subclass is 0x00, then we believe we
  // have a USB hub device.

  if ((hub->usbDev->classCode != 0x09) || (hub->usbDev->subClassCode != 0x00))
    {
      status = ERR_INVALID;
      goto out;
    }

  // Record the interrupt-in endpoint
  for (count = 0; count < hub->usbDev->numEndpoints; count ++)
    if ((hub->usbDev->endpointDesc[count]->attributes == 0x03) &&
	(hub->usbDev->endpointDesc[count]->endpntAddress & 0x80) &&
	!hub->intrInDesc)
      {
	hub->intrInDesc = hub->usbDev->endpointDesc[count];
	hub->intrInEndpoint = (hub->intrInDesc->endpntAddress & 0xF);
	kernelDebug(debug_usb, "Got interrupt endpoint %d",
		    hub->intrInEndpoint);
	break;
      }

  // We *must* have an interrupt in endpoint.
  if (!hub->intrInDesc)
    {
      kernelError(kernel_error, "Hub device %d has no interrupt endpoint",
		  target);
      status = ERR_NODATA;
      goto out;
    }

  // Try to get the hub descriptor
  status = getHubDescriptor(hub);
  if (status < 0)
    goto out;

  debugHubDesc(&hub->hubDesc);

  // Get structures for recording the port statuses.
  hub->portStatus =
    kernelMalloc(hub->hubDesc.numPorts * sizeof(usbPortStatus));
  if (hub->portStatus == NULL)
    {
      status = ERR_MEMORY;
      goto out;
    }

  // Add our function pointers
  hub->threadCall = &threadCall;

  // Add the device
  hub->dev.device.class = kernelDeviceGetClass(DEVICECLASS_HUB);
  hub->dev.device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_HUB_USB);
  hub->dev.driver = driver;
  status = kernelDeviceAdd(parent, (kernelDevice *) &hub->dev);

  // Set the power on for all ports
  for (count = 1; count <= hub->hubDesc.numPorts; count ++)
    setPortFeature(hub, count, USB_HUBFEAT_PORTPOWER);

  // Schedule the regular interrupt.
  kernelUsbScheduleInterrupt(hub->usbDev, hub->intrInEndpoint,
			     hub->intrInDesc->interval,
			     hub->intrInDesc->maxPacketSize, &interrupt);

 out:
  if (status < 0)
    {
      if (hub)
	{
	  if (hub->portStatus)
	    kernelFree(hub->portStatus);
	  kernelFree((void *) hub);
	}
    }
  else
    kernelDebug(debug_usb, "Detected USB hub device");

  return (status);
}


static int detect(void *parent, kernelDriver *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.

  int status = 0;
  kernelBusTarget *busTargets = NULL;
  int numBusTargets = 0;
  int deviceCount = 0;
  usbDevice usbDev;

  // Search the USB bus(es) for devices
  numBusTargets = kernelBusGetTargets(bus_usb, &busTargets);
  if (numBusTargets <= 0)
    return (status = 0);
      
  // Search the bus targets for USB hub devices
  for (deviceCount = 0; deviceCount < numBusTargets; deviceCount ++)
    {
      // Try to get the USB information about the target
      status = kernelBusGetTargetInfo(bus_usb, busTargets[deviceCount].target,
				      (void *) &usbDev);
      if (status < 0)
	continue;

      if (usbDev.classCode != 0x03)
	continue;
  
      detectTarget(parent, busTargets[deviceCount].target, driver);
    }

  kernelFree(busTargets);
  return (status = 0);
}


static int hotplug(void *parent, int busType __attribute__((unused)),
		   int target, int connected, kernelDriver *driver)
{
  // This routine is used to detect whether a newly-connected, hotplugged
  // device is supported by this driver during runtime, and if so to do the
  // appropriate device setup and registration.  Alternatively if the device
  // is disconnected a call to this function lets us know to stop trying
  // to communicate with it.

  int status = 0;
  usbDevice *usbDev = NULL;
  usbHub *hub = NULL;

  if (connected)
    {
      status = detectTarget(parent, target, driver);
      if (status < 0)
	return (status);
    }
  else
    {
      usbDev = kernelUsbGetDevice(target);
      if (usbDev == NULL)
	{
	  kernelError(kernel_error, "No such USB device %d", target);
	  return (status = ERR_NOSUCHENTRY);
	}

      hub = usbDev->data;
      if (hub == NULL)
	{
	  kernelError(kernel_error, "No such hub device %d", target);
	  return (status = ERR_NOSUCHENTRY);
	}

      // Found it.
      kernelDebug(debug_scsi, "Hub device removed");

      // Unschedule any interrupts
      if (hub->intrInDesc)
	kernelUsbUnscheduleInterrupt(usbDev);

      // Remove it from the device tree
      kernelDeviceRemove((kernelDevice *) &hub->dev);

      // Free the memory.
      if (hub->portStatus)
	kernelFree(hub->portStatus);
      kernelFree((void *) hub);
    }

  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelUsbHubDriverRegister(kernelDriver *driver)
{
   // Device driver registration.

  driver->driverDetect = detect;
  driver->driverHotplug = hotplug;

  return;
}
