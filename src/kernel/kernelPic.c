//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  kernelPic.c
//

#include "kernelPic.h"
#include "kernelProcessorX86.h"
#include "kernelError.h"
#include <string.h>

static kernelDevice *systemPic = NULL;
static kernelPicOps *ops = NULL;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelPicInitialize(kernelDevice *dev)
{
  int status = 0;

  if (dev == NULL)
    {
      kernelError(kernel_error, "The PIC device is NULL");
      return (status = ERR_NOTINITIALIZED);
    }

  systemPic = dev;

  if ((systemPic->driver == NULL) || (systemPic->driver->ops == NULL))
    {
      kernelError(kernel_error, "The PIC driver or ops are NULL");
      return (status = ERR_NULLPARAMETER);
    }

  ops = systemPic->driver->ops;

  // Enable interrupts now.
  kernelProcessorEnableInts();

  return (status = 0);
}


int kernelPicEndOfInterrupt(int interruptNumber)
{
  // This instructs the PIC to end the current interrupt.  Note that the
  // interrupt number parameter is merely so that the driver can determine
  // which controller(s) to send the command to.

  int status = 0;

  if (systemPic == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Ok, now we can call the routine.
  if (ops->driverEndOfInterrupt)
    status = ops->driverEndOfInterrupt(interruptNumber);

  return (status);
}


int kernelPicMask(int interruptNumber, int on)
{
  // This instructs the PIC to enable (on) or mask the interrupt.

  int status = 0;

  if (systemPic == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Ok, now we can call the routine.
  if (ops->driverMask)
    status = ops->driverMask(interruptNumber, on);

  return (status);
}


int kernelPicGetActive(void)
{
  // This asks the PIC for the currently-active interrupt

  int interruptNumber = 0;

  if (systemPic == NULL)
    return (interruptNumber = ERR_NOTINITIALIZED);

  // Ok, now we can call the routine.
  if (ops->driverGetActive)
    interruptNumber = ops->driverGetActive();

  return (interruptNumber);
}
