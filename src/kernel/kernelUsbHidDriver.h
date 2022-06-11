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
//  kernelUsbHidDriver.h
//
	
#if !defined(_KERNELUSBHIDDRIVER_H)

#include "kernelUsbDriver.h"

#define USB_HID_GET_REPORT    0x01
#define USB_HID_GET_IDLE      0x02
#define USB_HID_GET_PROTOCOL  0x03
#define USB_HID_SET_REPORT    0x09
#define USB_HID_SET_IDLE      0x0A
#define USB_HID_SET_PROTOCOL  0x0B

//#define USB_HID_DEBUG 1

typedef struct {
  unsigned char descLength;     // Number of bytes in this descriptor
  unsigned char descType;       // Type, HID descriptor type
  unsigned short hidVersion;    // BCD version of HID spec
  unsigned char countryCode;    // Hardware target country
  unsigned char numDescriptors; // Number of HID class descriptors to follow
  unsigned char repDescType;    // Report descriptor type
  unsigned short repDescLength; // Report descriptor total length

} __attribute__((packed)) usbHidDesc;

typedef enum {
  hid_mouse, hid_keyboard
} hidType;

typedef struct {
  hidType type;
  int target;
  kernelDevice dev;
  usbDevice usbDev;
  usbHidDesc hidDesc;
  usbEndpointDesc *intIn;
  unsigned char intInEndpoint;

} hidDevice;

typedef struct {
  unsigned char buttons;
  unsigned char xChange;
  unsigned char yChange;
  unsigned char devSpec[9];

} usbHidMouseData;

#define _KERNELUSBHIDDRIVER_H
#endif
