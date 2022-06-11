//
//  Visopsys
//  Copyright (C) 1998-2018 J. Andrew McLaughlin
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
//  ifconfig.c
//

// Displays information about the system's network devices

/* This is the text that appears when a user requests help about this program
<help>

 -- ifconfig --

Network device control.

Usage:
  ifconfig [-T] [-e] [-d] [device_name]

This command will show information about the system's network devices, and
allow a privileged user to perform various network administration tasks.

In text mode:

  The -d option will will disable networking, de-configuring network devices.

  The -e option will enable networking, causing network devices to be
  configured.

In graphics mode, the program is interactive and the user can view network
device status and perform tasks visually.

Options:
-d  : Disable networking (text mode).
-e  : Enable networking (text mode).
-T  : Force text mode operation

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/kernconf.h>
#include <sys/paths.h>

#define _(string) gettext(string)

#define WINDOW_TITLE		_("Network Devices")
#define ENABLED_STARTUP		_("Enabled at startup")
#define HOST_NAME			_("Host name")
#define DOMAIN_NAME			_("Domain name")
#define OK					_("OK")
#define CANCEL				_("Cancel")
#define NO_DEVICES			_("No supported network devices.")
#define DEVSTRMAXVALUE		32

typedef struct {
	char *label;
	char value[DEVSTRMAXVALUE];

} devStringItem;

typedef struct {
	char name[NETWORK_ADAPTER_MAX_NAMELENGTH];
	devStringItem linkEncap;
	devStringItem hwAddr;
	devStringItem inetAddr;
	devStringItem mask;
	devStringItem bcast;
	devStringItem gateway;
	devStringItem dns;
	devStringItem rxPackets;
	devStringItem rxErrors;
	devStringItem rxDropped;
	devStringItem rxOverruns;
	devStringItem txPackets;
	devStringItem txErrors;
	devStringItem txDropped;
	devStringItem txOverruns;
	devStringItem linkStat;
	devStringItem running;
	devStringItem collisions;
	devStringItem txQueueLen;
	devStringItem interrupt;

} devStrings;

static int graphics = 0;
static int numDevices = 0;
static int enabled = 0;
static int readOnly = 1;
static objectKey window = NULL;
static objectKey enabledLabel = NULL;
static objectKey enableButton = NULL;
static objectKey enableCheckbox = NULL;
static objectKey hostLabel = NULL;
static objectKey domainLabel = NULL;
static objectKey hostField = NULL;
static objectKey domainField = NULL;
static objectKey deviceLabel[NETWORK_MAX_ADAPTERS];
static objectKey okButton = NULL;
static objectKey cancelButton = NULL;


__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
	// Generic error message code for either text or graphics modes

	va_list list;
	char output[MAXSTRINGLENGTH];

	va_start(list, format);
	vsnprintf(output, MAXSTRINGLENGTH, format, list);
	va_end(list);

	if (graphics)
		windowNewErrorDialog(NULL, _("Error"), output);
	else
		printf("\n%s\n", output);
}


static devStrings *getDevStrings(networkDevice *dev)
{
	devStrings *str = NULL;
	char *link = NULL;

	str = calloc(1, sizeof(devStrings));
	if (!str)
		return (str);

	switch (dev->linkProtocol)
	{
		case NETWORK_LINKPROTOCOL_LOOP:
			link = _("Local Loopback");
			break;
		case NETWORK_LINKPROTOCOL_ETHERNET:
			link = _("Ethernet");
			break;
		default:
			link = _("Unknown");
			break;
	}

	strncpy(str->name, dev->name, NETWORK_ADAPTER_MAX_NAMELENGTH);

	str->linkEncap.label = _("Link encap");
	snprintf(str->linkEncap.value, DEVSTRMAXVALUE, "%s", link);

	str->hwAddr.label = _("HWaddr");
	snprintf(str->hwAddr.value, DEVSTRMAXVALUE,
		"%02x:%02x:%02x:%02x:%02x:%02x",
		dev->hardwareAddress.byte[0], dev->hardwareAddress.byte[1],
		dev->hardwareAddress.byte[2], dev->hardwareAddress.byte[3],
		dev->hardwareAddress.byte[4], dev->hardwareAddress.byte[5]);

	str->inetAddr.label = _("inet addr");
	snprintf(str->inetAddr.value, DEVSTRMAXVALUE, "%d.%d.%d.%d",
		dev->hostAddress.byte[0], dev->hostAddress.byte[1],
		dev->hostAddress.byte[2], dev->hostAddress.byte[3]);

	str->mask.label = _("Mask");
	snprintf(str->mask.value, DEVSTRMAXVALUE, "%d.%d.%d.%d",
		dev->netMask.byte[0], dev->netMask.byte[1],
		dev->netMask.byte[2], dev->netMask.byte[3]);

	str->bcast.label = _("Bcast");
	snprintf(str->bcast.value, DEVSTRMAXVALUE, "%d.%d.%d.%d",
		dev->broadcastAddress.byte[0], dev->broadcastAddress.byte[1],
		dev->broadcastAddress.byte[2], dev->broadcastAddress.byte[3]);

	str->gateway.label = _("Gateway");
	snprintf(str->gateway.value, DEVSTRMAXVALUE, "%d.%d.%d.%d",
		dev->gatewayAddress.byte[0], dev->gatewayAddress.byte[1],
		dev->gatewayAddress.byte[2], dev->gatewayAddress.byte[3]);

	str->dns.label = _("DNS");
	snprintf(str->dns.value, DEVSTRMAXVALUE, "%d.%d.%d.%d",
		dev->dnsAddress.byte[0], dev->dnsAddress.byte[1],
		dev->dnsAddress.byte[2], dev->dnsAddress.byte[3]);

	str->rxPackets.label = _("RX packets");
	snprintf(str->rxPackets.value, DEVSTRMAXVALUE, "%u", dev->recvPackets);

	str->rxErrors.label = _("errors");
	snprintf(str->rxErrors.value, DEVSTRMAXVALUE, "%u", dev->recvErrors);

	str->rxDropped.label = _("dropped");
	snprintf(str->rxDropped.value, DEVSTRMAXVALUE, "%u", dev->recvDropped);

	str->rxOverruns.label = _("overruns");
	snprintf(str->rxOverruns.value, DEVSTRMAXVALUE, "%u", dev->recvOverruns);

	str->txPackets.label = _("TX packets");
	snprintf(str->txPackets.value, DEVSTRMAXVALUE, "%u", dev->transPackets);

	str->txErrors.label = _("errors");
	snprintf(str->txErrors.value, DEVSTRMAXVALUE, "%u", dev->transErrors);

	str->txDropped.label = _("dropped");
	snprintf(str->txDropped.value, DEVSTRMAXVALUE, "%u", dev->transDropped);

	str->txOverruns.label = _("overruns");
	snprintf(str->txOverruns.value, DEVSTRMAXVALUE, "%u", dev->transOverruns);

	str->linkStat.label = _("link status");
	snprintf(str->linkStat.value, DEVSTRMAXVALUE, "%s",
		((dev->flags & NETWORK_ADAPTERFLAG_LINK)? _("LINK") : _("NOLINK")));

	str->running.label = _("running");
	snprintf(str->running.value, DEVSTRMAXVALUE, "%s",
		((dev->flags & NETWORK_ADAPTERFLAG_RUNNING)? _("UP") : _("DOWN")));

	str->collisions.label = _("collisions");
	snprintf(str->collisions.value, DEVSTRMAXVALUE, "%u", dev->collisions);

	str->txQueueLen.label = _("txqueuelen");
	snprintf(str->txQueueLen.value, DEVSTRMAXVALUE, "%u", dev->transQueueLen);

	str->interrupt.label = _("Interrupt");
	snprintf(str->interrupt.value, DEVSTRMAXVALUE, "%d", dev->interruptNum);

	return (str);
}


static int getDevString(char *name, char *buffer)
{
	int status = 0;
	networkDevice dev;
	devStrings *str = NULL;

	status = networkDeviceGet(name, &dev);
	if (status < 0)
	{
		error(_("Can't get info for device %s"), name);
		return (status);
	}

	str = getDevStrings(&dev);
	if (!str)
		return (status = ERR_MEMORY);

	sprintf(buffer, "%s   %s:%s  %s %s\n"
		"       %s:%s  %s:%s  %s:%s\n"
		"       %s:%s %s:%s %s:%s %s:%s\n"
		"       %s:%s %s:%s %s:%s %s:%s\n"
		"       %s, %s %s:%s %s:%s %s:%s",
		str->name, str->linkEncap.label, str->linkEncap.value,
		str->hwAddr.label, str->hwAddr.value,
		str->inetAddr.label, str->inetAddr.value,
		str->bcast.label, str->bcast.value,
		str->mask.label, str->mask.value,
		str->rxPackets.label, str->rxPackets.value,
		str->rxErrors.label, str->rxErrors.value,
		str->rxDropped.label, str->rxDropped.value,
		str->rxOverruns.label, str->rxOverruns.value,
		str->txPackets.label, str->txPackets.value,
		str->txErrors.label, str->txErrors.value,
		str->txDropped.label, str->txDropped.value,
		str->txOverruns.label, str->txOverruns.value,
		str->linkStat.value, str->running.value,
		str->collisions.label, str->collisions.value,
		str->txQueueLen.label, str->txQueueLen.value,
		str->interrupt.label, str->interrupt.value);

	free(str);

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
		status = getDevString(arg, buffer);
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
				status = getDevString(name, buffer);
				if (status < 0)
					return (status);

				printf("%s\n\n", buffer);
			}
		}
		else
		{
			printf("%s\n\n", NO_DEVICES);
		}
	}

	return (status = 0);
}


static void updateEnabled(void)
{
	// Update the networking enabled widgets

	char name[NETWORK_ADAPTER_MAX_NAMELENGTH];
	char *buffer = NULL;
	char tmp[128];
	int count;

	snprintf(tmp, 128, _("Networking is %s"), (enabled? _("enabled") :
		_("disabled")));
	windowComponentSetData(enabledLabel, tmp, strlen(tmp), 1 /* redraw */);
	windowComponentSetData(enableButton, (enabled? _("Disable") :
		_("Enable")), 8, 1 /* redraw */);

	// Update the device strings as well.
	buffer = malloc(MAXSTRINGLENGTH);
	if (buffer)
	{
		for (count = 0; count < numDevices; count ++)
		{
			if (deviceLabel[count])
			{
				sprintf(name, "net%d", count);

				if (getDevString(name, buffer) < 0)
					continue;

				windowComponentSetData(deviceLabel[count], buffer,
					MAXSTRINGLENGTH, 1 /* redraw */);
			}
		}

		free(buffer);
	}
}


static void updateHostName(void)
{
	char hostName[NETWORK_MAX_HOSTNAMELENGTH];
	char domainName[NETWORK_MAX_DOMAINNAMELENGTH];
	const char *value = NULL;
	variableList kernelConf;

	if (enabled)
	{
		if (networkGetHostName(hostName, NETWORK_MAX_HOSTNAMELENGTH) >= 0)
		{
			windowComponentSetData(hostField, hostName,
				NETWORK_MAX_HOSTNAMELENGTH, 1 /* redraw */);
		}

		if (networkGetDomainName(domainName,
			NETWORK_MAX_DOMAINNAMELENGTH) >= 0)
		{
			windowComponentSetData(domainField, domainName,
				NETWORK_MAX_DOMAINNAMELENGTH, 1 /* redraw */);
		}
	}
	else
	{
		if (configRead(KERNEL_DEFAULT_CONFIG, &kernelConf) >= 0)
		{
			value = variableListGet(&kernelConf, KERNELVAR_NET_HOSTNAME);
			if (value)
			{
				windowComponentSetData(hostField, (void *) value,
					NETWORK_MAX_HOSTNAMELENGTH, 1 /* redraw */);
			}

			value = variableListGet(&kernelConf, KERNELVAR_NET_DOMAINNAME);
			if (value)
			{
				windowComponentSetData(domainField, (void *) value,
					NETWORK_MAX_DOMAINNAMELENGTH, 1 /* redraw */);
			}

			variableListDestroy(&kernelConf);
		}
	}

	return;
}


static void refreshWindow(void)
{
	// We got a 'window refresh' event (probably because of a language switch),
	// so we need to update things

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("ifconfig");

	// Re-get the character set
	if (getenv(ENV_CHARSET))
		windowSetCharSet(window, getenv(ENV_CHARSET));

	// Refresh the 'networking enabled' label, button, and device strings
	updateEnabled();

	// Refresh the 'enabled at startup' checkbox
	windowComponentSetData(enableCheckbox, ENABLED_STARTUP,
		strlen(ENABLED_STARTUP), 1 /* redraw */);

	// Refresh the 'host name' label
	windowComponentSetData(hostLabel, HOST_NAME, strlen(HOST_NAME),
		1 /* redraw */);

	// Refresh the 'domain name' label
	windowComponentSetData(domainLabel, DOMAIN_NAME, strlen(DOMAIN_NAME),
		1 /* redraw */);

	// Refresh the 'ok' button
	windowComponentSetData(okButton, OK, strlen(OK), 1 /* redraw */);

	// Refresh the 'cancel' button
	windowComponentSetData(cancelButton, CANCEL, strlen(CANCEL),
		1 /* redraw */);

	// Refresh the window title
	windowSetTitle(window, WINDOW_TITLE);
}


static void eventHandler(objectKey key, windowEvent *event)
{
	objectKey enableDialog = NULL;
	int selected = 0;
	variableList kernelConf;
	char hostName[NETWORK_MAX_HOSTNAMELENGTH];
	char domainName[NETWORK_MAX_DOMAINNAMELENGTH];

	// Check for window events.
	if (key == window)
	{
		// Check for window refresh
		if (event->type == EVENT_WINDOW_REFRESH)
			refreshWindow();

		// Check for the window being closed
		else if (event->type == EVENT_WINDOW_CLOSE)
		{
			windowGuiStop();
			windowDestroy(window);
		}
	}

	// Check for the user clicking the enable/disable networking button
	else if ((key == enableButton) && (event->type == EVENT_MOUSE_LEFTUP))
	{
		// The user wants to enable or disable networking.  Make a little
		// dialog while we're doing this because enabling can take a few
		// seconds
		enableDialog = windowNewBannerDialog(window, (enabled?
			_("Shutting down networking") : _("Initializing networking")),
			_("One moment please..."));

		if (enabled)
			networkShutdown();
		else
			networkEnable();

		windowDestroy(enableDialog);

		enabled = networkEnabled();
		updateEnabled();
		updateHostName();
	}

	// Check for the user clicking the 'OK' button
	else if ((key == okButton) && (event->type == EVENT_MOUSE_LEFTUP))
	{
		windowComponentGetSelected(enableCheckbox, &selected);
		windowComponentGetData(hostField, hostName,
			NETWORK_MAX_HOSTNAMELENGTH);
		windowComponentGetData(domainField, domainName,
			NETWORK_MAX_DOMAINNAMELENGTH);

		// Set new values in the kernel
		networkSetHostName(hostName, NETWORK_MAX_HOSTNAMELENGTH);
		networkSetDomainName(domainName, NETWORK_MAX_DOMAINNAMELENGTH);

		// Try to read and change the kernel config
		if (!readOnly && configRead(KERNEL_DEFAULT_CONFIG, &kernelConf) >= 0)
		{
			variableListSet(&kernelConf, KERNELVAR_NETWORK, (selected?
				"yes" : "no"));
			variableListSet(&kernelConf, KERNELVAR_NET_HOSTNAME, hostName);
			variableListSet(&kernelConf, KERNELVAR_NET_DOMAINNAME,
				domainName);
			configWrite(KERNEL_DEFAULT_CONFIG, &kernelConf);
			variableListDestroy(&kernelConf);
		}

		windowGuiStop();
		windowDestroy(window);
	}

	// Check for the user clicking the 'cancel' button
	else if ((key == cancelButton) && (event->type == EVENT_MOUSE_LEFTUP))
	{
		windowGuiStop();
		windowDestroy(window);
	}

	return;
}


static int constructWindow(char *arg)
{
	int status = 0;
	componentParameters params;
	objectKey container = NULL;
	char name[NETWORK_ADAPTER_MAX_NAMELENGTH];
	char *buffer = NULL;
	char tmp[8];
	int count;

	// Create a new window
	window = windowNew(multitaskerGetCurrentProcessId(), WINDOW_TITLE);
	if (!window)
		return (status = ERR_NOTINITIALIZED);

	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 5;
	params.padRight = 5;
	params.padTop = 5;
	params.orientationX = orient_left;
	params.orientationY = orient_middle;

	// A container for the 'enable networking' stuff
	params.gridWidth = 2;
	container = windowNewContainer(window, "enable", &params);

	// Make a label showing the status of networking
	params.gridWidth = 1;
	params.padTop = 0;
	params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
	enabledLabel = windowNewTextLabel(container, _("Networking is disabled"),
		&params);

	// Make a button for enabling/disabling networking
	params.gridX = 1;
	enableButton = windowNewButton(container, _("Enable"), NULL, &params);
	windowRegisterEventHandler(enableButton, &eventHandler);

	// Make a checkbox so the user can choose to always enable/disable
	params.gridX = 2;
	enableCheckbox = windowNewCheckbox(container, ENABLED_STARTUP,
		&params);
	params.gridY += 1;

	// Try to find out whether networking is enabled
	if (configGet(KERNEL_DEFAULT_CONFIG, KERNELVAR_NETWORK, tmp, 8) >= 0)
	{
		if (!strncmp(tmp, "yes", 8))
			windowComponentSetSelected(enableCheckbox, 1);
		else
			windowComponentSetSelected(enableCheckbox, 0);
	}

	if (readOnly)
		windowComponentSetEnabled(enableCheckbox, 0);

	updateEnabled();

	// A container for the host and domain name stuff
	params.gridX = 0;
	params.gridWidth = 2;
	params.padTop = 5;
	params.flags &= ~WINDOW_COMPFLAG_FIXEDWIDTH;
	container = windowNewContainer(window, "hostname", &params);

	params.gridWidth = 1;
	hostLabel = windowNewTextLabel(container, HOST_NAME, &params);

	params.gridX = 1;
	params.padTop = 0;
	domainLabel = windowNewTextLabel(container, DOMAIN_NAME, &params);
	params.gridY += 1;

	params.gridX = 0;
	params.padBottom = 5;
	hostField = windowNewTextField(container, 16, &params);
	windowRegisterEventHandler(hostField, &eventHandler);

	params.gridX = 1;
	domainField = windowNewTextField(container, 16, &params);
	windowRegisterEventHandler(domainField, &eventHandler);
	params.gridY += 1;

	updateHostName();

	params.gridX = 0;
	params.gridY += 1;
	params.gridWidth = 2;
	params.padTop = 5;
	params.padBottom = 0;
	params.orientationX = orient_center;
	params.flags &= ~WINDOW_COMPFLAG_FIXEDWIDTH;

	buffer = malloc(MAXSTRINGLENGTH);
	if (!buffer)
		return (status = ERR_MEMORY);

	// Did the user specify a device name?
	if (arg)
	{
		// Get the device information
		status = getDevString(arg, buffer);
		if (status < 0)
		{
			free(buffer);
			return (status);
		}

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
				status = getDevString(name, buffer);
				if (status < 0)
				{
					free(buffer);
					return (status);
				}

				deviceLabel[count] = windowNewTextLabel(window, buffer,
					&params);
				params.gridY += 1;
			}
		}
		else
		{
			windowNewTextLabel(window, NO_DEVICES, &params);
			params.gridY += 1;
		}
	}

	free(buffer);

	// Create an 'OK' button
	params.gridWidth = 1;
	params.padBottom = 5;
	params.orientationX = orient_right;
	params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
	okButton = windowNewButton(window, OK, NULL, &params);
	windowRegisterEventHandler(okButton, &eventHandler);
	windowComponentFocus(okButton);

	// Create a 'Cancel' button
	params.gridX = 1;
	params.orientationX = orient_left;
	cancelButton = windowNewButton(window, CANCEL, NULL, &params);
	windowRegisterEventHandler(cancelButton, &eventHandler);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);

	windowSetVisible(window, 1);

	return (status = 0);
}


int main(int argc, char *argv[])
{
	int status = 0;
	char opt;
	int enable = 0, disable = 0;
	char *arg = NULL;
	disk sysDisk;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("ifconfig");

	// Are graphics enabled?
	graphics = graphicsAreEnabled();

	// Check options
	while (strchr("deT?", (opt = getopt(argc, argv, "deT"))))
	{
		switch (opt)
		{
			case 'd':
				// Disable networking
				disable = 1;
				break;

			case 'e':
				// Enable networking
				enable = 1;
				break;

			case 'T':
				// Force text mode
				graphics = 0;
				break;

			default:
				error(_("Unknown option '%c'"), optopt);
				return (status = ERR_INVALID);
		}
	}

	numDevices = networkDeviceGetCount();
	if (numDevices < 0)
	{
		error("%s", _("Can't get the count of network devices"));
		return (numDevices);
	}

	// Is the last argument a non-option?
	if ((argc > 1) && (argv[argc - 1][0] != '-'))
		arg = argv[argc - 1];

	// Find out whether we are currently running on a read-only filesystem
	memset(&sysDisk, 0, sizeof(disk));
	if (!fileGetDisk(KERNEL_DEFAULT_CONFIG, &sysDisk))
		readOnly = sysDisk.readOnly;

	enabled = networkEnabled();

	if (graphics)
	{
		status = constructWindow(arg);
		if (status >= 0)
			windowGuiRun();
	}
	else
	{
		if (disable && enabled)
			networkShutdown();
		else if (enable && !enabled)
			networkEnable();

		status = printDevices(arg);
	}

	return (status);
}

