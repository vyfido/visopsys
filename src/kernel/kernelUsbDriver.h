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
//  kernelUsbDriver.h
//

#if !defined(_KERNELUSBDRIVER_H)

#include "kernelBus.h"

#define USB_MAX_CONTROLLERS       2
#define USB_MAX_DEVICES           0x7F
#define USB_MAX_INTERFACES        8
#define USB_MAX_ENDPOINTS         16

// USB class codes
#define USB_CLASSCODE_HUB         0x09

// USB device classes
#define USB_DEVCLASS_DISPLAY      1
#define USB_DEVCLASS_COMMS        2
#define USB_DEVCLASS_AUDIO        3
#define USB_DEVCLASS_STORAGE      4
#define USB_DEVCLASS_HID          5

// USB descriptor types
#define USB_DESCTYPE_DEVICE       1
#define USB_DESCTYPE_CONFIG       2
#define USB_DESCTYPE_STRING       3
#define USB_DESCTYPE_INTERFACE    4
#define USB_DESCTYPE_ENDPOINT     5
// USB 2.0+
#define USB_DESCTYPE_DEVICEQUAL   6
#define USB_DESCTYPE_OTHERSPEED   7
#define USB_DESCTYPE_INTERPOWER   8
// Class-specific
#define USB_DESCTYPE_HID          0x21
#define USB_DESCTYPE_HIDREPORT    0x22
#define USB_DESCTYPE_HIDPHYSDESC  0x23

// USB commands (control transfer types)
#define USB_GET_STATUS            0
#define USB_CLEAR_FEATURE         1
#define USB_SET_FEATURE           3
#define USB_SET_ADDRESS           5
#define USB_GET_DESCRIPTOR        6
#define USB_SET_DESCRIPTOR        7
#define USB_GET_CONFIGURATION     8
#define USB_SET_CONFIGURATION     9
#define USB_GET_INTERFACE         10
#define USB_SET_INTERFACE         11
#define USB_SYNCH_FRAME           12
// Class-specific
#define USB_MASSSTORAGE_RESET     0xFF

// USB device request types
#define USB_DEVREQTYPE_HOST2DEV   0x00
#define USB_DEVREQTYPE_DEV2HOST   0x80
#define USB_DEVREQTYPE_STANDARD   0x00
#define USB_DEVREQTYPE_CLASS      0x20
#define USB_DEVREQTYPE_VENDOR     0x40
#define USB_DEVREQTYPE_RESERVED   0x60
#define USB_DEVREQTYPE_DEVICE     0x00
#define USB_DEVREQTYPE_INTERFACE  0x01
#define USB_DEVREQTYPE_ENDPOINT   0x02
#define USB_DEVREQTYPE_OTHER      0x03

// USB features (for set/clear commands)
#define USB_FEATURE_REMOTEWAKEUP  0x01
#define USB_FEATURE_ENDPOINTHALT  0x00
#define USB_FEATURE_TESTMODE      0x02

#define USB_INVALID_CLASSCODE     -1
#define USB_INVALID_SUBCLASSCODE  -2

// Values for the 'packet ID' field of USB transfer descriptors
#define USB_PID_IN                0x69
#define USB_PID_OUT               0xE1
#define USB_PID_SETUP             0x2D

#define USB_CMDBLOCKWRAPPER_SIG   0x43425355
#define USB_CMDSTATUSWRAPPER_SIG  0x53425355

// The 4 USB data transfer types
typedef enum {
  usbxfer_isochronous, usbxfer_interrupt, usbxfer_control, usbxfer_bulk

} usbxferType;

typedef struct {
  unsigned char requestType;
  unsigned char request;
  unsigned short value;
  unsigned short index;
  unsigned short length;

} __attribute__((packed)) usbDeviceRequest;

typedef struct {
  unsigned char descLength;     // Number of bytes in this descriptor
  unsigned char descType;       // Type, DEVICE descriptor type
  unsigned short usbVersion;    // BCD USB version supported
  unsigned char deviceClass;    // Major device class
  unsigned char deviceSubClass; // Minor device class
  unsigned char deviceProtocol; // Device protocol
  unsigned char maxPacketSize0; // Max packet size (8/16/32/64) for endpoint 0
  unsigned short vendorId;      // Vendor ID
  unsigned short productId;     // Product ID
  unsigned short deviceVersion; // BCD device version
  unsigned char manuStringIdx;  // Index of manufacturer string
  unsigned char prodStringIdx;  // Index of product string
  unsigned char serStringIdx;   // Index of serial number string 
  unsigned char numConfigs;     // Number of possible configurations

} __attribute__((packed)) usbDeviceDesc;

typedef struct {
  unsigned char descLength;     // Number of bytes in this descriptor
  unsigned char descType;       // Type, DEVICEQUAL descriptor type
  unsigned short usbVersion;    // BCD USB version supported
  unsigned char deviceClass;    // Major device class
  unsigned char deviceSubClass; // Minor device class
  unsigned char deviceProtocol; // Device protocol
  unsigned char maxPacketSize0; // Max packet size (8/16/32/64) for endpoint 0
  unsigned char numConfigs;     // Number of possible configurations
  unsigned char res;            // Reserved, must be zero

} __attribute__((packed)) usbDevQualDesc;

typedef struct {
  unsigned char descLength;     // Number of bytes in this descriptor
  unsigned char descType;       // Type, CONFIGURATION descriptor type
  unsigned short totalLength;   // Total length returned for this configuration
  unsigned char numInterfaces;  // Number of interfaces in this configuration
  unsigned char confValue;      // Value for 'set config' requests
  unsigned char confStringIdx;  // Index of config descriptor string
  unsigned char attributes;     // Bitmap of attributes
  unsigned char maxPower;       // Max consumption in this config

} __attribute__((packed)) usbConfigDesc;

typedef struct {
  unsigned char descLength;     // Number of bytes in this descriptor
  unsigned char descType;       // Type, INTERFACE descriptor type
  unsigned char interNum;       // Number of interface
  unsigned char altSetting;     // Alternate setting for interface
  unsigned char numEndpoints;   // Endpoints that use this interface
  unsigned char interClass;     // Interface class
  unsigned char interSubClass;  // Interface subclass
  unsigned char interProtocol;  // Interface protocol code
  unsigned char interStringIdx; // Index of interface descriptor string

} __attribute__((packed)) usbInterDesc;

typedef struct {
  unsigned char descLength;     // Number of bytes in this descriptor
  unsigned char descType;       // Type, ENDPOINT descriptor type
  unsigned char endpntAddress;  // Endpoint address
  unsigned char attributes;     // Bitmap of attributes
  unsigned short maxPacketSize; // Max packet size for this endpoint
  unsigned char interval;       // ms interval for enpoint data polling

} __attribute__((packed)) usbEndpointDesc;

typedef struct {
  unsigned char descLength;     // Number of bytes in this descriptor
  unsigned char descType;       // Type, STRING descriptor type
  unsigned char string[];       // String data, not NULL-terminated

} __attribute__((packed)) usbStringDesc;

typedef struct {
  unsigned char controller;
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
  usbEndpointDesc *endpointDesc[USB_MAX_ENDPOINTS];

} usbDevice;

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
  usbxferType type;
  unsigned char address;
  unsigned char endpoint;
  struct {
    unsigned char requestType;
    unsigned char request;
    unsigned short value;
    unsigned short index;
  } control;
  unsigned short length;
  void *buffer;
  unsigned bytes;
  unsigned char pid;

} usbTransaction;

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

typedef struct {
  kernelDevice *device;
  unsigned char controller;
  unsigned short usbVersion;
  void *ioAddress;
  int interrupt;
  unsigned short portStatus[2];
  unsigned char addressCounter;
  usbDevice *devices[USB_MAX_DEVICES];
  unsigned char numDevices;
  int didEnum;
  void *data;

  // Functions provided by the core USB driver
  void (*getClass) (int, usbClass **);
  void (*getSubClass) (usbClass *, int,	int, usbSubClass **);
  int (*getClassName) (int, int, int, char **, char **);

  // Functions provided by the specific USB root hub driver
  void (*threadCall) (void *);
  int (*transaction) (void *, usbDevice *, usbTransaction *);

} usbRootHub;

// Make our proprietary USB target code
#define usbMakeTargetCode(controller, address, endpoint)                \
  ((((controller) & 0xFF) << 16) | (((address) & 0xFF) << 8) |          \
   ((endpoint) & 0xFF))

// Translate a target code back to controller, address, endpoint
#define usbMakeContAddrEndp(targetCode, controller, address, endpoint)  \
  {  (controller) = (((targetCode) >> 16) & 0xFF);                      \
     (address) = (((targetCode) >> 8) & 0xFF);                          \
     (endpoint) = ((targetCode) & 0xFF);  }

// Functions exported by kernelUsbInitialize.c
int kernelUsbInitialize(void);

// Detection routines for different driver types
kernelDevice *kernelUsbUhciDetect(kernelDevice *, kernelBusTarget *, void *);
kernelDevice *kernelUsbEhciDetect(kernelDevice *, kernelBusTarget *, void *);

#define _KERNELUSBDRIVER_H
#endif
