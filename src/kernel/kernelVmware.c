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
//  kernelVmware.c
//

#include "kernelVmware.h"
#include "kernelLog.h"
#include <sys/errors.h>
#include <sys/vmware.h>

static int backdoorChecked = 0;
static int backdoorPresent = 0;


static void backdoor(vmwareBackdoorProto *cmd)
{
	__asm__ __volatile__(
		"pushal \n\t"
		"movl %0, %%ebp \n\t"
		"movl (%%ebp), %%eax \n\t"
		"movl 4(%%ebp), %%ebx \n\t"
		"movl 8(%%ebp), %%ecx \n\t"
		"movl 12(%%ebp), %%edx \n\t"
		"movl 16(%%ebp), %%esi \n\t"
		"movl 20(%%ebp), %%edi \n\t"
		"inl %%dx, %%eax \n\t"
		"movl %%edi, 20(%%ebp) \n\t"
		"movl %%esi, 16(%%ebp) \n\t"
		"movl %%edx, 12(%%ebp) \n\t"
		"movl %%ecx, 8(%%ebp) \n\t"
		"movl %%ebx, 4(%%ebp) \n\t"
		"movl %%eax, (%%ebp) \n\t"
		"popal" : : "m" (cmd) : "memory");
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelVmwareCheck(void)
{
	// Returns 1 if the VMware backdoor is present, 0 otherwise

	vmwareBackdoorProto cmd = {
		VMWARE_BACKDOOR_MAGIC,
		0,
		VMWARE_BACKDOORCMD_VERSION,
		VMWARE_BACKDOOR_PORT,
		0, 0
	};

	if (!backdoorChecked)
	{
		backdoor(&cmd);

		if (cmd.regB == VMWARE_BACKDOOR_MAGIC)
			backdoorPresent = 1;

		kernelLog("VMware backdoor is %spresent",
			(backdoorPresent? "" : "not "));

		backdoorChecked = 1;
	}

	return (backdoorPresent);
}


int kernelVmwareMouseGetPos(unsigned short *xPos, unsigned short *yPos)
{
	// Request the current mouse position

	int status = 0;
	vmwareBackdoorProto cmd = {
		VMWARE_BACKDOOR_MAGIC,
		0,
		VMWARE_BACKDOORCMD_GETMOUSEPTR,
		VMWARE_BACKDOOR_PORT,
		0, 0
	};

	// Check params
	if (!xPos || !yPos)
		return (status = ERR_NULLPARAMETER);

	backdoor(&cmd);

	*xPos = (unsigned short)(cmd.regA >> 16);
	*yPos = (unsigned short)(cmd.regA & 0xFFFF);

	return (status = 0);
}


void kernelVmwareMouseSetPos(unsigned short xPos, unsigned short yPos)
{
	// Set the current mouse position

	vmwareBackdoorProto cmd = {
		VMWARE_BACKDOOR_MAGIC,
		0,
		VMWARE_BACKDOORCMD_SETMOUSEPTR,
		VMWARE_BACKDOOR_PORT,
		0, 0
	};

	cmd.regB = (((unsigned) xPos << 16) | yPos);
	backdoor(&cmd);
}

