//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  kernelInterrupt.h
//
	
// This is the header file to go with the kernel's interrupt handlers

#if !defined(_KERNELINTERRUPT_H)

// Interrupt numbers for interrupts we care about
#define INTERRUPT_VECTORS			16
#define INTERRUPT_NUM_SYSTIMER		0x00
#define INTERRUPT_NUM_KEYBOARD		0x01
#define INTERRUPT_NUM_FLOPPY		0x06
#define INTERRUPT_NUM_MOUSE			0x0C
#define INTERRUPT_NUM_PRIMARYIDE	0x0E
#define INTERRUPT_NUM_SECONDARYIDE	0x0F

int kernelInterruptInitialize(void);
void *kernelInterruptGetHandler(int);
int kernelInterruptHook(int, void *);
int kernelProcessingInterrupt(void);
int kernelInterruptGetCurrent(void);
void kernelInterruptSetCurrent(int);
void kernelInterruptClearCurrent(void);

#define _KERNELINTERRUPT_H
#endif
