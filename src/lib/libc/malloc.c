// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  malloc.c
//
	
// These routines comprise Visopsys heap memory management system.  It relies
// upon the kernelMemory code, and does similar things, but instead of whole
// memory pages, it allocates arbitrary-sized chunks.

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/memory.h>
#include <sys/api.h>

static mallocBlock *blockList = NULL;
static mallocBlock *firstUnusedBlock = NULL;
static volatile unsigned totalBlocks = 0;
static volatile unsigned usedBlocks = 0;
static volatile unsigned totalMemory = 0;
static volatile unsigned usedMemory = 0;
static lock blocksLock;

unsigned mallocHeapMultiple = USER_MEMORY_HEAP_MULTIPLE;
mallocKernelOps mallocKernOps;

#define blockSize(block) \
  (((unsigned) block->end - (unsigned) block->start) + 1)

#define kernelMultitaskerGetCurrentProcessId(id) do {    \
  if (visopsys_in_kernel)                                \
    id = mallocKernOps.multitaskerGetCurrentProcessId(); \
  else                                                   \
    id = multitaskerGetCurrentProcessId();               \
} while (0)

#define kernelMemoryGetSystem(size, desc, ptr) do {  \
  if (visopsys_in_kernel)                            \
    ptr = mallocKernOps.memoryGetSystem(size, desc); \
  else                                               \
    ptr = memoryGet(size, desc);                     \
} while (0)

#define kernelLockGet(lk, status) do {  \
  if (visopsys_in_kernel)               \
    status = mallocKernOps.lockGet(lk); \
  else                                  \
    status = lockGet(lk);               \
} while (0)

#define kernelLockRelease(lk) do { \
  if (visopsys_in_kernel)          \
    mallocKernOps.lockRelease(lk); \
  else                             \
    lockRelease(lk);               \
} while (0)

#define kernelError(kind, message, arg...) do {                          \
  if (visopsys_in_kernel)                                                \
    mallocKernOps.error(__FILE__, __FUNCTION__, __LINE__, kind, message, \
                        ##arg);                                          \
  else                                                                   \
    {                                                                    \
      printf("%s:%s(%d): ", __FILE__, __FUNCTION__, __LINE__);           \
      printf(message, ##arg);                                            \
      printf("\n");                                                      \
    }                                                                    \
} while (0)


static inline void insertBlock(mallocBlock *firstBlock,
			       mallocBlock *secondBlock)
{
  // Stick the first block in front of the second block
  firstBlock->previous = secondBlock->previous;
  firstBlock->next = secondBlock;
  if (secondBlock->previous)
    secondBlock->previous->next = firstBlock;
  secondBlock->previous = firstBlock;
}


static int sortInsertBlock(mallocBlock *block)
{
  // Find the correct (sorted) place for it
  int status = 0;
  mallocBlock *nextBlock = blockList;

  if (blockList == firstUnusedBlock)
    {
      insertBlock(block, firstUnusedBlock);
      blockList = block;
      return (status = 0);
    }
  
  while (1)
    {
      // This should never happen
      if (nextBlock == NULL)
	{
	  kernelError(kernel_error, "Unable to insert memory block %s %u->%u "
		      "(%u)", block->function, (unsigned) block->start,
		      (unsigned) block->end, blockSize(block));
	  return (status = ERR_BADDATA);
	}

      if ((nextBlock->start > block->start) || (nextBlock == firstUnusedBlock))
	{
	  insertBlock(block, nextBlock);
	  if (nextBlock == blockList)
	    blockList = block;
	  return (status = 0);
	}
      
      nextBlock = nextBlock->next;
    }
}


static int growList(void)
{
  // This grows the block list by 1 memory page.  It should only be called
  // when the list is empty/full.

  int status = 0;
  mallocBlock *newBlocks = NULL;
  int numBlocks = 0;
  int count;

  kernelMemoryGetSystem(MEMORY_BLOCK_SIZE, "application memory", newBlocks);
  if (newBlocks == NULL)
    {
      kernelError(kernel_error, "Unable to allocate heap management memory");
      return (status = ERR_MEMORY);
    }

  // How many blocks is that?
  numBlocks = (MEMORY_BLOCK_SIZE / sizeof(mallocBlock));

  // Initialize the pointers in our list of blocks
  for (count = 0; count < numBlocks; count ++)
    {
      if (count > 0)
	newBlocks[count].previous = &(newBlocks[count - 1]);
      if (count < (numBlocks - 1))
	newBlocks[count].next = &(newBlocks[count + 1]);
    }

  if (blockList == NULL)
    {
      blockList = newBlocks;
      firstUnusedBlock = newBlocks;
    }
  else
    {
      // Add our new stuff to the end of the existing list
      firstUnusedBlock->next = newBlocks;
      newBlocks[0].previous = firstUnusedBlock;
    }

  totalBlocks += numBlocks;

  return (status = 0);
}


static mallocBlock *getBlock(void)
{
  mallocBlock *block = NULL;
  mallocBlock *previousBlock = NULL;
  mallocBlock *nextBlock = NULL;

  // Do we have more than one free block?
  if ((firstUnusedBlock == NULL) || (firstUnusedBlock->next == NULL))
    {
      if (growList() < 0)
	return (block = NULL);
    }

  block = firstUnusedBlock;
  previousBlock = block->previous;
  nextBlock = block->next;

  // Remove it from its place in the list, linking its previous and next
  // blocks together.
  if (previousBlock)
    previousBlock->next = nextBlock;
  if (nextBlock)
    nextBlock->previous = previousBlock;

  firstUnusedBlock = nextBlock;
  if (block == blockList)
    blockList = nextBlock;

  // Clear it
  bzero(block, sizeof(mallocBlock));

  usedBlocks++;

  return (block);
}


static void releaseBlock(mallocBlock *block)
{
  // This function gets called when a block is no longer needed.  We
  // zero out its fields and move it to the end of the used blocks.

  mallocBlock *previousBlock = block->previous;
  mallocBlock *nextBlock = block->next;

  // Temporarily remove it from the list, linking its previous and next
  // blocks together.
  if (previousBlock)
    previousBlock->next = nextBlock;
  if (nextBlock)
    {
      nextBlock->previous = previousBlock;

      if (block == blockList)
	blockList = nextBlock;
    }

  // Clear it
  bzero(block, sizeof(mallocBlock));

  // Stick it in front of the first unused block
  insertBlock(block, firstUnusedBlock);
  if (firstUnusedBlock == blockList)
    blockList = block;

  firstUnusedBlock = block;

  usedBlocks--;

  return;
}


static void mergeFree(mallocBlock *block)
{
  // Merge any free blocks on either side of this one with this one

  mallocBlock *previous = block->previous;
  mallocBlock *next = block->next;

  if (previous)
    if ((previous->used == 0) && (previous->end == (block->start - 1)))
      {
	block->start = previous->start;
	releaseBlock(previous);
      }
  
  if (next)
    if ((next->used == 0) && (next->start == (block->end + 1)))
      {
	block->end = next->end;
	releaseBlock(next);
      }

  return;
}


static int addBlock(int used, void *start, void *end)
{
  // This puts the supplied data into our block list

  int status = 0;
  mallocBlock *block = NULL;

  block = getBlock();
  if (block == NULL)
    return (status = ERR_NOFREE);

  block->used = used;
  block->start = start;
  block->end = end;

  status = sortInsertBlock(block);
  if (status < 0)
    return (status);

  if (used == 0)
    // If it's free, make sure it's merged with any other adjacent free
    // blocks on either side
    mergeFree(block);

  return (status = 0);
}


static int growHeap(unsigned minSize)
{
  // This grows the pool of heap memory by mallocHeapMultiple bytes.

  void *newHeap = NULL;

  if (minSize < mallocHeapMultiple)
    minSize = mallocHeapMultiple;

  // Get the heap memory
  kernelMemoryGetSystem(minSize, "application memory", newHeap);
  if (newHeap == NULL)
    {
      kernelError(kernel_error, "Unable to allocate heap memory");
      return (ERR_MEMORY);
    }

  totalMemory += minSize;

  // Add it as a single free block
  return (addBlock(0 /* Free */, newHeap,
		   ((void *)((((unsigned) newHeap) + minSize) - 1)))); 
}


static mallocBlock *findFree(unsigned size)
{
  mallocBlock *block = blockList;

  while (1)
    {
      if (block == NULL)
	return (block);

      if ((block->used == 0) && (blockSize(block) >= size))
	return (block);
      
      block = block->next;
      
      if (block == firstUnusedBlock)
	return (block = NULL);
    }
}


static void *allocateBlock(unsigned size, const char *function)
{
  // Find a block of unused memory, and return the start pointer.

  mallocBlock *block = NULL;

  block = findFree(size);

  if (block == NULL)
    {
      // There is no block big enough to accommodate this.
      if (growHeap(size) < 0)
	return (NULL);

      block = findFree(size);
      if (block == NULL)
	{
	  // Something really wrong.
	  kernelError(kernel_error, "Unable to allocate block of size %u (%s)",
		      size, function);
	  return (NULL);
	}
    }

  block->used = 1;
  block->function = function;
  kernelMultitaskerGetCurrentProcessId(block->process);

  // If part of this block will be unused, we will need to create a free
  // block for the remainder
  if (blockSize(block) > size)
    {
      if (addBlock(0 /* unused */, (block->start + size), block->end) < 0)
	return (NULL);
      block->end = ((block->start + size) - 1);
    }

  usedMemory += size;

  return (block->start);
}


static int deallocateBlock(void *start, const char *function)
{
  // Find an allocated (used) block and deallocate it.

  int status = 0;
  mallocBlock *block = blockList;

  while (1)
    {
      if (block == NULL)
	{
	  kernelError(kernel_error, "Block is NULL (%s)", function);
	  return (status = ERR_NODATA);
	}

      if (block->start == start)
	{
	  if (block->used == 0)
	    {
	      kernelError(kernel_error, "Block at %u is not allocated (%s)",
			  (unsigned) start, function);
	      return (status = ERR_ALREADY);
	    }
	  
	  // Clear out the memory
	  bzero(block->start, blockSize(block));

	  block->function = NULL;
	  block->process = 0;
	  block->used = 0;

	  usedMemory -= blockSize(block);

	  // Merge free blocks on either side of this one
	  mergeFree(block);
  
	  return (status = 0);
	}

      block = block->next;

      if (block == firstUnusedBlock)
	{
	  kernelError(kernel_error, "No such memory block %u to deallocate "
		      "(%s)", (unsigned) start, function);
	  return (status = ERR_NOSUCHENTRY);
	}
    }
}


static inline void mallocBlock2MemoryBlock(mallocBlock *maBlock,
					   memoryBlock *meBlock)
{
  meBlock->processId = maBlock->process;
  strncpy(meBlock->description,
	  (maBlock->used? maBlock->function : "--free--"),
	  MEMORY_MAX_DESC_LENGTH);
  meBlock->description[MEMORY_MAX_DESC_LENGTH - 1] = '\0';
  meBlock->startLocation = (unsigned) maBlock->start;
  meBlock->endLocation = (unsigned) maBlock->end;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void *_doMalloc(unsigned size, const char *function)
{
  // These are the guts of malloc() and kernelMalloc()

  int status = 0;
  void *address = NULL;

  kernelLockGet(&blocksLock, status);
  if (status < 0)
    {
      kernelError(kernel_error, "Can't get memory lock");
      errno = status;
      return (address = NULL);
    }

  // Make sure we do allocations on nice boundaries
  if (size % sizeof(int))
    size += (sizeof(int) - (size % sizeof(int)));

  // Make sure there's enough heap memory.  This will get called the first
  // time we're invoked, as totalMemory will be zero.
  while (size > (totalMemory - usedMemory))
    {
      status = growHeap(size);
      if (status < 0)
	{
	  kernelLockRelease(&blocksLock);
	  kernelError(kernel_error, "Can't grow heap");
	  errno = status;
	  return (address = NULL);
	}
    }

  // Find a free block big enough
  address = allocateBlock(size, function);

  kernelLockRelease(&blocksLock);

  return (address);
}


void *_malloc(size_t size, const char *function)
{
  // User space wrapper for _doMalloc() so we can ensure kernel-space calls
  // use kernelMalloc()

  if (visopsys_in_kernel)
    {
      kernelError(kernel_error, "Cannot call malloc() directly from kernel "
		  "space (%s)", function);
      return (NULL);
    }
  else
    return (_doMalloc(size, function));
}


void _doFree(void *start, const char *function)
{
  // These are the guts of free() and kernelFree()

  int status = 0;

  kernelLockGet(&blocksLock, status);
  if (status < 0)
    {
      kernelError(kernel_error, "Can't get memory lock");
      errno = status;
      return;
    }

  // Make sure we've been initialized
  if (!usedBlocks)
    {
      kernelError(kernel_error, "Malloc not initialized");
      kernelLockRelease(&blocksLock);
      errno = ERR_NOSUCHENTRY;
      return;
    }

  status = deallocateBlock(start, function);

  kernelLockRelease(&blocksLock);

  if (status < 0)
    errno = status;

  return;
}


void _free(void *start, const char *function)
{
  // User space wrapper for _doFree() so we can ensure kernel-space calls
  // use kernelFree()

  if (visopsys_in_kernel)
    {
      kernelError(kernel_error, "Cannot call free() directly from kernel "
		  "space (%s)", function);
      return;
    }
  else
    return (_doFree(start, function));
}


int _mallocBlockInfo(void *start, memoryBlock *meBlock)
{
  // Try to find the block that starts at the supplied address and fill out
  // the structure with information about it.

  int status = 0;
  mallocBlock *maBlock = NULL;
  int count;

  // Check params
  if ((start == NULL) || (meBlock == NULL))
    return (status = ERR_NULLPARAMETER);

  // Loop through the block list
  for (count = 0, maBlock = blockList;
       (maBlock && (maBlock != firstUnusedBlock)); count ++)
    {
      if (maBlock->start == start)
	{
	  mallocBlock2MemoryBlock(maBlock, meBlock);
	  return (status = 0);
	}	

      maBlock = maBlock->next;
    }

  // Fell through -- no such block
  return (status = ERR_NOSUCHENTRY);
}


int _mallocGetStats(memoryStats *stats)
{
  // Return malloc memory usage statistics
  
  int status = 0;

  // Check params
  if (stats == NULL)
    {
      kernelError(kernel_error, "Stats structure pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  stats->totalBlocks = totalBlocks;
  stats->usedBlocks = usedBlocks;
  stats->totalMemory = totalMemory;
  stats->usedMemory = usedMemory;
  return (status = 0);
}


int _mallocGetBlocks(memoryBlock *blocksArray, int doBlocks)
{
  // Fill a memoryBlock array with 'doBlocks' used malloc blocks information
  
  int status = 0;
  mallocBlock *block = NULL;
  int count;

  // Check params
  if (blocksArray == NULL)
    {
      kernelError(kernel_error, "Blocks array pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Loop through the block list
  for (count = 0, block = blockList;
       (block && (block != firstUnusedBlock) && (count < doBlocks));
       count ++)
    {
      mallocBlock2MemoryBlock(block, &(blocksArray[count]));
      block = block->next;
    }
  
  return (status = 0);
}
