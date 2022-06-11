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
//  ifconfig.c
//

// Displays information about the system's network devices

/* This is the text that appears when a user requests help about this program
<help>

 -- ifconfig --

Network device control.

Usage:
  ifconfig [-T] [device_name]

This command will show information about the system's network devices.

Options:
-T              : Force text mode operation

</help>
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/api.h>

#define KERNELCONF  "/system/config/kernel.conf"
#define NODEVS      "No supported network devices."

static int graphics = 0;
static int numDevices = 0;
static int networkEnabled = 0;
static int readOnly = 1;
static objectKey window = NULL;
static objectKey enabledLabel = NULL;
static objectKey enableButton = NULL;
static objectKey enableCheckbox = NULL;
static objectKey okButton = NULL;


static void error(const char *format, ...)
{
  // Generic error message code for either text or graphics modes
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  _expandFormatString(output, format, list);
  va_end(list);

  if (graphics)
    windowNewErrorDialog(NULL, "Error", output);
  else
    printf("\n%s\n", output);
}


static int devString(char *name, char *buffer)
{
  int status = 0;
  networkDevice dev;
  char *link = NULL;
  
  status = networkDeviceGet(name, &dev);
  if (status < 0)
    {
      error("Can't get info for device %s", name);
      return (status);
    }

  switch (dev.linkProtocol)
    {
    case NETWORK_LINKPROTOCOL_ETHERNET:
      link = "Ethernet";
      break;
    default:
      link = "Unknown";
      break;
    }

  sprintf(buffer, "%s   Link encap:%s  HWaddr %02x:%02x:%02x:%02x:%02x:%02x\n"
	  "       inet addr:%d.%d.%d.%d  Bcast:%d.%d.%d.%d  Mask:%d.%d.%d.%d\n"
	  "       RX packets:%u errors:%u dropped:%u overruns:%u\n"
	  "       TX packets:%u errors:%u dropped:%u overruns:%u\n"
	  "       %s, %s collisions:%u txqueuelen:%u Interrupt:%d",
	  dev.name, link,
	  dev.hardwareAddress.bytes[0], dev.hardwareAddress.bytes[1],
	  dev.hardwareAddress.bytes[2], dev.hardwareAddress.bytes[3],
	  dev.hardwareAddress.bytes[4], dev.hardwareAddress.bytes[5],
	  dev.hostAddress.bytes[0], dev.hostAddress.bytes[1],
	  dev.hostAddress.bytes[2], dev.hostAddress.bytes[3],
	  dev.broadcastAddress.bytes[0], dev.broadcastAddress.bytes[1],
	  dev.broadcastAddress.bytes[2], dev.broadcastAddress.bytes[3],
	  dev.netMask.bytes[0], dev.netMask.bytes[1],
	  dev.netMask.bytes[2], dev.netMask.bytes[3],
	  dev.recvPackets, dev.recvErrors, dev.recvDropped, dev.recvOverruns,
	  dev.transPackets, dev.transErrors, dev.transDropped,
	  dev.transOverruns,
	  ((dev.flags & NETWORK_ADAPTERFLAG_LINK)? "LINK" : "NOLINK"),
	  ((dev.flags & NETWORK_ADAPTERFLAG_RUNNING)? "UP" : "DOWN"),
	  dev.collisions, dev.transQueueLen, dev.interruptNum);

  return (status = 0);
}


static int printDevices(char *arg)
{
  int status = 0;
  char name[NETWORK_ADAPTER_MAX_NAMELENGTH];
  char buffer[MAXSTRINGLENGTH];
  int count;
  
  // Did the user specify a list of device names?
  if (arg)
    {
      // Get the device information
      status = devString(arg, buffer);
      if (status < 0)
	return (status);

      printf("%s\n\n", buffer);
    }
  else
    {
      if (numDevices)
	{
	  // Show all of them
	  for (count = 0; count < numDevices; count ++)
	    {
	      sprintf(name, "net%d", count);
	      
	      // Get the device information
	      status = devString(name, buffer);
	      if (status < 0)
		return (status);

	      printf("%s\n\n", buffer);
	    }
	}
      else
	printf(NODEVS "\n\n");
    }

  return (status = 0);
}


static void updateEnabled(void)
{
  // Update the networking enabled widgets

  char enabled[80];
  variableList kernelConf;
  int haveConf = 0;

  snprintf(enabled, 80, "Networking is %s", (networkEnabled? "enabled" :
					     "disabled"));
  windowComponentSetData(enabledLabel, enabled, strlen(enabled));
  windowComponentSetData(enableButton, (networkEnabled? "Disable" : "Enable"),
			 8);

  // Try to read the kernel config
  if (configurationReader(KERNELCONF, &kernelConf) >= 0)
    haveConf = 1;

  if (haveConf)
    {
      variableListGet(&kernelConf, "network", enabled, 128);
      if (!strncmp(enabled, "yes", 80))
	windowComponentSetSelected(enableCheckbox, 1);
      else
	windowComponentSetSelected(enableCheckbox, 0);

      variableListDestroy(&kernelConf);
    }

  if (readOnly || !haveConf)
    windowComponentSetEnabled(enableCheckbox, 0);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  objectKey enableDialog = NULL;
  int selected = 0;
  variableList kernelConf;

  // Check for the window being closed by a GUI event.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == okButton) && (event->type == EVENT_MOUSE_LEFTUP)))
    {
      windowGuiStop();
      windowDestroy(window);
    }

  // Check for the user clicking the enable/disable networking button
  if ((key == enableButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      // The user wants to enable or disable networking button.  Make a little
      // dialog while we're doing this because enabling can take a few seconds
      enableDialog =
	windowNewBannerDialog(window,
			      (networkEnabled? "Shutting down networking" :
			       "Initializing networking"),
			      "One moment please...");

      if (networkEnabled)
	networkShutdown();
      else
	networkInitialize();

      windowDestroy(enableDialog);

      networkEnabled = networkInitialized();
      updateEnabled();
    }

  // Check for the user clicking the 'enable at startup' checkbox
  if ((key == enableCheckbox) && (event->type & EVENT_SELECTION))
    {
      windowComponentGetSelected(enableCheckbox, &selected);

      // Try to read the kernel config
      if (configurationReader(KERNELCONF, &kernelConf) >= 0)
	{
	  variableListSet(&kernelConf, "network", (selected? "yes" : "no"));
	  configurationWriter(KERNELCONF, &kernelConf);
	  variableListDestroy(&kernelConf);
	}
    }

  return;
}


static int constructWindow(char *arg)
{
  int status = 0;
  componentParameters params;
  char name[NETWORK_ADAPTER_MAX_NAMELENGTH];
  char buffer[MAXSTRINGLENGTH];
  int count;

  // Create a new window
  window = windowNew(multitaskerGetCurrentProcessId(), "Network Devices");
  if (window == NULL)
    return (status = ERR_NOTINITIALIZED);

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.fixedWidth = 1;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Make a label showing the status of networking
  params.orientationX = orient_right;
  enabledLabel = windowNewTextLabel(window, "Networking is disabled", &params);

  // Make a button for enabling/disabling networking
  params.gridX = 1;
  params.orientationX = orient_middle;
  enableButton = windowNewButton(window, "Enable", NULL, &params);
  windowRegisterEventHandler(enableButton, &eventHandler);

  // Make a checkbox so the user can choose to always enable/disable
  params.gridX = 2;
  params.orientationX = orient_left;
  enableCheckbox = windowNewCheckbox(window, "Enabled at startup", &params);
  windowRegisterEventHandler(enableCheckbox, &eventHandler);
  params.gridY += 1;

  updateEnabled();

  params.gridX = 0;
  params.gridWidth = 3;
  params.orientationX = orient_center;
  params.fixedWidth = 0;

  // Did the user specify a device name?
  if (arg)
    {
      // Get the device information
      status = devString(arg, buffer);
      if (status < 0)
	return (status);
      
      windowNewTextLabel(window, buffer, &params);
      params.gridY += 1;
    }
  else
    {
      if (numDevices)
	{
	  // Show all of them
	  for (count = 0; count < numDevices; count ++)
	    {
	      sprintf(name, "net%d", count);
	      
	      // Get the device information
	      status = devString(name, buffer);
	      if (status < 0)
		return (status);
	      
	      windowNewTextLabel(window, buffer, &params);
	      params.gridY += 1;
	    }
	}
      else
	{
	  windowNewTextLabel(window, NODEVS, &params);
	  params.gridY += 1;
	}
    }
  
  // Create an 'OK' button
  params.padBottom = 5;
  params.fixedWidth = 1;
  okButton = windowNewButton(window, "OK", NULL, &params);
  windowRegisterEventHandler(okButton, &eventHandler);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return (status = 0);
}


int main(int argc, char *argv[])
{
  int status = 0;
  char *arg = NULL;
  char opt;
  disk sysDisk;

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  while (strchr("T", (opt = getopt(argc, argv, "T"))))
    {
      // Force text mode?
      if (opt == 'T')
	graphics = 0;
    }

  numDevices = networkDeviceGetCount();
  if (numDevices < 0)
    {
      error("Can't get the count of network devices");
      return (numDevices);
    }

  // Is the last argument a non-option?
  if ((argc > 1) && (argv[argc - 1][0] != '-'))
    arg = argv[argc - 1];

  // Find out whether we are currently running on a read-only filesystem
  bzero(&sysDisk, sizeof(disk));
  if (!fileGetDisk(KERNELCONF, &sysDisk))
    readOnly = sysDisk.readOnly;

  networkEnabled = networkInitialized();

  if (graphics)
    {
      status = constructWindow(arg);
      if (status >= 0)
	windowGuiRun();
    }
  else
    status = printDevices(arg);

  return (status);
}
