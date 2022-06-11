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
//  kernelMemoryManager.c
//
	
// These routines comprise Visopsys' memory management subsystem.  This
// memory manager is implemented using a "first-fit" strategy because
// it's a speedy algorithm, and because supposedly "best-fit" and 
// "worst-fit" don't really provide a significant memory utilization 
// advantage but do imply significant overhead.

#include "kernelMemoryManager.h"
#include "kernelParameters.h"
#include "kernelText.h"
#include "kernelPageManager.h"
#include "kernelMultitasker.h"
#include "kernelResourceManager.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <stdio.h>
#include <string.h>


static volatile int memoryManagerInitialized = 0;
static volatile int memoryManagerLock = 0;

static volatile unsigned totalMemory = 0;
static kernelMemoryBlock usedBlockMemory[MAXMEMORYBLOCKS];
static kernelMemoryBlock *usedBlockList[MAXMEMORYBLOCKS];
static volatile unsigned usedBlocks = 0;
static unsigned char * volatile freeBlockBitmap = NULL;
static volatile unsigned totalBlocks = 0;
static volatile unsigned totalFree = 0;
static volatile unsigned totalUsed = 0;


// This structure can be used to "reserve" memory blocks so that they
// will be marked as "used" by the memory manager and then left alone.
// It should be terminated with a NULL entry.  The addresses used here
// are defined in kernelParamers.h
static kernelMemoryBlock reservedBlocks[] =
{
  // { processId, description, startlocation, endlocation }

  { 1, "conventional memory", 0, (VIDEO_MEMORY - 1) },

  { 1, "video memory", VIDEO_MEMORY, 
    (VIDEO_MEMORY + VIDEO_MEMORY_SIZE - 1) },

  { 1, "conventional memory", (VIDEO_MEMORY + VIDEO_MEMORY_SIZE),
    (KERNEL_LOAD_ADDRESS - 1) },

  // Ending value is set during initialization, since it is variable
  { 1, "kernel memory", KERNEL_LOAD_ADDRESS, 0 },

  // Starting and ending values are set during initialization, since they
  // are variable
  { 1, "kernel paging data", 0, 0 },

  // The bitmap for free memory.  This one is also completely variable and
  // dependent upon the previous two
  { 1, "free memory bitmap", 0, 0 },

  { 0, "", 0, 0 }
};


static int allocateBlock(int processId, unsigned start, unsigned end, 
			 const char *description)
{
  // This routine will allocate a block in the used block list, mark the
  // corresponding blocks as allocated in the free-block bitmap, and adjust
  // the totalUsed and totalFree values accordingly.  Returns 0 on success, 
  // negative otherwise.

  int status = 0;
  unsigned temp = 0;
  unsigned lastBit = 0;

  // The description pointer is allowed to be NULL

  // Clear the memory occupied by the first unused memory block structure
  kernelMemClear((void *) usedBlockList[usedBlocks], 
			sizeof(kernelMemoryBlock));

  // Assign the appropriate values to the block structure
  usedBlockList[usedBlocks]->processId = processId;
  usedBlockList[usedBlocks]->startLocation = start;
  usedBlockList[usedBlocks]->endLocation = end;
  if (description != NULL)
    {
      strncpy((char *) usedBlockList[usedBlocks]->description, 
	      description, MAX_DESC_LENGTH);
      usedBlockList[usedBlocks]->description[MAX_DESC_LENGTH - 1] = '\0';
    }
  else
    usedBlockList[usedBlocks]->description[0] = '\0';

  // Adjust the total used and free memory quantities
  temp = ((usedBlockList[usedBlocks]->endLocation - 
	   usedBlockList[usedBlocks]->startLocation) + 1);
  totalUsed += temp;
  totalFree -= temp;

  // Now increase the total count of used memory blocks.
  usedBlocks += 1;

  // Take the whole range of memory covered by this new block, and mark
  // each of its physical memory blocks as "used" in the free-block bitmap.
  lastBit = (end / MEMBLOCKSIZE);
  for (temp = (start / MEMBLOCKSIZE); temp <= lastBit; temp ++)
    freeBlockBitmap[temp / 8] |= (0x80 >> (temp % 8));

  // Return success
  return (status = 0);
}


static int requestBlock(int processId, unsigned requestedSize, 
			unsigned alignment, const char *description,
			void **memory)
{
  // This routine takes a pointer and some other information, and allocates
  // memory blocks.  It makes the pointer point to the allocated memory 
  // according to the specifications, and returns the amount of memory
  // allocated.  The alignment parameter allows the caller to request the
  // alignment of the block (but only on a MEMBLOCKSIZE boundary, otherwise
  // an error will result).  If no particular alignment is needed, 
  // specifying this parameter as 0 will effectively nullify this.  It
  // returns a pointer to the memory allocated if successful, NULL otherwise.

  int status = 0;
  void *blockPointer = NULL;
  unsigned consecutiveBlocks = 0;
  int foundBlock = 0;
  int count;


  // If the requested block size is zero, forget it.  We can probably
  // assume something has gone wrong in the calling program
  if (requestedSize == 0)
    return (status = ERR_INVALID);

  // Make sure that we have room for a new block.  If we don't, return
  // NULL.
  if (usedBlocks >= MAXMEMORYBLOCKS)
    {
      // Not enough memory blocks left over
      kernelError(kernel_error, "The number of memory blocks has been "
		  "exhausted");
      return (status = ERR_MEMORY);
    }
  
  // Make sure the requested alignment is a multiple of MEMBLOCKSIZE.
  // Obviously, if MEMBLOCKSIZE is the size of each block, we can't
  // really start allocating memory which uses only bits and pieces
  // of blocks
  if (alignment != 0)
    if ((alignment % MEMBLOCKSIZE) != 0)
      return (status = ERR_ALIGN);

  // Make the requested size be a multiple of MEMBLOCKSIZE.  Our memory 
  // blocks are all going to be multiples of this size
  if ((requestedSize % MEMBLOCKSIZE) != 0)
      requestedSize = (((requestedSize / MEMBLOCKSIZE) + 1) *
      MEMBLOCKSIZE);

  // Now, make sure that there's enough total free memory to satisfy
  // this request (whether or not there is a large enough contiguous
  // block is another matter.)
  if (requestedSize > totalFree)
    {
      // There is not enough free memory
      kernelError(kernel_error, "The computer is out of physical memory");
      return (status = ERR_MEMORY);
    }

  // Adjust "alignment" so that it is expressed in blocks
  alignment /= MEMBLOCKSIZE;

  consecutiveBlocks = 0;

  // Skip through the free-block bitmap and find the first block large
  // enough to fit the requested size, plus the alignment value if
  // applicable.
  for (count = 0; count < totalBlocks; count ++)
    {
      // Is the current block used or free?
      if (freeBlockBitmap[count / 8] & (0x80 >> (count % 8)))
	{
	  // Ug.  This block is allocated.  We're not there yet.
	  consecutiveBlocks = 0;

	  // If alignment is desired, we need to advance "count" to the
	  // next multiple of the alignment size
	  if (alignment)
	    // Subract one from the expected number because count will get 
	    // incremented by the loop
	    count += ((alignment - (count % alignment)) - 1);
	  
	  continue;
	}

      // This block is free.
      consecutiveBlocks++;

      // Do we have enough yet?
      if ((consecutiveBlocks * MEMBLOCKSIZE) >= requestedSize)
	{
	  blockPointer = (void *) 
	    ((count - (consecutiveBlocks - 1)) * MEMBLOCKSIZE);
	  foundBlock = 1;
	  break;
	}
    }

  // If we found an appropriate block, blockPointer will be non-NULL
  if (!foundBlock)
    return (status = ERR_MEMORY);

  // Otherwise, it looks like we will be able to satisfy this request.  
  // We found a block in the loop above.  We now have to allocate the new 
  // "used" block.

  // blockPointer should point to the start of the memory area.
  status = allocateBlock(processId, (unsigned) blockPointer, 
			 (unsigned) (blockPointer + requestedSize - 1),
			 description);

  // Make sure we were successful with the allocation, above
  if (status < 0)
    return (status);

  // Assign the start location of the new memory
  *memory = blockPointer;

  // Success
  return (status = 0);
}


static int releaseBlock(int blockLocation)
{
  // This routine will remove a block from the used block list, mark the
  // corresponding blocks as free in the free-block bitmap, and adjust
  // the totalUsed and totalFree values accordingly.  Returns 0 on success, 
  // negative otherwise.

  int status = 0;
  kernelMemoryBlock *unused;
  unsigned temp = 0;
  unsigned lastBit = 0;

  // Make sure the block location is reasonable
  if ((blockLocation < 0) || (blockLocation > (usedBlocks - 1)))
    return (status = ERR_NOSUCHENTRY);

  // Mark all of the applicable blocks in the free block bitmap as unused
  lastBit = (usedBlockList[blockLocation]->endLocation / MEMBLOCKSIZE);
  for (temp = (usedBlockList[blockLocation]->startLocation / MEMBLOCKSIZE);
       temp <= lastBit; temp ++)
    freeBlockBitmap[temp / 8] &= (0xFF7F >> (temp % 8));

  // Adjust the total used and free memory quantities
  temp = ((usedBlockList[blockLocation]->endLocation -
	   usedBlockList[blockLocation]->startLocation) + 1);
  totalUsed -= temp;
  totalFree += temp;

  // Remove this element from the "used" part of the list.  What we have to 
  // do is this: Since this is an unordered list, the way we accomplish this 
  // is by copying the last entry into the place of the current entry and 
  // reducing the total block count, unless the entry we're removing happens 
  // to be the only item, or the last item in the list.  In those cases we 
  // simply reduce the total number

  if ((usedBlocks > 1) && (blockLocation < (usedBlocks - 1)))
    {
      unused = usedBlockList[blockLocation];
      usedBlockList[blockLocation] = usedBlockList[usedBlocks - 1];
      usedBlockList[usedBlocks - 1] = unused;
    }

  // Now reduce the total count.
  usedBlocks--;

  // Return success
  return (status = 0);
}


static unsigned percentUsage(void)
{
  // Calculates the percent usage of total memory (base + extended)
  
  if (totalMemory == 0)
    return (0);
  else
    return ((totalUsed * 100) / totalMemory);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelMemoryInitialize(unsigned kernelMemory, loaderInfoStruct *info)
{
  // This routine will initialize all of the machine's memory between the
  // starting point and the ending point.  It will call a routine to test 
  // all memory using a test pattern defined in the header file, then it 
  // will "zero" all the memory.  Returns 0 on success, negative otherwise.

  int status = 0;
  unsigned char *bitmapPhysical = NULL;
  unsigned bitmapSize = 0;
  int count;

  // Make sure that this initialization routine only gets called once
  if (memoryManagerInitialized)
    return (status = ERR_ALREADY);

  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return (status);

  // Calculate the amount of total memory we're managing.  First, count
  // 1024 kilobytes for standard and high memory (first "megabyte")
  totalMemory = (1024 * 1024);

  // Add all the extended memory
  totalMemory += (info->extendedMemory * 1024);

  // Make sure that totalMemory is a multiple of MEMBLOCKSIZE.
  if ((totalMemory % MEMBLOCKSIZE) != 0)
    totalMemory = ((totalMemory / MEMBLOCKSIZE) * MEMBLOCKSIZE);

  totalUsed = 0;
  totalFree = totalMemory;

  // Initialize the used memory block list.  We don't need to initialize all
  // of the memory we use (we will be careful to initialize blocks when we
  // allocate them), but we do need to fill up the list of pointers to those
  // memory structures
  for (count = 0; count < MAXMEMORYBLOCKS; count ++)
    usedBlockList[count] = &(usedBlockMemory[count]);
  
  totalBlocks = (totalMemory / MEMBLOCKSIZE);
  usedBlocks = 0;

  // We need to define memory for the free-block bitmap.  However,
  // we will have to do it manually since, without the bitmap, we can't
  // do a "normal" block allocation.
  // This is a physical address.
  bitmapPhysical = (unsigned char *)
    (KERNEL_LOAD_ADDRESS + kernelMemory + KERNEL_PAGING_DATA_SIZE);

  // Calculate the size of the free-block bitmap, based on the total
  // number of memory blocks we'll be managing
  bitmapSize = (totalBlocks / 8);

  // Make sure the bitmap is allocated to block boundaries
  bitmapSize += (MEMBLOCKSIZE - (bitmapSize % MEMBLOCKSIZE));

  // If we want to actually USE the memory we just allocated, we will
  // have to map it into the kernel's address space
  status = kernelPageMapToFree(KERNELPROCID, (void *) bitmapPhysical, 
			       (void **) &freeBlockBitmap, bitmapSize);

  // Was this successful?
  if (status < 0)
    return (status);
  
  // Clear the memory we use for the bitmap
  kernelMemClear((void *) freeBlockBitmap, bitmapSize);

  // The list of reserved memory blocks needs to be completed here, before
  // we attempt to use it to calculate the location of the free-block
  // bitmap.  
  for (count = 0; reservedBlocks[count].processId != 0; count ++)
    {
      // Set the end value for the "kernel memory" reserved block
      if (!strcmp((char *) reservedBlocks[count].description,
		  "kernel memory"))
	{
	  reservedBlocks[count].endLocation = 
	    (KERNEL_LOAD_ADDRESS + kernelMemory - 1);
	}

      // Set the start and end values for the "kernel paging data"
      // reserved block
      if (!strcmp((char *) reservedBlocks[count].description,
		  "kernel paging data"))
	{
	  reservedBlocks[count].startLocation = 
	    (KERNEL_LOAD_ADDRESS + kernelMemory);
	  reservedBlocks[count].endLocation =
	    (reservedBlocks[count].startLocation +
	     KERNEL_PAGING_DATA_SIZE - 1);
	}

      // Set the start and end values for the "free memory bitmap"
      // reserved block
      if (!strcmp((char *) reservedBlocks[count].description,
		  "free memory bitmap"))
	{
	  reservedBlocks[count].startLocation = (unsigned) bitmapPhysical;
	  reservedBlocks[count].endLocation =
	    (reservedBlocks[count].startLocation + bitmapSize - 1);
	}
    }

  // Allocate blocks for all the reserved memory ranges, including the free
  // block bitmap (which is cool, because this allocation will use the
  // bitmap itself (which is 'unofficially' allocated).  Woo, paradox...
  for (count = 0; reservedBlocks[count].processId != 0; count ++)
    {
      status = allocateBlock(reservedBlocks[count].processId,
			     reservedBlocks[count].startLocation, 
			     reservedBlocks[count].endLocation, 
			     (char *) reservedBlocks[count].description);
      if (status < 0)
	{
	  // Release the lock on the memory data
	  kernelResourceManagerUnlock(&memoryManagerLock);
	  return (status);
	}
    }

  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);

  // Make note of the fact that we've now been initialized
  memoryManagerInitialized = 1;

  // Return success
  return (status = 0);
}


void *kernelMemoryRequestBlock(unsigned requestedSize, unsigned alignment,
			       const char *description)
{
  // This is a wrapper function for the real requestBlock routine.
  // It will take the physical memory address returned by requestBlock
  // and ask the page manager to map it to virtual memory pages in the
  // address space of the current process, and return the virtual address
  // instead of the physical one.

  int status = 0;
  void *physicalMemoryBlock = NULL;
  void *virtualMemoryBlock = NULL;
  int processId = 0;


  // Make sure the memory manager has been initialized
  if (!memoryManagerInitialized)
    return (virtualMemoryBlock = NULL);

  // Get the current process Id
  processId = kernelMultitaskerGetCurrentProcessId();

  if (processId < 0)
    {
      kernelError(kernel_error, "Unable to determine the current process");
      return (virtualMemoryBlock = NULL);
    }

  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return (virtualMemoryBlock = NULL);

  // Call requestBlock to do all the real work of finding a free
  // memory region
  status = requestBlock(processId, requestedSize, alignment, description,
			&physicalMemoryBlock);

  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);

  // Make sure it was successful
  if (status < 0)
    return (virtualMemoryBlock = NULL);

  // Now we will ask the page manager to map this physical memory to
  // virtual memory pages in the address space of the current process.
  status = kernelPageMapToFree(processId, physicalMemoryBlock, 
			       &virtualMemoryBlock, requestedSize);

  // If this was unsuccessful, we have a problem.
  if (status < 0)
    {
      // Attempt to deallocate the physical memory block.
      kernelMemoryReleaseBlock(physicalMemoryBlock);
      return (virtualMemoryBlock = NULL);
    }

  // Clear the memory area we allocated
  kernelMemClear(virtualMemoryBlock, requestedSize);

  return (virtualMemoryBlock);
}


void *kernelMemoryRequestSystemBlock(unsigned requestedSize,
				     unsigned alignment,
				     const char *description)
{
  // This is a wrapper function for the real requestBlock routine.
  // It will take the physical memory address returned by requestBlock
  // and ask the page manager to map it to virtual memory pages in the
  // address space of the kernel, and return the virtual address instead
  // of the physical one.

  int status = 0;
  void *physicalMemoryBlock = NULL;
  void *virtualMemoryBlock = NULL;


  // Make sure the memory manager has been initialized
  if (!memoryManagerInitialized)
    return (virtualMemoryBlock = NULL);

  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return (virtualMemoryBlock = NULL);

  // Call requestBlock to do all the real work of finding a free
  // memory region
  status = requestBlock(KERNELPROCID, requestedSize, alignment, description,
			&physicalMemoryBlock);

  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);

  // Make sure it was successful
  if (status < 0)
    return (virtualMemoryBlock = NULL);

  // Now we will ask the page manager to map this physical memory to
  // virtual memory pages in the address space of the kernel.
  status = kernelPageMapToFree(KERNELPROCID, physicalMemoryBlock, 
			       &virtualMemoryBlock, requestedSize);

  // If this was unsuccessful, we have a problem.
  if (status < 0)
    {
      // Attempt to deallocate the physical memory block.
      kernelError(kernel_error, "Unable to map system memory block");
      kernelMemoryReleasePhysicalBlock(physicalMemoryBlock);
      return (virtualMemoryBlock = NULL);
    }

  // Make sure the memory is really in the kernel's address space
  if (virtualMemoryBlock < (void *) KERNEL_VIRTUAL_ADDRESS)
    {
      kernelError(kernel_error, "Page manager did not correctly map system "
		  "memory block");
      kernelMemoryReleasePhysicalBlock(physicalMemoryBlock);
      return (virtualMemoryBlock = NULL);
    }

  // Clear the memory area we allocated
  kernelMemClear(virtualMemoryBlock, requestedSize);

  return (virtualMemoryBlock);
}


void *kernelMemoryRequestPhysicalBlock(unsigned requestedSize, 
				       unsigned alignment,
				       const char *description)
{
  // This is a wrapper function for the real requestBlock routine.
  // It return the physical memory address returned by requestBlock
  // unlike the virtual address returned by kernelMemoryRequestPhysicalBlock.
  // This function will need to be called very seldom.  Mostly by the
  // page manager, which needs to allocate page tables and page directories
  // based on their physical addresses.

  int status = 0;
  void *memory = NULL;

  // Make sure the memory manager has been initialized
  if (!memoryManagerInitialized)
    return (memory = NULL);

  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return (memory = NULL);

  // Call requestBlock to do all the real work of finding a free
  // memory region.  We will call it system memory for the time being.
  status = requestBlock(KERNELPROCID, requestedSize, alignment, description,
			&memory);
  
  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);

  if (status < 0)
    return (memory = NULL);
  
  else
    // Don't clear this memory, since it has not been mapped into the
    // virtual address space.  The caller must clear it, if desired.
    return (memory);
}


int kernelMemoryChangeOwner(int oldPid, int newPid, int remap, 
			    void *oldVirtualAddress, void **newVirtualAddress)
{
  // This function can be used by a privileged process to change the
  // process owner of a block of allocated memory.

  int status = 0;
  void *physicalAddress = NULL;
  unsigned blockSize = 0;
  int count;

  
  // Make sure the memory manager has been initialized
  if (!memoryManagerInitialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure newVirtualAddress isn't NULL if we're remapping
  if (remap && (newVirtualAddress == NULL))
    {
      kernelError(kernel_error, "Pointer for new virtual address is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // We do a privilege check here, to make sure that this operation
  // is allowed

  // Do we really need to change anything?
  if (oldPid == newPid)
    {
      // Nope.  The processes are the same.
      if (remap)
	*newVirtualAddress = oldVirtualAddress;
      return (status = 0);
    }

  // The caller will be pass memoryPointer as a virtual address.  Turn the 
  // memoryPointer into a physical address
  physicalAddress = kernelPageGetPhysical(oldPid, oldVirtualAddress);

  if (physicalAddress == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The memory pointer is NULL");
      return (status = ERR_NOSUCHENTRY);
    }
  
  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return (status);

  // Now we can go through the "used" list watching for a start-end
  // range that encompasses this pointer
  for (count = 0; ((count < usedBlocks) && (count < MAXMEMORYBLOCKS)); 
       count ++)
    {
      if (usedBlockList[count]->startLocation == (unsigned) physicalAddress)
	{
	  // This is the one.

	  if (usedBlockList[count]->processId != oldPid)
	    {
	      kernelError(kernel_error, "Attempt to change memory ownership "
			  "from incorrect owner (%d should be %d)", oldPid,
			  usedBlockList[count]->processId);
	      return (status = ERR_PERMISSION);
	    }

	  if (remap)
	    {
	      blockSize = ((usedBlockList[count]->endLocation - 
			    usedBlockList[count]->startLocation) + 1);

	      // Map the memory into the new owner's address space
	      status = kernelPageMapToFree(newPid, physicalAddress, 
					   newVirtualAddress, blockSize);

	      if (status < 0)
		{
		  kernelResourceManagerUnlock(&memoryManagerLock);
		  return (status);
		}

	      // Unmap the memory from the old owner's address space
	      status = kernelPageUnmap(oldPid, oldVirtualAddress, blockSize);
	      
	      if (status < 0)
		{
		  kernelResourceManagerUnlock(&memoryManagerLock);
		  return (status);
		}
	    }

	  // Change the pid number on this block
	  usedBlockList[count]->processId = newPid;

	  // Quit
	  break;
	}
    }

  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);

  // Did we find it?
  if ((count >= usedBlocks) || (count >= MAXMEMORYBLOCKS))
    return (status = ERR_NOSUCHENTRY);

  // Return success
  return (status = 0);
}


int kernelMemoryShare(int sharerPid, int shareePid, void *oldVirtualAddress,
		      void **newVirtualAddress)
{
  // This function can be used by a privileged process to share a piece
  // of memory owned by one process with another process.  This will not
  // adjust the privilege levels of memory that is shared, so a privileged
  // process cannot share memory with an unprivileged process (an access by
  // the second process will cause a page fault).

  int status = 0;
  void *physicalAddress = NULL;
  unsigned blockSize = 0;
  int count;

  
  // Make sure the memory manager has been initialized
  if (!memoryManagerInitialized)
    return (status = ERR_NOTINITIALIZED);

  // We do a privilege check here, to make sure that this operation
  // is allowed

  // Do we really need to change anything?
  if (sharerPid == shareePid)
    {
      // Nope.  The processes are the same.
      *newVirtualAddress = oldVirtualAddress;
      return (status = 0);
    }

  // The caller will pass memoryPointer as a virtual address.  Turn the 
  // memoryPointer into a physical address
  physicalAddress = kernelPageGetPhysical(sharerPid, oldVirtualAddress);

  if (physicalAddress == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The memory pointer is NULL");
      return (status = ERR_NOSUCHENTRY);
    }
  
  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return (status);

  // Now we can go through the "used" list watching for a start value
  // that matches this pointer
  for (count = 0; ((count < usedBlocks) && (count < MAXMEMORYBLOCKS)); 
       count ++)
    {
      if (usedBlockList[count]->startLocation == (unsigned) physicalAddress)
	{
	  // This is the one.

	  if (usedBlockList[count]->processId != sharerPid)
	    {
	      kernelError(kernel_error, "Attempt to share memory from "
			  "incorrect owner (%d should be %d)", sharerPid,
			  usedBlockList[count]->processId);
	      return (status = ERR_PERMISSION);
	    }

	  blockSize = ((usedBlockList[count]->endLocation - 
			usedBlockList[count]->startLocation) + 1);

	  // Map the memory into the sharee's address space
	  status = kernelPageMapToFree(shareePid, physicalAddress, 
				       newVirtualAddress, blockSize);

	  if (status < 0)
	    {
	      kernelResourceManagerUnlock(&memoryManagerLock);
	      return (status);
	    }

	  // The sharer still owns the memory, so don't change the block's
	  // PID.  Quit.
	  break;
	}
    }

  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);

  // Did we find it?
  if ((count >= usedBlocks) || (count >= MAXMEMORYBLOCKS))
    return (status = ERR_NOSUCHENTRY);

  // Return success
  return (status = 0);
}


int kernelMemoryReleaseBlock(void *virtualAddress)
{
  // This routine will determine the blockId of the block that contains
  // the memory location pointed to by the parameter.  After it has done
  // this, it calls the kernelMemoryReleaseByBlockId routine.  It returns
  // 0 if successful, negative otherwise.

  int currentPid = 0;
  void *physicalAddress = NULL;
  int status = 0;
  int count;

  //return (0);

  // Make sure the memory manager has been initialized
  if (!memoryManagerInitialized)
    return (status = ERR_NOTINITIALIZED);

  // It's OK for virtualAddress to be NULL.

  currentPid = kernelMultitaskerGetCurrentProcessId();

  // Permission check: This function can only be used to release system
  // blocks by privileged processes because it is accessible to user functions
  if ((virtualAddress >= (void *) KERNEL_VIRTUAL_ADDRESS) &&
      (currentPid != KERNELPROCID) &&
      (kernelMultitaskerGetProcessPrivilege(currentPid) !=
       PRIVILEGE_SUPERVISOR))
    {
      kernelError(kernel_error, "Cannot release system memory block from "
		  "unprivileged user process %d", currentPid);
      return (status = ERR_PERMISSION);
    }

  // The caller will be pass memoryPointer as a virtual address.  Turn the 
  // memoryPointer into a physical address.  We could simply unmap it
  // here except that we don't yet know the size of the block.
  physicalAddress = kernelPageGetPhysical(currentPid, virtualAddress);

  if (physicalAddress == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The memory pointer is NULL");
      return (status = ERR_NOSUCHENTRY);
    }
  
  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return (status);

  // Now we can go through the "used" block list watching for a start
  // value that matches this pointer
  for (count = 0; ((count < usedBlocks) && (count < MAXMEMORYBLOCKS)); 
       count ++)
    if (usedBlockList[count]->startLocation == (unsigned) physicalAddress)
      {
	// This is the one.  Now that we know the size of the memory
	// block, we can unmap it from the virtual address space.
	status = kernelPageUnmap(currentPid, virtualAddress, 
				 (usedBlockList[count]->endLocation - 
				  usedBlockList[count]->startLocation + 1));
	
	if (status < 0)
	  {
	    // Make the error
	    kernelError(kernel_error, "Unable to unmap memory from the "
			"virtual address space");
	    kernelResourceManagerUnlock(&memoryManagerLock);
	    return (status);
	  }

	// Call the releaseBlock routine with this block's blockId.
	status = releaseBlock(count);
	
	// Release the lock on the memory data
	kernelResourceManagerUnlock(&memoryManagerLock);

	return (status);
      }

  // If we fall through, we didn't find the requested block
  
  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);
  
  return (status = ERR_NOSUCHENTRY);
}


int kernelMemoryReleaseSystemBlock(void *virtualAddress)
{
  // This routine will determine the blockId of the block that contains
  // the memory location pointed to by the parameter.  After it has done
  // this, it calls the kernelMemoryReleaseByBlockId routine.  It returns
  // 0 if successful, negative otherwise.

  void *physicalAddress = NULL;
  int status = 0;
  int count;

  // Make sure the memory manager has been initialized
  if (!memoryManagerInitialized)
    return (status = ERR_NOTINITIALIZED);

  // It is NOT OK for virtualAddress to be NULL in this function.  System
  // memory blocks should never start at 0
  if (virtualAddress == NULL)
    {
      kernelError(kernel_error, "The memory pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // The virtual address must be in kernel address space
  if (virtualAddress < (void *) KERNEL_VIRTUAL_ADDRESS)
    {
      kernelError(kernel_error, "The system memory block to release is not "
		  "in the kernel's address space");
      return (status = ERR_INVALID);
    }

  // The caller will be pass memoryPointer as a virtual address.  Turn the 
  // memoryPointer into a physical address.  We could simply unmap it
  // here except that we don't yet know the size of the block.
  physicalAddress = 
    kernelPageGetPhysical(KERNELPROCID, virtualAddress);

  if (physicalAddress == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The memory pointer is NULL");
      return (status = ERR_NOSUCHENTRY);
    }
  
  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return (status);

  // Now we can go through the "used" block list watching for a start
  // value that matches this pointer
  for (count = 0; ((count < usedBlocks) && (count < MAXMEMORYBLOCKS)); 
       count ++)
    if (usedBlockList[count]->startLocation == (unsigned) physicalAddress)
      {
	// This is the one.  Now that we know the size of the memory
	// block, we can unmap it from the virtual address space.
	status = kernelPageUnmap(KERNELPROCID, virtualAddress, 
				 ((usedBlockList[count]->endLocation - 
				   usedBlockList[count]->startLocation) + 1));

	if (status < 0)
	  {
	    // Make the error
	    kernelError(kernel_error, "Unable to unmap memory from the "
			"virtual address space");
	    kernelResourceManagerUnlock(&memoryManagerLock);
	    return (status);
	  }

	// Call the releaseBlock routine with this block's blockId.
	status = releaseBlock(count);
	
	// Release the lock on the memory data
	kernelResourceManagerUnlock(&memoryManagerLock);

	return (status);
      }

  // If we fall through, we didn't find the requested block
  
  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);
  
  return (status = ERR_NOSUCHENTRY);
}


int kernelMemoryReleasePhysicalBlock(void *physicalAddress)
{
  // This routine will determine the blockId of the block that contains
  // the memory location pointed to by the parameter.  After it has done
  // this, it calls the kernelMemoryReleaseByBlockId routine.  It returns
  // 0 if successful, negative otherwise.

  int status = 0;
  int count;


  // Make sure the memory manager has been initialized
  if (!memoryManagerInitialized)
    return (status = ERR_NOTINITIALIZED);

  // It's not OK for physicalAddress to be NULL.
  if (physicalAddress == NULL)
    return (status = ERR_NOSUCHENTRY);

  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return (status);

  // Now we can go through the "used" block list watching for a start
  // value that matches this pointer
  for (count = 0; ((count < usedBlocks) && (count < MAXMEMORYBLOCKS)); 
       count ++)
    if (usedBlockList[count]->startLocation == (unsigned) physicalAddress)
      {
	// Call the releaseBlock routine with this block's blockId.
	status = releaseBlock(count);
	
	// Release the lock on the memory data
	kernelResourceManagerUnlock(&memoryManagerLock);

	// Make sure it was successful
	if (status < 0)
	  return (status);

	// Otherwise, return success
	return (status = 0);
      }

  // If we fall through, we didn't find the requested block
  
  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);
  
  return (status = ERR_NOSUCHENTRY);
}


int kernelMemoryReleaseAllByProcId(int procId)
{
  // This routine will find all memory blocks owned by a particular
  // process and call releaseBlock to remove each one.  It returns 0 
  // on success, negative otherwise.

  int status = 0;
  int count;


  // Make sure the memory manager has been initialized
  if (!memoryManagerInitialized)
    return (status = ERR_NOTINITIALIZED);

  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return (status);

  for (count = 0; ((count < usedBlocks) && (count < MAXMEMORYBLOCKS)); 
       count ++)
    if (usedBlockList[count]->processId == procId)
      {
	// This is one.  Release this block
	status = releaseBlock(count);
	
	// Make sure it was successful
	if (status < 0)
	  {
	    // Release the lock on the memory data
	    kernelResourceManagerUnlock(&memoryManagerLock);
	    return (status);
	  }

	// Reduce "count" by one, since the block it points at now
	// will be one forward of where we want to be
	count  -= 1;
      }

  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);

  // Return success
  return (status = 0);
}


void kernelMemoryPrintUsage(void)
{
  // This routine prints some formatted information about the 
  // current usage of memory

  int status = 0;
  kernelMemoryBlock *temp = NULL;
  char buffer[1024];
  int count, count2;

  // Make sure the memory manager has been initialized
  if (!memoryManagerInitialized)
    return;

  // Obtain a lock on the memory data
  status = kernelResourceManagerLock(&memoryManagerLock);
  if (status < 0)
    return;

  // Before we display the list of used memory blocks, we should sort it so
  // that it's a little easier to see the distribution of memory.  This will
  // not help the memory manager to function more efficiently or anything,
  // it's purely cosmetic.  Just a bubble sort.
  for (count = 0; count < usedBlocks; count ++)
    for (count2 = 0;  count2 < (usedBlocks - 1); count2 ++)
      if (usedBlockList[count2]->startLocation > 
	  usedBlockList[count2 + 1]->startLocation)
	{
	  temp = usedBlockList[count2 + 1];
	  usedBlockList[count2 + 1] = usedBlockList[count2];
	  usedBlockList[count2] = temp;
	}

  // Release the lock on the memory data
  kernelResourceManagerUnlock(&memoryManagerLock);

  // Print the header lines
  kernelTextPrintLine(" --- Memory usage information by block ---");

  // Here's the loop through the used list
  for (count = 0; count < usedBlocks; count ++)
    {
      // go to the "count"th element and print the asociated info
      sprintf(buffer, " proc=%d %u", usedBlockList[count]->processId,
	      usedBlockList[count]->startLocation);
      kernelTextPrint(buffer);
      kernelTextTab();
      sprintf(buffer, "-> %u", usedBlockList[count]->endLocation);
      kernelTextPrint(buffer);
      kernelTextTab();
      sprintf(buffer, " size=%u", usedBlockList[count]->endLocation - 
	      usedBlockList[count]->startLocation + 1);
      kernelTextPrint(buffer);
      kernelTextTab();
      kernelTextPrintLine((char *) usedBlockList[count]->description);
    }

  // Print out the percent usage information
  kernelTextPrintLine(" --- Usage totals ---\nTotal used blocks - %d\nTotal "
		      "used - %u - %d%%\nTotal free - %u - %d%%", usedBlocks,
		      totalUsed, percentUsage(), totalFree,
		      (100 - percentUsage()));
  return;
}
