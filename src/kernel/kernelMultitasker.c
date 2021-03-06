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
//  kernelMultitasker.c
//

// This file contains the C functions belonging to the kernel's multitasker

#include "kernelMultitasker.h"
#include "kernelCpu.h"
#include "kernelDebug.h"
#include "kernelEnvironment.h"
#include "kernelError.h"
#include "kernelFile.h"
#include "kernelInterrupt.h"
#include "kernelLog.h"
#include "kernelMain.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelNetwork.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelPic.h"
#include "kernelShutdown.h"
#include "kernelSysTimer.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/processor.h>
#include <sys/user.h>
#include <sys/vis.h>

#define PROC_KILLABLE(proc) ((proc != kernelProc) && \
	(proc != exceptionProc) && \
	(proc != idleProc) && \
	(proc != kernelCurrentProcess))

#define SET_PORT_BIT(bitmap, port) \
	do { bitmap[port / 8] |=  (1 << (port % 8)); } while (0)
#define UNSET_PORT_BIT(bitmap, port) \
	do { bitmap[port / 8] &= ~(1 << (port % 8)); } while (0)
#define GET_PORT_BIT(bitmap, port) ((bitmap[port / 8] >> (port % 8)) & 0x01)

// Global multitasker stuff
static int multitaskingEnabled = 0;
static volatile int processIdCounter = KERNELPROCID;
static kernelProcess *kernelProc = NULL;
static kernelProcess *idleProc = NULL;
static kernelProcess *exceptionProc = NULL;
static volatile int processingException = 0;
static volatile unsigned exceptionAddress = 0;
static volatile int schedulerSwitchedByCall = 0;
static kernelProcess *fpuProcess = NULL;

// We allow the pointer to the current process to be exported, so that when a
// process uses system calls, there is an easy way for the process to get
// information about itself
kernelProcess *kernelCurrentProcess = NULL;

// Process list for CPU execution
static linkedList processList;

// Things specific to the scheduler.  The scheduler process is just a
// convenient place to keep things, we don't use all of it and it doesn't go
// in the process list.
static kernelProcess *schedulerProc = NULL;
static volatile int schedulerStop = 0;
static volatile unsigned schedulerTimeslices = 0;

// An array of exception types.  The selectors are initialized later.
static struct {
	int index;
	kernelSelector tssSelector;
	const char *a;
	const char *name;
	int (*handler)(void);

} exceptionVector[19] = {
	{ EXCEPTION_DIVBYZERO, 0, "a", "divide-by-zero", NULL },
	{ EXCEPTION_DEBUG, 0, "a", "debug", NULL },
	{ EXCEPTION_NMI, 0, "a", "non-maskable interrupt (NMI)", NULL },
	{ EXCEPTION_BREAK, 0, "a", "breakpoint", NULL },
	{ EXCEPTION_OVERFLOW, 0, "a", "overflow", NULL },
	{ EXCEPTION_BOUNDS, 0, "a", "out-of-bounds", NULL },
	{ EXCEPTION_OPCODE, 0, "an", "invalid opcode", NULL },
	{ EXCEPTION_DEVNOTAVAIL, 0, "a", "device not available", NULL },
	{ EXCEPTION_DOUBLEFAULT, 0, "a", "double-fault", NULL },
	{ EXCEPTION_COPROCOVER, 0, "a", "co-processor segment overrun", NULL },
	{ EXCEPTION_INVALIDTSS, 0, "an", "invalid TSS", NULL },
	{ EXCEPTION_SEGNOTPRES, 0, "a", "segment not present", NULL },
	{ EXCEPTION_STACK, 0, "a", "stack", NULL },
	{ EXCEPTION_GENPROTECT, 0, "a", "general protection", NULL },
	{ EXCEPTION_PAGE, 0, "a", "page fault", NULL },
	{ EXCEPTION_RESERVED, 0, "a", "\"reserved\"", NULL },
	{ EXCEPTION_FLOAT, 0, "a", "floating point", NULL },
	{ EXCEPTION_ALIGNCHECK, 0, "an", "alignment check", NULL },
	{ EXCEPTION_MACHCHECK, 0, "a", "machine check", NULL }
};


static void debugContext(kernelProcess *proc, char *buffer, int len)
{
	if (!buffer)
		return;

#ifdef ARCH_X86
	snprintf(buffer, len, "Multitasker debug TSS selector:\n");
	snprintf((buffer + strlen(buffer)), (len - strlen(buffer)),
		"  oldTss=%08x", proc->context.taskStateSegment.oldTss);

	snprintf((buffer + strlen(buffer)), (len - strlen(buffer)),
		"  ESP0=%08x SS0=%08x\n", proc->context.taskStateSegment.ESP0,
		proc->context.taskStateSegment.SS0);

	snprintf((buffer + strlen(buffer)), (len - strlen(buffer)),
		"  ESP1=%08x SS1=%08x\n", proc->context.taskStateSegment.ESP1,
		proc->context.taskStateSegment.SS1);

	snprintf((buffer + strlen(buffer)), (len - strlen(buffer)),
		"  ESP2=%08x SS2=%08x\n", proc->context.taskStateSegment.ESP2,
		proc->context.taskStateSegment.SS2);

	snprintf((buffer + strlen(buffer)), (len - strlen(buffer)),
		"  CR3=%08x EIP=%08x EFLAGS=%08x\n",
		proc->context.taskStateSegment.CR3,
		proc->context.taskStateSegment.EIP,
		proc->context.taskStateSegment.EFLAGS);

	// Skip general-purpose registers for now -- not terribly interesting

	snprintf((buffer + strlen(buffer)), (len - strlen(buffer)),
		"  ESP=%08x EBP=%08x ESI=%08x EDI=%08x\n",
		proc->context.taskStateSegment.ESP,
		proc->context.taskStateSegment.EBP,
		proc->context.taskStateSegment.ESI,
		proc->context.taskStateSegment.EDI);

	snprintf((buffer + strlen(buffer)), (len - strlen(buffer)),
		"  CS=%08x SS=%08x\n", proc->context.taskStateSegment.CS,
		proc->context.taskStateSegment.SS);

	snprintf((buffer + strlen(buffer)), (len - strlen(buffer)),
		"  ES=%08x DS=%08x FS=%08x GS=%08x\n",
		proc->context.taskStateSegment.ES, proc->context.taskStateSegment.DS,
		proc->context.taskStateSegment.FS, proc->context.taskStateSegment.GS);

	snprintf((buffer + strlen(buffer)), (len - strlen(buffer)),
		"  LDTSelector=%08x IOMapBase=%04x\n",
		proc->context.taskStateSegment.LDTSelector,
		proc->context.taskStateSegment.IOMapBase);
#endif
}


static kernelProcess *getProcessById(int processId)
{
	// This function is used to find a process's pointer based on the process
	// Id.  Returns NULL if the process doesn't exist.

	kernelProcess *proc = NULL;
	linkedListItem *iter = NULL;

	proc = linkedListIterStart(&processList, &iter);

	while (proc)
	{
		if (proc->processId == processId)
			return (proc);

		proc = linkedListIterNext(&processList, &iter);
	}

	// Not found
	return (proc = NULL);
}


static kernelProcess *getProcessByName(const char *name)
{
	// As above, but searches by name

	kernelProcess *proc = NULL;
	linkedListItem *iter = NULL;

	proc = linkedListIterStart(&processList, &iter);

	while (proc)
	{
		if (!strcmp((char *) proc->name, name))
			return (proc);

		proc = linkedListIterNext(&processList, &iter);
	}

	// Not found
	return (proc = NULL);
}


static inline int allocProcess(kernelProcess **procPointer)
{
	// Allocate new process control memory.  It should be passed a reference
	// to a pointer for the new process.

	int status = 0;
	kernelProcess *proc = NULL;

	// Check params
	if (!procPointer)
		return (status = ERR_NULLPARAMETER);

	proc = kernelMalloc(sizeof(kernelProcess));
	if (!proc)
		return (status = ERR_MEMORY);

	// Success.  Set the pointer for the calling process.
	*procPointer = proc;
	return (status = 0);
}


static inline int freeProcess(kernelProcess *proc)
{
	// Free process control memory

	return (kernelFree((void *) proc));
}


static int addProcessToList(kernelProcess *proc)
{
	// This function will add a process to the process list.  It returns zero
	// on success, negative otherwise.

	int status = 0;
	kernelProcess *listProc = NULL;
	linkedListItem *iter = NULL;

	// Check params
	if (!proc)
		return (status = ERR_NULLPARAMETER);

	// Make sure the priority is a legal value
	if ((proc->priority < 0) || (proc->priority > (PRIORITY_LEVELS - 1)))
	{
		// The process's priority is an illegal value
		return (status = ERR_INVALID);
	}

	// Search the process list to make sure it isn't already present

	listProc = linkedListIterStart(&processList, &iter);

	while (listProc)
	{
		if (listProc == proc)
		{
			// Oops, it's already there
			return (status = ERR_ALREADY);
		}

		listProc = linkedListIterNext(&processList, &iter);
	}

	// OK, now we can add the process to the list
	status = linkedListAddBack(&processList, (void *) proc);
	if (status < 0)
		return (status);

	// Done
	return (status = 0);
}


static int removeProcessFromList(kernelProcess *proc)
{
	// This function will remove a process from the process list.  It returns
	// zero on success, negative otherwise.

	int status = 0;
	kernelProcess *listProc = NULL;
	linkedListItem *iter = NULL;

	// Check params
	if (!proc)
		return (status = ERR_NULLPARAMETER);

	// Search the list for the matching process

	listProc = linkedListIterStart(&processList, &iter);

	while (listProc)
	{
		if (listProc == proc)
			break;

		listProc = linkedListIterNext(&processList, &iter);
	}

	// Make sure we found the process
	if (listProc != proc)
		return (status = ERR_NOSUCHPROCESS);

	// OK, now we can remove the process from the list
	status = linkedListRemove(&processList, (void *) proc);
	if (status < 0)
		return (status);

	// Done
	return (status = 0);
}


static int createProcessContext(kernelProcess *proc)
{
	// This function will create a processor context (for x86 it's a TSS -
	// Task State Segment) for a new process based on the attributes of the
	// process.  This function relies on the privilege, userStackSize, and
	// superStackSize attributes having been previously set.  Returns 0 on
	// success, negative on error.

	int status = 0;

#ifdef ARCH_X86
	// Get a free descriptor for the process's TSS
	status = kernelDescriptorRequest(&proc->context.tssSelector);
	if ((status < 0) || !proc->context.tssSelector)
		return (status);

	// Fill in the process's Task State Segment descriptor
	status = kernelDescriptorSet(
		proc->context.tssSelector,		// TSS selector number
		&proc->context.taskStateSegment, // Starts at...
		sizeof(x86TSS),					// Limit of a TSS segment
		1,								// Present in memory
		PRIVILEGE_SUPERVISOR,			// TSSs are supervisor privilege level
		0,								// TSSs are system segs
		0xB,							// TSS, 32-bit, busy
		0,								// 0 for SMALL size granularity
		0);								// Must be 0 in TSS
	if (status < 0)
	{
		kernelDescriptorRelease(proc->context.tssSelector);
		return (status);
	}

	// Now, fill in the TSS (Task State Segment) for the new process.  Parts
	// of this will be different depending on whether this is a user or
	// supervisor mode process.

	memset((void *) &proc->context.taskStateSegment, 0, sizeof(x86TSS));

	// Set the IO bitmap's offset
	proc->context.taskStateSegment.IOMapBase = X86_IOBITMAP_OFFSET;

	if (proc->processorPrivilege == PRIVILEGE_SUPERVISOR)
	{
		proc->context.taskStateSegment.CS = PRIV_CODE;
		proc->context.taskStateSegment.DS = PRIV_DATA;
		proc->context.taskStateSegment.SS = PRIV_STACK;
	}
	else
	{
		proc->context.taskStateSegment.CS = USER_CODE;
		proc->context.taskStateSegment.DS = USER_DATA;
		proc->context.taskStateSegment.SS = USER_STACK;

		// Turn off access to all I/O ports by default
		memset((void *) proc->context.taskStateSegment.IOMap, 0xFF,
			X86_PORTS_BYTES);
	}

	// All other data segments same as DS
	proc->context.taskStateSegment.ES = proc->context.taskStateSegment.FS =
		proc->context.taskStateSegment.GS = proc->context.taskStateSegment.DS;

	proc->context.taskStateSegment.ESP = ((unsigned) proc->userStack +
		(proc->userStackSize - sizeof(void *)));

	if (proc->processorPrivilege != PRIVILEGE_SUPERVISOR)
	{
		proc->context.taskStateSegment.SS0 = PRIV_STACK;
		proc->context.taskStateSegment.ESP0 = ((unsigned) proc->superStack +
			(proc->superStackSize - sizeof(int)));
	}

	proc->context.taskStateSegment.EFLAGS = 0x00000202; // Interrupts enabled
	proc->context.taskStateSegment.CR3 = (unsigned)
		proc->pageDirectory->physical;

	// All remaining values will be NULL from initialization.  Note that this
	// includes the EIP.
#endif

	// Return success
	return (status = 0);
}


static int createNewProcess(const char *name, int priority, int privilege,
	processImage *execImage, int newPageDir)
{
	// This function is used to create a new process in the process list.  It
	// makes a "defaults" kind of process -- it sets up all of the process's
	// attributes with default values.  If the calling process wants something
	// different, it should reset those attributes afterward.  If successful,
	// it returns the processId of the new process.  Otherwise, it returns
	// negative.

	int status = 0;
	kernelProcess *proc = NULL;
	void *stackMemoryAddr = NULL;
	unsigned physicalCodeData = 0;
	int argMemorySize = 0;
	char *argMemory = NULL;
	char *oldArgPtr = NULL;
	char *newArgPtr = NULL;
	int *stackArgs = NULL;
	char **argv = NULL;
	int length = 0;
	int count;

	// Don't bother checking the parameters, as the external functions should
	// have done this already

	// We need to see if we can get some fresh process control memory
	status = allocProcess(&proc);
	if (status < 0)
		return (status);

	if (!proc)
	{
		kernelError(kernel_error, "New process structure is NULL");
		return (status = ERR_NOFREE);
	}

	// Ok, we got a new, fresh process.  We need to start filling in some of
	// the process's data (after initializing it, of course).
	memset((void *) proc, 0, sizeof(kernelProcess));

	// Set the process name
	strncpy((char *) proc->name, name, MAX_PROCNAME_LENGTH);
	proc->name[MAX_PROCNAME_LENGTH] = '\0';

	// Copy the process image data
	memcpy((processImage *) &proc->execImage, &execImage,
		sizeof(processImage));

	// Set the Id number
	proc->processId = processIdCounter++;

	// By default, the type is a normal process
	proc->type = proc_normal;

	// Now, if the process Id is KERNELPROCID, then we are creating the kernel
	// process, and it will be its own parent.  Otherwise, get the current
	// process and make IT be the parent of this new process.
	if (proc->processId == KERNELPROCID)
	{
		proc->parentProcessId = proc->processId;
		// Give it "/" as current working directory
		strncpy((char *) proc->currentDirectory, "/", 2);
	}
	else
	{
		// Make sure the current process isn't NULL
		if (!kernelCurrentProcess)
		{
			kernelError(kernel_error, "Can't determine the current process");
			status = ERR_NOSUCHPROCESS;
			goto out;
		}

		// Inherit the parent process's user session (if any)
		proc->session = kernelCurrentProcess->session;

		// Set the parent Id number
		proc->parentProcessId = kernelCurrentProcess->processId;

		// Set the current working directory
		strncpy((char *) proc->currentDirectory,
			(char *) kernelCurrentProcess->currentDirectory, MAX_PATH_LENGTH);
		proc->currentDirectory[MAX_PATH_LENGTH] = '\0';
	}

	// Set the priority level
	proc->priority = priority;

	// Set the privilege level
	proc->privilege = privilege;

	// Set the processor privilege level.  The kernel and its threads get
	// PRIVILEGE_SUPERVISOR, all others get PRIVILEGE_USER.
	if (execImage->virtualAddress >= (void *) KERNEL_VIRTUAL_ADDRESS)
		proc->processorPrivilege = PRIVILEGE_SUPERVISOR;
	else
		proc->processorPrivilege = PRIVILEGE_USER;

	// The thread's initial state will be "stopped"
	proc->state = proc_stopped;

	// Add the process to the process list so we can continue whilst doing
	// things like changing memory ownerships
	status = addProcessToList(proc);
	if (status < 0)
		goto out;

	// Do we need to create a new page directory and a set of page tables for
	// this process?
	if (newPageDir)
	{
		if (!execImage->virtualAddress || !execImage->code ||
			!execImage->codeSize || !execImage->data ||
			!execImage->dataSize || !execImage->imageSize)
		{
			kernelError(kernel_error, "New process \"%s\" executable image "
				"is missing data", name);
			status = ERR_NODATA;
			goto out;
		}

		// We need to make a new page directory, etc
		proc->pageDirectory = kernelPageNewDirectory(proc->processId);
		if (!proc->pageDirectory)
		{
			// Not able to setup a page directory
			status = ERR_NOVIRTUAL;
			goto out;
		}

		// Get the physical address of the code/data
		physicalCodeData = kernelPageGetPhysical(proc->parentProcessId,
			execImage->code);

		// Make the process own its code/data memory.  Don't remap it yet
		// because we want to map it at the requested virtual address.
		status = kernelMemoryChangeOwner(proc->parentProcessId,
			proc->processId, 0, execImage->code, NULL);
		if (status < 0)
			goto out;

		// Remap the code/data to the requested virtual address
		status = kernelPageMap(proc->processId, physicalCodeData,
			execImage->virtualAddress, execImage->imageSize);
		if (status < 0)
			goto out;

		// Code should be read-only
		status = kernelPageSetAttrs(proc->processId, pageattr_readonly,
			execImage->virtualAddress, execImage->codeSize);
		if (status < 0)
			goto out;
	}
	else
	{
		// This process will share a page directory with its parent
		proc->pageDirectory = kernelPageShareDirectory(proc->parentProcessId,
			proc->processId);
		if (!proc->pageDirectory)
		{
			status = ERR_NOVIRTUAL;
			goto out;
		}
	}

	// Give the process a stack
	proc->userStackSize = DEFAULT_STACK_SIZE;
	if (proc->processorPrivilege != PRIVILEGE_SUPERVISOR)
		proc->superStackSize = DEFAULT_SUPER_STACK_SIZE;

	stackMemoryAddr = kernelMemoryGet((proc->userStackSize +
		proc->superStackSize), "process stack");
	if (!stackMemoryAddr)
	{
		status = ERR_MEMORY;
		goto out;
	}

	// Copy 'argc' and 'argv' arguments to the new process's stack while we
	// still own the stack memory

	// Calculate the amount of memory we need to allocate for argument data.
	// Leave space for pointers to the strings, since the (int argc,
	// char *argv[]) scheme means just 2 values on the stack: an integer and a
	// pointer to an array of char* pointers.
	argMemorySize = ((execImage->argc + 1) * sizeof(char *));
	for (count = 0; count < execImage->argc; count ++)
	{
		if (execImage->argv[count])
			argMemorySize += (strlen(execImage->argv[count]) + 1);
	}

	// Get memory for the argument data
	argMemory = kernelMemoryGet(argMemorySize, "process arguments");
	if (!argMemory)
	{
		status = ERR_MEMORY;
		goto out;
	}

	// Change ownership to the new process.  If it has its own page directory,
	// remap, and share it back with this process.

	if (kernelMemoryChangeOwner(proc->parentProcessId, proc->processId,
		(newPageDir? 1 : 0 /* remap */), argMemory,
		(newPageDir? (void **) &newArgPtr : NULL /* newVirtual */)) < 0)
	{
		status = ERR_MEMORY;
		goto out;
	}

	if (newPageDir)
	{
		if (kernelMemoryShare(proc->processId, proc->parentProcessId,
			newArgPtr, (void **) &argMemory) < 0)
		{
			status = ERR_MEMORY;
			goto out;
		}
	}
	else
	{
		newArgPtr = argMemory;
	}

	oldArgPtr = argMemory;

	// Set pointers to the beginning stack location for the arguments
	stackArgs = (stackMemoryAddr + proc->userStackSize - (2 * sizeof(int)));
	stackArgs[0] = execImage->argc;
	stackArgs[1] = (int) newArgPtr;

	argv = (char **) oldArgPtr;
	oldArgPtr += ((execImage->argc + 1) * sizeof(char *));
	newArgPtr += ((execImage->argc + 1) * sizeof(char *));

	// Copy the args into argv
	for (count = 0; count < execImage->argc; count ++)
	{
		if (execImage->argv[count])
		{
			strcpy((oldArgPtr + length), execImage->argv[count]);
			argv[count] = (newArgPtr + length);
			length += (strlen(execImage->argv[count]) + 1);
		}
	}

	// argv[argc] is supposed to be a NULL pointer, according to some standard
	// or other
	argv[execImage->argc] = NULL;

	if (newPageDir)
	{
		// Unmap the argument space from this process
		kernelPageUnmap(proc->parentProcessId, argMemory, argMemorySize);
		argMemory = NULL;
	}

	// Make the process own its stack memory
	status = kernelMemoryChangeOwner(proc->parentProcessId, proc->processId,
		1 /* remap */, stackMemoryAddr, (void **) &proc->userStack);
	if (status < 0)
		goto out;

	stackMemoryAddr = NULL;

	// Make the topmost page of the user stack privileged, so that we have a
	// 'guard page' that produces a page fault in case of (userspace) stack
	// overflow
	kernelPageSetAttrs(proc->processId, pageattr_privileged, proc->userStack,
		MEMORY_PAGE_SIZE);

	if (proc->processorPrivilege != PRIVILEGE_SUPERVISOR)
	{
		// Get the new virtual address of supervisor stack
		proc->superStack = (proc->userStack + DEFAULT_STACK_SIZE);

		// Make the entire supervisor stack privileged
		kernelPageSetAttrs(proc->processId, pageattr_privileged,
			proc->superStack, proc->superStackSize);
	}

	// Create the processor context for this process
	status = createProcessContext(proc);
	if (status < 0)
	{
		// Not able to create the TSS
		goto out;
	}

#ifdef ARCH_X86
	// Adjust the stack pointer to account for the arguments that we copied to
	// the process's stack
	proc->context.taskStateSegment.ESP -= sizeof(int);

	// Set the EIP to the entry point
	proc->context.taskStateSegment.EIP = (unsigned) execImage->entryPoint;
#endif

	// Get memory for the user process environment structure
	proc->environment = kernelMalloc(sizeof(variableList));
	if (!proc->environment)
	{
		status = ERR_MEMORY;
		goto out;
	}

	// Return the processId on success
	status = proc->processId;

out:
	if (status < 0)
	{
		if (stackMemoryAddr)
			kernelMemoryRelease(stackMemoryAddr);

		if (argMemory)
			kernelMemoryRelease(argMemory);

		removeProcessFromList(proc);
		freeProcess(proc);
	}

	return (status);
}


static int deleteProcess(kernelProcess *proc)
{
	// Does all the work of fully destroyng a process.  This occurs after all
	// descendent threads have terminated, for example.

	int status = 0;

	// Processes cannot delete themselves
	if (proc == kernelCurrentProcess)
	{
		kernelError(kernel_error, "Process %d cannot delete itself",
			proc->processId);
		return (status = ERR_INVALID);
	}

#ifdef ARCH_X86
	// We need to deallocate the TSS descriptor allocated to the process, if
	// it has one
	if (proc->context.tssSelector)
	{
		status = kernelDescriptorRelease(proc->context.tssSelector);
		if (status < 0)
		{
			// If this was unsuccessful, we don't want to continue and "lose"
			// the descriptor
			kernelError(kernel_error, "Can't release TSS");
			return (status);
		}
	}
#endif

	// If the process has a signal stream, destroy it
	if (proc->signalStream.buffer)
		kernelStreamDestroy(&proc->signalStream);

	// Deallocate all memory owned by this process
	status = kernelMemoryReleaseAllByProcId(proc->processId);
	if (status < 0)
	{
		// If this deallocation was unsuccessful, we don't want to deallocate
		// the process structure.  If we did, the memory would become "lost".
		kernelError(kernel_error, "Can't release process memory");
		return (status);
	}

	// Delete the page table we created for this process
	status = kernelPageDeleteDirectory(proc->processId);
	if (status < 0)
	{
		// If this deletion was unsuccessful, we don't want to deallocate the
		// process structure.  If we did, the page directory would become
		// "lost".
		kernelError(kernel_error, "Can't release page directory");
		return (status);
	}

	// If this is a normal process and it has an environment memory structure,
	// deallocate it (threads share environment with their parents)
	if ((proc->type == proc_normal) && proc->environment)
	{
		variableListDestroy(proc->environment);

		status = kernelFree(proc->environment);
		if (status < 0)
		{
			kernelError(kernel_error, "Can't release environment structure");
			return (status);
		}
	}

	// If this is a normal process and there's a symbol table for it,
	// deallocate the table (threads share tables with their parents)
	if ((proc->type == proc_normal) && proc->symbols)
	{
		status = kernelFree(proc->symbols);
		if (status < 0)
		{
			kernelError(kernel_error, "Can't release symbol table");
			return (status);
		}
	}

	// If this process was using the FPU, it's not any more
	if (fpuProcess == proc)
		fpuProcess = NULL;

	// Remove the process from the multitasker's process list
	status = removeProcessFromList(proc);
	if (status < 0)
	{
		// Not able to remove the process
		kernelError(kernel_error, "Can't delist process");
		return (status);
	}

	// Finally, release the process structure
	status = freeProcess(proc);
	if (status < 0)
	{
		kernelError(kernel_error, "Can't release process structure");
		return (status);
	}

	return (status = 0);
}


static void exceptionHandler(void)
{
	// This code is the general exception handler.  Before multitasking
	// starts, it will be called as a function in the exception context (the
	// context of the process that experienced the exception).  After
	// multitasking starts, it is a separate kernel thread that sleeps until
	// woken up.

	int status = 0;
	kernelProcess *proc = NULL;
	char *message = NULL;
	char *details = NULL;
	const char *symbolName = NULL;
	const char *kernOrApp = NULL;

	message = kernelMalloc(MAXSTRINGLENGTH + 1);
	details = kernelMalloc(MAXSTRINGLENGTH + 1);
	if (!message || !details)
	{
		status = ERR_MEMORY;
		goto out;
	}

	while (1)
	{
		// We got an exception

		proc = kernelCurrentProcess;
		kernelCurrentProcess = exceptionProc;

		if (multitaskingEnabled)
		{
			if (!proc)
			{
				// We have to make an error here.  We can't return to the
				// program that caused the exception, and we can't tell the
				// multitasker to kill it.  We'd better make a kernel panic.
				kernelPanic("Exception handler unable to determine current "
					"process");
			}
			else
			{
				proc->state = proc_stopped;
			}
		}

		if (!multitaskingEnabled || (proc == kernelProc))
			sprintf(message, "The kernel has experienced");
		else
			sprintf(message, "Process \"%s\" caused", proc->name);

		sprintf((message + strlen(message)), " %s %s exception",
			exceptionVector[processingException].a,
			exceptionVector[processingException].name);

		if (exceptionAddress >= KERNEL_VIRTUAL_ADDRESS)
			kernOrApp = "kernel";
		else
			kernOrApp = "application";

		if (multitaskingEnabled)
		{
			// Find roughly the symbolic address where the exception happened
			symbolName = kernelLookupClosestSymbol(proc, (void *)
				exceptionAddress);
		}

		if (symbolName)
		{
			sprintf((message + strlen(message)), " in %s function %s (%08x)",
				kernOrApp, symbolName, exceptionAddress);
		}
		else
		{
			sprintf((message + strlen(message)), " at %s address %08x",
				kernOrApp, exceptionAddress);
		}

		if (kernelProcessingInterrupt())
		{
			sprintf((message + strlen(message)), " while processing "
				"interrupt %d", kernelInterruptGetCurrent());

			// If the fault occurred while we were processing an interrupt, we
			// should tell the PIC that the interrupt service function is
			// finished.  It's not really fair to kill a process because an
			// interrupt handler is screwy, but that's what we have to do for
			// the time being.
			kernelPicEndOfInterrupt(0xFF);
		}

		kernelError(kernel_error, "%s", message);

		if (multitaskingEnabled)
		{
			// Get process info
			debugContext(proc, details, MAXSTRINGLENGTH);

			// Try a stack trace
			kernelStackTrace(proc, (details + strlen(details)),
				(MAXSTRINGLENGTH - strlen(details)));

			// Output to the console
			kernelTextPrintLine("%s", details);
		}

		if (!multitaskingEnabled || (proc == kernelProc))
		{
			// If it's the kernel, we're finished
			kernelPanic("%s", message);
		}

		// If we're in graphics mode, make an error dialog (but don't get into
		// an endless loop if the crashed process was an error dialog thread
		// itself)
		if (kernelGraphicsAreEnabled() && strcmp((char *) proc->name,
			ERRORDIALOG_THREADNAME))
		{
			kernelErrorDialog("Application Exception", message, details);
		}

		// The scheduler may now dismantle the process
		proc->state = proc_finished;

		kernelInterruptClearCurrent();
		processingException = 0;
		exceptionAddress = 0;

		// Yield the timeslice back to the scheduler.  The scheduler will take
		// care of dismantling the process.
		kernelMultitaskerYield();
	}

out:
	if (message)
		kernelFree(message);
	if (details)
		kernelFree(details);

	kernelMultitaskerTerminate(status);
}


static int spawnExceptionThread(void)
{
	// This function will initialize the kernel's exception handler thread. It
	// should be called after multitasking has been initialized.

	int status = 0;
	int procId = 0;

	// Create the kernel's exception handler thread
	procId = kernelMultitaskerSpawn(&exceptionHandler, "exception thread",
		0 /* no args */, NULL /* no args */, 0 /* don't run */);
	if (procId < 0)
		return (status = procId);

	exceptionProc = getProcessById(procId);
	if (!exceptionProc)
		return (status = ERR_NOCREATE);

	// Set the process state to sleep
	exceptionProc->state = proc_sleeping;

#ifdef ARCH_X86
	status = kernelDescriptorSet(
		exceptionProc->context.tssSelector,	// TSS selector
		&exceptionProc->context.taskStateSegment, // Starts at...
		sizeof(x86TSS),						// Maximum size of a TSS selector
		1,									// Present in memory
		PRIVILEGE_SUPERVISOR,				// Highest privilege level
		0,									// TSS's are system segs
		0x9,								// TSS, 32-bit, non-busy
		0,									// 0 for SMALL size granularity
		0);									// Must be 0 in TSS
	if (status < 0)
		return (status);

	// Interrupts should always be disabled for this task
	exceptionProc->context.taskStateSegment.EFLAGS = 0x00000002;
#endif

	return (status = 0);
}


__attribute__((noreturn))
static void idleThread(void)
{
	// This is the idle thread.  It runs in this loop whenever no other
	// processes need the CPU.  This should be run at the absolute lowest
	// possible priority so that it will not be chosen to run unless there is
	// nothing else.

	kernelProcess *proc = NULL;
	linkedListItem *iter = NULL;

	while (1)
	{
		// Idle the processor until something happens
		processorIdle();

		// Loop through the process list looking for any that have changed
		// state to "I/O ready"

		proc = linkedListIterStart(&processList, &iter);

		while (proc)
		{
			if (proc->state == proc_ioready)
			{
				kernelMultitaskerYield();
				break;
			}

			proc = linkedListIterNext(&processList, &iter);
		}
	}
}


static int spawnIdleThread(void)
{
	// This function will create the idle thread at initialization time.
	// Returns 0 on success, negative otherwise.

	int status = 0;
	int idleProcId = 0;

	// The idle thread needs to be a child of the kernel
	idleProcId = kernelMultitaskerSpawn(idleThread, "idle thread",
		0 /* no args */, NULL /* no args */, 1 /* run */);
	if (idleProcId < 0)
		return (idleProcId);

	idleProc = getProcessById(idleProcId);
	if (!idleProc)
		return (status = ERR_NOSUCHPROCESS);

	// Set it to the lowest priority
	status = kernelMultitaskerSetProcessPriority(idleProcId,
		(PRIORITY_LEVELS - 1));
	if (status < 0)
	{
		// There's no reason we should have to fail here, but make a warning
		kernelError(kernel_warn, "The multitasker was unable to lower the "
			"priority of the idle thread");
	}

	// Return success
	return (status = 0);
}


static int markProcessBusy(kernelProcess *proc, int busy)
{
	// This function gets the requested TSS selector from the GDT and marks it
	// as busy/not busy.  Returns negative on error.

	int status = 0;

#ifdef ARCH_X86
	kernelDescriptor descriptor;

	status = kernelDescriptorGet(proc->context.tssSelector, &descriptor);
	if (status < 0)
		return (status);

	if (busy)
		descriptor.attributes1 |= 0x2;
	else
		descriptor.attributes1 &= ~0x2;

	// Re-set the descriptor in the GDT
	status =  kernelDescriptorSetUnformatted(proc->context.tssSelector,
		descriptor.segSizeByte1, descriptor.segSizeByte2,
		descriptor.baseAddress1, descriptor.baseAddress2,
		descriptor.baseAddress3, descriptor.attributes1,
		descriptor.attributes2, descriptor.baseAddress4);
	if (status < 0)
		return (status);
#else
	if (proc && busy) { }
#endif

	// Return success
	return (status = 0);
}


static int schedulerShutdown(void)
{
	// This function will perform all of the necessary shutdown to stop the
	// sheduler and return control to the kernel's main task.  This will
	// probably only be useful at system shutdown time.

	// NOTE that this function should NEVER be called directly.  If this
	// advice is ignored, you have no assurance that things will occur the way
	// you might expect them to.  To shut down the scheduler, set the variable
	// schedulerStop to a nonzero value.  The scheduler will then invoke this
	// function when it's ready.

	int status = 0;

	// Restore the normal operation of the system timer 0, which is mode 3,
	// initial count of 0
	status = kernelSysTimerSetupTimer(0 /* timer */, 3 /* mode */,
		0 /* count 0x10000 */);
	if (status < 0)
		kernelError(kernel_warn, "Couldn't restore system timer");

	// Restore the old default system timer interrupt handler
	status = kernelSysTimerHook();
	if (status < 0)
		kernelError(kernel_warn, "Couldn't hook system timer interrupt");

#ifdef ARCH_X86
	// Give exclusive control to the current task
	markProcessBusy(kernelCurrentProcess, 0);
	processorFarJump(kernelCurrentProcess->context.tssSelector);
#endif

	// We should never get here
	return (status = 0);
}


static kernelProcess *chooseNextProcess(void)
{
	// Loops through the process list, and determines which process to run
	// next

	unsigned long long theTime = 0;
	kernelProcess *miscProc = NULL;
	kernelProcess *nextProc = NULL;
	linkedListItem *iter = NULL;
	unsigned processWeight = 0;
	unsigned topProcessWeight = 0;

	// Here is where we make decisions about which tasks to schedule, and
	// when.  Below is a brief description of the scheduling algorithm.

	// Priority level 0 (highest-priority) processes will be "real time"
	// scheduled.  When there are any processes running and ready at this
	// priority level, they will be serviced to the exclusion of all processes
	// not at level 0.  Not even the kernel process has this level of
	// priority.

	// The last (lowest-priority) priority level will be "background"
	// scheduled.  Processes at this level will only receive processor time
	// when there are no ready processes at any other level.  Unlike processes
	// at any other level, it will be possible for background processes to
	// starve.

	// The number of priority levels is flexible based on the configuration
	// macro in the multitasker's header file.  However, the "special" levels
	// mentioned above will exhibit the same behavior regardless of the number
	// of "normal" priority levels in the system.

	// Amongst all of the processes at other priority levels, there will be a
	// more even-handed approach to scheduling.  We will attempt a fair
	// algorithm with a weighting scheme.  Among the weighting variables will
	// be the following: priority, waiting time, and "shortness".  Shortness
	// will probably come later (shortest-job-first), so for now we will
	// concentrate on priority and waiting time.  The formula will look like
	// this:
	//
	// weight = ((PRIORITY_LEVELS - task_priority) * PRIORITY_RATIO) +
	//		wait_time
	//
	// This means that the inverse of the process priority will be multiplied
	// by the "priority ratio", and to that will be added the current waiting
	// time.  For example, if we have 4 priority levels, the priority ratio is
	// 3, and we have two tasks as follows:
	//
	//	Task 1: priority=1, waiting time=4
	//	Task 2: priority=2, waiting time=6
	//
	// then
	//
	//	task1Weight = ((4 - 1) * 3) + 4 = 13  <- winner
	//	task2Weight = ((4 - 2) * 3) + 6 = 12
	//
	// Thus, even though task 2 has been waiting longer, task 1's higher
	// priority wins.  However in a slightly different scenario -- using the
	// same constants -- if we had:
	//
	//	Task 1: priority=1, waiting time=3
	//	Task 2: priority=2, waiting time=7
	//
	// then
	//
	//	task1Weight = ((4 - 1) * 3) + 3 = 12
	//	task2Weight = ((4 - 2) * 3) + 7 = 13  <- winner
	//
	// In this case, task 2 gets to run since it has been waiting long enough
	// to overcome task 1's higher priority.  This possibility helps to ensure
	// that no processes will starve.  The priority ratio determines the
	// weighting of priority vs. waiting time.  A priority ratio of zero would
	// give higher-priority processes no advantage over lower-priority, and
	// waiting time would determine execution order.
	//
	// A tie beteen the highest-weighted tasks is broken based on list order.
	// The list is neither FIFO nor LIFO (it's unordered), but closer to LIFO.

	// Get the CPU time
	theTime = kernelCpuGetMs();

	miscProc = linkedListIterStart(&processList, &iter);

	while (miscProc)
	{
		if (miscProc->state == proc_waiting)
		{
			// This will change the state of a waiting process to "ready" if
			// the specified "waiting reason" has come to pass

			// If the process is waiting for a specified time.  Has the
			// requested time come?
			if (miscProc->waitUntil && (miscProc->waitUntil < theTime))
			{
				// The process is ready to run
				miscProc->state = proc_ready;
			}
			else
			{
				// The process must continue waiting
				goto checkNext;
			}
		}

		if (miscProc->state == proc_finished)
		{
			// This will dismantle any process that has identified itself as
			// finished, and remove it from the list
			kernelMultitaskerKillProcess(miscProc->processId);
			goto checkNext;
		}

		if ((miscProc->state != proc_ready) &&
			(miscProc->state != proc_ioready))
		{
			// This process is not ready (might be stopped, sleeping, or
			// zombie)
			goto checkNext;
		}

		// This process is ready to run.  Determine its weight.

		if (!miscProc->priority)
		{
			// If the process is of the highest (real-time) priority, it
			// should get an infinite weight
			processWeight = 0xFFFFFFFF;
		}
		else if (miscProc->priority == (PRIORITY_LEVELS - 1))
		{
			// Else if the process is of the lowest priority, it should get a
			// weight of zero
			processWeight = 0;
		}
		else if (miscProc->state == proc_ioready)
		{
			// If this process was waiting for I/O which has now arrived, give
			// it a high (1) temporary priority level
			processWeight = (((PRIORITY_LEVELS - 1) * PRIORITY_RATIO) +
				miscProc->waitTime);
		}
		else if (schedulerSwitchedByCall && (miscProc->lastSlice ==
			schedulerTimeslices))
		{
			// If this process has yielded this timeslice already, we should
			// give it no weight this time so that a bunch of yielding
			// processes don't gobble up all the CPU time.
			processWeight = 0;
		}
		else
		{
			// Otherwise, calculate the weight of this task, using the
			// algorithm described above
			processWeight = (((PRIORITY_LEVELS - miscProc->priority) *
				PRIORITY_RATIO) + miscProc->waitTime);
		}

		// Did this process win?

		if (processWeight < topProcessWeight)
		{
			// No.  Increase the waiting time of this process, since it's not
			// the one we're selecting.
			miscProc->waitTime += 1;
		}
		else
		{
			if (nextProc)
			{
				if ((processWeight == topProcessWeight) &&
					(nextProc->waitTime >= miscProc->waitTime))
				{
					// If the process's weight is tied with that of the
					// previously winning process, it will NOT win if the
					// other process has been waiting as long or longer
					miscProc->waitTime += 1;
					goto checkNext;
				}
				else
				{
					// We have a new winning process.  Increase the waiting
					// time of the previous winner this one is replacing.
					nextProc->waitTime += 1;
				}
			}

			topProcessWeight = processWeight;
			nextProc = miscProc;
		}

	checkNext:
		miscProc = linkedListIterNext(&processList, &iter);
	}

	return (nextProc);
}


static int scheduler(void)
{
	// This is the kernel multitasker's scheduler thread.  This little program
	// will run continually in a loop, handing out time slices to all
	// processes, including the kernel itself, initially, before it goes to
	// sleep.

	// By the time this scheduler is invoked, the kernel should already have
	// created itself a process in the process list.  Thus, the scheduler can
	// begin by simply handing all time slices to the kernel.

	// Additional processes will be created with calls to the kernel, which
	// will create them and place them in the list.  Thus, when the scheduler
	// regains control after a time slice has expired, the list of processes
	// that it examines will have the new process added.

	int status = 0;
	unsigned timeUsed = 0;
	unsigned systemTime = 0;
	unsigned schedulerTime = 0;
	unsigned sliceCount = 0;
	unsigned oldSliceCount = 0;
	kernelProcess *listProc = NULL;
	linkedListItem *iter = NULL;

	// This is info about the processes we run
	kernelProcess *nextProc = NULL;
	kernelProcess *prevProc = NULL;

	// Here is the scheduler's big loop

	while (!schedulerStop)
	{
		// Make sure.  No interrupts allowed inside this task.
		processorDisableInts();

		// The scheduler is the current process
		kernelCurrentProcess = schedulerProc;

		// Calculate how many timer ticks were used in the previous time
		// slice.  This will be different depending on whether the previous
		// timeslice actually expired, or whether we were called for some
		// other reason (for example a yield()).

		if (!schedulerSwitchedByCall)
			timeUsed = TIME_SLICE_LENGTH;
		else
			timeUsed = (TIME_SLICE_LENGTH - kernelSysTimerReadValue(0));

		// Count the time used for legacy system timer purposes
		systemTime += timeUsed;

		// Have we had the equivalent of a full timer revolution?  If so, we
		// need to call the standard timer interrupt handler.
		if (systemTime >= SYSTIMER_FULLCOUNT)
		{
			// Reset to zero
			systemTime = 0;

			// Artifically register a system timer tick
			kernelSysTimerTick();
		}

		// Count the time used for the purpose of tracking CPU usage
		schedulerTime += timeUsed;
		sliceCount = (schedulerTime / TIME_SLICE_LENGTH);
		if (sliceCount > oldSliceCount)
		{
			// Increment the count of time slices.  This can just keep going
			// up until it wraps, which is no problem.
			schedulerTimeslices += 1;

			oldSliceCount = sliceCount;
		}

		// Remember the previous process we ran
		prevProc = nextProc;

		if (prevProc)
		{
			if (prevProc->state == proc_running)
			{
				// Change the state of the previous process to ready, since it
				// was interrupted while still on the CPU
				prevProc->state = proc_ready;
			}

			// Add the last timeslice to the process's CPU time
			prevProc->cpuTime += timeUsed;

			// Record the current timeslice number, so we can remember when
			// this process was last active (see chooseNextProcess())
			prevProc->lastSlice = schedulerTimeslices;
		}

		// Every CPU_PERCENT_TIMESLICES timeslices we will update the %CPU
		// value for each process currently in the list
		if (sliceCount >= CPU_PERCENT_TIMESLICES)
		{
			// Calculate the CPU percentage

			listProc = linkedListIterStart(&processList, &iter);

			while (listProc)
			{
				if (!schedulerTime)
				{
					listProc->cpuPercent = 0;
				}
				else
				{
					listProc->cpuPercent = ((listProc->cpuTime * 100) /
						schedulerTime);
				}

				// Reset the process's cpuTime counter
				listProc->cpuTime = 0;

				listProc = linkedListIterNext(&processList, &iter);
			}

			// Reset the schedulerTime and slice counters
			schedulerTime = sliceCount = oldSliceCount = 0;
		}

		if (processingException)
		{
			// If we were processing an exception (either the exception
			// process or another exception handler), keep it active
			nextProc = prevProc;
			kernelDebugError("Scheduler interrupt while processing "
				"exception");
		}
		else
		{
			// Choose the next process to run
			nextProc = chooseNextProcess();
		}

		// We should now have selected a process to run.  If not, we should
		// re-start the old one.  This should only be likely to happen if
		// something kills the idle thread.
		if (!nextProc)
			nextProc = prevProc;

		// Update some info about the next process
		nextProc->waitTime = 0;
		nextProc->state = proc_running;

		// Export (to the rest of the multitasker) the pointer to the
		// currently selected process
		kernelCurrentProcess = nextProc;

		if (!schedulerSwitchedByCall)
		{
			// Acknowledge the timer interrupt if one occurred
			kernelPicEndOfInterrupt(INTERRUPT_NUM_SYSTIMER);
		}
		else
		{
			// Reset the "switched by call" flag
			schedulerSwitchedByCall = 0;
		}

		// Set up a new time slice - PIT single countdown
		while (kernelSysTimerSetupTimer(0 /* timer */, 0 /* mode */,
			TIME_SLICE_LENGTH) < 0)
		{
			kernelError(kernel_warn, "The scheduler was unable to control "
				"the system timer");
		}

		// In the final part, we do the actual context switch

		// Mark the exception handler and scheduler tasks as not busy so they
		// can be jumped back to
		if (exceptionProc)
			markProcessBusy(exceptionProc, 0);
		markProcessBusy(schedulerProc, 0);

		// Mark the next task as not busy and jump to it
		markProcessBusy(nextProc, 0);

#ifdef ARCH_X86
		processorFarJump(nextProc->context.tssSelector);
#endif

		// Continue to loop
	}

	// If we get here, then the scheduler is supposed to shut down
	schedulerShutdown();

	// We should never get here
	return (status = 0);
}


static int schedulerInitialize(void)
{
	// This function will do all of the necessary initialization for the
	// scheduler.  Returns 0 on success, negative otherwise.

	// The scheduler needs to make a task (but not a fully-fledged process)
	// for itself

	int status = 0;
	int interrupts = 0;
	processImage schedImage = {
		scheduler, scheduler,
		NULL, 0xFFFFFFFF,
		NULL, 0xFFFFFFFF,
		0xFFFFFFFF,
		"", 0, { NULL }
	};

	status = createNewProcess("scheduler process", kernelProc->priority,
		kernelProc->privilege, &schedImage, 0 /* no page directory */);
	if (status < 0)
		return (status);

	schedulerProc = getProcessById(status);

	// The scheduler process doesn't sit in the normal process list
	removeProcessFromList(schedulerProc);

	// Interrupts should always be disabled for this task
#ifdef ARCH_X86
	schedulerProc->context.taskStateSegment.EFLAGS = 0x00000002;
#endif

	// Not busy
	markProcessBusy(schedulerProc, 0);

	kernelDebug(debug_multitasker, "Multitasker initialize scheduler");

	// Disable interrupts, so we can ensure that we don't immediately get a
	// timer interrupt
	processorSuspendInts(interrupts);

	// Hook the system timer interrupt
	kernelDebug(debug_multitasker, "Multitasker hook system timer interrupt");

	// Install a task gate for the interrupt, which will be the scheduler's
	// timer interrupt.  After this point, our new scheduler task will run
	// with every clock tick.
	status = kernelInterruptHook(INTERRUPT_NUM_SYSTIMER, (void *)
		schedulerProc->context.tssSelector);
	if (status < 0)
	{
		processorRestoreInts(interrupts);
		return (status);
	}

	// The scheduler task should now be set up to run.  We should set up the
	// kernel task to resume operation.

	// Before we load the kernel's selector into the task reg, mark it as not
	// busy, since one cannot load the task register with a busy TSS selector
	markProcessBusy(kernelProc, 0);

#ifdef ARCH_X86
	// Make the kernel's Task State Segment be the current one
	processorLoadTaskReg(kernelProc->context.tssSelector);
#endif

	// Make note that the multitasker has been enabled
	multitaskingEnabled = 1;

	// Set up the initial timer countdown
	kernelSysTimerSetupTimer(0 /* timer */, 0 /* mode */, TIME_SLICE_LENGTH);

	processorRestoreInts(interrupts);

	// Yield control to the scheduler
	kernelMultitaskerYield();

	return (status = 0);
}


static void floatingPointInitialize(void)
{
#ifdef ARCH_X86
	unsigned cr0 = 0;

	// Initialize the CPU for floating point operation.  We set
	// CR0[MP]=1 (math present)
	// CR0[EM]=0 (no emulation)
	// CR0[NE]=1 (floating point errors cause exceptions)
	processorGetCR0(cr0);
	cr0 = ((cr0 & ~0x04U) | 0x22);
	processorSetCR0(cr0);
#endif
}


static int createKernelProcess(void *kernelStack, unsigned kernelStackSize)
{
	// This function will create the kernel process at initialization time.
	// Returns 0 on success, negative otherwise.

	int status = 0;
	int kernelProcId = 0;
	processImage kernImage = {
		(void *) KERNEL_VIRTUAL_ADDRESS,
		kernelMain,
		NULL, 0xFFFFFFFF,
		NULL, 0xFFFFFFFF,
		0xFFFFFFFF,
		"", 0, { NULL }
	};

	// The kernel process is its own parent, of course, and it is owned by
	// "admin".  We create no page directory, and there are no arguments.
	kernelProcId = createNewProcess("kernel process", 1, PRIVILEGE_SUPERVISOR,
		&kernImage, 0 /* no page directory */);
	if (kernelProcId < 0)
		return (kernelProcId);

	// Get the pointer to the kernel's process
	kernelProc = getProcessById(kernelProcId);

	// Make sure it's not NULL
	if (!kernelProc)
		return (status = ERR_NOSUCHPROCESS);

#ifdef ARCH_X86
	// Interrupts are initially disabled for the kernel
	kernelProc->context.taskStateSegment.EFLAGS = 0x00000002;
#endif

	// Set the current process to initially be the kernel process
	kernelCurrentProcess = kernelProc;

	// Deallocate the stack that was allocated, since the kernel already has
	// one set up by the OS loader
	kernelMemoryRelease(kernelProc->userStack);
	kernelProc->userStack = kernelStack;
	kernelProc->userStackSize = kernelStackSize;

	// Make the kernel's text streams be the console streams
	kernelProc->textInputStream = kernelTextGetConsoleInput();
	kernelProc->textInputStream->ownerPid = KERNELPROCID;
	kernelProc->textOutputStream = kernelTextGetConsoleOutput();

	// Make the kernel process runnable
	kernelProc->state = proc_ready;

	// Return success
	return (status = 0);
}


static void incrementDescendents(kernelProcess *proc)
{
	// This will walk up a chain of dependent child threads, incrementing the
	// descendent count of each parent

	kernelProcess *parentProc = NULL;

	if (proc->processId == KERNELPROCID)
	{
		// The kernel is its own parent
		return;
	}

	parentProc = getProcessById(proc->parentProcessId);
	if (!parentProc)
	{
		// No worries.  Probably not a problem.
		return;
	}

	parentProc->descendentThreads++;

	if (parentProc->type == proc_thread)
	{
		// Do a recursion to walk up the chain
		incrementDescendents(parentProc);
	}
}


static void decrementDescendents(kernelProcess *proc)
{
	// This will walk up a chain of dependent child threads, decrementing the
	// descendent count of each parent

	kernelProcess *parentProc = NULL;

	if (proc->processId == KERNELPROCID)
	{
		// The kernel is its own parent
		return;
	}

	parentProc = getProcessById(proc->parentProcessId);
	if (!parentProc)
	{
		// No worries.  Probably not a problem.
		return;
	}

	parentProc->descendentThreads--;

	if (parentProc->type == proc_thread)
	{
		// Do a recursion to walk up the chain
		decrementDescendents(parentProc);
	}
}


static void kernelProcess2Process(kernelProcess *kernProc, process *userProc)
{
	// Given a kernel-space process structure, create the corresponding
	// user-space version

	strncpy(userProc->name, (char *) kernProc->name, MAX_PROCNAME_LENGTH);
	strncpy(userProc->userId, (kernProc->session?
		kernProc->session->name : ""), USER_MAX_NAMELENGTH);
	userProc->processId = kernProc->processId;
	userProc->type = kernProc->type;
	userProc->priority = kernProc->priority;
	userProc->privilege = kernProc->privilege;
	userProc->parentProcessId = kernProc->parentProcessId;
	userProc->descendentThreads = kernProc->descendentThreads;
	userProc->cpuPercent = kernProc->cpuPercent;
	userProc->state = kernProc->state;
}


static int fpuExceptionHandler(void)
{
	// This function gets called when a EXCEPTION_DEVNOTAVAIL (7) exception
	// occurs.  It can happen under two circumstances:
	// CR0[EM] is set: No FPU is present.  We can implement emulation here
	//		later in this case, if we want.
	// CR0[TS] and CR0[MP] are set: A task switch has occurred since the last
	//		FP operation, and we need to restore the state.

	int status = 0;

#ifdef ARCH_X86

	unsigned short fpuReg = 0;

	//kernelDebug(debug_multitasker, "Multitasker FPU exception start");

	processorClearTaskSwitched();

	if (fpuProcess && (fpuProcess == kernelCurrentProcess))
	{
		// This was the last process to use the FPU.  The state should be the
		// same as it was, so there's nothing to do.
		//kernelDebug(debug_multitasker, "Multitasker FPU exception end: "
		//	"nothing to do");
		return (status = 0);
	}

	processorGetFpuStatus(fpuReg);
	while (fpuReg & 0x8000)
	{
		kernelDebugError("FPU is busy");
		processorGetFpuStatus(fpuReg);
	}

	// Save the FPU state for the previous process
	if (fpuProcess)
	{
		// Save FPU state
		//kernelDebug(debug_multitasker, "Multitasker switch FPU ownership "
		//	"from %s to %s", fpuProcess->name,
		//	kernelCurrentProcess->name);
		//kernelDebug(debug_multitasker, "Multitasker save FPU state for %s",
		//	fpuProcess->name);
		processorFpuStateSave(fpuProcess->context.fpuState[0]);
		fpuProcess->context.fpuStateSaved = 1;
	}

	if (kernelCurrentProcess->context.fpuStateSaved)
	{
		// Restore the FPU state
		//kernelDebug(debug_multitasker, "Multitasker restore FPU state for "
		//	"%s", kernelCurrentProcess->name);
		processorFpuStateRestore(kernelCurrentProcess->context.fpuState[0]);
	}
	else
	{
		// No saved state for the FPU.  Initialize it.
		//kernelDebug(debug_multitasker, "Multitasker initialize FPU for %s",
		//	kernelCurrentProcess->name);
		processorFpuInit();
		processorGetFpuControl(fpuReg);
		// Mask FPU exceptions.
		fpuReg |= 0x3F;
		processorSetFpuControl(fpuReg);
	}

	kernelCurrentProcess->context.fpuStateSaved = 0;

	processorFpuClearEx();

	fpuProcess = kernelCurrentProcess;

#endif

	//kernelDebug(debug_multitasker, "Multitasker FPU exception end");
	return (status = 0);
}


static int propagateEnvironmentRecursive(kernelProcess *parentProc,
	variableList *srcEnv, const char *variable)
{
	// Recursive propagation of the value of environment variables to child
	// processes.  If 'variable' is set, only the named variable will
	// propagate.  Otherwise, all parent variables will propagate.  Variables
	// in the childrens' environments that don't exist in the parent process
	// are unaffected.

	int status = 0;
	kernelProcess *childProc = NULL;
	linkedListItem *iter = NULL;
	const char *currentVariable = NULL;
	const char *value = NULL;
	int count;

	childProc = linkedListIterStart(&processList, &iter);

	while (childProc)
	{
		if (childProc->parentProcessId == parentProc->processId)
		{
			if (childProc->type != proc_thread)
			{
				kernelDebug(debug_multitasker, "Multitasker propagate "
					"environment from %s to %s", parentProc->name,
					childProc->name);

				// Set variables
				for (count = 0; count < srcEnv->numVariables; count ++)
				{
					currentVariable = variableListGetVariable(srcEnv, count);

					if (!variable || !strcmp(variable, currentVariable))
					{
						value = variableListGet(srcEnv, currentVariable);

						if (value)
						{
							variableListSet(childProc->environment,
								currentVariable, value);
						}

						if (variable)
							break;
					}
				}
			}

			// Recurse to propagate to the child process's children
			status = propagateEnvironmentRecursive(childProc, srcEnv,
				variable);
			if (status < 0)
				return (status);
		}

		childProc = linkedListIterNext(&processList, &iter);
	}

	return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelMultitaskerInitialize(void *kernelStack, unsigned kernelStackSize)
{
	// This function intializes the kernel's multitasker

	int status = 0;

	// Make sure multitasking is NOT enabled already
	if (multitaskingEnabled)
		return (status = ERR_ALREADY);

	// Initialize the process list
	memset(&processList, 0, sizeof(linkedList));

	// Initialize floating point handling
	floatingPointInitialize();

	// We need to create the kernel's own process
	status = createKernelProcess(kernelStack, kernelStackSize);
	if (status < 0)
		return (status);

	// Now start the scheduler
	status = schedulerInitialize();
	if (status < 0)
	{
		// The scheduler couldn't start
		return (status);
	}

	// Create an "idle" thread to consume all unused cycles
	status = spawnIdleThread();
	if (status < 0)
		return (status);

	// Set up any specific exception handlers
	exceptionVector[EXCEPTION_DEVNOTAVAIL].handler = fpuExceptionHandler;

	// Start the exception handler thread
	status = spawnExceptionThread();
	if (status < 0)
		return (status);

	// Log a boot message
	kernelLog("Multitasking started");

	// Return success
	return (status = 0);
}


int kernelMultitaskerShutdown(int nice)
{
	// This function will shut down the multitasker and halt the scheduler,
	// returning exclusive control to the kernel process.  If the nice
	// argument is non-zero, this function will do a nice orderly shutdown,
	// killing all the running processes gracefully.  If it is zero, the
	// resources allocated to the processes will never be freed, and the
	// multitasker will just stop.  Returns 0 on success, negative otherwise.

	int status = 0;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
	{
		// We can't shut down if we're not multitasking yet
		return (status = ERR_NOTINITIALIZED);
	}

	// If we are doing a "nice" shutdown, we will kill all the running
	// processes (except the kernel and scheduler) gracefully
	if (nice)
		kernelMultitaskerKillAll();

	// Set the schedulerStop flag to stop the scheduler
	schedulerStop = 1;

	// Yield control back to the scheduler, so that it can stop
	kernelMultitaskerYield();

	// Make note that the multitasker has been disabled
	multitaskingEnabled = 0;

	// Deallocate the stack used by the scheduler
	kernelMemoryRelease(schedulerProc->userStack);

	// Log a message
	kernelLog("Multitasking stopped");

	return (status = 0);
}


void kernelException(int num, unsigned address)
{
	// If we are already processing one, then it's a double-fault and we are
	// totally finished
	if (processingException)
	{
		kernelPanic("Double-fault (%s) while processing %s %s exception",
			exceptionVector[num].name, exceptionVector[processingException].a,
			exceptionVector[processingException].name);
	}

	processingException = num;
	exceptionAddress = address;

	// If there's a handler for this exception type, call it
	if (exceptionVector[processingException].handler &&
		(exceptionVector[processingException].handler() >= 0))
	{
		// The exception was handled.  Return to the caller.
		processingException = 0;
		exceptionAddress = 0;
		return;
	}

#ifdef ARCH_X86
	// If multitasking is enabled, switch to the exception thread.  Otherwise
	// just call the exception handler as a function.
	if (multitaskingEnabled)
		processorFarJump(exceptionProc->context.tssSelector);
	else
		exceptionHandler();
#endif

	// If the exception is handled, then we return
}


void kernelMultitaskerDumpProcessList(void)
{
	// This function is used to dump an internal listing of the current
	// process to the output

	kernelTextOutputStream *currentOutput = NULL;
	kernelProcess *proc = NULL;
	linkedListItem *iter = NULL;
	char buffer[1024];

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return;

	// Get the current output stream
	currentOutput = kernelTextGetCurrentOutput();

	if (processList.numItems > 0)
	{
		kernelTextStreamPrintLine(currentOutput, "Process list:");

		proc = linkedListIterStart(&processList, &iter);

		while (proc)
		{
			sprintf(buffer, "\"%s\"  PID=%d UID=%s priority=%d priv=%d "
				"parent=%d\n        %d%% CPU State=",
				(char *) proc->name, proc->processId, (proc->session?
				proc->session->name : ""), proc->priority, proc->privilege,
				proc->parentProcessId, proc->cpuPercent);

			// Get the state
			switch(proc->state)
			{
				case proc_running:
					strcat(buffer, "running");
					break;

				case proc_ready:
				case proc_ioready:
					strcat(buffer, "ready");
					break;

				case proc_waiting:
					strcat(buffer, "waiting");
					break;

				case proc_sleeping:
					strcat(buffer, "sleeping");
					break;

				case proc_stopped:
					strcat(buffer, "stopped");
					break;

				case proc_finished:
					strcat(buffer, "finished");
					break;

				case proc_zombie:
					strcat(buffer, "zombie");
					break;

				default:
					strcat(buffer, "unknown");
					break;
			}

			kernelTextStreamPrintLine(currentOutput, buffer);

			proc = linkedListIterNext(&processList, &iter);
		}
	}
	else
	{
		// This doesn't seem at all likely
		kernelTextStreamPrintLine(currentOutput, "No processes remaining");
	}

	kernelTextStreamNewline(currentOutput);
}


int kernelMultitaskerCreateProcess(const char *name, int privilege,
	processImage *execImage)
{
	// This function is called to set up an (initially) single-threaded
	// process in the multitasker.  This is the function used by external
	// sources -- the loader for example -- to define new processes.  This
	// new process thread we're creating will have its state set to "stopped"
	// after this call.  The caller should use the
	// kernelMultitaskerSetProcessState() function to start the new process.
	// This function returns the processId of the new process on success,
	// negative otherwise.

	int status = 0;
	int processId = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!name || !execImage)
		return (status = ERR_NULLPARAMETER);

	// Make sure that an unprivileged process is not trying to create a
	// privileged one
	if ((kernelCurrentProcess->privilege == PRIVILEGE_USER) &&
		(privilege == PRIVILEGE_SUPERVISOR))
	{
		kernelError(kernel_error, "An unprivileged process cannot create a "
			"privileged process");
		return (status == ERR_PERMISSION);
	}

	// Create the new process
	processId = createNewProcess(name, PRIORITY_DEFAULT, privilege, execImage,
		1 /* create page directory */);
	if (processId < 0)
		return (status = processId);

	// Get the pointer to the new process from its process Id
	proc = getProcessById(processId);
	if (!proc)
	{
		// We couldn't get access to the new process
		return (status = ERR_NOCREATE);
	}

	// Create the process's environment
	status = kernelEnvironmentCreate(proc->processId, proc->environment,
		kernelCurrentProcess->environment);
	if (status < 0)
	{
		// Couldn't create an environment structure for this process
		return (status);
	}

	// Don't assign input or output streams to this process.  There are
	// multiple possibilities here, and the caller will have to either block
	// (which takes care of such things) or sort it out for themselves.

	// Return whatever was returned by the previous call
	return (processId);
}


int kernelMultitaskerSpawn(void *startAddress, const char *name, int argc,
	void *argv[], int run)
{
	// This function is used to spawn a new thread from the current process.
	// The function needs to be told the starting address of the code to
	// execute, the name to use for the thread, and optional command-line
	// arguments to pass to the thread.  If 'run' is non-zero, the thread
	// state will be set to ready.  It returns the new process ID on success,
	// negative otherwise.

	int status = 0;
	int processId = 0;
	kernelProcess *proc = NULL;
	processImage execImage;
	int count;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!startAddress || !name)
		return (status = ERR_NULLPARAMETER);

	// If the number of arguments is not zero, make sure the arguments
	// pointer is not NULL
	if (argc && !argv)
		return (status = ERR_NULLPARAMETER);

	// Make sure the current process isn't NULL
	if (!kernelCurrentProcess)
	{
		kernelError(kernel_error, "Can't determine the current process");
		return (status = ERR_NOSUCHPROCESS);
	}

	memset(&execImage, 0, sizeof(processImage));
	execImage.virtualAddress = startAddress;
	execImage.entryPoint = startAddress;

	// Set up arguments
	execImage.argc = (argc + 1);
	execImage.argv[0] = (char *) name;
	for (count = 0; count < argc; count ++)
		execImage.argv[count + 1] = argv[count];

	// OK, now we should create the new process
	processId = createNewProcess(name, kernelCurrentProcess->priority,
		kernelCurrentProcess->privilege, &execImage,
		0 /* no page directory */);
	if (processId < 0)
		return (status = processId);

	// Get the pointer to the new process from its process Id
	proc = getProcessById(processId);

	// Make sure it's valid
	if (!proc)
	{
		// We couldn't get access to the new process
		return (status = ERR_NOCREATE);
	}

	// Change the type to thread
	proc->type = proc_thread;

	// Increment the descendent counts
	incrementDescendents(proc);

	// Since we assume that the thread is invoked as a function call, subtract
	// additional bytes from the stack pointer to account for the space where
	// the return address would normally go
#ifdef ARCH_X86
	proc->context.taskStateSegment.ESP -= sizeof(void *);
#endif

	// Share the environment of the parent

	if (proc->environment)
	{
		variableListDestroy(proc->environment);
		kernelFree(proc->environment);
	}

	proc->environment = kernelCurrentProcess->environment;

	// Share the symbols
	proc->symbols = kernelCurrentProcess->symbols;

	// The new process should share (but not own) the same text streams as the
	// parent

	proc->textInputStream = kernelCurrentProcess->textInputStream;

	if (proc->textInputStream)
	{
		memcpy((void *) &proc->oldInputAttrs,
			(void *) &proc->textInputStream->attrs,
			sizeof(kernelTextInputStreamAttrs));
	}

	proc->textOutputStream = kernelCurrentProcess->textOutputStream;

	if (run)
	{
		// Make the new thread runnable
		proc->state = proc_ready;
	}

	// Return the new process's Id.
	return (proc->processId);
}


int kernelMultitaskerSpawnKernelThread(void *startAddress, const char *name,
	int argc, void *argv[], int run)
{
	// This function is a wrapper around the regular spawn() call, which
	// causes threads to be spawned as children of the kernel, instead of
	// children of the calling process.  This is important for threads that
	// are spawned from code which belongs to the kernel.

	int status = 0;
	int interrupts = 0;
	kernelProcess *proc;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// What is the current process?
	proc = kernelCurrentProcess;

	// Disable interrupts while we're monkeying
	processorSuspendInts(interrupts);

	// Change the current process to the kernel process
	kernelCurrentProcess = kernelProc;

	// Spawn
	status = kernelMultitaskerSpawn(startAddress, name, argc, argv, run);

	// Reset the current process
	kernelCurrentProcess = proc;

	// Re-enable interrupts
	processorRestoreInts(interrupts);

	// Done
	return (status);
}


int kernelMultitaskerGetProcess(int processId, process *userProcess)
{
	// Return the requested process

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!userProcess)
		return (status = ERR_NULLPARAMETER);

	// Try to match the requested process Id number with a real live process
	// structure
	proc = getProcessById(processId);
	if (!proc)
	{
		// That means there's no such process
		return (status = ERR_NOSUCHENTRY);
	}

	// Make it into a user space process
	kernelProcess2Process(proc, userProcess);
	return (status = 0);
}


int kernelMultitaskerGetProcessByName(const char *processName,
	process *userProcess)
{
	// Return the requested process

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!processName || !userProcess)
		return (status = ERR_NULLPARAMETER);

	// Try to match the requested process Id number with a real live process
	// structure
	proc = getProcessByName(processName);
	if (!proc)
	{
		// That means there's no such process
		return (status = ERR_NOSUCHENTRY);
	}

	// Make it into a user space process
	kernelProcess2Process(proc, userProcess);
	return (status = 0);
}


int kernelMultitaskerGetProcesses(void *buffer, unsigned buffSize)
{
	// Return user-space process structures into the supplied buffer

	int status = 0;
	kernelProcess *proc = NULL;
	linkedListItem *iter = NULL;
	process *userProcess = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!buffer)
		return (status = ERR_NULLPARAMETER);

	proc = linkedListIterStart(&processList, &iter);

	while (proc)
	{
		userProcess = (buffer + (status * sizeof(process)));
		if ((void *) userProcess >= (buffer + buffSize))
			break;

		// Increase the count we're returning
		status += 1;

		kernelProcess2Process(proc, userProcess);

		proc = linkedListIterNext(&processList, &iter);
	}

	return (status);
}


int kernelMultitaskerGetCurrentProcessId(void)
{
	// This is a very simple function that can be called by external programs
	// to get the PID of the current running process.  Of course, internal
	// functions can perform this action very easily themselves.

	int status = 0;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
	{
		// If we're not multitasking,return the kernel's process Id
		return (status = KERNELPROCID);
	}

	// Double-check the current process to make sure it's not NULL
	if (!kernelCurrentProcess)
	{
		kernelError(kernel_error, "Can't determine the current process");
		return (status = ERR_NOSUCHPROCESS);
	}

	// OK, we can return process Id of the currently running process
	return (status = kernelCurrentProcess->processId);
}


userSession *kernelMultitaskerGetProcessUserSession(int processId)
{
	userSession *session = NULL;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (session = NULL);

	// We need to find the process structure based on the process Id
	proc = getProcessById(processId);

	if (!proc)
	{
		// The process does not exist
		return (session = NULL);
	}

	return (proc->session);
}


int kernelMultitaskerSetProcessUserSession(int processId,
	userSession *session)
{
	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!session)
		return (status = ERR_NULLPARAMETER);

	// We need to find the process structure based on the process Id
	proc = getProcessById(processId);

	if (!proc)
	{
		// The process does not exist
		return (status = ERR_NOSUCHPROCESS);
	}

	// Permission check.  Only a privileged process can change the user
	// session.
	if (kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR)
		return (status = ERR_PERMISSION);

	// Set it
	proc->session = session;

	return (status = 0);
}


int kernelMultitaskerGetProcessState(int processId, processState *state)
{
	// This is a very simple function that can be called by external programs
	// to request the state of a "running" process.  Of course, internal
	// functions can perform this action very easily themselves.

	int status = 0;
	kernelProcess *proc = NULL;

	// Check params
	if (!state)
		return (status = ERR_NULLPARAMETER);

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
	{
		if (processId == KERNELPROCID)
		{
			*state = proc_running;
			return (status = 0);
		}
		else
		{
			return (status = ERR_NOTINITIALIZED);
		}
	}

	// We need to find the process structure based on the process Id
	proc = getProcessById(processId);

	if (!proc)
	{
		// The process does not exist
		return (status = ERR_NOSUCHPROCESS);
	}

	// Set the state value of the process
	*state = proc->state;

	return (status = 0);
}


int kernelMultitaskerSetProcessState(int processId, processState newState)
{
	// This is a very simple function that can be called by external programs
	// to change the state of a "running" process.  Of course, internal
	// functions can perform this action very easily themselves.

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// We need to find the process structure based on the process Id
	proc = getProcessById(processId);

	if (!proc)
	{
		// The process does not exist
		return (status = ERR_NOSUCHPROCESS);
	}

	// Permission check.  A privileged process can change the state of any
	// other process, but a non-privileged process can only change the state
	// of processes owned by the same user.
	if ((kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR) &&
		(!kernelCurrentProcess->session || !proc->session ||
		strcmp(kernelCurrentProcess->session->name, proc->session->name)))
	{
		return (status = ERR_PERMISSION);
	}

	// Make sure the new state is a legal one
	switch (newState)
	{
		case proc_running:
		case proc_ready:
		case proc_ioready:
		case proc_waiting:
		case proc_sleeping:
		case proc_stopped:
		case proc_finished:
		case proc_zombie:
			// Ok
			break;

		default:
			// Not a legal state value
			return (status = ERR_INVALID);
	}

	// Set the state value of the process
	proc->state = newState;

	return (status);
}


int kernelMultitaskerProcessIsAlive(int processId)
{
	// Returns 1 if a process exists and has not finished (or been terminated)

	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
	{
		if (processId == KERNELPROCID)
			return (1);
		else
			return (0);
	}

	// Try to match the requested process Id number with a real live process
	// structure
	proc = getProcessById(processId);

	if (proc && (proc->state != proc_finished) && (proc->state !=
		proc_zombie))
	{
		return (1);
	}
	else
	{
		return (0);
	}
}


int kernelMultitaskerGetProcessPriority(int processId)
{
	// This is a very simple function that can be called by external programs
	// to get the priority of a process.  Of course, internal functions can
	// perform this action very easily themselves.

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
	{
		if (processId == KERNELPROCID)
			return (0);
		else
			return (status = ERR_NOTINITIALIZED);
	}

	// We need to find the process structure based on the process Id
	proc = getProcessById(processId);

	if (!proc)
	{
		// The process does not exist
		return (status = ERR_NOSUCHPROCESS);
	}

	// No permission check necessary here

	// Return the privilege value of the process
	return (proc->priority);
}


int kernelMultitaskerSetProcessPriority(int processId, int newPriority)
{
	// This is a very simple function that can be called by external programs
	// to change the priority of a process.  Of course, internal functions can
	// perform this action very easily themselves.

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// We need to find the process structure based on the process Id
	proc = getProcessById(processId);

	if (!proc)
	{
		// The process does not exist
		return (status = ERR_NOSUCHPROCESS);
	}

	// Permission check.  A privileged process can set the priority of any
	// other process, but a non-privileged process can only change the
	// priority of processes owned by the same user.  Additionally, a
	// non-privileged process can only set the new priority to a value equal
	// to or lower than its own priority.
	if ((kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR) &&
		(!kernelCurrentProcess->session || !proc->session ||
		strcmp(kernelCurrentProcess->session->name, proc->session->name) ||
		(newPriority < kernelCurrentProcess->priority)))
	{
		return (status = ERR_PERMISSION);
	}

	// Make sure the new priority is a legal one
	if ((newPriority < 0) || (newPriority >= (PRIORITY_LEVELS)))
	{
		// Not a legal priority value
		return (status = ERR_INVALID);
	}

	// Set the priority value of the process
	proc->priority = newPriority;

	return (status = 0);
}


int kernelMultitaskerGetProcessPrivilege(int processId)
{
	// This is a very simple function that can be called by external programs
	// to request the privilege of a "running" process.  Of course, internal
	// functions can perform this action very easily themselves.

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
	{
		if (processId == KERNELPROCID)
			return (PRIVILEGE_SUPERVISOR);
		else
			return (status = ERR_NOTINITIALIZED);
	}

	// We need to find the process structure based on the process Id
	proc = getProcessById(processId);

	if (!proc)
	{
		// The process does not exist
		return (status = ERR_NOSUCHPROCESS);
	}

	// Return the nominal privilege value of the process
	return (proc->privilege);
}


int kernelMultitaskerGetProcessParent(int processId)
{
	// This is a very simple function that can be called by external programs
	// to get the parent of a process.  Of course, internal functions can
	// perform this action very easily themselves.

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
	{
		if (processId == KERNELPROCID)
			return (KERNELPROCID);
		else
			return (status = ERR_NOTINITIALIZED);
	}

	// We need to find the process structure based on the process Id
	proc = getProcessById(processId);

	if (!proc)
	{
		// The process does not exist
		return (status = ERR_NOSUCHPROCESS);
	}

	// No permission check necessary here

	// Return the parent value of the process
	return (proc->parentProcessId);
}


variableList *kernelMultitaskerGetProcessEnvironment(int processId)
{
	// Subject to permissions, return a pointer to the requested process's
	// environment variable list structure

	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (NULL);

	// We need to find the process structure based on the process Id
	proc = getProcessById(processId);

	if (!proc)
	{
		// The process does not exist
		return (NULL);
	}

	// Permission check.  A privileged process can get the environment of any
	// other process, but a non-privileged process can only get the
	// environment of processes owned by the same user.
	if ((kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR) &&
		(!kernelCurrentProcess->session || !proc->session ||
		strcmp(kernelCurrentProcess->session->name, proc->session->name)))
	{
		return (NULL);
	}

	return (proc->environment);
}


int kernelMultitaskerGetCurrentDirectory(char *buffer, int buffSize)
{
	// This function will fill the supplied buffer with the name of the
	// current working directory for the current process.  Returns 0 on
	// success, negative otherwise.

	int status = 0;
	int lengthToCopy = 0;

	// Check params
	if (!buffer)
		return (status = ERR_NULLPARAMETER);

	// Now, the number of characters we will copy is the lesser of buffSize or
	// MAX_PATH_LENGTH
	lengthToCopy = min(buffSize, MAX_PATH_LENGTH);

	// Copy the name of the current directory into the caller's buffer
	if (!multitaskingEnabled)
	{
		strncpy(buffer, "/", lengthToCopy);
	}
	else
	{
		strncpy(buffer, (char *) kernelCurrentProcess->currentDirectory,
			lengthToCopy);
	}

	// Return success
	return (status = 0);
}


int kernelMultitaskerSetProcessCurrentDirectory(int processId,
	const char *newDirName)
{
	// This function will change the current directory of the requested
	// process.  Returns 0 on success, negative otherwise.

	int status = 0;
	kernelProcess *proc = NULL;
	kernelFileEntry *newDir = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!newDirName)
		return (status = ERR_NULLPARAMETER);

	// We need to find the process structure based on the process Id
	proc = getProcessById(processId);

	if (!proc)
	{
		// The process does not exist
		return (status = ERR_NOSUCHPROCESS);
	}

	// Permission check.  A privileged process can set the current directory
	// of any other process, but a non-privileged process can only change the
	// current directory of processes owned by the same user.
	if ((kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR) &&
		(!kernelCurrentProcess->session || !proc->session ||
		strcmp(kernelCurrentProcess->session->name, proc->session->name)))
	{
		return (status = ERR_PERMISSION);
	}

	// Call the appropriate filesystem function to find this supposed new
	// directory
	newDir = kernelFileLookup(newDirName);
	if (!newDir)
		return (status = ERR_NOSUCHDIR);

	// Make sure the target is actually a directory
	if (newDir->type != dirT)
		return (status = ERR_NOTADIR);

	// Okay, copy the full name of the directory into the process
	kernelFileGetFullName(newDir, (char *) proc->currentDirectory,
		MAX_PATH_LENGTH);

	// Return success
	return (status = 0);
}


int kernelMultitaskerSetCurrentDirectory(const char *newDirName)
{
	// This function will change the current directory of the current process.
	// Returns 0 on success, negative otherwise.

	int status = 0;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// This will check the new directory name parameter
	return (status = kernelMultitaskerSetProcessCurrentDirectory(
		kernelCurrentProcess->processId, newDirName));
}


kernelTextInputStream *kernelMultitaskerGetTextInput(void)
{
	// This function will return the text input stream that is attached to the
	// current process

	// If multitasking hasn't yet been enabled, we can safely assume that
	// we're currently using the default console text input
	if (!multitaskingEnabled)
		return (kernelTextGetCurrentInput());
	else
		return (kernelCurrentProcess->textInputStream);
}


int kernelMultitaskerSetTextInput(int processId,
	 kernelTextInputStream *theStream)
{
	// Change the input stream of the process

	int status = 0;
	kernelProcess *proc = NULL;
	kernelProcess *childProc = NULL;
	linkedListItem *iter = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// theStream is allowed to be NULL

	proc = getProcessById(processId);
	if (!proc)
		return (status = ERR_NOSUCHPROCESS);

	proc->textInputStream = theStream;

	if (theStream)
	{
		if (proc->type == proc_normal)
			theStream->ownerPid = proc->processId;

		// Remember the current input attributes
		memcpy((void *) &proc->oldInputAttrs, (void *) &theStream->attrs,
			sizeof(kernelTextInputStreamAttrs));
	}

	// Do any child threads recursively as well
	if (proc->descendentThreads)
	{
		childProc = linkedListIterStart(&processList, &iter);

		while (childProc)
		{
			if ((childProc->parentProcessId == processId) &&
				(childProc->type == proc_thread))
			{
				status = kernelMultitaskerSetTextInput(childProc->processId,
					theStream);
				if (status < 0)
					return (status);
			}

			childProc = linkedListIterNext(&processList, &iter);
		}
	}

	return (status = 0);
}


kernelTextOutputStream *kernelMultitaskerGetTextOutput(void)
{
	// This function will return the text output stream that is attached to
	// the current process

	// If multitasking hasn't yet been enabled, we can safely assume that
	// we're currently using the default console text output
	if (!multitaskingEnabled)
		return (kernelTextGetCurrentOutput());
	else
		return (kernelCurrentProcess->textOutputStream);
}


int kernelMultitaskerSetTextOutput(int processId,
	kernelTextOutputStream *theStream)
{
	// Change the output stream of the process

	int status = 0;
	kernelProcess *proc = NULL;
	kernelProcess *childProc = NULL;
	linkedListItem *iter = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// theStream is allowed to be NULL

	proc = getProcessById(processId);
	if (!proc)
		return (status = ERR_NOSUCHPROCESS);

	proc->textOutputStream = theStream;

	// Do any child threads recursively as well
	if (proc->descendentThreads)
	{
		childProc = linkedListIterStart(&processList, &iter);

		while (childProc)
		{
			if ((childProc->parentProcessId == processId) &&
				(childProc->type == proc_thread))
			{
				status = kernelMultitaskerSetTextOutput(childProc->processId,
					theStream);
				if (status < 0)
					return (status);
			}

			childProc = linkedListIterNext(&processList, &iter);
		}
	}

	return (status = 0);
}


int kernelMultitaskerDuplicateIo(int firstPid, int secondPid, int clear)
{
	// Copy the input and output streams of the first process to the second
	// process

	int status = 0;
	kernelProcess *firstProc = NULL;
	kernelProcess *secondProc = NULL;
	kernelTextInputStream *input = NULL;
	kernelTextOutputStream *output = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	firstProc = getProcessById(firstPid);
	secondProc = getProcessById(secondPid);

	if (!firstProc || !secondProc)
		return (status = ERR_NOSUCHPROCESS);

	input = firstProc->textInputStream;
	output = firstProc->textOutputStream;

	if (input)
	{
		secondProc->textInputStream = input;
		input->ownerPid = secondPid;

		// Remember the current input attributes
		memcpy((void *) &secondProc->oldInputAttrs, (void *) &input->attrs,
			sizeof(kernelTextInputStreamAttrs));

		if (clear)
			kernelTextInputStreamRemoveAll(input);
	}

	if (output)
		secondProc->textOutputStream = output;

	return (status = 0);
}


int kernelMultitaskerGetProcessorTime(clock_t *clk)
{
	// Returns processor time used by a process since its start.  This value
	// is the number of timer ticks from the system timer.

	int status = 0;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	if (!clk)
		return (status = ERR_NULLPARAMETER);

	// Return the processor time of the current process
	*clk = kernelCurrentProcess->cpuTime;

	return (status = 0);
}


void kernelMultitaskerYield(void)
{
	// This function will yield control from the current running process back
	// to the scheduler

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return;

	// Don't do this inside an interrupt
	if (kernelProcessingInterrupt())
		return;

	// We accomplish a yield by doing a far call to the scheduler's task.  The
	// scheduler sees this almost as if the current timeslice had expired.
	schedulerSwitchedByCall = 1;

#ifdef ARCH_X86
	processorFarJump(schedulerProc->context.tssSelector);
#endif
}


void kernelMultitaskerWait(unsigned milliseconds)
{
	// This function will put a process into the waiting state for *at least*
	// the specified number of milliseconds, and yield control back to the
	// scheduler

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
	{
		// We can't wait properly if we're not multitasking yet, but we can
		// try to spin
		kernelDebugError("Cannot wait() before multitasking is enabled.  "
			"Spinning.");
		kernelCpuSpinMs(milliseconds);
		return;
	}

	// Don't do this inside an interrupt
	if (kernelProcessingInterrupt())
	{
		kernelPanic("Cannot wait() inside an interrupt handler (%d)",
			kernelInterruptGetCurrent());
	}

	// Make sure the current process isn't NULL
	if (!kernelCurrentProcess)
	{
		kernelError(kernel_error, "Can't determine the current process");
		return;
	}

	// Set the wait until time
	kernelCurrentProcess->waitUntil = (kernelCpuGetMs() + milliseconds);
	kernelCurrentProcess->waitForProcess = 0;

	// Set the current process to "waiting"
	kernelCurrentProcess->state = proc_waiting;

	// And yield
	kernelMultitaskerYield();
}


int kernelMultitaskerBlock(int processId)
{
	// This function will put a process into the waiting state until the
	// requested blocking process has completed

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Don't do this inside an interrupt
	if (kernelProcessingInterrupt())
	{
		kernelPanic("Cannot block() inside an interrupt handler (%d)",
			kernelInterruptGetCurrent());
	}

	// Get the process that we're supposed to block on
	proc = getProcessById(processId);
	if (!proc)
	{
		// The process does not exist
		kernelError(kernel_error, "The process on which to block does not "
			"exist");
		return (status = ERR_NOSUCHPROCESS);
	}

	// Make sure the current process isn't NULL
	if (!kernelCurrentProcess)
	{
		kernelError(kernel_error, "Can't determine the current process");
		return (status = ERR_BUG);
	}

	// Take the text streams that belong to the current process and
	// give them to the target process
	kernelMultitaskerDuplicateIo(kernelCurrentProcess->processId, processId,
		0 /* don't clear */);

	// Set the wait for process values
	kernelCurrentProcess->waitForProcess = processId;
	kernelCurrentProcess->waitUntil = 0;

	// Set the current process to "waiting"
	kernelCurrentProcess->state = proc_waiting;

	// And yield
	kernelMultitaskerYield();

	// Get the exit code from the process
	return (kernelCurrentProcess->blockingExitCode);
}


int kernelMultitaskerDetach(void)
{
	// This will allow a program or daemon to detach from its parent process
	// if the parent process is blocking

	int status = 0;
	kernelProcess *parentProc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Make sure the current process isn't NULL
	if (!kernelCurrentProcess)
	{
		kernelError(kernel_error, "Can't determine the current process");
		return (status = ERR_BUG);
	}

	// Set the input/output streams to the console
	kernelMultitaskerDuplicateIo(KERNELPROCID,
		kernelCurrentProcess->processId, 0 /* don't clear */);

	// Get the process that's blocking on this one, if any
	parentProc = getProcessById(kernelCurrentProcess->parentProcessId);

	if (parentProc && (parentProc->waitForProcess ==
		kernelCurrentProcess->processId))
	{
		// Clear the return code of the parent process
		parentProc->blockingExitCode = 0;

		// Clear the parent's wait for process value
		parentProc->waitForProcess = 0;

		// Make it runnable
		parentProc->state = proc_ready;
	}

	return (status = 0);
}


int kernelMultitaskerKillProcess(int processId)
{
	// This function should be used to properly kill a process.  This will
	// deallocate all of the internal resources used by the multitasker in
	// maintaining the process and all of its children.  This function will
	// commonly employ a recursive tactic for killing processes with spawned
	// children.  Returns 0 on success, negative on error.

	int status = 0;
	kernelProcess *proc = NULL;
	kernelProcess *listProc = NULL;
	linkedListItem *iter = NULL;

	kernelDebug(debug_multitasker, "Multitasker kill process %d", processId);

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Find the process structure based on the Id we were passed
	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		return (status = ERR_NOSUCHPROCESS);
	}

	// Processes are not allowed to actually kill themselves.  They must use
	// the terminate function to do it normally.
	if (proc == kernelCurrentProcess)
		kernelMultitaskerTerminate(0);

	// Permission check.  A privileged process can kill any other process, but
	// a non-privileged process can only kill processes owned by the same
	// user.
	if ((kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR) &&
		(!kernelCurrentProcess->session || !proc->session ||
		strcmp(kernelCurrentProcess->session->name, proc->session->name)))
	{
		kernelError(kernel_error, "Permission denied killing %s", proc->name);
		return (status = ERR_PERMISSION);
	}

	// You can't kill the kernel on purpose
	if (proc == kernelProc)
	{
		kernelError(kernel_error, "It's not possible to kill the kernel "
			"process");
		return (status = ERR_INVALID);
	}

	// You can't kill the exception handler thread on purpose
	if (proc == exceptionProc)
	{
		kernelError(kernel_error, "It's not possible to kill the exception "
			"thread");
		return (status = ERR_INVALID);
	}

	// If a thread is trying to kill its parent, we won't do that here.
	// Instead we will mark it as 'finished' and let the kernel clean us all
	// up later.
	if ((kernelCurrentProcess->type == proc_thread) &&
		(processId == kernelCurrentProcess->parentProcessId))
	{
		proc->state = proc_finished;
		while (1)
			kernelMultitaskerYield();
	}

	// The request is legitimate

	// Mark the process as stopped in the process list, so that the scheduler
	// will not inadvertently select it to run while we're destroying it
	proc->state = proc_stopped;

	// We must iterate through the list of existing processes, looking for any
	// other processes whose states depend on this one (such as child threads
	// who don't have a page directory).  If we remove a process, we need to
	// call this function recursively to kill it (and any of its own dependant
	// children).

	listProc = linkedListIterStart(&processList, &iter);

	while (listProc)
	{
		// Is this process blocking on the process we're killing?
		if (listProc->waitForProcess == processId)
		{
			// This process is blocking on the process we're killing.  If the
			// process being killed was not, in turn, blocking on another
			// process, the blocked process will be made runnable.  Otherwise,
			// the blocked process will be forced to block on the same
			// process as the one being killed.
			if (proc->waitForProcess)
			{
				listProc->waitForProcess = proc->waitForProcess;
			}
			else
			{
				listProc->blockingExitCode = ERR_KILLED;
				listProc->waitForProcess = 0;
				listProc->state = proc_ready;
			}

			goto checkNext;
		}

		// If this process is a child thread of the process we're killing, or
		// if the process we're killing was blocking on this process, kill it
		// first
		if ((listProc->state != proc_finished) &&
			(listProc->parentProcessId == proc->processId) &&
			((listProc->type == proc_thread) ||
				(proc->waitForProcess == listProc->processId)))
		{
			status = kernelMultitaskerKillProcess(listProc->processId);
			if (status < 0)
			{
				kernelError(kernel_warn, "Unable to kill child process "
					"\"%s\" of parent process \"%s\"", listProc->name,
					proc->name);
			}
			else
			{
				goto checkNext;
			}
		}

		// If this process is a child of the process we're killing, re-parent
		// it to its grandparent
		if (listProc->parentProcessId == proc->processId)
			listProc->parentProcessId = proc->parentProcessId;

	checkNext:
		listProc = linkedListIterNext(&processList, &iter);
	}

	// Now we look after killing the process with the Id we were passed

	if (kernelNetworkEnabled())
	{
		// Try to close all network connections owned by this process
		status = kernelNetworkCloseAll(proc->processId);
		if (status < 0)
			kernelError(kernel_warn, "Can't release network connections");
	}

	// Restore previous attributes to the input stream, if applicable
	if (proc->textInputStream)
	{
		memcpy((void *) &proc->textInputStream->attrs,
			(void *) &proc->oldInputAttrs,
			sizeof(kernelTextInputStreamAttrs));
	}

	// If this process is a thread, decrement the count of descendent threads
	// of its parent
	if (proc->type == proc_thread)
		decrementDescendents(proc);

	// Dismantle the process
	status = deleteProcess(proc);
	if (status < 0)
	{
		// Eek, there was a problem deallocating something, we guess.  Simply
		// mark the process as a zombie so that it won't be run any more, but
		// its resources won't be 'lost'.
		kernelError(kernel_error, "Couldn't delete process %d: \"%s\"",
			proc->processId, proc->name);
		proc->state = proc_zombie;
		return (status);
	}

	// If the target process is the idle process, spawn another one
	if (proc == idleProc)
		spawnIdleThread();

	// Done.  Return success.
	return (status = 0);
}


int kernelMultitaskerKillByName(const char *name)
{
	// Try to kill all processes whose names match the one supplied

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!name)
		return (status = ERR_NULLPARAMETER);

	while ((proc = getProcessByName(name)))
		status = kernelMultitaskerKillProcess(proc->processId);

	return (status);
}


int kernelMultitaskerKillAll(void)
{
	// This function is used to shut down all processes currently running.
	// Normally, this will be done during multitasker shutdown.  Returns 0 on
	// success, negative otherwise.

	int status = 0;
	kernelProcess *proc = NULL;
	linkedListItem *iter = NULL;

	kernelDebug(debug_multitasker, "Multitasker kill all processes");

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Stop all processes, except the kernel process, exception process, idle
	// process, and the current process

	proc = linkedListIterStart(&processList, &iter);

	while (proc)
	{
		if (PROC_KILLABLE(proc))
			proc->state = proc_stopped;

		proc = linkedListIterNext(&processList, &iter);
	}

	// Kill all of the processes, minus the exceptions mentioned above.  This
	// won't kill the scheduler's task either.

	proc = linkedListIterStart(&processList, &iter);

	while (proc)
	{
		if (PROC_KILLABLE(proc))
		{
			// Attempt to kill it
			kernelMultitaskerKillProcess(proc->processId);
		}

		proc = linkedListIterNext(&processList, &iter);
	}

	// Return success
	return (status = 0);
}


int kernelMultitaskerTerminate(int retCode)
{
	// This function is designed to allow a process to terminate itself
	// normally, and return a result code (this is the normal way to exit()).
	// On error, the function returns negative.  On success, of course, it
	// doesn't return at all.

	int status = 0;
	kernelProcess *parentProc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Don't do this inside an interrupt
	if (kernelProcessingInterrupt())
	{
		kernelPanic("Cannot terminate() inside an interrupt handler (%d)",
			kernelInterruptGetCurrent());
	}

	// Find the parent process before we terminate ourselves
	parentProc = getProcessById(kernelCurrentProcess->parentProcessId);
	if (parentProc)
	{
		// We found our parent process.  Is it blocking, waiting for us?
		if (parentProc->waitForProcess == kernelCurrentProcess->processId)
		{
			// It's waiting for us to finish.  Put our return code into
			// its blockingExitCode field.
			parentProc->blockingExitCode = retCode;
			parentProc->waitForProcess = 0;
			parentProc->state = proc_ready;

			// Done
		}
	}

	while (1)
	{
		// If we still have threads out there, we don't dismantle until they
		// are finished
		if (!kernelCurrentProcess->descendentThreads)
		{
			// Terminate
			kernelCurrentProcess->state = proc_finished;
		}

		kernelMultitaskerYield();
	}
}


int kernelMultitaskerSignalSet(int processId, int sig, int on)
{
	// Set signal handling enabled (on) or disabled for the specified signal

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Make sure the signal number fits in the signal mask
	if ((sig <= 0) || (sig >= SIGNALS_MAX))
	{
		kernelError(kernel_error, "Invalid signal code %d", sig);
		return (status = ERR_RANGE);
	}

	// Try to find the process
	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		kernelError(kernel_error, "No process %d to signal", processId);
		return (status = ERR_NOSUCHPROCESS);
	}

	// If there is not yet a signal stream allocated for this process, do it
	// now
	if (!(proc->signalStream.buffer))
	{
		status = kernelStreamNew(&proc->signalStream, 16, itemsize_dword);
		if (status < 0)
			return (status);
	}

	if (on)
		proc->signalMask |= (1 << sig);
	else
		proc->signalMask &= ~(1 << sig);

	return (status = 0);
}


int kernelMultitaskerSignal(int processId, int sig)
{
	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Make sure the signal number fits in the signal mask
	if ((sig <= 0) || (sig >= SIGNALS_MAX))
	{
		kernelError(kernel_error, "Invalid signal code %d", sig);
		return (status = ERR_RANGE);
	}

	// Try to find the process
	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		kernelError(kernel_error, "No process %d to signal", processId);
		return (status = ERR_NOSUCHPROCESS);
	}

	// See if the signal is handled, and make sure there's a signal stream
	if (!(proc->signalMask & (1 << sig)) || !proc->signalStream.buffer)
	{
		// Not handled.  Terminate the process.
		proc->state = proc_finished;
		return (status = 0);
	}

	// Put the signal into the signal stream
	status = proc->signalStream.append(&proc->signalStream, sig);

	return (status);
}


int kernelMultitaskerSignalRead(int processId)
{
	int status = 0;
	kernelProcess *proc = NULL;
	int sig;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Try to find the process
	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		kernelError(kernel_error, "No process %d to signal", processId);
		return (status = ERR_NOSUCHPROCESS);
	}

	// Any signals handled?
	if (!(proc->signalMask))
		return (status = 0);

	// Make sure there's a signal stream
	if (!(proc->signalStream.buffer))
	{
		kernelError(kernel_error, "Process has no signal stream");
		return (status = ERR_NOTINITIALIZED);
	}

	if (!proc->signalStream.count)
		return (sig = 0);

	status = proc->signalStream.pop(&proc->signalStream, &sig);

	if (status < 0)
		return (status);
	else
		return (sig);
}


int kernelMultitaskerGetIoPerm(int processId, int portNum)
{
	// Check if the given process can use I/O ports specified.  Returns 1 if
	// permission is allowed.

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		kernelError(kernel_error, "No process %d to get I/O permissions",
			processId);
		return (status = ERR_NOSUCHPROCESS);
	}

#ifdef ARCH_X86
	if (portNum >= X86_IO_PORTS)
		return (status = ERR_BOUNDS);

	// If the bit is clear, permission is granted
	if (!GET_PORT_BIT(proc->context.taskStateSegment.IOMap, portNum))
		return (status = 1);
#endif

	return (status = 0);
}


int kernelMultitaskerSetIoPerm(int processId, int portNum, int yesNo)
{
	// Allow or deny I/O port permission to the given process

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		kernelError(kernel_error, "No process %d to set I/O permissions",
			processId);
		return (status = ERR_NOSUCHPROCESS);
	}

#ifdef ARCH_X86
	if (portNum >= X86_IO_PORTS)
		return (status = ERR_BOUNDS);

	if (yesNo)
		UNSET_PORT_BIT(proc->context.taskStateSegment.IOMap, portNum);
	else
		SET_PORT_BIT(proc->context.taskStateSegment.IOMap, portNum);
#endif

	return (status = 0);
}


kernelPageDirectory *kernelMultitaskerGetPageDir(int processId)
{
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (NULL);

	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		kernelError(kernel_error, "No process %d to get page directory",
			processId);
		return (NULL);
	}

	return (proc->pageDirectory);
}


loaderSymbolTable *kernelMultitaskerGetSymbols(int processId)
{
	// Given a process ID, return the symbol table of the process

	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (NULL);

	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		kernelError(kernel_error, "No process %d to get symbols", processId);
		return (NULL);
	}

	return (proc->symbols);
}


int kernelMultitaskerSetSymbols(int processId, loaderSymbolTable *symbols)
{
	// Given a process ID and a symbol table, attach the symbol table to the
	// process

	int status = 0;
	kernelProcess *proc = NULL;

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		kernelError(kernel_error, "No process %d to set symbols", processId);
		return (status = ERR_NOSUCHPROCESS);
	}

	proc->symbols = symbols;
	return (status = 0);
}


int kernelMultitaskerStackTrace(int processId)
{
	// Locate the process by ID and do a stack trace of it

	int status = 0;
	kernelProcess *proc = NULL;
	char buffer[MAXSTRINGLENGTH + 1];

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Locate the process
	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		kernelError(kernel_error, "No process %d to trace", processId);
		return (status = ERR_NOSUCHPROCESS);
	}

	// Do the stack trace
	status = kernelStackTrace(proc, buffer, MAXSTRINGLENGTH);

	if (status >= 0)
		kernelTextPrint("%s", buffer);

	return (status);
}


int kernelMultitaskerPropagateEnvironment(int processId, const char *variable)
{
	// Set and overwrite descendent processes' environment variables.  If
	// 'variable' is set, only the named variable will propagate.  Otherwise,
	// all variables will propagate.  Variables in the descendents'
	// environments that don't exist in the parent process are unaffected.

	int status = 0;
	kernelProcess *proc = NULL;

	kernelDebug(debug_multitasker, "Multitasker propagate environment");

	// Make sure multitasking has been enabled
	if (!multitaskingEnabled)
		return (status = ERR_NOTINITIALIZED);

	// Locate the process
	proc = getProcessById(processId);
	if (!proc)
	{
		// There's no such process
		kernelError(kernel_error, "No process %d to propagate environment",
			processId);
		return (status = ERR_NOSUCHPROCESS);
	}

	status = propagateEnvironmentRecursive(proc, proc->environment, variable);

	return (status);
}

