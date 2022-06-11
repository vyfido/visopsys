// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
static mallocBlock *unusedBlockList = NULL;
static volatile unsigned totalBlocks = 0;
static volatile unsigned usedBlocks = 0;
static volatile unsigned totalMemory = 0;
static volatile unsigned usedMemory = 0;
static lock blocksLock;

unsigned mallocHeapMultiple = USER_MEMORY_HEAP_MULTIPLE;
mallocKernelOps mallocKernOps;

#define blockEnd(block) (block->start + block->size - 1)

#if defined(DEBUG)
#define debug(message, arg...) do {                                    \
  if (visopsys_in_kernel)                                              \
    {                                                                  \
      if (mallocKernOps.debug)                                         \
	mallocKernOps.debug(__FILE__, __FUNCTION__, __LINE__,          \
			    debug_memory, message, ##arg);             \
    }                                                                  \
  else                                                                 \
    {                                                                  \
      printf("DEBUG: %s:%s(%d): ", __FILE__, __FUNCTION__, __LINE__);  \
      printf(message, ##arg);                                          \
      printf("\n");                                                    \
    }                                                                  \
} while (0)
#else
#define debug(message, arg...) do { } while (0)
#endif // defined(DEBUG)

#define error(message, arg...) do {                                      \
  if (visopsys_in_kernel)                                                \
    mallocKernOps.error(__FILE__, __FUNCTION__, __LINE__, kernel_error,  \
			message, ##arg);                                 \
  else                                                                   \
    {                                                                    \
      printf("Error: %s:%s(%d): ", __FILE__, __FUNCTION__, __LINE__);    \
      printf(message, ##arg);                                            \
      printf("\n");                                                      \
    }                                                                    \
} while (0)


static inline int procid(void)
{
  if (visopsys_in_kernel)
    return (mallocKernOps.multitaskerGetCurrentProcessId());
  else
    return (multitaskerGetCurrentProcessId());
}


static inline void *memory_get(unsigned size, const char *desc)
{
  debug("%s %u", __FUNCTION__, size);
  if (visopsys_in_kernel)
    return (mallocKernOps.memoryGet(size, desc));
  else
    return (memoryGet(size, desc));
}


static inline int memory_release(void *start)
{
  debug("%s %p", __FUNCTION__, start);
  if (visopsys_in_kernel)
    return (mallocKernOps.memoryRelease(start));
  else
    return (memoryRelease(start));
}


static inline int lock_get(lock *lk)
{
  if (visopsys_in_kernel)
    return (mallocKernOps.lockGet(lk));
  else
    return (lockGet(lk));
}


static inline void lock_release(lock *lk)
{
  if (visopsys_in_kernel)
    mallocKernOps.lockRelease(lk);
  else
    lockRelease(lk);
}


static inline void insertBlock(mallocBlock *insBlock, mallocBlock *nextBlock)
{
  // Stick the first block in front of the second block

  debug("Insert block %u->%u before %u->%u", insBlock->start,
	blockEnd(insBlock), nextBlock->start, blockEnd(nextBlock));

  insBlock->prev = nextBlock->prev;
  insBlock->next = nextBlock;

  if (nextBlock->prev)
    nextBlock->prev->next = insBlock;
  nextBlock->prev = insBlock;

  if (nextBlock == blockList)
    blockList = insBlock;
}


static inline void appendBlock(mallocBlock *appBlock, mallocBlock *prevBlock)
{
  // Stick the first block behind the second block

  debug("Append block %u->%u after %u->%u", appBlock->start,
	blockEnd(appBlock), prevBlock->start, blockEnd(prevBlock));

  appBlock->prev = prevBlock;
  appBlock->next = prevBlock->next;

  if (prevBlock->next)
    prevBlock->next->prev = appBlock;
  prevBlock->next = appBlock;
}


static int sortInsertBlock(mallocBlock *block)
{
  // Find the correct (sorted) place for in the block list for this block.

  int status = 0;
  mallocBlock *nextBlock = NULL;

  if (blockList)
    {
      nextBlock = blockList;

      while (nextBlock)
	{
	  if (nextBlock->start > block->start)
	    {
	      insertBlock(block, nextBlock);
	      break;
	    }

	  if (!nextBlock->next)
	    {
	      appendBlock(block, nextBlock);
	      break;
	    }

	  nextBlock = nextBlock->next;
	}
    }
  else
    {
      block->prev = NULL;
      block->next = NULL;
      blockList = block;
    }

  return (status = 0);
}


static int allocUnusedBlocks(void)
{
  // This grows the unused block list by 1 memory page.

  int status = 0;
  int numBlocks = 0;
  int count;

  debug("Allocating new unused blocks");

  if (visopsys_in_kernel)
    unusedBlockList = memory_get(MEMORY_BLOCK_SIZE, "kernel heap metadata");
  else
    unusedBlockList = memory_get(MEMORY_BLOCK_SIZE, "user heap metadata");
  if (unusedBlockList == NULL)
    {
      error("Unable to allocate heap management memory");
      return (status = ERR_MEMORY);
    }

  // How many blocks is that?
  numBlocks = (MEMORY_BLOCK_SIZE / sizeof(mallocBlock));

  // Initialize the pointers in our list of blocks
  for (count = 0; count < numBlocks; count ++)
    {
      if (count > 0)
	unusedBlockList[count].prev = &unusedBlockList[count - 1];
      if (count < (numBlocks - 1))
	unusedBlockList[count].next = &unusedBlockList[count + 1];
    }

  totalBlocks += numBlocks;

  return (status = 0);
}


static mallocBlock *getBlock(void)
{
  // Get a block from the unused block list.

  mallocBlock *block = NULL;

  // Do we have any more unused blocks?
  if (!unusedBlockList)
    {
      if (allocUnusedBlocks() < 0)
	return (block = NULL);
    }

  block = unusedBlockList;
  unusedBlockList = block->next;
  
  // Clear it
  bzero(block, sizeof(mallocBlock));

  usedBlocks += 1;

  return (block);
}


static void putBlock(mallocBlock *block)
{
  // This function gets called when a block is no longer needed.  We
  // zero out its fields and move it to the end of the used blocks.

  // Remove the block from the list.
  if (block->prev)
    block->prev->next = block->next;
  if (block->next)
    block->next->prev = block->prev;

  if (block == blockList)
    blockList = block->next;

  // Clear it
  bzero(block, sizeof(mallocBlock));

  // Put it at the head of the unused block list
  block->next = unusedBlockList;
  unusedBlockList = block;

  usedBlocks -= 1;

  return;
}


static int addBlock(int used, unsigned start, unsigned size,
		    unsigned heapAlloc)
{
  // Creates a used or free block in our block list.

  int status = 0;
  mallocBlock *block = NULL;

  block = getBlock();
  if (block == NULL)
    return (status = ERR_NOFREE);

  block->used = used;
  block->start = start;
  block->size = size;
  block->heapAlloc = heapAlloc;

  status = sortInsertBlock(block);
  if (status < 0)
    return (status);

  return (status = 0);
}


static int growHeap(unsigned minSize)
{
  // This grows the pool of heap memory by at least minSize bytes.

  void *newHeap = NULL;

  // Don't allocate less than the default heap multiple
  if (minSize < mallocHeapMultiple)
    minSize = mallocHeapMultiple;

  // Allocation should be a multiple of MEMORY_BLOCK_SIZE
  if (minSize % MEMORY_BLOCK_SIZE)
    minSize += (MEMORY_BLOCK_SIZE - (minSize % MEMORY_BLOCK_SIZE));

  debug("%s %u", __FUNCTION__, minSize);
  if (minSize > mallocHeapMultiple)
    debug("Size is greater than %u", mallocHeapMultiple);

  // Get the heap memory
  if (visopsys_in_kernel)
    newHeap = memory_get(minSize, "kernel heap");
  else
    newHeap = memory_get(minSize, "user heap");
  if (newHeap == NULL)
    {
      error("Unable to allocate heap memory");
      return (ERR_MEMORY);
    }

  totalMemory += minSize;

  // Add it as a single free block
  return (addBlock(0 /* unused */, (unsigned) newHeap, minSize,
		   (unsigned) newHeap));
}


static mallocBlock *findFree(unsigned size)
{
  mallocBlock *block = blockList;

  while (block)
    {
      if (!block->used && (block->size >= size))
	return (block);
      
      block = block->next;
    }

  return (block = NULL);
}


static void *allocateBlock(unsigned size, const char *function)
{
  // Find a block of unused memory, and return the start pointer.

  int status = 0;
  mallocBlock *block = NULL;

  // Make sure we do allocations on nice boundaries
  if (size % sizeof(int))
    size += (sizeof(int) - (size % sizeof(int)));

  // Make sure there's enough heap memory.  This will get called the first
  // time we're invoked, as totalMemory will be zero.
  if ((size > (totalMemory - usedMemory)) ||
      ((block = findFree(size)) == NULL))
    {
      status = growHeap(size);
      if (status < 0)
	{
	  errno = status;
	  return (NULL);
	}

      block = findFree(size);
      if (block == NULL)
	{
	  // Something really wrong.
	  error("Unable to allocate block of size %u (%s)", size,
		function);
	  return (NULL);
	}
    }

  block->used = 1;
  block->function = function;
  block->process = procid();

  // If part of this block will be unused, we will need to create a free
  // block for the remainder
  if (block->size > size)
    {
      if (addBlock(0 /* unused */, (block->start + size),
		   (block->size - size), block->heapAlloc) < 0)
	return (NULL);
      block->size = size;
    }

  usedMemory += size;

  return ((void *) block->start);
}


static void mergeFree(mallocBlock *block)
{
  // Merge this free block with the previous and/or next blocks if they
  // are also free.

  if (block->prev && !block->prev->used &&
      (blockEnd(block->prev) == (block->start - 1)) &&
      (block->prev->heapAlloc == block->heapAlloc))
    {
      block->start = block->prev->start;
      block->size += block->prev->size;
      putBlock(block->prev);
    }
  
  if (block->next && !block->next->used &&
      (blockEnd(block) == (block->next->start - 1)) &&
      (block->next->heapAlloc == block->heapAlloc))
    {
      block->size += block->next->size;
      putBlock(block->next);
    }
}


static void cleanupHeap(mallocBlock *block)
{
  // If the supplied free block comprises an entire heap allocation,
  // return that heap memory and get rid of the block.

  if (block->prev && (block->prev->heapAlloc == block->heapAlloc))
    return;

  if (block->next && (block->next->heapAlloc == block->heapAlloc))
    return;

  // Looks like we can return this memory.
  memory_release((void *) block->start);
  totalMemory -= block->size;
  putBlock(block);
}


static int deallocateBlock(void *start, const char *function)
{
  // Find an allocated (used) block and deallocate it.

  int status = 0;
  mallocBlock *block = blockList;

  while (block)
    {
      if (block->start == (unsigned) start)
	{
	  if (!block->used)
	    {
	      error("Block at %p is not allocated (%s)", start, function);
	      return (status = ERR_ALREADY);
	    }
	  
	  // Clear out the memory
	  bzero(start, block->size);

	  block->used = 0;
	  block->process = 0;
	  block->function = NULL;

	  usedMemory -= block->size;

	  // Merge free blocks on either side of this one
	  mergeFree(block);

	  // Can the heap be deallocated?
	  cleanupHeap(block);
  
	  return (status = 0);
	}

      block = block->next;
    }

  error("No such memory block %u to deallocate (%s)", (unsigned) start,
	function);
  return (status = ERR_NOSUCHENTRY);
}


static inline void mallocBlock2MemoryBlock(mallocBlock *maBlock,
					   memoryBlock *meBlock)
{
  meBlock->processId = maBlock->process;
  strncpy(meBlock->description,
	  (maBlock->used? maBlock->function : "--free--"),
	  MEMORY_MAX_DESC_LENGTH);
  meBlock->description[MEMORY_MAX_DESC_LENGTH - 1] = '\0';
  meBlock->startLocation = maBlock->start;
  meBlock->endLocation = blockEnd(maBlock);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void *_doMalloc(unsigned size, const char *function __attribute__((unused)))
{
  // These are the guts of malloc() and kernelMalloc()

  int status = 0;
  void *address = NULL;

  // If the requested block size is zero, forget it.  We can probably
  // assume something has gone wrong in the calling program
  if (size == 0)
    {
      error("Can't allocate 0 bytes");
      errno = ERR_INVALID;
      return (address = NULL);
    }

  status = lock_get(&blocksLock);
  if (status < 0)
    {
      error("Can't get memory lock");
      errno = status;
      return (address = NULL);
    }

  // Find a free block big enough
  address = allocateBlock(size, function);

  lock_release(&blocksLock);

  return (address);
}


void *_malloc(size_t size, const char *function)
{
  // User space wrapper for _doMalloc() so we can ensure kernel-space calls
  // use kernelMalloc()

  if (visopsys_in_kernel)
    {
      error("Cannot call malloc() directly from kernel space (%s)", function);
      return (NULL);
    }
  else
    return (_doMalloc(size, function));
}


void _doFree(void *start, const char *function __attribute__((unused)))
{
  // These are the guts of free() and kernelFree()

  int status = 0;

  if (start == NULL)
    {
      error("Can't free NULL pointer");
      errno = ERR_INVALID;
      return;
    }

  // Make sure we've been initialized
  if (!usedBlocks)
    {
      error("Malloc not initialized");
      errno = ERR_NOTINITIALIZED;
      return;
    }

  status = lock_get(&blocksLock);
  if (status < 0)
    {
      error("Can't get memory lock");
      errno = status;
      return;
    }

  status = deallocateBlock(start, function);

  lock_release(&blocksLock);

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
      error("Cannot call free() directly from kernel space (%s)", function);
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
  mallocBlock *maBlock = blockList;

  // Check params
  if ((start == NULL) || (meBlock == NULL))
    return (status = ERR_NULLPARAMETER);

  // Loop through the block list
  while (maBlock)
    {
      if (maBlock->start == (unsigned) start)
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
      error("Stats structure pointer is NULL");
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
  mallocBlock *block = blockList;
  int count;

  // Check params
  if (blocksArray == NULL)
    {
      error("Blocks array pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Loop through the block list
  for (count = 0; (block && (count < doBlocks)); count ++)
    {
      mallocBlock2MemoryBlock(block, &(blocksArray[count]));
      block = block->next;
    }
  
  return (status = 0);
}
