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
//  kernelMalloc.c
//
	
// These routines comprise Visopsys' internal, kernel-only memory management
// system.  It relies upon kernelMemoryManager, and does similar things,
// but instead of whole memory pages, it allocates arbitrary-sized chunks.

#include "kernelMalloc.h"
#include "kernelMemoryManager.h"
#include "kernelParameters.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <stdio.h>

#include "kernelText.h"
#include "kernelMultitasker.h"

static kernelMallocBlock *blockList = NULL;
static kernelMallocBlock *firstUnusedBlock = NULL;
static volatile unsigned totalBlocks = 0;
static volatile unsigned usedBlocks = 0;
static volatile unsigned totalMemory = 0;
static volatile unsigned usedMemory = 0;

static char *FUNCTION;


static inline void insertBlock(kernelMallocBlock *newBlock,
			       kernelMallocBlock *moveBlock)
{
  // Stick the first block in front of the second block
  newBlock->previous = moveBlock->previous;
  newBlock->next = (void *) moveBlock;
  if (moveBlock->previous)
    ((kernelMallocBlock *) moveBlock->previous)->next = (void *) newBlock;
  moveBlock->previous = (void *) newBlock;
}


static int removeBlock(kernelMallocBlock *block)
{
  // This function gets called when a block is no longer needed.  We
  // zero out its fields and move it to the end of the used blocks.

  // Temporarily remove it from the list, linking its previous and next
  // blocks together.
  if (block->previous)
    ((kernelMallocBlock *) block->previous)->next = block->next;
  if (block->next)
    ((kernelMallocBlock *) block->next)->previous = block->previous;
  
  // Stick it in front of the first unused block
  insertBlock(block, firstUnusedBlock);

  firstUnusedBlock = block;

  block->used = 0;
  block->start = 0;
  block->end = 0;

  usedBlocks--;

  return (0);
}


static int growList(void)
{
  // This grows the block list by 1 memory page.  It should only be called
  // when the list is empty/full.

  int status = 0;
  kernelMallocBlock *newBlocks = NULL;
  int numBlocks = 0;
  int count;

  newBlocks = kernelMemoryGetSystem(MEMBLOCKSIZE, "kernel memory data");
  if (newBlocks == NULL)
    {
      kernelError(kernel_error, "Unable to allocate kernel memory %s",
		  FUNCTION);
      return (status = ERR_MEMORY);
    }

  // How many blocks is that?
  numBlocks = (MEMBLOCKSIZE / sizeof(kernelMallocBlock));

  // Initialize the pointers in our list of blocks
  for (count = 0; count < numBlocks; count ++)
    {
      if (count > 0)
	newBlocks[count].previous = (void *) &(newBlocks[count - 1]);
      if (count < (numBlocks - 1))
	newBlocks[count].next = (void *) &(newBlocks[count + 1]);
    }

  if (blockList == NULL)
    {
      blockList = newBlocks;
      firstUnusedBlock = newBlocks;
    }
  else
    {
      // Add our new stuff to the end of the existing list
      firstUnusedBlock->next = (void *) newBlocks;
      newBlocks[0].previous = (void *) firstUnusedBlock;
    }

  totalBlocks += numBlocks;

  return (status = 0);
}


static int mergeFree(kernelMallocBlock *block)
{
  // Merge any free blocks on either side of this one with this one

  kernelMallocBlock *tmp = NULL;
  int status = 0;

  tmp = (kernelMallocBlock *) block->previous;
  if (tmp && (tmp->used != 1) && (tmp->end == (block->start - 1)))
    {
      block->start = tmp->start;
      status = removeBlock(tmp);
      if (status < 0)
	return (status);
    }

  tmp = (kernelMallocBlock *) block->next;
  if (tmp && (tmp->used != 1) && (tmp->start == (block->end + 1)))
    {
      block->end = tmp->end;
      status = removeBlock(tmp);
      if (status < 0)
	return (status);
    }

  return (status = 0);
}


static int addBlock(int used, void *start, void *end)
{
  // This puts the supplied data into our block list

  int status = 0;
  kernelMallocBlock *block = NULL;
  kernelMallocBlock *nextBlock = NULL;

  // Do we have more than one free block?
  if ((totalBlocks == 0) || (firstUnusedBlock->next == NULL))
    {
      status = growList();
      if (status < 0)
	return (status);
    }

  block = firstUnusedBlock;
  block->used = used;
  block->start = start;
  block->end = end;
  
  firstUnusedBlock = block->next;

  // Temporarily remove it from the list, linking its previous and next
  // blocks together.
  if (block->previous)
    ((kernelMallocBlock *) block->previous)->next = block->next;
  if (block->next)
    ((kernelMallocBlock *) block->next)->previous = block->previous;

  // Find the correct (sorted) place for it
  nextBlock = blockList;
  while (nextBlock)
    {
      if ((nextBlock == firstUnusedBlock) ||
	  (nextBlock->start > block->start))
	break;
      nextBlock = (kernelMallocBlock *) nextBlock->next;
    }

  // This should never happen
  if (nextBlock == NULL)
    {
      kernelError(kernel_error, "Unable to add %s memory block %s %u->%u (%u)",
		  (used? "used" : "free"), FUNCTION, block->start, block->end,
		  ((block->end - block->start) + 1));
      return (status = ERR_BADDATA);
    }

  insertBlock(block, nextBlock);

  usedBlocks++;

  if (!used)
    {
      // If it's free, make sure it's merged with any other adjacent free
      // blocks on either side
      status = mergeFree(block);
      if (status < 0)
	return (status);
    }

  return (status = 0);
}


static int growHeap(unsigned minSize)
{
  // This grows the pool of heap memory by MEMORY_HEAP_MULTIPLE bytes.

  void *newHeap = NULL;

  if (minSize < MEMORY_HEAP_MULTIPLE)
    minSize = MEMORY_HEAP_MULTIPLE;

  // Get the heap memory
  newHeap = kernelMemoryGetSystem(minSize, "kernel memory");
  if (newHeap == NULL)
    {
      kernelError(kernel_error, "Unable to allocate kernel memory %s",
		  FUNCTION);
      return (ERR_MEMORY);
    }

  totalMemory += minSize;

  // Add it as a single free block
  return (addBlock(0 /* Free */, newHeap, ((newHeap + minSize) - 1))); 
}


static inline kernelMallocBlock *findFree(unsigned size)
{
  kernelMallocBlock *block = blockList;
  while (block)
    {
      if ((block == firstUnusedBlock) ||
	  ((block->used == 0) && (((block->end - block->start) + 1) >= size)))
	break;
      block = (kernelMallocBlock *) block->next;
    }

  if (block == firstUnusedBlock)
    return (NULL);
  else
    return (block);
}


static void *allocateBlock(unsigned size)
{
  // Find a block of unused memory, and return the start pointer.

  kernelMallocBlock *block = NULL;
  
  block = findFree(size);
  if (block == NULL)
    {
      // Hmm.  Maybe still no single block big enough.
      if (growHeap(size) < 0)
	return (NULL);

      block = findFree(size);
      if (block == NULL)
	{
	  // Something really wrong.
	  kernelError(kernel_error, "Unable to allocate block of size %u",
		      size);
	  return (NULL);
	}
    }

  block->used = 1;
  block->function = FUNCTION;
  block->process = kernelMultitaskerGetCurrentProcessId();
  usedMemory += size;

  // If part of this block will be unused, we will need to create a free
  // block for the remainder
  if (((block->end - block->start) + 1) > size)
    {
      unsigned diff = (((block->end - block->start) + 1) - size);
      block->end = ((block->start + size) - 1);
      if (addBlock(0 /* Free */, (block->end + 1),
		   ((block->end + diff) - 1)) < 0)
	return (NULL);
    }

  return (block->start);
}


static int deallocateBlock(void *start)
{
  // Find an allocated (used) block and deallocate it.

  int status = 0;
  kernelMallocBlock *block = NULL;

  block = blockList;
  while (block)
    {
      if ((block == firstUnusedBlock) ||
	  (block->used && (block->start == start)))
	break;
      block = (kernelMallocBlock *) block->next;
    }

  if ((block == NULL) || (block == firstUnusedBlock))
    {
      kernelError(kernel_error, "No such memory block to deallocate %s",
		  FUNCTION);
      return (status = ERR_NOSUCHENTRY);
    }

  block->used = 0;
  block->function = NULL;
  block->process = 0;
  usedMemory -= ((block->end - block->start) + 1);

  // Clear out the memory
  kernelMemClear(block->start, ((block->end - block->start) + 1));

  // Merge free blocks on either side of this one
  return (mergeFree(block));
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void *_kernelMalloc(char *function, unsigned size)
{
  // Just like a malloc(), for kernel memory, but the data is cleared like
  // calloc.

  int status = 0;
  void *block = NULL;

  FUNCTION = function;

  // Make sure we do allocations on nice boundaries
  if (size % sizeof(unsigned))
    size += (sizeof(unsigned) - (size % sizeof(unsigned)));

  // Make sure there's enough heap memory.  This will get called the first
  // time we're invoked, as totalMemory will be zero.
  while (size > (totalMemory - usedMemory))
    {
      status = growHeap(size);
      if (status < 0)
	return (block = NULL);
    }

  // Find a free block big enough
  return (allocateBlock(size));
}


int _kernelFree(char *function, void *start)
{
  // Just like free(), for kernel memory

  int status = 0;

  FUNCTION = function;

  // Make sure we've been initialized
  if (!usedBlocks)
    return (status = ERR_NOSUCHENTRY);

  // The start address must be in kernel address space
  if (start < (void *) KERNEL_VIRTUAL_ADDRESS)
    {
      kernelError(kernel_error, "The kernel memory block to release is not "
		  "in the kernel's address space %s", FUNCTION);
      return (status = ERR_INVALID);
    }

  return (deallocateBlock(start));
}


void kernelMallocDump(void)
{
  kernelMallocBlock *block = NULL;

  kernelTextPrintLine("\n --- Dynamic memory ---");
  block = blockList;
  while (block && (block != firstUnusedBlock))
    {
      kernelTextPrintLine("%u->%u (%u) %s %d %s", block->start,
			  block->end, ((block->end - block->start) + 1),
			  (block->used? "Used" : "Free"), block->process,
			  (block->used? block->function : ""));
      block = (kernelMallocBlock *) block->next;
    }

  kernelTextPrintLine("%u blocks, %u used (%u%%)\n%u bytes, %u used "
		      "(%u%%)", totalBlocks, usedBlocks,
		      ((usedBlocks * 100) / totalBlocks), totalMemory,
		      usedMemory, ((usedMemory * 100) / totalMemory));
}
