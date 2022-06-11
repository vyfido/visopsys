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
//  kernelUsbHidDriver.h
//
	
#if !defined(_KERNELUSBHIDDRIVER_H)

#include "kernelUsbDriver.h"

// Bit positions for the keyboard modifier byte
#define USB_HID_KEYBOARD_RIGHTGUI    0x80
#define USB_HID_KEYBOARD_RIGHTALT    0x40
#define USB_HID_KEYBOARD_RIGHTSHIFT  0x20
#define USB_HID_KEYBOARD_RIGHTCTRL   0x10
#define USB_HID_KEYBOARD_LEFTGUI     0x08
#define USB_HID_KEYBOARD_LEFTALT     0x04
#define USB_HID_KEYBOARD_LEFTSHIFT   0x02
#define USB_HID_KEYBOARD_LEFTCTRL    0x01

// Bit positions for mouse buttons
#define USB_HID_MOUSE_RIGHTBUTTON    0x04
#define USB_HID_MOUSE_MIDDLEBUTTON   0x02
#define USB_HID_MOUSE_LEFTBUTTON     0x01

typedef struct {
	unsigned char descLength;		// Number of bytes in this descriptor
	unsigned char descType;			// Type, HID descriptor type
	unsigned short hidVersion;		// BCD version of HID spec
	unsigned char countryCode;		// Hardware target country
	unsigned char numDescriptors;	// Number of HID class descriptors to follow
	unsigned char repDescType;		// Report descriptor type
	unsigned short repDescLength;	// Report descriptor total length

} __attribute__((packed)) usbHidDesc;

typedef enum {
	hid_mouse, hid_keyboard, hid_any
} hidType;

typedef struct {
	unsigned char modifier;
	unsigned char res;
	unsigned char code[6];

} __attribute__((packed)) usbHidKeyboardData;

typedef struct {
	unsigned char buttons;
	char xChange;
	char yChange;
	unsigned char devSpec[];

} __attribute__((packed)) usbHidMouseData;

typedef struct {
	hidType type;
	kernelBusTarget *busTarget;
	usbDevice *usbDev;
	usbHidDesc hidDesc;
	unsigned char interNum;
	usbEndpointDesc *intrInDesc;
	unsigned char intrInEndpoint;
	usbHidKeyboardData oldKeyboardData;
	unsigned keyboardFlags;
	unsigned char oldMouseButtons;

} hidDevice;

#define _KERNELUSBHIDDRIVER_H
#endif
