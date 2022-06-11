//
//	Visopsys
//	Copyright (C) 1998-2014 J. Andrew McLaughlin
//
//	This library is free software; you can redistribute it and/or modify it
//	under the terms of the GNU Lesser General Public License as published by
//	the Free Software Foundation; either version 2.1 of the License, or (at
//	your option) any later version.
//
//	This library is distributed in the hope that it will be useful, but
//	WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU Lesser
//	General Public License for more details.
//
//	You should have received a copy of the GNU Lesser General Public License
//	along with this library; if not, write to the Free Software Foundation,
//	Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//	usb.h
//

#if !defined(_USB_H)

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

// USB mass storage CSW status codes
#define USB_CMDSTATUS_GOOD			0x00
#define USB_CMDSTATUS_FAILED		0x01
#define USB_CMDSTATUS_PHASEERROR	0x02

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
	unsigned char maxPacketSize0;	// Max packet size (8/16/32/64) for endpt 0
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
	unsigned char maxPacketSize0;	// Max packet size (8/16/32/64) for endpt 0
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
	unsigned char pwrOn2PwrGood;	// 2ms intervals until port power stable
	unsigned char maxPower;			// Max consumption of the controller
	unsigned devRemovable[];		// Bitmap of removable devices

} __attribute__((packed)) usbHubDesc;

#define _USB_H
#endif

