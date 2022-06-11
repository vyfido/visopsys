//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  kernelApicDriver.c
//

// Driver for standard Advanced Programmable Interrupt Controllers (APICs)

#include "kernelDriver.h" // Contains my prototypes
#include "kernelApicDriver.h"
#include "kernelCpu.h"
#include "kernelDebug.h"
#include "kernelDevice.h"
#include "kernelError.h"
#include "kernelInterrupt.h"
#include "kernelMalloc.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelPic.h"
#include "kernelSystemDriver.h"
#include <stdio.h>
#include <string.h>
#include <sys/multiproc.h>
#include <sys/processor.h>

#define READSLOTLO(ioApic, num) readIoReg((ioApic), (0x10 + (num * 2)))
#define READSLOTHI(ioApic, num) readIoReg((ioApic), (0x10 + (num * 2) + 1))
#define WRITESLOTLO(ioApic, num, value) \
	writeIoReg((ioApic), (0x10 + (num * 2)), value)
#define WRITESLOTHI(ioApic, num, value) \
	writeIoReg((ioApic), (0x10 + (num * 2) + 1), value)

static volatile void *localApicRegs = NULL;


static int calcVector(int intNumber)
{
	// This looks a bit complicated, so some explanation is in order
	//
	// For APICs, the upper 4 bits specify the priority level, with 0xF being
	// the highest.  The lower 4 bits are the index at that level.
	//
	// There should ideally be no more than 2 vectors per priority level
	//
	// Since ISA IRQs 0-15 are numbered by priority (ish), with the highest
	// being 0, we want IRQs 0+1 at level F, IRQs 2+3 at level E, etc.  We
	// only go down to level 2, because below that are the CPU exceptions.
	// That leaves up to 14 priority levels available.  This gives us a
	// sensible distribution for up to 28 IRQs.
	//
	// After 28 IRQs, we fudge it and start back at the top, so IRQs 28+29
	// become vectors F2+F3, IRQs 30+31 become vectors E2+E3, etc

	int vector = 0;
	int priorities = 0;

	priorities = ((0x100 - INTERRUPT_VECTORSTART) >> 4);

	vector = (((0xF - ((intNumber % (priorities * 2)) / 2)) << 4) |
		(((intNumber / (priorities * 2)) * 2) + (intNumber & 1)));

	return (vector);
}


static int calcIntNumber(int vector)
{
	// Reverse the calculation from calcVector

	int intNumber = 0;
	int priorities = 0;

	priorities = ((0x100 - INTERRUPT_VECTORSTART) >> 4);

	intNumber = ((((vector & 0xF) / 2) * (priorities * 2)) +
		(((0xF - (vector >> 4)) * 2) + (vector & 1)));

	return (intNumber);
}


static unsigned readLocalReg(unsigned offset)
{
	if (localApicRegs)
		return (*((unsigned *)(localApicRegs + offset)));
	else
		return (0);
}


static void writeLocalReg(unsigned offset, unsigned value)
{
	if (localApicRegs)
		*((unsigned *)(localApicRegs + offset)) = value;
}


static unsigned readIoReg(kernelIoApic *ioApic, unsigned char offset)
{
	// Select the register
	ioApic->regs[0] = offset;

	// Read the data
	return (ioApic->regs[4]);
}


static void writeIoReg(kernelIoApic *ioApic, unsigned char offset,
	unsigned value)
{
	// Select the register
	ioApic->regs[0] = offset;

	// Write the value
	ioApic->regs[4] = value;
}


static int timerIrqMapped(kernelDevice *mpDevice)
{
	// Loop through the buses and I/O interrupt assignments to determine
	// whether the system timer ISA IRQ 0 is connected to an APIC

	kernelMultiProcOps *mpOps = NULL;
	multiProcBusEntry *busEntry = NULL;
	multiProcIoIntAssEntry *intEntry = NULL;
	int count1, count2;

	mpOps = (kernelMultiProcOps *) mpDevice->driver->ops;

	for (count1 = 0; ; count1 ++)
	{
		busEntry = mpOps->driverGetEntry(mpDevice, MULTIPROC_ENTRY_BUS,
			count1);
		if (!busEntry)
			break;

		// Is it ISA?
		if (strncmp(busEntry->type, MULTIPROC_BUSTYPE_ISA, 6))
			continue;

		for (count2 = 0; ; count2 ++)
		{
			intEntry = mpOps->driverGetEntry(mpDevice,
				MULTIPROC_ENTRY_IOINTASSMT, count2);
			if (!intEntry)
				break;

			// Is it for this ISA bus?
			if (intEntry->busId != busEntry->busId)
				continue;

			// Is it the timer interrupt?
			if ((intEntry->intType == MULTIPROC_INTTYPE_INT) &&
				!intEntry->busIrq)
			{
				kernelDebug(debug_io, "APIC timer interrupt assigned to I/O "
					"APIC %d", intEntry->ioApicId);
				return ((1 << 8) | intEntry->ioApicId);
			}
		}
	}

	// Not found
	return (0);
}


static int enableLocalApic(kernelDevice *mpDevice, unsigned char *logicalDest)
{
	// Detect whether the CPU has a local APIC, and if so, enable it

	int status = 0;
	x86CpuFeatures cpuFeatures;
	int hasLocal = 0;
	int hasMsrs = 0;
	unsigned rega = 0, regd = 0;
	unsigned apicBase = 0;
	int apicId = 0;
	kernelMultiProcOps *mpOps = NULL;
	multiProcLocalIntAssEntry *intEntry = NULL;
	unsigned lint = 0;
	int count;

	memset(&cpuFeatures, 0, sizeof(x86CpuFeatures));

	// Get the CPU features
	kernelCpuGetFeatures(&cpuFeatures, sizeof(x86CpuFeatures));

	// Required feature bits supported?
	if (!cpuFeatures.cpuid1)
		return (status = ERR_NOTINITIALIZED);

	// Is there a local APIC?
	hasLocal = ((cpuFeatures.cpuid1Edx >> 9) & 1);

	kernelDebug(debug_io, "APIC CPU %s a local APIC", (hasLocal? "has" :
		"does not have"));

	if (!hasLocal)
		return (status = ERR_NOTINITIALIZED);

	// Does the CPU have model-specific registers?
	hasMsrs = ((cpuFeatures.cpuid1Edx >> 5) & 1);

	kernelDebug(debug_io, "APIC CPU %s MSRs", (hasMsrs? "has" :
		"does not have"));

	if (!hasMsrs)
		return (status = ERR_NOTINITIALIZED);

	// Read the local APIC base MSR
	processorReadMsr(X86_MSR_APICBASE, rega, regd);

	apicBase = (rega & (X86_MSR_APICBASE_BASEADDR | X86_MSR_APICBASE_BSP));

	// Set the APIC enable bit (11)
	apicBase |= X86_MSR_APICBASE_APICENABLE;

	// Write it back
	processorWriteMsr(X86_MSR_APICBASE, apicBase, regd);

	apicBase &= X86_MSR_APICBASE_BASEADDR;

	kernelDebug(debug_io, "APIC CPU local APIC base=0x%08x", apicBase);

	localApicRegs = (void *) apicBase;

	// Identity-map the local APIC's registers (4KB)
	if (!kernelPageMapped(KERNELPROCID, (void *) localApicRegs, 0x1000))
	{
		kernelDebug(debug_io, "APIC CPU local APIC registers memory is not "
			"mapped");
		status = kernelPageMap(KERNELPROCID, apicBase, (void *) localApicRegs,
			0x1000);
		if (status < 0)
		{
			kernelError(kernel_error, "Couldn't map local APIC registers");
			return (status);
		}
	}
	else
	{
		kernelDebug(debug_io, "APIC CPU local APIC registers memory is "
			"already mapped");
	}

	// Make it non-cacheable, since this memory represents memory-mapped
	// hardware registers
	status = kernelPageSetAttrs(KERNELPROCID, pageattr_uncacheable,
		(void *) localApicRegs, 0x1000);
	if (status < 0)
		kernelDebugError("Error setting page attrs");

	// Get this processor's local APIC ID
	apicId = (readLocalReg(APIC_LOCALREG_APICID) >> 24);

	kernelDebug(debug_io, "APIC boot CPU local APIC ID=%d", apicId);

	// Set the task priority register to accept all interrupts
	writeLocalReg(APIC_LOCALREG_TASKPRI, 0);

	// Set up the local interrupt vectors

	// Clear/mask them off initially
	writeLocalReg(APIC_LOCALREG_LVT_TIMER, (1 << 16));
	writeLocalReg(APIC_LOCALREG_LVT_PERFCNT, (1 << 16));
	writeLocalReg(APIC_LOCALREG_LVT_LINT0, (1 << 16));
	writeLocalReg(APIC_LOCALREG_LVT_LINT1, (1 << 16));
	writeLocalReg(APIC_LOCALREG_LVT_ERROR, (1 << 16));

	mpOps = (kernelMultiProcOps *) mpDevice->driver->ops;

	// Loop through the local interrupt assignments
	for (count = 0; ; count ++)
	{
		intEntry = mpOps->driverGetEntry(mpDevice,
			MULTIPROC_ENTRY_LOCINTASSMT, count);
		if (!intEntry)
			break;

		// Is it for this local APIC?  Or all?
		if ((intEntry->localApicId != 0xFF) &&
			(intEntry->localApicId != apicId))
		{
			continue;
		}

		kernelDebug(debug_io, "APIC processing local int entry lint%d",
			intEntry->localApicLint);

		lint = 0;

		// Trigger mode for 'fixed' interrupts.  This bit is only used when
		// the delivery mode is 'fixed'.
		if ((intEntry->intType == MULTIPROC_INTTYPE_INT) &&
			((intEntry->intFlags & MULTIPROC_INTTRIGGER_MASK) ==
				MULTIPROC_INTTRIGGER_LEVEL))
		{
			lint |= (1 << 15);
		}

		// Polarity
		if ((intEntry->intFlags & MULTIPROC_INTPOLARITY_MASK) ==
			MULTIPROC_INTPOLARITY_ACTIVELO)
		{
			lint |= (1 << 13);
		}

		// Delivery mode.  Default is 000 (fixed).
		if (intEntry->intType == MULTIPROC_INTTYPE_NMI)
			lint |= (0x04 << 8);
		else if (intEntry->intType == MULTIPROC_INTTYPE_EXTINT)
			lint |= (0x07 << 8);

		// Vector, if applicable
		if (intEntry->intType == MULTIPROC_INTTYPE_INT)
			lint |= calcVector(intEntry->busIrq);

		if (!intEntry->localApicLint)
			writeLocalReg(APIC_LOCALREG_LVT_LINT0, lint);
		else
			writeLocalReg(APIC_LOCALREG_LVT_LINT1, lint);
	}

	kernelDebug(debug_io, "APIC LINT0=0x%08x",
		readLocalReg(APIC_LOCALREG_LVT_LINT0));
	kernelDebug(debug_io, "APIC LINT1=0x%08x",
		readLocalReg(APIC_LOCALREG_LVT_LINT1));

	// Clear the task priority register
	writeLocalReg(APIC_LOCALREG_TASKPRI, 0);

	// Set the destination format register bits 28-31 to 0xF to set 'flat
	// model'
	writeLocalReg(APIC_LOCALREG_DESTFMT,
		(readLocalReg(APIC_LOCALREG_DESTFMT) | (0xF << 28)));

	// Choose a logical destination ID.  For now, it's just a bitmap of the
	// low 4 bits of the local APIC ID.
	if (logicalDest)
		*logicalDest = (01 << (apicId & 0xF));

	// Set the logical destination register bitmap
	writeLocalReg(APIC_LOCALREG_LOGDEST,
		((readLocalReg(APIC_LOCALREG_LOGDEST) & 0x00FFFFFF) |
		(logicalDest? ((unsigned) *logicalDest << 24) : 0)));

	// Set bit 8 of the spurious interrupt vector register to enable the APIC,
	// and set the spurious interrupt vector to 0xFF
	writeLocalReg(APIC_LOCALREG_SPURINT,
		(readLocalReg(APIC_LOCALREG_SPURINT) | 0x000001FF));

	return (status = 0);
}


static multiProcCpuEntry *getBootCpu(kernelDevice *mpDevice)
{
	// Find the multiprocessor boot CPU entry

	kernelMultiProcOps *mpOps = NULL;
	multiProcCpuEntry *cpuEntry = NULL;
	int count;

	mpOps = (kernelMultiProcOps *) mpDevice->driver->ops;

	for (count = 0; ; count ++)
	{
		cpuEntry = mpOps->driverGetEntry(mpDevice, MULTIPROC_ENTRY_CPU,
			count);

		if (!cpuEntry || (cpuEntry->cpuFlags & MULTIPROC_CPUFLAG_BOOT))
			return (cpuEntry);
	}

	// Not found
	return (cpuEntry = NULL);
}


static int setupIsaInts(kernelPic *pic, kernelDevice *mpDevice,
	unsigned char logicalDest)
{
	int status = 0;
	kernelMultiProcOps *mpOps = NULL;
	multiProcCpuEntry *cpuEntry = NULL;
	multiProcBusEntry *busEntry = NULL;
	multiProcIoIntAssEntry *intEntry = NULL;
	kernelIoApic *ioApic = pic->driverData;
	unsigned slotLo = 0, slotHi = 0;
	int count1, count2;

	mpOps = (kernelMultiProcOps *) mpDevice->driver->ops;

	if (!logicalDest)
	{
		// Get the boot CPU entry
		cpuEntry = getBootCpu(mpDevice);
		if (!cpuEntry)
			return (status = ERR_NOSUCHENTRY);
	}

	// We used to set up default, identity-mapped ISA vectors for interrupts
	// 0-15 here.  I think that's probably not the right thing to do, but I'm
	// sure somebody out there has a system that will prove me wrong.

	// Loop through the MP bus entries
	for (count1 = 0; ; count1 ++)
	{
		busEntry = mpOps->driverGetEntry(mpDevice, MULTIPROC_ENTRY_BUS,
			count1);
		if (!busEntry)
			break;

		// Is it ISA?
		if (strncmp(busEntry->type, MULTIPROC_BUSTYPE_ISA, 6))
			continue;

		kernelDebug(debug_io, "APIC processing ISA bus %d", busEntry->busId);

		// Loop through the I/O interrupt assignments
		for (count2 = 0; ; count2 ++)
		{
			intEntry = mpOps->driverGetEntry(mpDevice,
				MULTIPROC_ENTRY_IOINTASSMT,	count2);
			if (!intEntry)
				break;

			// Is it for this I/O APIC?
			if (intEntry->ioApicId != ioApic->id)
				continue;

			// Is it for this ISA bus?
			if (intEntry->busId != busEntry->busId)
				continue;

			kernelDebug(debug_io, "APIC processing ISA int entry pin=%d "
				"IRQ=%d vector=%02x", intEntry->ioApicIntPin,
				(pic->startIrq + intEntry->busIrq),
				calcVector(pic->startIrq + intEntry->busIrq));

			slotLo = slotHi = 0;

			// Destination (boot processor)
			if (logicalDest)
				slotHi = (logicalDest << 24);
			else
				slotHi = (cpuEntry->localApicId << 24);

			// Mask it off
			slotLo |= (1 << 16);

			// Trigger mode.  Default for ISA is edge-triggered.
			if ((intEntry->intFlags & MULTIPROC_INTTRIGGER_MASK) ==
				MULTIPROC_INTTRIGGER_LEVEL)
			{
				slotLo |= (1 << 15);
			}

			// Polarity.  Default for ISA is active high for edge-triggered,
			// and active-low for level-triggered.
			switch (intEntry->intFlags & MULTIPROC_INTPOLARITY_MASK)
			{
				case MULTIPROC_INTPOLARITY_ACTIVEHI:
					break;

				case MULTIPROC_INTPOLARITY_ACTIVELO:
					slotLo |= (1 << 13);
					break;

				default:
					if (slotLo & (1 << 15))
						slotLo |= (1 << 13);
					break;
			}

			// Logical destination mode
			if (logicalDest)
				slotLo |= (1 << 11);

			// Delivery mode.  Default is 000 (fixed).
			if (intEntry->intType == MULTIPROC_INTTYPE_SMI)
				slotLo |= (0x02 << 8);
			else if (intEntry->intType == MULTIPROC_INTTYPE_NMI)
				slotLo |= (0x04 << 8);
			else if (intEntry->intType == MULTIPROC_INTTYPE_EXTINT)
				slotLo |= (0x07 << 8);

			// Vector
			slotLo |= calcVector(intEntry->busIrq);

			WRITESLOTHI(ioApic, intEntry->ioApicIntPin, slotHi);
			WRITESLOTLO(ioApic, intEntry->ioApicIntPin, slotLo);
		}
	}

	return (status = 0);
}


static int setupPciInts(kernelPic *pic, kernelDevice *mpDevice,
	unsigned char logicalDest)
{
	int status = 0;
	kernelMultiProcOps *mpOps = NULL;
	multiProcCpuEntry *cpuEntry = NULL;
	multiProcBusEntry *busEntry = NULL;
	multiProcIoIntAssEntry *intEntry = NULL;
	kernelIoApic *ioApic = pic->driverData;
	unsigned slotLo = 0, slotHi = 0;
	int count1, count2;

	mpOps = (kernelMultiProcOps *) mpDevice->driver->ops;

	if (!logicalDest)
	{
		// Get the boot CPU entry
		cpuEntry = getBootCpu(mpDevice);
		if (!cpuEntry)
			return (status = ERR_NOSUCHENTRY);
	}

	// Loop through the MP bus entries
	for (count1 = 0; ; count1 ++)
	{
		busEntry = mpOps->driverGetEntry(mpDevice, MULTIPROC_ENTRY_BUS,
			count1);
		if (!busEntry)
			break;

		// Is it PCI?
		if (strncmp(busEntry->type, MULTIPROC_BUSTYPE_PCI, 6))
			continue;

		kernelDebug(debug_io, "APIC processing PCI bus %d", busEntry->busId);

		// Loop through the I/O interrupt assignments
		for (count2 = 0; ; count2 ++)
		{
			intEntry = mpOps->driverGetEntry(mpDevice,
				MULTIPROC_ENTRY_IOINTASSMT, count2);
			if (!intEntry)
				break;

			// Is it for this I/O APIC?
			if (intEntry->ioApicId != ioApic->id)
				continue;

			// Is it for this PCI bus?
			if (intEntry->busId != busEntry->busId)
				continue;

			kernelDebug(debug_io, "APIC processing PCI int entry %d:%c "
				"pin=%d IRQ=%d vector=%02x", ((intEntry->busIrq >> 2) & 0x1F),
				('A' + (intEntry->busIrq & 0x03)), intEntry->ioApicIntPin,
				(pic->startIrq + intEntry->ioApicIntPin),
				calcVector(pic->startIrq + intEntry->ioApicIntPin));

			slotLo = slotHi = 0;

			// Destination (boot processor)
			if (logicalDest)
				slotHi = (logicalDest << 24);
			else
				slotHi = (cpuEntry->localApicId << 24);

			// Mask it off
			slotLo |= (1 << 16);

			// Trigger mode.  Default for PCI is level-triggered.
			if ((intEntry->intFlags & MULTIPROC_INTTRIGGER_MASK) !=
				MULTIPROC_INTTRIGGER_EDGE)
			{
				slotLo |= (1 << 15);
			}

			// Polarity.  Default for PCI is active low.
			if ((intEntry->intFlags & MULTIPROC_INTPOLARITY_MASK) !=
				MULTIPROC_INTPOLARITY_ACTIVEHI)
			{
				slotLo |= (1 << 13);
			}

			// Logical destination mode
			if (logicalDest)
				slotLo |= (1 << 11);

			// Delivery mode.  Default is 000 (fixed).
			if (intEntry->intType == MULTIPROC_INTTYPE_SMI)
				slotLo |= (0x02 << 8);
			else if (intEntry->intType == MULTIPROC_INTTYPE_NMI)
				slotLo |= (0x04 << 8);
			else if (intEntry->intType == MULTIPROC_INTTYPE_EXTINT)
				slotLo |= (0x07 << 8);

			// Vector
			slotLo |= calcVector(pic->startIrq + intEntry->ioApicIntPin);

			WRITESLOTHI(ioApic, intEntry->ioApicIntPin, slotHi);
			WRITESLOTLO(ioApic, intEntry->ioApicIntPin, slotLo);
		}
	}

	return (status = 0);
}


#ifdef DEBUG
static inline void debugLocalRegs(void)
{
	kernelDebug(debug_io, "APIC debug local APIC regs:\n"
		"  apicId=0x%08x\n"
		"  version=0x%08x\n"
		"  taskPriority=0x%08x\n"
		"  processorPriority=0x%08x\n"
		"  logicalDestination=0x%08x\n"
		"  destinationFormat=0x%08x\n"
		"  spuriousInterrupt=0x%08x\n"
		"  errorStatus=0x%08x\n"
		"  interruptCommand=0x%08x%08x\n"
		"  lvtTimer=0x%08x\n"
		"  lvtLint0=0x%08x\n"
		"  lvtLint1=0x%08x\n"
		"  lvtError=0x%08x\n"
		"  timerInitialCount=0x%08x\n"
		"  timerCurrentCount=0x%08x",
		readLocalReg(APIC_LOCALREG_APICID),
		readLocalReg(APIC_LOCALREG_VERSION),
		readLocalReg(APIC_LOCALREG_TASKPRI),
		readLocalReg(APIC_LOCALREG_PROCPRI),
		readLocalReg(APIC_LOCALREG_LOGDEST),
		readLocalReg(APIC_LOCALREG_DESTFMT),
		readLocalReg(APIC_LOCALREG_SPURINT),
		readLocalReg(APIC_LOCALREG_ERRSTAT),
		readLocalReg(APIC_LOCALREG_INTCMDHI),
		readLocalReg(APIC_LOCALREG_INTCMDLO),
		readLocalReg(APIC_LOCALREG_LVT_TIMER),
		readLocalReg(APIC_LOCALREG_LVT_LINT0),
		readLocalReg(APIC_LOCALREG_LVT_LINT1),
		readLocalReg(APIC_LOCALREG_LVT_ERROR),
		readLocalReg(APIC_LOCALREG_TIMERINIT),
		readLocalReg(APIC_LOCALREG_TIMERCURR));
}
#else
	#define debugLocalRegs() do { } while(0);
#endif


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Standard PIC driver functions
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

static int driverGetIntNumber(kernelPic *pic, unsigned char busId,
	unsigned char busIrq)
{
	kernelDevice *mpDevice = NULL;
	kernelMultiProcOps *mpOps = NULL;
	multiProcIoIntAssEntry *intEntry = NULL;
	kernelIoApic *ioApic = pic->driverData;
	int count;

	kernelDebug(debug_io, "APIC get interrupt for busId=%d busIrq=%d",
		busId, busIrq);

	// See whether we have a multiprocessor table
	if (kernelDeviceFindType(
		kernelDeviceGetClass(DEVICESUBCLASS_SYSTEM_MULTIPROC), NULL,
			&mpDevice, 1) < 1)
	{
		kernelDebugError("No multiprocessor support detected");
		return (ERR_NOTIMPLEMENTED);
	}

	mpOps = (kernelMultiProcOps *) mpDevice->driver->ops;

	// Loop through the I/O interrupt assignments
	for (count = 0; ; count ++)
	{
		intEntry = mpOps->driverGetEntry(mpDevice, MULTIPROC_ENTRY_IOINTASSMT,
			count);
		if (!intEntry)
			break;

		// Is it for this I/O APIC?
		if (intEntry->ioApicId != ioApic->id)
			continue;

		// Is it for the correct bus?
		if (intEntry->busId != busId)
			continue;

		// Is it the correct type?
		if (intEntry->intType != MULTIPROC_INTTYPE_INT)
			continue;

		// Is it for the correct device?
		if (intEntry->busIrq != busIrq)
			continue;

		// Found it
		return (pic->startIrq + intEntry->ioApicIntPin);
	}

	// Not found
	return (ERR_NOSUCHENTRY);
}


static int driverGetVector(kernelPic *pic __attribute__((unused)),
	int intNumber)
{
	kernelDebug(debug_io, "APIC get vector for interrupt %d (0x%02x)",
		intNumber, calcVector(intNumber));

	return (calcVector(intNumber));
}


static int driverEndOfInterrupt(kernelPic *pic __attribute__((unused)),
	int intNumber __attribute__((unused)))
{
	//kernelDebug(debug_io, "APIC EOI for interrupt %d", intNumber);

	writeLocalReg(APIC_LOCALREG_EOI, 0);

	return (0);
}


static int driverMask(kernelPic *pic, int intNumber, int on)
{
	// This masks or unmasks an interrupt

	int found = 0;
	kernelIoApic *ioApic = pic->driverData;
	unsigned slotLo = 0;
	int count;

	kernelDebug(debug_io, "APIC mask interrupt %d %s", intNumber,
		(on? "on" : "off"));

	for (count = 0; count < pic->numIrqs; count ++)
	{
		slotLo = READSLOTLO(ioApic, count);

		if (((slotLo & 0x700) != 0x700) &&
			(calcIntNumber(slotLo & 0xFF) == intNumber))
		{
			found += 1;

			if (on)
				slotLo &= ~((unsigned) 1 << 16);
			else
				slotLo |= (1 << 16);

			WRITESLOTLO(ioApic, count, slotLo);

			kernelDebug(debug_io, "APIC slot %d %08x %08x", count,
				READSLOTHI(ioApic, count), READSLOTLO(ioApic, count));
		}
	}

	if (found)
	{
		kernelDebug(debug_io, "APIC masked %s %d sources", (on? "on" : "off"),
			found);
		return (0);
	}
	else
	{
		// Vector not found
		kernelDebugError("Vector not found for interrupt %d", intNumber);
		return (ERR_NOSUCHENTRY);
	}
}


static int driverGetActive(kernelPic *pic __attribute__((unused)))
{
	// Returns the number of the active interrupt

	int intNumber = ERR_NODATA;
	unsigned isrReg = 0;
	int vector = 0;
	int count;

	kernelDebug(debug_io, "APIC active interrupt requested");

	for (count = 16, vector = 0x20; count < 128; count += 16)
	{
		isrReg = readLocalReg(APIC_LOCALREG_ISR + count);

		kernelDebug(debug_io, "APIC ISR %02x-%02x %08x", (vector + 31),
			vector, isrReg);

		if (isrReg)
		{
			while (!(isrReg & 1))
			{
				isrReg >>= 1;
				vector += 1;
			}

			intNumber = calcIntNumber(vector);

			kernelDebug(debug_io, "APIC active vector=%02x irq=%d", vector,
				intNumber);

			break;
		}

		vector += 32;
	}

	return (intNumber);
}


static int driverDetect(void *parent, kernelDriver *driver)
{
	// This function is used to detect and initialize each I/O APIC device, as
	// well as registering each one with the higher-level interface

	int status = 0;
	kernelDevice *mpDevice = NULL;
	kernelMultiProcOps *mpOps = NULL;
	int haveTimerIrq = 0;
	multiProcCpuEntry *cpuEntry = NULL;
	unsigned apicIdBitmap = 0;
	unsigned char logicalDest = 0;
	multiProcIoApicEntry *ioApicEntry = NULL;
	unsigned char newApicId = 0;
	multiProcIoIntAssEntry *intEntry = NULL;
	kernelIoApic *ioApic = NULL;
	kernelPic *pic = NULL;
	int startIrq = 0;
	kernelDevice *dev = NULL;
	char value[80];
	int count1, count2;

	// See whether we have a multiprocessor table
	if (kernelDeviceFindType(
		kernelDeviceGetClass(DEVICESUBCLASS_SYSTEM_MULTIPROC), NULL,
			&mpDevice, 1) < 1)
	{
		kernelDebug(debug_io, "APIC no multiprocessor support detected");
		return (status = 0);
	}

	kernelDebug(debug_io, "APIC multiprocessor support is present");

	// See whether the system timer ISA IRQ 0 is connected to an APIC.  If not,
	// we will still try to detect everything and set up, but we won't enable
	// the APICs.
	haveTimerIrq = timerIrqMapped(mpDevice);

	kernelDebug(debug_io, "APIC system timer IRQ is %smapped",
		(haveTimerIrq? "" : "not "));

	mpOps = (kernelMultiProcOps *) mpDevice->driver->ops;

	// Loop through the CPU entries and record a bitmap of their local APIC
	// IDs
	for (count1 = 0; ; count1 ++)
	{
		cpuEntry = mpOps->driverGetEntry(mpDevice, MULTIPROC_ENTRY_CPU,
			count1);
		if (!cpuEntry)
			break;

		kernelDebug(debug_io, "APIC local APIC ID=%d", cpuEntry->localApicId);

		apicIdBitmap |= (1 << cpuEntry->localApicId);
	}

	// Enable this processor's (the boot processor's) local APIC
	status = enableLocalApic(mpDevice, &logicalDest /* logical mode */);
	if (status < 0)
		goto out;

	// Loop through the multiprocessor entries looking for I/O APICs
	for (count1 = 0; ; count1 ++)
	{
		ioApicEntry = mpOps->driverGetEntry(mpDevice, MULTIPROC_ENTRY_IOAPIC,
			count1);

		if (!ioApicEntry)
			break;

		// If the APIC is not enabled, skip it
		if (!(ioApicEntry->apicFlags & MULTIPROC_IOAPICFLAG_ENABLED))
		{
			kernelDebugError("APIC skipping disabled I/O APIC %d",
				ioApicEntry->apicId);
			continue;
		}

		kernelDebug(debug_io, "APIC I/O APIC device found, apicId=%d, "
			"address=0x%08x", ioApicEntry->apicId, ioApicEntry->apicPhysical);

		// Sometimes the MP tables contain invalid I/O APIC IDs, and we need
		// to assign one
		if (apicIdBitmap & (1 << ioApicEntry->apicId))
		{
			kernelDebugError("I/O APIC ID %d invalid or in use",
				ioApicEntry->apicId);

			newApicId = 0;
			while ((newApicId < 32) && (apicIdBitmap & (1 << newApicId)))
				newApicId += 1;

			if (newApicId > 31)
			{
				kernelDebugError("Couldn't find an ID for I/O APIC");
				status = ERR_NOFREE;
				break;
			}

			kernelDebug(debug_io, "APIC chose new ID %d", newApicId);

			// If it's the timer I/O APIC ID, change that one too
			if (haveTimerIrq && ((haveTimerIrq & 0xFF) ==
				ioApicEntry->apicId))
			{
				haveTimerIrq = ((1 << 8) | newApicId);
			}

			// Loop through the I/O interrupt assignments and fix the target
			// I/O APIC IDs as appropriate
			for (count2 = 0; ; count2 ++)
			{
				intEntry = mpOps->driverGetEntry(mpDevice,
					MULTIPROC_ENTRY_IOINTASSMT, count2);
				if (!intEntry)
					break;

				// Is it for this I/O APIC?
				if (intEntry->ioApicId == ioApicEntry->apicId)
					intEntry->ioApicId = newApicId;
			}

			apicIdBitmap |= (1 << newApicId);
			ioApicEntry->apicId = newApicId;
		}

		// Allocate memory for driver data
		ioApic = kernelMalloc(sizeof(kernelIoApic));
		if (!ioApic)
		{
			status = ERR_MEMORY;
			break;
		}

		// Set up our driver data

		ioApic->id = ioApicEntry->apicId;
		ioApic->regs = (volatile unsigned *) ioApicEntry->apicPhysical;

		// Identity-map the registers
		if (!kernelPageMapped(KERNELPROCID, (void *) ioApic->regs,
			(5 * sizeof(unsigned))))
		{
			kernelDebug(debug_io, "APIC I/O APIC registers memory is not "
				"mapped");
			status = kernelPageMap(KERNELPROCID, ioApicEntry->apicPhysical,
				(void *) ioApic->regs, (5 * sizeof(unsigned)));
			if (status < 0)
				break;
		}
		else
		{
			kernelDebug(debug_io, "APIC I/O APIC registers memory is already "
				"mapped");
		}

		// Make it non-cacheable, since this memory represents memory-mapped
		// hardware registers
		status = kernelPageSetAttrs(KERNELPROCID, pageattr_uncacheable,
			(void *) ioApic->regs, (5 * sizeof(unsigned)));
		if (status < 0)
			kernelDebugError("Error setting page attrs");

		// Make sure the APIC ID is correctly set
		writeIoReg(ioApic, 0, ((readIoReg(ioApic, 0) & 0xF0FFFFFF) |
			(ioApic->id << 24)));

		kernelDebug(debug_io, "APIC id=0x%08x", readIoReg(ioApic, 0));
		kernelDebug(debug_io, "APIC ver=0x%08x", readIoReg(ioApic, 1));
		kernelDebug(debug_io, "APIC arb=0x%08x", readIoReg(ioApic, 2));

		// Allocate memory for the PIC
		pic = kernelMalloc(sizeof(kernelPic));
		if (!pic)
		{
			status = ERR_MEMORY;
			break;
		}

		pic->type = pic_ioapic;
		pic->enabled = (haveTimerIrq? 1 : 0);
		if (!haveTimerIrq || ((haveTimerIrq & 0xFF) != ioApicEntry->apicId))
			pic->startIrq = startIrq;
		pic->numIrqs = (((readIoReg(ioApic, 1) >> 16) & 0xFF) + 1);
		pic->driver = driver;
		pic->driverData = ioApic;

		kernelDebug(debug_io, "APIC startIrq=%d numIrqs=%d", pic->startIrq,
			pic->numIrqs);

		// The next PIC's IRQs will start where this one left off
		startIrq += pic->numIrqs;

		// Mask/clear all the slots
		for (count2 = 0; count2 < pic->numIrqs; count2 ++)
		{
			WRITESLOTHI(ioApic, count2, 0);
			WRITESLOTLO(ioApic, count2, (1 << 16));
		}

		// Set up any ISA interrupts
		status = setupIsaInts(pic, mpDevice, logicalDest /* logical mode */);
		if (status < 0)
			break;

		// Set up any PCI interrupts
		status = setupPciInts(pic, mpDevice, logicalDest /* logical mode */);
		if (status < 0)
			break;

		for (count2 = 0; count2 < pic->numIrqs; count2 ++)
		{
			kernelDebug(debug_io, "APIC slot %d %08x %08x", count2,
				READSLOTHI(ioApic, count2), READSLOTLO(ioApic, count2));
		}

		// Allocate memory for the kernel device
		dev = kernelMalloc(sizeof(kernelDevice));
		if (!dev)
		{
			status = ERR_MEMORY;
			break;
		}

		// Set up the device structure
		dev->device.class = kernelDeviceGetClass(DEVICECLASS_INTCTRL);
		dev->device.subClass =
			kernelDeviceGetClass(DEVICESUBCLASS_INTCTRL_APIC);
		dev->driver = driver;

		// Initialize the variable list for attributes of the controller
		variableListCreateSystem(&dev->device.attrs);

		sprintf(value, "%d", ioApic->id);
		variableListSet(&dev->device.attrs, "apic.id", value);

		sprintf(value, "%d", pic->startIrq);
		variableListSet(&dev->device.attrs, "start.irq", value);

		sprintf(value, "%d", pic->numIrqs);
		variableListSet(&dev->device.attrs, "num.irqs", value);

		// Add the kernel device
		status = kernelDeviceAdd(parent, dev);
		if (status < 0)
			break;

		// Can't free this now
		dev = NULL;

		// Add the PIC to the higher-level interface
		status = kernelPicAdd(pic);
		if (status < 0)
			break;
	}

out:
	if (status < 0)
	{
		if (dev)
			kernelFree(dev);
		if (pic)
			kernelFree(pic);
		if (ioApic)
			kernelFree(ioApic);
	}

	return (status);
}


static kernelPicOps apicOps = {
	driverGetIntNumber,
	driverGetVector,
	driverEndOfInterrupt,
	driverMask,
	driverGetActive,
	NULL	// driverDisable
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

void kernelApicDriverRegister(kernelDriver *driver)
{
	 // Device driver registration

	driver->driverDetect = driverDetect;
	driver->ops = &apicOps;

	return;
}


#ifdef DEBUG
void kernelApicDebug(void)
{
	unsigned isrReg = 0;
	int vector = 0;
	int count1, count2;

	debugLocalRegs();

	for (count1 = 0; count1 < 128; count1 += 32)
	{
		kernelDebug(debug_io, "APIC ISR %02x-%02x %08x %08x",
			((count1 * 2) + 63), (count1 * 2),
			readLocalReg(APIC_LOCALREG_ISR + count1 + 16),
			readLocalReg(APIC_LOCALREG_ISR + count1));
	}

	for (count1 = 0, vector = 0x00; count1 < 128; count1 += 16)
	{
		isrReg = readLocalReg(APIC_LOCALREG_ISR + count1);

		for (count2 = 0; count2 < 32; count2 ++)
		{
			if (isrReg & 1)
			{
				kernelDebug(debug_io, "APIC in service=%02x irq=%d", vector,
					calcIntNumber(vector));
			}

			isrReg >>= 1;
			vector += 1;
		}
	}

	for (count1 = 0; count1 < 128; count1 += 32)
	{
		kernelDebug(debug_io, "APIC TMR %02x-%02x %08x %08x",
			((count1 * 2) + 63), (count1 * 2),
			readLocalReg(APIC_LOCALREG_TMR + count1 + 16),
			readLocalReg(APIC_LOCALREG_TMR + count1));
	}

	for (count1 = 0; count1 < 128; count1 += 32)
	{
		kernelDebug(debug_io, "APIC IRR %02x-%02x %08x %08x",
			((count1 * 2) + 63), (count1 * 2),
			readLocalReg(APIC_LOCALREG_IRR + count1 + 16),
			readLocalReg(APIC_LOCALREG_IRR + count1));
	}

	for (count1 = 0, vector = 0x00; count1 < 128; count1 += 16)
	{
		isrReg = readLocalReg(APIC_LOCALREG_IRR + count1);

		for (count2 = 0; count2 < 32; count2 ++)
		{
			if (isrReg & 1)
			{
				kernelDebug(debug_io, "APIC request=%02x irq=%d", vector,
					calcIntNumber(vector));
			}

			isrReg >>= 1;
			vector += 1;
		}
	}
}
#endif

