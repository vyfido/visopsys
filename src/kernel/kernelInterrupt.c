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
//  kernelInterrupt.c
//

// Interrupt handling routines for basic exceptions and hardware interfaces.

#include "kernelInterrupt.h"
#include "kernelProcessorX86.h"
#include "kernelDescriptor.h"
#include "kernelMultitasker.h"
#include "kernelError.h"

int kernelProcessingInterrupt = 0;
static int initialized = 0;


static void intHandlerUnimp(void)
{
  // This is the "unimplemented interrupt" handler

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 1;

  // Issue an end-of-interrupt (EOI) to the slave PIC
  kernelProcessorOutPort8(0xA0, 0x20);
  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


// All the interrupt vectors
static void *vectorList[INTERRUPT_VECTORS] = {
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp,
    intHandlerUnimp
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelInterruptInitialize(void)
{
  // This function is called once at startup time to install all
  // of the appropriate interrupt vectors into the Interrupt Descriptor
  // Table.  Returns 0 on success, negative otherwise.

  int status = 0;
  int count;
    
  // Set the kernel's exception handler code to handle exceptions as
  // an interrupt handler until multitasking is enabled.  After that,
  // exceptions will point to a task gate for the exception handler.
  for (count = 0; count < 19; count ++)
    {
      status =
	kernelDescriptorSetIDTInterruptGate(count, &kernelExceptionHandler);
      if (status < 0) 
	return (status);
    }

  // Initialize the rest of the table with the vector for the standard
  // "unimplemented" interrupt vector
  for (count = 19; count < IDT_SIZE; count ++)
    kernelDescriptorSetIDTInterruptGate(count, intHandlerUnimp);

  // Note that we've been called
  initialized = 1;

  // Return success
  return (status = 0);
}


void *kernelInterruptGetHandler(int intNumber)
{
  // Returns the address of the handler for the requested interrupt.

  if (!initialized)
    return (NULL);

  if ((intNumber < 0) || (intNumber >= INTERRUPT_VECTORS))
    return (NULL);

  return (vectorList[intNumber]);
}


int kernelInterruptHook(int intNumber, void *handlerAddress)
{
  // This allows the requested interrupt number to be hooked by a new
  // handler.  At the moment it doesn't chain them, so anyone who calls
  // this needs to fully implement the handler, or else chain them manually
  // using the 'get handler' function, above.  If you don't know what this
  // means, please stay away from hooking interrupts!  ;)

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if ((intNumber < 0) || (intNumber >= INTERRUPT_VECTORS))
    return (status = ERR_INVALID);

  status =
    kernelDescriptorSetIDTInterruptGate((0x20 + intNumber), handlerAddress);
  if (status < 0)
    return (status);

  vectorList[intNumber] = handlerAddress;
  return (status = 0);
}
