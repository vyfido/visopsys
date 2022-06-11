//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  kernelUsbHubDriver.c
//

// Driver for USB hubs.

#include "kernelUsbHubDriver.h"
#include "kernelBus.h"
#include "kernelCpu.h"
#include "kernelDebug.h"
#include "kernelDevice.h"
#include "kernelDriver.h"
#include "kernelError.h"
#include "kernelLinkedList.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelVariableList.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
static void debugHubDesc(volatile usbHubDesc *hubDesc)
{
	kernelDebug(debug_usb, "USB HUB descriptor:\n"
		"    descLength=%d\n"
		"    descType=%x\n"
		"    numPorts=%d\n"
		"    hubAttrs=%04x\n"
		"    pwrOn2PwrGood=%d\n"
		"    maxPower=%d", hubDesc->descLength, hubDesc->descType,
		hubDesc->numPorts, hubDesc->hubAttrs, hubDesc->pwrOn2PwrGood,
		hubDesc->maxPower);
}

static const char *portFeat2String(int featNum)
{
	switch (featNum)
	{
		case USB_HUBFEAT_PORTCONN:
			return "PORT_CONNECTION";
		case USB_HUBFEAT_PORTENABLE:
			return "PORT_ENABLE";
		case USB_HUBFEAT_PORTSUSPEND:
			return "PORT_SUSPEND";
		case USB_HUBFEAT_PORTOVERCURR:
			return "PORT_OVERCURR";
		case USB_HUBFEAT_PORTRESET:
			return "PORT_RESET";
		case USB_HUBFEAT_PORTPOWER:
			return "PORT_POWER";
		case USB_HUBFEAT_PORTLOWSPEED:
			return "PORT_LOWSPEED";
		case USB_HUBFEAT_PORTCONN_CH:
			return "PORT_CONNECTION_CHANGE";
		case USB_HUBFEAT_PORTENABLE_CH:
			return "PORT_ENABLE_CHANGE";
		case USB_HUBFEAT_PORTSUSPEND_CH:
			return "PORT_SUSPEND_CHANGE";
		case USB_HUBFEAT_PORTOVERCURR_CH:
			return "PORT_OVERCURR_CHANGE";
		case USB_HUBFEAT_PORTRESET_CH:
			return "PORT_RESET_CHANGE";
		default:
			return "(UNKNOWN)";
	}
}
#else
	#define debugHubDesc(hubDesc) do { } while (0)
#endif // DEBUG

kernelDriver *hubDriver = NULL;


static int getHubStatus(usbHub *hub, usbHubStatus *hubStatus)
{
	// Fills in the usbHubStatus structure with data returned by the hub.

	usbTransaction usbTrans;

	kernelDebug(debug_usb, "USB HUB get hub status for address %d",
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
	return (kernelBusWrite(hub->busTarget, sizeof(usbTransaction),
		(void *) &usbTrans));
}


static int clearHubFeature(usbHub *hub, unsigned char feature)
{
	// Sends a 'clear feature' request to the hub.

	kernelDebug(debug_usb, "USB HUB clear hub feature %d for address %d",
		feature, hub->usbDev->address);

	return (kernelUsbControlTransfer(hub->usbDev, USB_CLEAR_FEATURE, feature,
		0, 0, NULL, NULL));
}


static int getPortStatus(usbHub *hub, unsigned char port,
	usbPortStatus *portStatus)
{
	// Fills in the usbPortStatus for the requested port with data returned
	// by the hub.

	usbTransaction usbTrans;

	kernelDebug(debug_usb, "USB HUB get port status for address %d port %d",
		hub->usbDev->address, port);

	kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
	usbTrans.type = usbxfer_control;
	usbTrans.address = hub->usbDev->address;
	usbTrans.control.requestType = (USB_DEVREQTYPE_DEV2HOST |
		USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_OTHER);
	usbTrans.control.request = USB_GET_STATUS;
	usbTrans.control.index = (port + 1);
	usbTrans.length = sizeof(usbPortStatus);
	usbTrans.buffer = portStatus;
	usbTrans.pid = USB_PID_IN;

	// Write the command
	return (kernelBusWrite(hub->busTarget, sizeof(usbTransaction),
		(void *) &usbTrans));
}


static int setPortFeature(usbHub *hub, unsigned char port,
	unsigned char feature)
{
	// Sends a 'set feature' request to the hub for the requested port.

	usbTransaction usbTrans;

	kernelDebug(debug_usb, "USB HUB set port feature %s for address %d "
		"port %d", portFeat2String(feature), hub->usbDev->address, port);

	kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
	usbTrans.type = usbxfer_control;
	usbTrans.address = hub->usbDev->address;
	usbTrans.control.requestType = (USB_DEVREQTYPE_HOST2DEV |
		USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_OTHER);
	usbTrans.control.request = USB_SET_FEATURE;
	usbTrans.control.value = feature;
	usbTrans.control.index = (port + 1);
	usbTrans.pid = USB_PID_OUT;

	// Write the command
	return (kernelBusWrite(hub->busTarget, sizeof(usbTransaction),
		(void *) &usbTrans));
}


static int clearPortFeature(usbHub *hub, unsigned char port,
	unsigned char feature)
{
	// Sends a 'clear feature' request to the hub for the requested port.

	usbTransaction usbTrans;

	kernelDebug(debug_usb, "USB HUB clear port feature %s for address %d "
		"port %d", portFeat2String(feature), hub->usbDev->address, port);

	kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
	usbTrans.type = usbxfer_control;
	usbTrans.address = hub->usbDev->address;
	usbTrans.control.requestType = (USB_DEVREQTYPE_HOST2DEV |
		USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_OTHER);
	usbTrans.control.request = USB_CLEAR_FEATURE;
	usbTrans.control.value = feature;
	usbTrans.control.index = (port + 1);
	usbTrans.pid = USB_PID_OUT;

	// Write the command
	return (kernelBusWrite(hub->busTarget, sizeof(usbTransaction),
		(void *) &usbTrans));
}


static int readChanges(usbHub *hub)
{
	int status = 0;
	usbPortStatus portStatus;
	int count;

	if (!hub->gotInterrupt)
		return (status = ERR_NODATA);

	if (hub->changeBitmap[0] & 0x01)
	{
		status = getHubStatus(hub, (usbHubStatus *) &hub->hubStatus);
		if (status < 0)
			goto out;

		kernelDebug(debug_usb, "USB HUB status=%04x change=%04x",
			hub->hubStatus.status, hub->hubStatus.change);

		if (hub->hubStatus.change & USB_HUBSTAT_LOCPOWER)
		{
			status = clearHubFeature(hub, USB_HUBFEAT_HUBLOCPOWER_CH);
			if (status < 0)
				goto out;
		}
		else if (hub->hubStatus.change & USB_HUBSTAT_OVERCURR)
		{
			status = clearHubFeature(hub, USB_HUBFEAT_HUBOVERCURR_CH);
			if (status < 0)
				goto out;
		}
	}
	else
	{
		for (count = 0; count < hub->hubDesc.numPorts; count ++)
		{
			if ((hub->changeBitmap[(count + 1) / 8] >> ((count + 1) % 8)) &
				0x01)
			{
				status = getPortStatus(hub, count, &portStatus);
				if (status < 0)
					goto out;

				kernelDebug(debug_usb, "USB HUB port %d status=%04x "
					"change=%04x", count, portStatus.status,
					portStatus.change);

				// Record the current status of the port
				hub->portStatus[count].status = portStatus.status;

				// Accumulate the status change info
				hub->portStatus[count].change |= portStatus.change;

				if (portStatus.change & USB_HUBPORTSTAT_CONN)
				{
					status =
						clearPortFeature(hub, count, USB_HUBFEAT_PORTCONN_CH);
					if (status < 0)
						goto out;
				}
				if (portStatus.change & USB_HUBPORTSTAT_ENABLE)
				{
					status = clearPortFeature(hub, count,
						USB_HUBFEAT_PORTENABLE_CH);
					if (status < 0)
						goto out;
				}
				if (portStatus.change & USB_HUBPORTSTAT_SUSPEND)
				{
					status = clearPortFeature(hub, count,
						USB_HUBFEAT_PORTSUSPEND_CH);
					if (status < 0)
						goto out;
				}
				if (portStatus.change & USB_HUBPORTSTAT_OVERCURR)
				{
					status = clearPortFeature(hub, count,
						USB_HUBFEAT_PORTOVERCURR_CH);
					if (status < 0)
						goto out;
				}
				if (portStatus.change & USB_HUBPORTSTAT_RESET)
				{
					status =
						clearPortFeature(hub, count, USB_HUBFEAT_PORTRESET_CH);
					if (status < 0)
						goto out;
				}
			}
		}
	}

	// Return success
	status = 0;

out:
	hub->gotInterrupt = 0;

	return (status);
}


static void doDetectDevices(usbHub *hub, int hotplug)
{
	// Detect devices connected to the hub

	usbDevSpeed speed = usbspeed_unknown;
	int portCount, count;

	if (readChanges(hub) < 0)
		return;

	// Look for port status changes
	for (portCount = 0; portCount < hub->hubDesc.numPorts; portCount ++)
	{
		if (hub->portStatus[portCount].change & USB_HUBPORTSTAT_CONN)
		{
			// A device connected or disconnected
			kernelDebug(debug_usb, "USB HUB CONNECTION change (=%d) on port "
				"%d", (hub->portStatus[portCount].status &
					USB_HUBPORTSTAT_CONN), portCount);

			hub->portStatus[portCount].change &= ~USB_HUBPORTSTAT_CONN;

			if (hub->portStatus[portCount].status & USB_HUBPORTSTAT_CONN)
			{
				// A device connected.  Set the reset feature on the port.
				setPortFeature(hub, portCount, USB_HUBFEAT_PORTRESET);

				kernelDebug(debug_usb, "USB HUB wait for port reset to clear");

				// Try to wait up to 500ms, until the hub clears the reset
				// feature
				for (count = 0; count < 500; count ++)
				{
					if ((readChanges(hub) >= 0) &&
						(hub->portStatus[portCount].change &
							USB_HUBPORTSTAT_RESET) &&
						!(hub->portStatus[portCount].status &
							USB_HUBPORTSTAT_RESET))
					{
						kernelDebug(debug_usb, "USB HUB port reset took %dms",
							count);
						break;
					}

					kernelCpuSpinMs(1);
				}

				if (count >= 500)
					kernelDebugError("Port reset did not clear");
			}
			else
			{
				// A device disconnected.
				kernelUsbDevDisconnect(hub->controller, hub, portCount);
				kernelDebug(debug_usb, "USB HUB port %d is disconnected",
					portCount);
			}
		}

		if (hub->portStatus[portCount].change & USB_HUBPORTSTAT_ENABLE)
		{
			kernelDebug(debug_usb, "USB HUB ENABLED change (=%d) on port %d",
				((hub->portStatus[portCount].status &
					USB_HUBPORTSTAT_ENABLE) >> 1), portCount);

			hub->portStatus[portCount].change &= ~USB_HUBPORTSTAT_ENABLE;
		}

		if (hub->portStatus[portCount].change & USB_HUBPORTSTAT_RESET)
		{
			// A reset has finished.
			kernelDebug(debug_usb, "USB HUB RESET change (=%d) on port %d",
				((hub->portStatus[portCount].status &
					USB_HUBPORTSTAT_RESET) >> 4), portCount);

			hub->portStatus[portCount].change &= ~USB_HUBPORTSTAT_RESET;

			if (!(hub->portStatus[portCount].status & USB_HUBPORTSTAT_RESET) &&
				(hub->portStatus[portCount].status & USB_HUBPORTSTAT_ENABLE))
			{
				kernelDebug(debug_usb, "USB HUB port %d status=0x%04x",
					portCount, hub->portStatus[portCount].status);

				// A port reset/enable has completed.
				if (hub->portStatus[portCount].status &
					USB_HUBPORTSTAT_LOWSPEED)
					speed = usbspeed_low;
				else if ((hub->usbDev->usbVersion >= 0x0200) &&
					(hub->portStatus[portCount].status &
						USB_HUBPORTSTAT_HIGHSPEED))
					speed = usbspeed_high;
				else
					speed = usbspeed_full;

				kernelDebug(debug_usb, "USB HUB port %d is connected, "
					"speed=%s", portCount, usbDevSpeed2String(speed));

				if (kernelUsbDevConnect(hub->controller, hub, portCount, speed,
					hotplug) < 0)
				{
					kernelError(kernel_error, "Error enumerating new USB "
						"device");
				}
			}
		}
	}
}


static void detectDevices(usbHub *hub, int hotplug)
{
	// This function gets called once at startup.

	int count;

	kernelDebug(debug_usb, "USB HUB initial device detection, hotplug=%d",
		hotplug);

	// Check params
	if (hub == NULL)
	{
		kernelError(kernel_error, "NULL hub pointer");
		return;
	}

	// Try to wait up to 500ms, until we've received our first interrupt
	for (count = 0; count < 500; count ++)
	{
		if (hub->gotInterrupt)
		{
			kernelDebug(debug_usb, "USB HUB wait for first interrupt took "
				"%dms", count);
			break;
		}

		kernelCpuSpinMs(1);
	}

	if (hub->gotInterrupt)
		doDetectDevices(hub, hotplug);
	else
		kernelDebug(debug_usb, "USB HUB no first hub interrupt received "
			"after %dms", count);

	hub->doneColdDetect = 1;
}


static void threadCall(usbHub *hub)
{
	// This function gets called periodically by the USB thread, to give us
	// an opportunity to detect connections/disconnections, or whatever else
	// we want.

	// Check params
	if (hub == NULL)
	{
		kernelError(kernel_error, "NULL hub pointer");
		return;
	}

	// Only continue if we've already completed 'cold' device connection
	// detection.  Don't want to interfere with that.
	if (!hub->doneColdDetect)
		return;

	doDetectDevices(hub, 1 /* hotplug */);
}


static int getHubDescriptor(usbHub *hub)
{
	usbTransaction usbTrans;

	kernelDebug(debug_usb, "USB HUB get hub descriptor for target 0x%08x",
		hub->busTarget->id);

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
	return (kernelBusWrite(hub->busTarget, sizeof(usbTransaction),
		(void *) &usbTrans));
}


static void interrupt(usbDevice *usbDev, void *buffer, unsigned length)
{
	// This is called when the hub wants to report a change, on the hub or
	// else on one of the ports.

	usbHub *hub = usbDev->data;

	kernelDebug(debug_usb, "USB HUB interrupt %u bytes", length);

	kernelMemCopy(buffer, hub->changeBitmap,
		min(hub->intrInDesc->maxPacketSize, length));
	hub->gotInterrupt = 1;

	return;
}


static int detectHub(usbDevice *usbDev, int hotplug)
{
	int status = 0;
	usbHub *hub = NULL;
	char value[32];
	int count;

	kernelDebug(debug_usb, "USB HUB detect hub device %p", usbDev);

	// Get a hub structure
	hub = kernelMalloc(sizeof(usbHub));
	if (hub == NULL)
		return (status = ERR_MEMORY);

	usbDev->data = (void *) hub;

	hub->controller = usbDev->controller;
	hub->usbDev = usbDev;

	hub->busTarget =
		kernelBusGetTarget(bus_usb, usbMakeTargetCode(hub->controller->num,
			hub->usbDev->address, 0));
	if (hub->busTarget == NULL)
	{
		status = ERR_NODATA;
		goto out;
	}

	// Record the interrupt-in endpoint
	for (count = 0; count < hub->usbDev->numEndpoints; count ++)
	{
		if (((hub->usbDev->endpointDesc[count]->attributes &
				USB_ENDP_ATTR_MASK) == USB_ENDP_ATTR_INTERRUPT) &&
			(hub->usbDev->endpointDesc[count]->endpntAddress & 0x80) &&
				!hub->intrInDesc)
		{
			hub->intrInDesc = hub->usbDev->endpointDesc[count];
			hub->intrInEndpoint = hub->intrInDesc->endpntAddress;
			kernelDebug(debug_usb, "USB HUB got interrupt endpoint %02x",
				hub->intrInEndpoint);
			break;
		}
	}

	// We *must* have an interrupt in endpoint.
	if (!hub->intrInDesc)
	{
		kernelError(kernel_error, "Hub device %p has no interrupt endpoint",
			hub->usbDev);
		status = ERR_NODATA;
		goto out;
	}

	// Try to get the hub descriptor
	status = getHubDescriptor(hub);
	if (status < 0)
		goto out;

	debugHubDesc(&hub->hubDesc);

	// Get memory for recording 'change bitmap' interrupt data

	if (!hub->intrInDesc->maxPacketSize)
	{
		kernelError(kernel_error, "Hub device %p max packet size is 0",
			hub->usbDev);
		status = ERR_INVALID;
		goto out;
	}

	hub->changeBitmap = kernelMalloc(hub->intrInDesc->maxPacketSize);
	if (hub->changeBitmap == NULL)
	{
		status = ERR_MEMORY;
		goto out;
	}

	// Get structures for recording the port statuses.

	if (!hub->hubDesc.numPorts)
	{
		kernelError(kernel_error, "Hub device %p has no ports",
			hub->usbDev);
		status = ERR_INVALID;
		goto out;
	}


	hub->portStatus =
		kernelMalloc(hub->hubDesc.numPorts * sizeof(usbPortStatus));
	if (hub->portStatus == NULL)
	{
		status = ERR_MEMORY;
		goto out;
	}

	// Add our function pointers
	hub->detectDevices = &detectDevices;
	hub->threadCall = &threadCall;

	hub->usbDev->dev.device.class = kernelDeviceGetClass(DEVICECLASS_HUB);
	hub->usbDev->dev.device.subClass =
		kernelDeviceGetClass(DEVICESUBCLASS_HUB_USB);
	hub->usbDev->dev.driver = hubDriver;

	// Set attributes of the hub
	snprintf(value, 32, "%d", hub->hubDesc.numPorts);
	kernelVariableListSet((variableList *) &hub->usbDev->dev.device.attrs,
		"hub.numPorts", value);

	// Tell USB that we're claiming this device.
	kernelBusDeviceClaim(hub->busTarget, hubDriver);

	// Add the device
	status = kernelDeviceAdd(hub->controller->dev,
		(kernelDevice *) &(hub->usbDev->dev));

	// Set the power on for all ports
	kernelDebug(debug_usb, "USB HUB turn on ports power");
	for (count = 0; count < hub->hubDesc.numPorts; count ++)
		setPortFeature(hub, count, USB_HUBFEAT_PORTPOWER);

	// Use the "power on to power good" value to delay for the appropriate
	// number of milliseconds
	kernelCpuSpinMs(hub->hubDesc.pwrOn2PwrGood * 2);

	// Schedule the regular interrupt.
	kernelDebug(debug_usb, "USB HUB schedule interrupt, %d bytes, interval=%d",
		hub->intrInDesc->maxPacketSize, hub->intrInDesc->interval);
	kernelUsbScheduleInterrupt(hub->usbDev, hub->intrInEndpoint,
		hub->intrInDesc->interval, hub->intrInDesc->maxPacketSize,
		&interrupt);

out:
	if (status < 0)
	{
		if (hub)
		{
			if (hub->changeBitmap)
				kernelFree(hub->changeBitmap);
			if (hub->portStatus)
				kernelFree(hub->portStatus);
			if (hub->busTarget)
				kernelFree(hub->busTarget);
			kernelFree((void *) hub);
		}
	}
	else
	{
		kernelDebug(debug_usb, "USB HUB detected USB hub device");
		kernelUsbAddHub(hub, hotplug);
	}

	return (status);
}


static int driverDetect(void *parent __attribute__((unused)),
	kernelDriver *driver __attribute__((unused)))
{
	// This routine is used to detect and initialize each device, as well as
	// registering each one with any higher-level interfaces.

	int status = 0;
	kernelBusTarget *busTargets = NULL;
	int numBusTargets = 0;
	int deviceCount = 0;
	usbDevice *usbDev = NULL;
	int found = 0;
	usbDevice tmpDev;

	kernelDebug(debug_usb, "USB HUB detect hubs");

	// Search the USB bus(es) for devices
	numBusTargets = kernelBusGetTargets(bus_usb, &busTargets);
	if (numBusTargets <= 0)
		return (status = 0);
      
	// Search the bus targets for USB hub devices
	for (deviceCount = 0; deviceCount < numBusTargets; deviceCount ++)
	{
		// Try to get the USB information about the target
		status =
			kernelBusGetTargetInfo(&busTargets[deviceCount], (void *) &tmpDev);
		if (status < 0)
			continue;

		if ((tmpDev.classCode != 0x09) || (tmpDev.subClassCode != 0x00))
			continue;
  
		found += 1;

		usbDev = kernelUsbGetDevice(busTargets[deviceCount].id);
		if (usbDev == NULL)
			continue;

		status = detectHub(usbDev, 0 /* no hotplug */);
		if (status < 0)
			continue;
	}

	kernelDebug(debug_usb, "USB HUB finished detecting hubs (found %d)",
		found);

	kernelFree(busTargets);
	return (status = 0);
}


static int driverHotplug(void *parent __attribute__((unused)),
	int busType __attribute__((unused)), int target, int connected,
	kernelDriver *driver __attribute__((unused)))
{
	// This routine is used to detect whether a newly-connected, hotplugged
	// device is supported by this driver during runtime, and if so to do the
	// appropriate device setup and registration.  Alternatively if the device
	// is disconnected a call to this function lets us know to stop trying
	// to communicate with it.

	int status = 0;
	usbDevice *usbDev = NULL;
	usbHub *hub = NULL;

	kernelDebug(debug_usb, "USB HUB hotplug %sconnection",
		(connected? "" : "dis"));

	usbDev = kernelUsbGetDevice(target);
	if (usbDev == NULL)
	{
		kernelError(kernel_error, "No such USB device %d", target);
		return (status = ERR_NOSUCHENTRY);
	}

	if (connected)
	{
		status = detectHub(usbDev, 1 /* hotplug */);
		if (status < 0)
			return (status);
	}
	else
	{
		hub = usbDev->data;
		if (hub == NULL)
		{
			kernelError(kernel_error, "No such hub device %d", target);
			return (status = ERR_NOSUCHENTRY);
		}

		// Found it.
		kernelDebug(debug_usb, "USB HUB hub device removed");

		// Unschedule any interrupts
		if (hub->intrInDesc)
			kernelUsbUnscheduleInterrupt(usbDev);

		// Remove it from the device tree
		kernelDeviceRemove((kernelDevice *) &hub->usbDev->dev);

		// Free the memory.
		if (hub->busTarget)
			kernelFree(hub->busTarget);
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

	driver->driverDetect = driverDetect;
	driver->driverHotplug = driverHotplug;

	hubDriver = driver;

	return;
}
