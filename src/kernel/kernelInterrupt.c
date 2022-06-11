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
//  kernelInterrupt.c
//

// Interrupt handling routines for basic exceptions and hardware interfaces.

#include "kernelInterrupt.h"
#include "kernelProcessorX86.h"
#include "kernelDescriptor.h"
#include "kernelMultitasker.h"
#include "kernelError.h"

int kernelProcessingInterrupt = 0;
int kernelProcessingException = 0;
static int initialized = 0;


static inline void exHandlerX(int exceptionNum)
{
  unsigned stackAddress = 0;
  unsigned exceptionAddress = 0;

  kernelProcessorExceptionEnter(stackAddress, exceptionAddress);

  kernelExceptionHandler(exceptionNum, exceptionAddress);

  // If the exception is handled, then this code is reached and we return
  // to the address of the exception.
  kernelProcessorExceptionExit(stackAddress);
}


static void exHandler0(void) { exHandlerX(0); }
static void exHandler1(void) { exHandlerX(1); }
static void exHandler2(void) { exHandlerX(2); }
static void exHandler3(void) { exHandlerX(3); }
static void exHandler4(void) { exHandlerX(4); }
static void exHandler5(void) { exHandlerX(5); }
static void exHandler6(void) { exHandlerX(6); }
static void exHandler7(void) { exHandlerX(7); }
static void exHandler8(void) { exHandlerX(8); }
static void exHandler9(void) { exHandlerX(9); }
static void exHandler10(void) { exHandlerX(10); }
static void exHandler11(void) { exHandlerX(11); }
static void exHandler12(void) { exHandlerX(12); }
static void exHandler13(void) { exHandlerX(13); }
static void exHandler14(void) { exHandlerX(14); }
static void exHandler15(void) { exHandlerX(15); }
static void exHandler16(void) { exHandlerX(16); }
static void exHandler17(void) { exHandlerX(17); }
static void exHandler18(void) { exHandlerX(18); }


static void intHandlerUnimp(void)
{
  // This is the "unimplemented interrupt" handler

  void *address = NULL;

  kernelProcessorIsrEnter(address);
  kernelProcessingInterrupt = 1;

  // Issue an end-of-interrupt (EOI) to the slave PIC
  kernelProcessorOutPort8(0xA0, 0x20);
  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit(address);
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
    
  // Set all the exception handlers
  kernelDescriptorSetIDTInterruptGate(0, &exHandler0);
  kernelDescriptorSetIDTInterruptGate(1, &exHandler1);
  kernelDescriptorSetIDTInterruptGate(2, &exHandler2);
  kernelDescriptorSetIDTInterruptGate(3, &exHandler3);
  kernelDescriptorSetIDTInterruptGate(4, &exHandler4);
  kernelDescriptorSetIDTInterruptGate(5, &exHandler5);
  kernelDescriptorSetIDTInterruptGate(6, &exHandler6);
  kernelDescriptorSetIDTInterruptGate(7, &exHandler7);
  kernelDescriptorSetIDTInterruptGate(8, &exHandler8);
  kernelDescriptorSetIDTInterruptGate(9, &exHandler9);
  kernelDescriptorSetIDTInterruptGate(10, &exHandler10);
  kernelDescriptorSetIDTInterruptGate(11, &exHandler11);
  kernelDescriptorSetIDTInterruptGate(12, &exHandler12);
  kernelDescriptorSetIDTInterruptGate(13, &exHandler13);
  kernelDescriptorSetIDTInterruptGate(14, &exHandler14);
  kernelDescriptorSetIDTInterruptGate(15, &exHandler15);
  kernelDescriptorSetIDTInterruptGate(16, &exHandler16);
  kernelDescriptorSetIDTInterruptGate(17, &exHandler17);
  kernelDescriptorSetIDTInterruptGate(18, &exHandler18);

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
