//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  kernelUsbMouseDriver.h
//

#ifndef _KERNELUSBMOUSEDRIVER_H
#define _KERNELUSBMOUSEDRIVER_H

#include "kernelUsbDriver.h"

// Bit positions for mouse buttons
#define USB_HID_MOUSE_MIDDLEBUTTON	0x04
#define USB_HID_MOUSE_RIGHTBUTTON	0x02
#define USB_HID_MOUSE_LEFTBUTTON	0x01

typedef struct {
	unsigned char buttons;
	char xChange;
	char yChange;
	unsigned char devSpec[];

} __attribute__((packed)) usbMouseData;

typedef struct {
	usbDevice *usbDev;
	kernelDevice dev;
	unsigned char oldMouseButtons;

} usbMouse;

#endif

