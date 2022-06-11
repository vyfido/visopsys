//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
#include "kernelSysTimer.h"
#include "kernelKeyboard.h"
#include "kernelMouse.h"
#include "kernelMultitasker.h"
#include "kernelDescriptor.h"
#include "kernelIdeDriver.h"
#include <sys/errors.h>


extern void kernelFloppyDriverReceiveInterrupt(void);

int kernelProcessingInterrupt = 0;


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


void kernelInterruptHandler20(void)
{
  // This is the system timer interrupt handler

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x20;

  // Issue an end-of-interrupt (EOI) to the PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Call the kernel's generalized driver function
  kernelSysTimerTick();

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler21(void)
{
  // This is the keyboard interrupt handler

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x21;

  // Issue an end-of-interrupt (EOI) to the PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Call the kernel's keyboard driver to handle this data
  kernelKeyboardReadData();

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}

	
void kernelInterruptHandler25(void)
{
  // This is the parallel port 2 interrupt handler

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x25;

  // Issue an end-of-interrupt (EOI) to the PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Nothing

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler26(void)
{
  // This is the floppy drive interrupt

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x26;

  // Issue an end-of-interrupt (EOI) to the PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Call the kernel's floppy disk driver
  kernelFloppyDriverReceiveInterrupt();

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler27(void)
{
  // This is a 'reserved' (unimplemented?) exception handler
	
  static unsigned char data;

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x27;

  // Issue an end-of-interrupt (EOI) to the PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // This interrupt can sometimes occur frivolously from "noise"
  // on the interrupt request lines.  Before we do anything at all,
  // we MUST ensure that the interrupt really occurred.

  // Poll bit 7 in the PIC
  kernelProcessorOutPort8(0x20, 0x0B);
  kernelProcessorDelay();
  kernelProcessorInPort8(0x20, data);

  if (data & 0x80)
    {
      // Nothing
    }

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler28(void)
{
  // This is the real time clock interrupt handler

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x28;

  // Issue an end-of-interrupt (EOI) to the slave PIC
  kernelProcessorOutPort8(0xA0, 0x20);
  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Nothing

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler29(void)
{
  // VGA Retrace interrupt

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x29;

  // Issue an end-of-interrupt (EOI) to the slave PIC
  kernelProcessorOutPort8(0xA0, 0x20);
  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Nothing

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler2A(void)
{
  // This is the 'available 1' interrupt handler

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x2A;

  // Issue an end-of-interrupt (EOI) to the slave PIC
  kernelProcessorOutPort8(0xA0, 0x20);
  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Nothing

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler2B(void)
{
  // This is the 'available 2' interrupt handler

  static unsigned char data;

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x2B;

  // Issue an end-of-interrupt (EOI) to the slave PIC
  kernelProcessorOutPort8(0xA0, 0x20);
  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // This interrupt can sometimes occur frivolously from "noise"
  // on the interrupt request lines.  Before we do anything at all,
  // we MUST ensure that the interrupt really occurred.

  // Poll bit 3 in the PIC
  kernelProcessorOutPort8(0xA0, 0x0B);
  kernelProcessorDelay();
  kernelProcessorInPort8(0xA0, data);

  if (data & 0x08)
    {
      // DON'T print the interrupt message.  This looks like it might be
      // connected somehow to the real-time clock on my K6-2 machine.
      // Issues one of these interrupts every second (by my count).
      ;
    }

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler2C(void)
{
  // This is the mouse interrupt handler

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x2C;

  // Issue an end-of-interrupt (EOI) to the slave PIC
  kernelProcessorOutPort8(0xA0, 0x20);
  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Call the kernel's mouse driver
  kernelMouseReadData();

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler2D(void)
{
  // This is the numeric co-processor error interrupt handler

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x2D;

  // Issue an end-of-interrupt (EOI) to the slave PIC
  kernelProcessorOutPort8(0xA0, 0x20);
  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Nothing

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler2E(void)
{
  // This is the hard disk interrupt handler

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x2E;

  // Issue an end-of-interrupt (EOI) to the slave PIC
  kernelProcessorOutPort8(0xA0, 0x20);
  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Call the kernel's hard disk driver
  kernelIdeDriverReceiveInterrupt();

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}


void kernelInterruptHandler2F(void)
{
  // This is the 'available 3' interrupt handler.  We will be using
  // it for the secondary hard disk controller interrupt.

  static unsigned char data;
	
  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x2F;

  // This interrupt can sometimes occur frivolously from "noise"
  // on the interrupt request lines.  Before we do anything at all,
  // we MUST ensure that the interrupt really occurred.
  kernelProcessorOutPort8(0xA0, 0x0B);
  kernelProcessorInPort8(0xA0, data);
  if (data & 0x80)
    {
      // Issue an end-of-interrupt (EOI) to the slave PIC
      kernelProcessorOutPort8(0xA0, 0x20);
      // Issue an end-of-interrupt (EOI) to the master PIC
      kernelProcessorOutPort8(0x20, 0x20);

      // Call the kernel's hard disk driver
      kernelIdeDriverReceiveInterrupt();
    }

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}

	
void kernelInterruptHandlerUnimp(void)
{
  // This is the "unimplemented interrupt" handler

  kernelProcessorIsrEnter();
  kernelProcessingInterrupt = 0x99;  // Bogus

  // Issue an end-of-interrupt (EOI) to the slave PIC
  kernelProcessorOutPort8(0xA0, 0x20);
  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  // Nothing

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit();
}
