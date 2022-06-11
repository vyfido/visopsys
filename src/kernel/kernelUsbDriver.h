//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  kernelUsbDriver.h
//

#if !defined(_KERNELUSBDRIVER_H)

#include "kernelBus.h"
#include "kernelLinkedList.h"
#include <sys/usb.h>

#define USB_STD_TIMEOUT_MS		2000

// The 4 USB controller types
typedef enum {
	usb_ohci, usb_uhci, usb_ehci, usb_xhci

} usbControllerType;

// The 4 USB device speeds
typedef enum {
	usbspeed_unknown, usbspeed_low, usbspeed_full, usbspeed_high,
	usbspeed_super

} usbDevSpeed;

// The 3 USB protocol levels
typedef enum {
	usbproto_unknown, usbproto_usb1, usbproto_usb2, usbproto_usb3

} usbProtocol;

// The 4 USB data transfer types
typedef enum {
	usbxfer_isochronous, usbxfer_interrupt, usbxfer_control, usbxfer_bulk

} usbXferType;

typedef volatile struct {
	usbXferType type;
	unsigned char address;
	unsigned char endpoint;
	struct {
		unsigned char requestType;
		unsigned char request;
		unsigned short value;
		unsigned short index;
	} control;
	unsigned length;
	void *buffer;
	unsigned bytes;
	unsigned char pid;
	unsigned timeout;

} usbTransaction;

// Forward declarations, where necessary
struct _usbController;
struct _usbHub;

typedef volatile struct {
	volatile struct _usbController *controller;
	volatile struct _usbHub *hub;
	kernelDevice dev;
	int rootPort;
	int hubAddress;
	int hubDepth;
	int hubPort;
	unsigned routeString;
	usbDevSpeed speed;
	unsigned char address;
	unsigned short usbVersion;
	unsigned char classCode;
	unsigned char subClassCode;
	unsigned char protocol;
	unsigned short vendorId;
	unsigned short deviceId;
	usbDeviceDesc deviceDesc;
	usbDevQualDesc devQualDesc;
	usbConfigDesc *configDesc;
	usbInterDesc *interDesc[USB_MAX_INTERFACES];
	usbEndpointDesc endpoint0Desc;
	usbEndpointDesc *endpointDesc[USB_MAX_ENDPOINTS];
	int numEndpoints;
	struct {
		unsigned char interNum;
		unsigned char number;
		unsigned char dataToggle;
		unsigned char maxBurst;
	} endpoint[USB_MAX_ENDPOINTS];
	void *claimed;
	void *data;

} usbDevice;

typedef volatile struct _usbHub {
	volatile struct _usbController *controller;
	usbDevice *usbDev;
	kernelBusTarget *busTarget;
	usbHubDesc hubDesc;
	unsigned char *changeBitmap;
	int gotInterrupt;
	int doneColdDetect;
	usbEndpointDesc *intrInDesc;
	unsigned char intrInEndpoint;
	kernelLinkedList devices;

	// Functions for managing the hub
	void (*detectDevices)(volatile struct _usbHub *, int);
	void (*threadCall)(volatile struct _usbHub *);

} usbHub;

typedef volatile struct _usbController {
	kernelBus *bus;
	kernelDevice *dev;
	int num;
	usbControllerType type;
	unsigned short usbVersion;
	int interruptNum;
	unsigned char addressCounter;
	usbHub hub;
	lock lock;
	void *data;

	// Functions provided by the specific USB root hub driver
	int (*reset)(volatile struct _usbController *);
	int (*interrupt)(volatile struct _usbController *);
	int (*queue)(volatile struct _usbController *, usbDevice *,
		usbTransaction *, int);
	int (*schedInterrupt)(volatile struct _usbController *, usbDevice *,
		unsigned char, int, unsigned,
		void (*)(usbDevice *, void *, unsigned));
	int (*deviceRemoved)(volatile struct _usbController *, usbDevice *);

} usbController;

typedef struct {
	int subClassCode;
	const char name[32];
	int systemClassCode;
	int systemSubClassCode;

} usbSubClass;

typedef struct {
	int classCode;
	const char name[32];
	usbSubClass *subClasses;

} usbClass;

// Make our proprietary USB target code
#define usbMakeTargetCode(controller, address, endpoint)			\
	((((controller) & 0xFF) << 16) | (((address) & 0xFF) << 8) |	\
	((endpoint) & 0xFF))

// Translate a target code back to controller, address, endpoint
#define usbMakeContAddrEndp(targetCode, controller, address, endpoint)	\
	{  (controller) = (((targetCode) >> 16) & 0xFF);					\
		(address) = (((targetCode) >> 8) & 0xFF);						\
		(endpoint) = ((targetCode) & 0xFF);  }

static inline const char *usbDevSpeed2String(usbDevSpeed speed) {	\
	switch (speed) {												\
		case usbspeed_low: return "low";							\
		case usbspeed_full: return "full";							\
		case usbspeed_high: return "high";							\
		case usbspeed_super: return "super";						\
		default: return "unknown"; } }

// Functions exported by kernelUsbDriver.c
int kernelUsbInitialize(void);
int kernelUsbShutdown(void);
usbClass *kernelUsbGetClass(int);
usbSubClass *kernelUsbGetSubClass(usbClass *, int, int);
int kernelUsbGetClassName(int, int, int, char **, char **);
void kernelUsbAddHub(usbHub *, int);
int kernelUsbDevConnect(usbController *, usbHub *, int, usbDevSpeed, int);
void kernelUsbDevDisconnect(usbController *, usbHub *, int);
usbDevice *kernelUsbGetDevice(int);
usbEndpointDesc *kernelUsbGetEndpointDesc(usbDevice *, unsigned char);
volatile unsigned char *kernelUsbGetEndpointDataToggle(usbDevice *,
	unsigned char);
int kernelUsbSetDeviceConfig(usbDevice *);
int kernelUsbSetupDeviceRequest(usbTransaction *, usbDeviceRequest *);
int kernelUsbControlTransfer(usbDevice *, unsigned char, unsigned short,
	unsigned short, unsigned char, unsigned short, void *,	unsigned *);
int kernelUsbScheduleInterrupt(usbDevice *, unsigned char, int, unsigned,
	void (*)(usbDevice *, void *, unsigned));

// Detection routines for different driver types
kernelDevice *kernelUsbUhciDetect(kernelBusTarget *, kernelDriver *);
kernelDevice *kernelUsbOhciDetect(kernelBusTarget *, kernelDriver *);
kernelDevice *kernelUsbEhciDetect(kernelBusTarget *, kernelDriver *);
kernelDevice *kernelUsbXhciDetect(kernelBusTarget *, kernelDriver *);

#define _KERNELUSBDRIVER_H
#endif

