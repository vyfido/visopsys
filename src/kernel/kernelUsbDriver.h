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
//  kernelUsbDriver.h
//

#if !defined(_KERNELUSBDRIVER_H)

#include "kernelBus.h"
#include "kernelLinkedList.h"

#define USB_MAX_INTERFACES			8
#define USB_MAX_ENDPOINTS			16

// USB descriptor types
#define USB_DESCTYPE_DEVICE			1
#define USB_DESCTYPE_CONFIG			2
#define USB_DESCTYPE_STRING			3
#define USB_DESCTYPE_INTERFACE		4
#define USB_DESCTYPE_ENDPOINT		5
// USB 2.0+
#define USB_DESCTYPE_DEVICEQUAL		6
#define USB_DESCTYPE_OTHERSPEED		7
#define USB_DESCTYPE_INTERPOWER		8
// Class-specific
#define USB_DESCTYPE_HID			33
#define USB_DESCTYPE_HIDREPORT		34
#define USB_DESCTYPE_HIDPHYSDESC	35
#define USB_DESCTYPE_HUB			41

// Endpoint attributes
#define USB_ENDP_ATTR_MASK			0x03
#define USB_ENDP_ATTR_CONTROL		0x00
#define USB_ENDP_ATTR_ISOCHRONOUS	0x01
#define USB_ENDP_ATTR_BULK			0x02
#define USB_ENDP_ATTR_INTERRUPT		0x03

// USB commands (control transfer types)
#define USB_GET_STATUS				0
#define USB_CLEAR_FEATURE			1
#define USB_GET_STATE				2
#define USB_SET_FEATURE				3
#define USB_SET_ADDRESS				5
#define USB_GET_DESCRIPTOR			6
#define USB_SET_DESCRIPTOR			7
#define USB_GET_CONFIGURATION		8
#define USB_SET_CONFIGURATION		9
#define USB_GET_INTERFACE			10
#define USB_SET_INTERFACE			11
#define USB_SYNCH_FRAME				12
// Class-specific
#define USB_HID_GET_REPORT			1
#define USB_HID_GET_IDLE			2
#define USB_HID_GET_PROTOCOL		3
#define USB_HID_SET_REPORT			9
#define USB_HID_SET_IDLE			10
#define USB_HID_SET_PROTOCOL		11
#define USB_HUB_CLEAR_TT_BUFFER		8
#define USB_HUB_RESET_TT			9
#define USB_HUB_GET_TT_STATE		10
#define USB_HUB_STOP_TT				11
#define USB_MASSSTORAGE_RESET		0xFF

// USB device request types
#define USB_DEVREQTYPE_HOST2DEV		0x00
#define USB_DEVREQTYPE_DEV2HOST		0x80
#define USB_DEVREQTYPE_STANDARD		0x00
#define USB_DEVREQTYPE_CLASS		0x20
#define USB_DEVREQTYPE_VENDOR		0x40
#define USB_DEVREQTYPE_RESERVED		0x60
#define USB_DEVREQTYPE_DEVICE		0x00
#define USB_DEVREQTYPE_INTERFACE	0x01
#define USB_DEVREQTYPE_ENDPOINT		0x02
#define USB_DEVREQTYPE_OTHER		0x03

// USB features (for set/clear commands)
#define USB_FEATURE_ENDPOINTHALT	0x00
#define USB_FEATURE_REMOTEWAKEUP	0x01
#define USB_FEATURE_TESTMODE		0x02

#define USB_INVALID_CLASSCODE		-1
#define USB_INVALID_SUBCLASSCODE	-2

// Values for the 'packet ID' field of USB transfer descriptors
#define USB_PID_IN					0x69
#define USB_PID_OUT					0xE1
#define USB_PID_SETUP				0x2D

// USB mass storage command and status block signatures
#define USB_CMDBLOCKWRAPPER_SIG		0x43425355
#define USB_CMDSTATUSWRAPPER_SIG	0x53425355

// The 4 USB data transfer types
typedef enum {
	usbxfer_isochronous, usbxfer_interrupt, usbxfer_control, usbxfer_bulk

} usbXferType;

// The 4 USB device speeds
typedef enum {
	usbspeed_unknown, usbspeed_low, usbspeed_full, usbspeed_high, usbspeed_super

} usbDevSpeed;

typedef struct {
	unsigned char requestType;
	unsigned char request;
	unsigned short value;
	unsigned short index;
	unsigned short length;

} __attribute__((packed)) usbDeviceRequest;

typedef struct {
	unsigned char descLength;		// Number of bytes in this descriptor
	unsigned char descType;			// Type, DEVICE descriptor type
	unsigned short usbVersion;		// BCD USB version supported
	unsigned char deviceClass;		// Major device class
	unsigned char deviceSubClass;	// Minor device class
	unsigned char deviceProtocol;	// Device protocol
	unsigned char maxPacketSize0;	// Max packet size (8/16/32/64) for endpnt 0
	unsigned short vendorId;		// Vendor ID
	unsigned short productId;		// Product ID
	unsigned short deviceVersion;	// BCD device version
	unsigned char manuStringIdx;	// Index of manufacturer string
	unsigned char prodStringIdx;	// Index of product string
	unsigned char serStringIdx;		// Index of serial number string 
	unsigned char numConfigs;		// Number of possible configurations

} __attribute__((packed)) usbDeviceDesc;

typedef struct {
	unsigned char descLength;		// Number of bytes in this descriptor
	unsigned char descType;			// Type, DEVICEQUAL descriptor type
	unsigned short usbVersion;		// BCD USB version supported
	unsigned char deviceClass;		// Major device class
	unsigned char deviceSubClass;	// Minor device class
	unsigned char deviceProtocol;	// Device protocol
	unsigned char maxPacketSize0;	// Max packet size (8/16/32/64) for endpnt 0
	unsigned char numConfigs;		// Number of possible configurations
	unsigned char res;				// Reserved, must be zero

} __attribute__((packed)) usbDevQualDesc;

typedef struct {
	unsigned char descLength;		// Number of bytes in this descriptor
	unsigned char descType;			// Type, CONFIGURATION descriptor type
	unsigned short totalLength;		// Total length returned for this config
	unsigned char numInterfaces;	// Number of interfaces in this config
	unsigned char confValue;		// Value for 'set config' requests
	unsigned char confStringIdx;	// Index of config descriptor string
	unsigned char attributes;		// Bitmap of attributes
	unsigned char maxPower;			// Max consumption in this config

} __attribute__((packed)) usbConfigDesc;

typedef struct {
	unsigned char descLength;		// Number of bytes in this descriptor
	unsigned char descType;			// Type, INTERFACE descriptor type
	unsigned char interNum;			// Number of interface
	unsigned char altSetting;		// Alternate setting for interface
	unsigned char numEndpoints;		// Endpoints that use this interface
	unsigned char interClass;		// Interface class
	unsigned char interSubClass;	// Interface subclass
	unsigned char interProtocol;	// Interface protocol code
	unsigned char interStringIdx;	// Index of interface descriptor string

} __attribute__((packed)) usbInterDesc;

typedef struct {
	unsigned char descLength;		// Number of bytes in this descriptor
	unsigned char descType;			// Type, ENDPOINT descriptor type
	unsigned char endpntAddress;	// Endpoint address
	unsigned char attributes;		// Bitmap of attributes
	unsigned short maxPacketSize;	// Max packet size for this endpoint
	unsigned char interval;			// ms interval for enpoint data polling

} __attribute__((packed)) usbEndpointDesc;

typedef struct {
	unsigned char descLength;		// Number of bytes in this descriptor
	unsigned char descType;			// Type, STRING descriptor type
	unsigned char string[];			// String data, not NULL-terminated

} __attribute__((packed)) usbStringDesc;

typedef struct {
	unsigned char descLength;		// Number of bytes in this descriptor
	unsigned char descType;			// Type, HUB descriptor type
	unsigned char numPorts;			// Number of ports on the hub
	unsigned short hubAttrs;		// Bitmap of hub characteristics
	unsigned char pwrOn2PwrGood;	// 2ms intervals till power stable on a port
	unsigned char maxPower;			// Max consumption of the controller
	unsigned devRemovable[];		// Bitmap of removable devices

} __attribute__((packed)) usbHubDesc;

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

} usbTransaction;

// Forward declarations, where necessary
struct _usbController;
struct _usbHub;

typedef volatile struct {
	volatile struct _usbController *controller;
	volatile struct _usbHub *hub;
	kernelDevice dev;
	unsigned char port;
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
		unsigned char endpntAddress;
		unsigned char toggle;
	} dataToggle[USB_MAX_ENDPOINTS];
	void *claimed;
	void *data;

} usbDevice;

typedef struct {
	unsigned short status;
	unsigned short change;

} __attribute__((packed)) usbHubStatus;

typedef struct {
	unsigned short status;
	unsigned short change;

} __attribute__((packed)) usbPortStatus;

typedef volatile struct _usbHub {
	volatile struct _usbController *controller;
	usbDevice *usbDev;
	kernelBusTarget *busTarget;
	usbHubDesc hubDesc;
	unsigned char *changeBitmap;
	int gotInterrupt;
	int doneColdDetect;
	usbHubStatus hubStatus;
	usbPortStatus *portStatus;
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
	unsigned char num;
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
	int (*unschedInterrupt)(volatile struct _usbController *, usbDevice *);

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

typedef struct {
	unsigned signature;
	unsigned tag;
	unsigned dataLength;
	unsigned char flags;
	unsigned char lun;
	unsigned char cmdLength;
	unsigned char cmd[16];
  
} __attribute__((packed)) usbCmdBlockWrapper;

typedef struct {
	unsigned signature;
	unsigned tag;
	unsigned dataResidue;
	unsigned char status;
  
} __attribute__((packed)) usbCmdStatusWrapper;

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
int kernelUsbSetupDeviceRequest(usbTransaction *, usbDeviceRequest *);
int kernelUsbControlTransfer(usbDevice *, unsigned char, unsigned short,
	unsigned short, unsigned short, void *,	unsigned *);
int kernelUsbScheduleInterrupt(usbDevice *, unsigned char, int, unsigned,
	void (*)(usbDevice *, void *, unsigned));
int kernelUsbUnscheduleInterrupt(usbDevice *);

// Detection routines for different driver types
kernelDevice *kernelUsbUhciDetect(kernelBusTarget *, kernelDriver *);
kernelDevice *kernelUsbOhciDetect(kernelBusTarget *, kernelDriver *);
kernelDevice *kernelUsbEhciDetect(kernelBusTarget *, kernelDriver *);
kernelDevice *kernelUsbXhciDetect(kernelBusTarget *, kernelDriver *);

#define _KERNELUSBDRIVER_H
#endif
