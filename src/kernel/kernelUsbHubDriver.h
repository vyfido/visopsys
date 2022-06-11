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
//  kernelUsbHubDriver.h
//

#if !defined(_KERNELUSBHUBDRIVER_H)

#include "kernelUsbDriver.h"

// Hub status/change bits.  Things with _V2 suffixes are obsolete in USB 3.0
#define USB_HUBSTAT_LOCPOWER			0x0001
#define USB_HUBSTAT_OVERCURR			0x0002

// Port status/change bits
#define USB_HUBPORTSTAT_CONN			0x0001
#define USB_HUBPORTSTAT_ENABLE			0x0002
#define USB_HUBPORTSTAT_SUSPEND_V2		0x0004
#define USB_HUBPORTSTAT_OVERCURR		0x0008
#define USB_HUBPORTSTAT_RESET			0x0010
#define USB_HUBPORTSTAT_POWER			0x0100
#define USB_HUBPORTSTAT_LOWSPEED_V2		0x0200
// Introduced in USB 2.0
#define USB_HUBPORTSTAT_HIGHSPEED_V2	0x0400
#define USB_HUBPORTSTAT_PORTTEST_V2		0x0800
#define USB_HUBPORTSTAT_PORTINDCONT		0x1000
// Introduced in USB 3.0
#define USB_HUBPORTSTAT_BH_RESET_CH		0x0020
#define USB_HUBPORTSTAT_LINKSTATE_CH	0x0040
#define USB_HUBPORTSTAT_CONFERR_CH		0x0080
#define USB_HUBPORTSTAT_LINKSTATE		0x00E0
#define USB_HUBPORTSTAT_SPEED			0x0E00

// Hub features
#define USB_HUBFEAT_HUBLOCPOWER_CH		0
#define USB_HUBFEAT_HUBOVERCURR_CH		1

// Hub port features
#define USB_HUBFEAT_PORTCONN			0
#define USB_HUBFEAT_PORTOVERCURR		3
#define USB_HUBFEAT_PORTRESET			4
#define USB_HUBFEAT_PORTPOWER			8
#define USB_HUBFEAT_PORTCONN_CH			16
#define USB_HUBFEAT_PORTOVERCURR_CH		19
#define USB_HUBFEAT_PORTRESET_CH		20

// Hub port features obsolete in USB 3.0
#define USB_HUBFEAT_PORTENABLE_V2		1
#define USB_HUBFEAT_PORTSUSPEND_V2		2
#define USB_HUBFEAT_PORTLOWSPEED_V2		9
#define USB_HUBFEAT_PORTENABLE_CH_V2	17
#define USB_HUBFEAT_PORTSUSPEND_CH_V2	18

// Hub port features introduced in USB 3.0
#define USB_HUBFEAT_PORTLINKSTATE		5
#define USB_HUBFEAT_PORTU1TIMEOUT		23
#define USB_HUBFEAT_PORTU2TIMEOUT		24
#define USB_HUBFEAT_PORTLINKSTATE_CH	25
#define USB_HUBFEAT_PORTCONFERR_CH		26
#define USB_HUBFEAT_PORTREMWAKEMASK		27
#define USB_HUBFEAT_PORTBHRESET			28
#define USB_HUBFEAT_PORTBHRESET_CH		29
#define USB_HUBFEAT_PORTFRCELNKPMACC	30

#define _KERNELUSBHUBDRIVER_H
#endif

