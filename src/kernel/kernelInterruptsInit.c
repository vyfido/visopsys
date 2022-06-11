//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  kernelInterruptsInit.c
//
	
// This file contains the C functions used to install the interrupt vectors
// for protected mode

#include "kernelInterruptsInit.h"
#include "kernelDescriptor.h"
#include "kernelMultitasker.h"
#include <sys/errors.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelInterruptVectorsInstall(void)
{
  // This function is called once at startup time to install all
  // of the appropriate interrupt vectors into the Interrupt Descriptor
  // Table.  Returns 0 on success, negative otherwise.

  int status = 0;
  static int calledOnce = 0;
  int count;
    
  void *vectorList[] = 
  {
    kernelInterruptHandler20,
    kernelInterruptHandler21,
    kernelInterruptHandlerUnimp,
    kernelInterruptHandlerUnimp,
    kernelInterruptHandlerUnimp,
    kernelInterruptHandler25,
    kernelInterruptHandler26,
    kernelInterruptHandler27,
    kernelInterruptHandler28,
    kernelInterruptHandler29,
    kernelInterruptHandler2A,
    kernelInterruptHandler2B,
    kernelInterruptHandler2C,
    kernelInterruptHandler2D,
    kernelInterruptHandler2E,
    kernelInterruptHandler2F
  };

  // Make sure we haven't already been called
  if (calledOnce)
    return (status = ERR_ALREADY);

  // Note that we've been called
  calledOnce = 1;

  // Initialize the entire table with the vector for the standard
  // "unimplemented" interrupt vector
  for (count = 0; count < IDT_SIZE; count ++)
    kernelDescriptorSetIDTInterruptGate(count, kernelInterruptHandlerUnimp);

  // OK, we need to begin installing the "defined" or "implemented" 
  // interrupt vectors into the IDT.  We do this with two loops of calls
  for (count = 0; count < 19; count ++)
    {
      status =
	kernelDescriptorSetIDTInterruptGate(count, &kernelExceptionHandler);
      if (status < 0) 
	return (status);
    }

  for (count = 0; count < 16; count ++)
    {
      status = kernelDescriptorSetIDTInterruptGate((0x20 + count), 
						   vectorList[count]);
      if (status < 0) 
	return (status);
    }

  // Return success
  return (status = 0);
}

