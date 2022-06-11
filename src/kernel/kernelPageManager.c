//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
//  kernelPageManager.c
//

// This file contains the C functions belonging to the kernel's 
// paging manager.  It keeps lists of page directories and page tables,
// and performs all the work of mapping and unmapping pages in the tables.

#include "kernelPageManager.h"
#include "kernelParameters.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelProcessorFunctions.h"
#include "kernelPicFunctions.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelError.h"
#include <sys/memory.h>
#include <sys/errors.h>
#include <string.h>


// A pointer to the kernel's page directory
static kernelPageDirectory *kernelPageDir = NULL;

// A list of all the page directories and page tables we've created, so we 
// can keep track of all the physical vs. virtual addresses of these.
static kernelPageDirectory pageDirMemory[MAX_PROCESSES];
static kernelPageDirectory *pageDirList[MAX_PROCESSES];
static volatile int numberPageDirectories = 0;
static kernelPageTable pageTableMemory[MAX_PROCESSES];
static kernelPageTable *pageTableList[MAX_PROCESSES];
static volatile int numberPageTables = 0;

// The physical memory location where we'll store the kernel's paging data.
static unsigned int kernelPagingData = 0;

static volatile int initialized = 0;

// Forward declaration, since the dependencies in this file are circular.
static int map(kernelPageDirectory *, void **, void **, unsigned int, int);


static kernelPageDirectory *createPageDirectory(int processId, int privilege)
{
  // This function creates an empty page directory by allocating physical
  // memory for it, and on success, returns a pointer to a 
  // kernelPageDirectory holding information about the directory.  Returns
  // NULL on error.

  int status = 0;
  kernelPageDirectory *directory = NULL;
  kernelPageDirPhysicalMem *physicalAddr = NULL;
  kernelPageDirVirtualMem *virtualAddr = NULL;


  // Get some physical memory for the page directory
  physicalAddr = (kernelPageDirPhysicalMem *) 
    kernelMemoryRequestPhysicalBlock(sizeof(kernelPageDirPhysicalMem), 
				     MEMORY_PAGE_SIZE, "page directory");
  if (physicalAddr == NULL)
    return (directory = NULL);

  // Map it into the kernel's virtual address space.
  status = map(kernelPageDir, (void **) &physicalAddr, 
	       (void **) &virtualAddr, sizeof(kernelPageDirPhysicalMem), 1);

  if (status < 0)
    return (directory = NULL);

  // Clear this memory block, since kernelMemoryRequestPhysicalBlock
  // can't do it for us
  kernelMemClear((void *) virtualAddr, sizeof(kernelPageDirPhysicalMem));

  // Put it in the next available kernelPageDirectory slot, and increase
  // the count of kernelPageDirectories
  directory = pageDirList[numberPageDirectories++];

  // Fill in this page directory
  directory->processId = processId;
  directory->numberShares = 0;
  directory->parent = 0;
  directory->privilege = privilege;
  directory->physical = physicalAddr;
  directory->virtual = virtualAddr;
  directory->lastPageTableNumber = 0;
  directory->firstTable = NULL;

  // kernelTextPrint("created page directory for process ");
  // kernelTextPrintInteger(directory->processId);
  // kernelTextNewline();

  // Return the directory
  return (directory);
}


static inline kernelPageDirectory *findPageDirectory(int processId)
{
  // This function just finds the page directory structure that belongs
  // to the requested process.  Returns NULL on failure.

  kernelPageDirectory *dir = NULL;
  int count;

  for (count = 0; count < numberPageDirectories; count ++)
    if (pageDirList[count]->processId == processId)
      {
	// If this is page directory is 'shared' from another page 
	// directory, then we need to recurse to find the parent (since
	// this one will, essentially, be 'empty')
	
	if (pageDirList[count]->parent)
	  dir = findPageDirectory(pageDirList[count]->parent);
	else
	  dir = pageDirList[count];

	break;
      }

  return (dir);
}


static int deletePageDirectory(kernelPageDirectory *dir)
{
  // This function is for the maintenance of our dynamic list of
  // page directory pointers.  It will remove the supplied page
  // directory from the list and deallocate the memory that was
  // reserved for the directory.  Returns 0 on success, negative
  // otherwise.

  int status = 0;
  kernelPageDirectory *parent = NULL;
  kernelPageDirectory *temp = NULL;
  int listPosition = 0;


  // Make sure this page directory isn't currently shared.  If it is, 
  // we can't delete it.
  if (dir->numberShares)
    return (status = ERR_BUSY);

  // If this page directory contains page tables, we can't delete it
  if (dir->firstTable)
    return (status = ERR_NOTEMPTY);

  // Figure out whether this page directory is 'sharing' from another
  // page directory.  If so, we don't need to do much other than remove
  // it from the list, and decrement the refcount in the parent
  if (dir->parent)
    {
      // Find the parent page directory
      parent = findPageDirectory(dir->parent);

      if (parent == NULL)
	return (status = ERR_NOSUCHENTRY);

      parent->numberShares -= 1;
    }
  else
    // It's a 'real' page table.  Deallocate the dynamic memory that
    // this directory is occupying
    kernelMemoryReleaseByPhysicalPointer((void *) dir->physical);

  // Now we need to remove it from the list.  First find its position
  // in the list.
  for (listPosition = 0; listPosition < numberPageDirectories; listPosition++)
    if (pageDirList[listPosition] == dir)
      break;

  if (pageDirList[listPosition] != dir)
    return (status = ERR_NOSUCHENTRY);

  // Decrement the count of page directories
  numberPageDirectories -= 1;

  // This list is the same as several other lists in the kernel.  We remove
  // this pointer from the list by swapping its pointer in the list with
  // that of the last item in the list and decrementing the count
  // (UNLESS: this is the last one, or the only one).

  if ((numberPageDirectories > 0) && 
      (listPosition < numberPageDirectories))
    {
      // Swap this item with the last item
      temp = dir;
      pageDirList[listPosition] = pageDirList[numberPageDirectories];
      pageDirList[numberPageDirectories] = temp;
    }

  // Return success
  return (status = 0);
}


static kernelPageTable *createPageTable(kernelPageDirectory *dir, int number)
{
  // This function creates an empty page table and maps it into the
  // supplied page directory.

  int status = 0;
  kernelPageTablePhysicalMem *physicalAddr = NULL;
  kernelPageTableVirtualMem *virtualAddr = NULL;
  kernelPageTable *table = NULL;
  kernelPageTable *listItemPointer = NULL;
  int count;


  // Allocate some physical memory for the page table
  physicalAddr = 
    kernelMemoryRequestPhysicalBlock(sizeof(kernelPageTablePhysicalMem), 
				     MEMORY_PAGE_SIZE, "page table");

  if (physicalAddr == NULL)
    return (table = NULL);

  // Map it into the kernel's virtual address space.
  status = map(kernelPageDir, (void **) &physicalAddr, 
	       (void **) &virtualAddr, sizeof(kernelPageTablePhysicalMem), 1);

  if (status < 0)
    return (table = NULL);

  // Clear this memory block, since kernelMemoryRequestPhysicalBlock
  // can't do it for us
  kernelMemClear((void *) virtualAddr, sizeof(kernelPageTablePhysicalMem));

  // Put it in the next available kernelPageTable slot, and increase
  // the count of kernelPageTables
  table = pageTableList[numberPageTables++];

  // Fill in this page table
  table->physical = physicalAddr;
  table->virtual = virtualAddr;

  // Set the table number.
  table->tableNumber = number;

  if (dir->firstTable == NULL)
    {
      // This is the first page table for this page directory.  Just
      // stick it in the first slot
      dir->firstTable = table;
      table->previous = NULL;
      table->next = NULL;
    }
  else
    {
      // Walk down the list of page tables attached to this directory, and
      // add this one at the end
      listItemPointer = dir->firstTable;
      while(listItemPointer->next != NULL)
	listItemPointer = (kernelPageTable *) listItemPointer->next;

      // listItemPointer is the last page table in the list.
      listItemPointer->next = (void *) table;
      table->previous = (void *) listItemPointer;
      table->next = NULL;
    }

  // Now we actually go into the page directory memory and add the
  // real page table to the requested slot number.  Always enable 
  // write-though (bit 3), read/write (bit 1), and page-present (bit 0).
  dir->virtual->table[number] = (unsigned int) table->physical;
  dir->virtual->table[number] |= 0x0000000B;

  // Set the 'user' bit, if this page table is not privileged
  if (dir->privilege != PRIVILEGE_SUPERVISOR)
    dir->virtual->table[number] |= 0x00000004;

  // If this new page table belongs to the kernel, it needs to be 'shared'
  // with all of the other real page directories.
  if (dir == kernelPageDir)
    {
      // kernelTextPrintLine("(sharing kernel page table)");
      
      for (count = 0; count < numberPageTables; count ++)
	if (pageDirList[count]->parent == 0)
	  pageDirList[count]->virtual->table[number] =
	    kernelPageDir->virtual->table[number];
    }

  // Return the table
  return (table);
}


static inline kernelPageTable *findPageTable(kernelPageDirectory *dir, 
					     int number)
{
  // This function just searches the supplied page directory for the
  // page table structure with the requested table number.  Returns NULL
  // on failure.

  kernelPageTable *table = NULL;
  kernelPageTable *temp = NULL;

  temp = dir->firstTable;

  while(temp != NULL)
    {
      if (temp->tableNumber == number)
	{
	  table = temp;
	  break;
	}
      else
	temp = temp->next;
    }

  return (table);
}


static int deletePageTable(kernelPageTable *table)
{
  // This function is for the maintenance of our dynamic list of page table
  // pointers.  It will remove the supplied page table from the list and
  // deallocate the memory that was reserved for it.  Returns 0 on success,
  // negative otherwise.

  int status = 0;
  kernelPageTable *temp = NULL;
  int listPosition = 0;
  

  // Make sure this page directory doesn't have any others chained
  // after it.  If it does, we can't delete it.
  if (table->next)
    return (status = ERR_BUSY);

  // Deallocate the memory that this page table was using
  kernelMemoryReleaseByPhysicalPointer((void *) table->physical);

  // Ok, now we need to find this page directory in the list.
  for (listPosition = 0; listPosition < numberPageTables; listPosition++)
    if (pageTableList[listPosition] == table)
      break;

  if (pageTableList[listPosition] != table)
    return (status = ERR_NOSUCHENTRY);

  // Decrease the count of page tables
  numberPageTables -= 1;

  // This list is the same as several other lists in the kernel.  We remove
  // this pointer from the list by swapping its pointer in the list with
  // that of the last item in the list and decrementing the count
  // (UNLESS: this is the last one, or the only one).

  if ((numberPageTables > 0) && 
      (listPosition < numberPageTables))
    {
      // Swap this item with the last item
      temp = table;
      pageTableList[listPosition] = pageTableList[numberPageTables];
      pageTableList[numberPageTables] = temp;
    }

  // Return success
  return (status = 0);
}


static unsigned int findPageTableEntry(kernelPageDirectory *pageDirectory, 
				       void *virtualAddress)
{
  // Given a page directory and a virtual address, this function will find
  // the appropriate page table entry and return it.  
  // Returns 0 on error. <-- Hmm.

  unsigned int pageTableEntry = 0;
  int pageTableNumber = 0;
  kernelPageTable *pageTable = NULL;
  int pageNumber = 0;


  // dirPhysAddr is allowed to be NULL.  We'll deal with that in a minute.
  // virtualAddress can be zero also

  if (((unsigned int) virtualAddress % MEMORY_PAGE_SIZE) != 0)
    return (pageTableEntry = NULL);

  // Figure out which page table corresponds to this virtual address, and
  // get the page table
  pageTableNumber = tableNumber(virtualAddress);
  
  // If the virtual address is in the user address space, get the page
  // table via the supplied page directory.  Otherwise, use the kernel's
  // page directory.
  if ((unsigned int) virtualAddress < KERNEL_VIRTUAL_ADDRESS)
    pageTable = findPageTable(pageDirectory, pageTableNumber);
  else
    pageTable = findPageTable(kernelPageDir, pageTableNumber);

  if (pageTable == NULL)
    // We're hosed.  This table should already exist.
    return (pageTableEntry = NULL);

  pageNumber = pageNumber(virtualAddress);
  
  // Grab the value from the page table
  return (pageTableEntry = pageTable->virtual->page[pageNumber]);
}


static inline void *getPhysical(kernelPageDirectory *directory,
				void *virtualAddress)
{
  return ((void *) (findPageTableEntry(directory, virtualAddress) 
		    & 0xFFFFF000));
}


static int findFreePages(kernelPageDirectory *directory, int pages,
			   void **startAddress)
{
  // This function will find a range of unused pages in the supplied
  // page directory that is as large as the number of pages requested.
  // Sets a pointer representing the virtual address of the free pages
  // on success, and returns 0.  On failure it returns negative.

  int status = ERR_NOFREE;
  int numberFree = 0;
  kernelPageTable *table = NULL;
  kernelPageTable *nextTable = NULL;
  int pageTableNumber = 0;
  int pageNumber = 0;
  unsigned int startingAt = 0;
  

  // We need to determine the address at which we will start looking for
  // free pages.  For the kernel, it is kernelVirtualAddress.  For all 
  // others, it is 0.
  if (directory->physical == kernelPageDir->physical)
    {
      // We're searching in kernel space
      startingAt = KERNEL_VIRTUAL_ADDRESS;
    }
  else
    {
      // We're searching in user space
      startingAt = 0;
    }

  // Based on the starting address, which page table number should
  // we start at?
  pageTableNumber = tableNumber(startingAt);

  // kernelTextPrint("find: first table number is ");
  // kernelTextPrintInteger(pageTableNumber);
  // kernelTextNewline();

  // Loop through the supplied page directory, starting at the beginning.
  for ( ; pageTableNumber < 1024; pageTableNumber++)
    {
      // Get a pointer to this page table.
      table = findPageTable(directory, pageTableNumber);

      if (table == NULL)
	{
	  // We're hosed
	  kernelError(kernel_error, "No such page table ");
	  return (status = ERR_NOFREE);
	}

      // Loop through the pages in this page table.  If we find a free
      // page and numberFree is zero, set freeSpace to the corresponding
      // virtual address.  If we find a used page, we reset both numberFree
      // and freeStart to NULL
      for (pageNumber = 0; pageNumber < 1024; pageNumber++)
	{
	  // If this is the last page table and it's nearly full,
	  // allocate another one right now.
	  if ((pageTableNumber >= directory->lastPageTableNumber) &&
	      (pageNumber >= 1000))
	    {
	      //kernelTextPrintLine("increasing page tables");

	      directory->lastPageTableNumber += 1;
	      nextTable = createPageTable(directory,
					  directory->lastPageTableNumber);
	      if (nextTable == NULL)
		// We're going to be hosed very shortly
		return (status = ERR_NOFREE);
	    }

	  if (table->virtual->page[pageNumber] == NULL)
	    {
	      if (numberFree == 0)
		*startAddress = (void *) 
		  ((pageTableNumber << 22) + (pageNumber << 12));
	      
	      numberFree += 1;

	      if (numberFree >= pages)
		{
		  status = 0;
		  break;
		}
	    }
	  else
	    {
	      numberFree = 0;
	    }
	}

      if (numberFree >= pages)
	break;

      // If we fall through here, we're moving on to the next page table. 
    }

  return (status);
}


static int map(kernelPageDirectory *pageDirectory, void **physicalAddress, 
       void **virtualAddress, unsigned int size, int add)
{
  // This function is used by the rest of the kernel to map or unmap
  // physical memory pages in the address space of a process.  If mapping
  // is being performed, this will map the physical memory to the first
  // range of the process' unused pages that is large enough to handle the
  // request.  By default, it will make all pages that it maps writable.
  // On success, it returns the virtual address of the newly-mapped memory.
  // On failure it returns NULL.

  int status = 0;
  kernelPageTable *pageTable = NULL;
  void *currentVirtualAddress = NULL;
  void *currentPhysicalAddress = NULL;
  int pageTableNumber = 0;
  unsigned int pageNumber = 0;
  unsigned int numPages = 0;

  
  // Make sure that our arguments are reasonable.  The wrapper functions
  // that are used to call us from external locations do not check them.

  if (size == 0)
    return (status = ERR_INVALID);

  // Make sure the pointers to virtualAddress/physicalAddress are not NULL
  if ((virtualAddress == NULL) || (physicalAddress == NULL))
    return (status = ERR_NULLPARAMETER);

  if (add)
    {
      if (((unsigned int) *physicalAddress % MEMORY_PAGE_SIZE) != 0)
	return (status = ERR_ALIGN);
    }
  else
    {
      if (((unsigned int) *virtualAddress % MEMORY_PAGE_SIZE) != 0)
	return (status = ERR_ALIGN);
    }

  // Ok, now determine how many pages we need to map or unmap
  numPages = (size / MEMORY_PAGE_SIZE);
  if ((size % MEMORY_PAGE_SIZE) != 0)
    numPages += 1;

  if (add)
    {
      // We're adding, so try to find an unused range of pages big enough
      status = findFreePages(pageDirectory, numPages, virtualAddress);

      // Did we find some?
      if (status < 0)
	return (status);
    }
  else
    {
      // When we are unmapping, the address passed to us is a virtual 
      // address, not a physical one.
      *physicalAddress = getPhysical(pageDirectory, *virtualAddress);

      // Is it OK?
      if (*physicalAddress == NULL)
	return (status = ERR_NOSUCHENTRY);
    }

  // Set the address variables we will use to walk through the table
  currentVirtualAddress = *virtualAddress;
  currentPhysicalAddress = *physicalAddress;
  
  // Change the entries in the page table
  while(1)
    {
      // Get the address of the page table.  Figure out the page table
      // number based on the virtual address we're currently working with,
      // and get the page table.
      pageTableNumber = tableNumber(currentVirtualAddress);

      // If we are unmapping an address from the user's address space,
      // use the supplied page directory.  Otherwise if we are unmapping
      // from the kernel's address space, use the kernel's page dir.
      if ((unsigned int) currentVirtualAddress < KERNEL_VIRTUAL_ADDRESS)
	pageTable = findPageTable(pageDirectory, pageTableNumber);
      else
	pageTable = findPageTable(kernelPageDir, pageTableNumber);

      if (pageTable == NULL)
	{
	  // We're hosed.  This table should already exist.
	  return (status = ERR_NOSUCHENTRY);
	}

      pageNumber = pageNumber(currentVirtualAddress);

      if (add)
	{
	  // Put the real address into the page table entry
	  pageTable->virtual->page[pageNumber] = 
	    (unsigned int) currentPhysicalAddress;

	  // Set the write-through bit, the writable bit, and the page
	  // present bit.
	  pageTable->virtual->page[pageNumber] |= 0x0000000B;

	  // Set the 'user' bit, if this page is not privileged
	  if (pageDirectory->privilege != PRIVILEGE_SUPERVISOR)
	    pageTable->virtual->page[pageNumber] |= 0x00000004;
	}
      else
	{
	  pageTable->virtual->page[pageNumber] = NULL;
	}

      // Decrement the number of pages left to map
      numPages -= 1;

      // Any pages left to do?
      if (numPages == 0)
	break;

      // Increment the working memory addresses
      currentVirtualAddress += MEMORY_PAGE_SIZE;
      currentPhysicalAddress += MEMORY_PAGE_SIZE;

      // Loop again
    }

  // Return success
  return (status = 0);
}


static kernelPageDirectory *firstPageDirectory(void)
{
  // This will create the first page directory, specifically for the
  // kernel.  This presents some special problems, since we don't want
  // to map it into the current, temporary page directory set up by
  // the loader.  We have to do this one manually.  Returns a pointer
  // to the directory on success, NULL on failure.

  kernelPageDirectory *firstDir = NULL;


  // Make it occupy the first spot.
  firstDir = pageDirList[0];
  numberPageDirectories += 1;

  // Get some physical memory for the page directory.  The physical
  // address we use for this is static, and is defined in kernelParameters.h 

  if ((kernelPagingData % MEMORY_PAGE_SIZE) != 0)
    return (firstDir = NULL);

  firstDir->physical = (kernelPageDirPhysicalMem *) kernelPagingData;

  // Clear the physical memory
  kernelMemClear((void *) firstDir->physical, 
		 sizeof(kernelPageDirPhysicalMem));

  // Make the virtual address equal the physical one, for now
  firstDir->virtual = (kernelPageDirVirtualMem *) firstDir->physical;

  firstDir->processId = KERNELPROCID;
  firstDir->numberShares = 0;
  firstDir->parent = 0;
  firstDir->privilege = PRIVILEGE_SUPERVISOR;
  firstDir->lastPageTableNumber = 0;
  firstDir->firstTable = NULL;


  // Set the global variable that will "remember" the pointer to the
  // kernel's page directory
  kernelPageDir = firstDir;

  return (firstDir);
}


static kernelPageTable *initialPageTables(int startingNumber)
{
  // This will create the first page table, specifically for the
  // kernel.  Just as like the first page directory, we don't want to map
  // it into the current, temporary page directory set up by the loader.
  // We have to do this one manually.  Returns a pointer to the first table
  // on success, NULL on failure.

  kernelPageTable *table;


  // kernelTextPrint("creating initial page table ");
  // kernelTextPrintInteger(startingNumber);
  // kernelTextNewline();

  // Assign the page table to a slot
  table = pageTableList[numberPageTables++];

  // Get some physical memory for the page table.  The base physical
  // address we use for this is static, and is defined in 
  // kernelParameters.h 

  if ((kernelPagingData % MEMORY_PAGE_SIZE) != 0)
    return (table = NULL);

  table->physical = (kernelPageTablePhysicalMem *) 
    (kernelPagingData + sizeof(kernelPageDirPhysicalMem));

  // Clear the physical memory
  kernelMemClear((void *) table->physical, 
		 sizeof(kernelPageTablePhysicalMem));

  // Make the virtual address equal the physical one, for now
  table->virtual = (kernelPageTableVirtualMem *) table->physical;

  // Set the table number
  table->tableNumber = startingNumber;

  // Set the kernel's 'last page table' variable
  kernelPageDir->lastPageTableNumber = table->tableNumber;

  // Since this is the first page table for the kernel's page directory,
  // stick it in the first slot
  kernelPageDir->firstTable = table;
  table->previous = NULL;
  table->next = NULL;

  // Now we actually go into the page directory memory and add the
  // real page table to the requested slot number.  Always enable 
  // write-though (bit 3), read/write (bit 1), and page-present (bit 0).

  kernelPageDir->virtual->table[startingNumber] = 
    (unsigned int) table->physical;

  kernelPageDir->virtual->table[startingNumber] |= 0x0000000B;

  return (table);
}


static int kernelPaging(unsigned int kernelMemory)
{
  // This function will reinitialize the paging environment at kernel
  // startup.  This needs to be handled differently than when regular
  // processes are created.
  
  int status = 0;
  kernelPageDirPhysicalMem *oldPageDirectory = NULL;
  kernelPageDirectory *newPageDirectory = NULL;
  void *virtualPD = NULL;
  kernelPageTablePhysicalMem *oldPageTable = NULL;
  kernelPageTable *newPageTable = NULL;
  int pageTableNumber = 0;
  int pageNumber = 0;
  int kernelPages = 0;
  int interrupts = 0;
  int count;

  
  // Interrupts should currently be disabled at this point.  Make sure
  // before we do anything drastic
  kernelPicInterruptStatus(interrupts);
  if (interrupts)
    return (status = ERR_NOTINITIALIZED);

  // The kernel is currently located at kernelVirtualAddress (virtually).
  // We need to locate the current, temporary page directory, then the page
  // table that corresponds to kernelVirtualAddress.  From there, we need
  // to copy the contents of the page table(s) until all of the kernel's
  // current memory set has been remapped.

  // Get the address of the old page directory.  In this special instance,
  // the physical address we get back from this call can be used like 
  // a virtual address, since the lower part of memory should presently
  // be identity-mapped.
  kernelProcessorGetCR3(oldPageDirectory);
  oldPageDirectory = (kernelPageDirPhysicalMem *) 
    ((unsigned int) oldPageDirectory & 0xFFFFF800);

  // The index of the page table can be determined from the virtual
  // address of the kernel.  This will be the same value we use in our
  // new page directory.
  pageTableNumber = tableNumber(KERNEL_VIRTUAL_ADDRESS);

  // Get the old page table address.  The number of the old page table
  // can be used as an index into the old page directory.  We mask out
  // the lower 12 bits of the value we find at that index, and voila, we
  // have a pointer to the old page table.  Again, we could normally
  // not use this for much since it's a physical address, but again,
  // this time it's also a virtual address.
  oldPageTable = (kernelPageTablePhysicalMem *) 
    (oldPageDirectory->table[pageTableNumber] & 0xFFFFF000);;

  // The index of the first page in the page table can also be determined
  // from the virtual address of the kernel.  This will be the same value
  // we use to start our new page tables.
  pageNumber = pageNumber(KERNEL_VIRTUAL_ADDRESS);

  // Create a new page directory for the kernel.  We have to do this
  // differently, as opposed to calling createPageDirectory(), since this
  // should not be "mapped" into the current, temporary page directory.
  newPageDirectory = firstPageDirectory();

  if (newPageDirectory == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Create a new, initial page table for the kernel.  Again, we have to
  // do this manually, as opposed to calling createPageTable(), since
  // this should also not be mapped into the current, temporary page
  // directory.
  newPageTable = initialPageTables(pageTableNumber);

  if (newPageTable == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Copy the RELEVANT contents of the old page table into the new
  // page table.  This suggests that some of the data in the old page
  // table might be irrelevant.  That would be correct.  You see, the 
  // loader might (presently DOES) map pages gratuitously, irrespective
  // of how many pages the kernel actually uses.  We will only copy the
  // pages that the kernel uses, based on the kernelSize.  The following
  // code needs to assume that the kernel does not cross a 4-Mb boundary.
  
  kernelPages = (kernelMemory / MEMORY_PAGE_SIZE);
  if ((kernelMemory % MEMORY_PAGE_SIZE) != 0)
    kernelPages += 1;

  for (count = pageNumber; count < (pageNumber + kernelPages); count ++)
    newPageTable->virtual->page[count] = oldPageTable->page[count];

  // Now that the new page directory and page tables are in place, we can
  // map them to virtual addresses inside themselves (woo, a paradox).
  status = map(newPageDirectory, (void **) &(newPageDirectory->physical), 
	       (void **) &virtualPD, sizeof(kernelPageDirPhysicalMem), 1);

  // Make sure it's OK
  if (status < 0)
    return (status);

  // Now map the page table as well.
  status = map(newPageDirectory, (void **) &(newPageTable->physical), 
	       (void **) &(newPageTable->virtual), 
	       sizeof(kernelPageTablePhysicalMem), 1);

  // Make sure it's OK
  if (status < 0)
    return (status);

  newPageDirectory->virtual = virtualPD;

  // Now we should be able to switch the processor to our new page 
  // directory and table(s).

  // Change CR3, with the write-through bit (bit 3) set
  kernelProcessorSetCR3((unsigned int) newPageDirectory->physical 
			| 0x00000008);

  // Return success
  return (status = 0);
}


static void shareKernelPages(kernelPageDirectory *directory)
{
  // This routine will put pointers to the kernel's page tables into the
  // supplied page directory.  This effectively puts the kernel "into
  // the virtual address space" of the process that owns the directory.

  int kernelStartingTable = 0;
  int count;


  // Determine the starting page table of the kernel's address space
  kernelStartingTable = tableNumber(KERNEL_VIRTUAL_ADDRESS);

  // We will do a loop, copying the table entries from the kernel's
  // page directory to the target page directory.
  for (count = kernelStartingTable; count < 1024; count ++)
    directory->virtual->table[count] = kernelPageDir->virtual->table[count];

  // Return
  return;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelPageManagerInitialize(unsigned int kernelMemory)
{
  // This function will initialize the page manager and call the kernelPaging
  // routine to create a set of new page tables for the kernel environment.
  // (This is based on the assumptions that paging has been enabled prior
  // to the kernel starting, and that there must be an existing set of
  // basic page tables created by the loader).  Returns 0 on success, 
  // negative on error.

  int status = 0;
  int count;


  // This should only get called once
  if (initialized)
    return (status = ERR_ALREADY);

  // kernelTextPrintLine("Initializing Page Manager");

  // Clear out the memory we'll use to keep track of all the page 
  // directories and page tables, and set both counters to zero.
  kernelMemClear((void *) pageDirMemory, 
			(sizeof(kernelPageDirectory) * MAX_PROCESSES));
  kernelMemClear((void *) pageTableMemory, 
			(sizeof(kernelPageTable) * MAX_PROCESSES));

  // Loop through both of the dynamic lists that we'll use to keep 
  // pointers to the memory space we just reserved
  for (count = 0; count < MAX_PROCESSES; count ++)
    {
      pageDirList[count] = &pageDirMemory[count];
      pageTableList[count] = &pageTableMemory[count];
    }

  numberPageDirectories = 0;
  numberPageTables = 0;

  // Calculate the physical memory location where we'll store the kernel's
  // paging data.
  kernelPagingData = (KERNEL_LOAD_ADDRESS + kernelMemory);

  // Initialize the kernel's paging environment, which is done differently
  // than for a normal process.
  status = kernelPaging(kernelMemory);

  if (status < 0)
    return (status);

  // Make note that we're initialized
  initialized = 1;

  // Return success
  return (status = 0);
}


void *kernelPageGetDirectory(int processId)
{
  // This is an accessor function, which just returns the physical address of
  // the requested page directory (suitable for putting in the CR3 register).
  // Returns NULL on failure

  kernelPageDirectory *directory = NULL;


  // Have we been initialized?
  if (!initialized)
    return (NULL);

  // Find the appropriate page directory
  directory = findPageDirectory(processId);

  if (directory == NULL)
    return (NULL);
  else
    return ((void *) directory->physical);
}


void *kernelPageNewDirectory(int processId, int privilege)
{
  // This function will create a new page directory and one page table for
  // a new process.

  kernelPageDirectory *pageDir = NULL;
  kernelPageTable *pageTable = NULL;


  // Have we been initialized?
  if (!initialized)
    return (NULL);

  // Make sure privilege is legal
  if ((privilege != PRIVILEGE_USER) && (privilege != PRIVILEGE_SUPERVISOR))
    return (NULL);

  // Create a page directory for the process
  pageDir = createPageDirectory(processId, privilege);

  // Is it OK?
  if (pageDir == NULL)
    return (NULL);

  // Create an initial page table in the page directory, in the first spot
  pageTable = createPageTable(pageDir, 0 /* slot 0 */);

  // Is it OK?
  if (pageTable == NULL)
    {
      // Deallocate the page directory we created
      deletePageDirectory(pageDir);
      return (NULL);
    }
  
  pageDir->lastPageTableNumber = 0;
  
  // Finally, we need to map the kernel's address space into that of this
  // new process.  The process will not receive copies of the kernel's
  // page tables.  It will only get pointers to them in its page directory.
  shareKernelPages(pageDir);

  // Return the physical address of the new page directory.
  return ((void *) pageDir->physical);
}


void *kernelPageShareDirectory(int parentId, int childId)
{
  // This function will allow a new process thread to share the page
  // directory of its parent.

  kernelPageDirectory *parentDirectory = NULL;
  kernelPageDirectory *childDirectory = NULL;


  // Have we been initialized?
  if (!initialized)
    return (NULL);

  // Find the page directory belonging to the parent process
  parentDirectory = findPageDirectory(parentId);

  if (parentDirectory == NULL)
    return (NULL);

  // It could happen that the parentId and childId are the same (the
  // kernel is its own parent, for example).  If so, we're going to skip
  // actually creating a new page directory structure
  if (parentId != childId)
    {
      // Ok.  Make room for a new page directory structure for the child.
      // We do this manually (i.e. without calling createPageDirectory())
      // because this shared page directory needs no real memory allocated
      // to it.
      childDirectory = pageDirList[numberPageDirectories++];

      // Clear the child directory.
      kernelMemClear((void *) childDirectory, sizeof(kernelPageDirectory));

      // Set the process Id of the child's directory
      childDirectory->processId = childId;

      // Note that the parent directory is referenced and child directory
      // is shared.
      parentDirectory->numberShares += 1;
      childDirectory->parent = parentDirectory->processId;
    }

  // Return the physical address of the shared page directory.
  return ((void *) parentDirectory->physical);
}


int kernelPageDeleteDirectory(int processId)
{
  // This will delete a page directory and all of its assoctiated (unshared)
  // page tables.

  int status = 0;
  kernelPageDirectory *dir = NULL;
  kernelPageTable *listItemPointer = NULL;
  kernelPageTable *previous = NULL;
  int count;

  
  // Have we been initialized?
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Find the page directory belonging to the process.  We can't use the
  // findPageDirectory() function to find it since THAT will always
  // return the PARENT page directory if a page directory is shared.  We
  // don't want that to happen, so we'll search through the list manually.
  for (count = 0; count < numberPageDirectories; count ++)
    if (pageDirList[count]->processId == processId)
      {
	dir = pageDirList[count];
	break;
      }

  if (dir == NULL)
    return (status = ERR_NOSUCHENTRY);

  // Ok, found it.  We need to walk backwards through all of its page
  // tables, deallocating them as we go.
  if (dir->firstTable != NULL)
    {
      listItemPointer = findPageTable(dir, dir->lastPageTableNumber);

      if (listItemPointer == NULL)
	return (status = ERR_NOSUCHENTRY);

      while(listItemPointer != NULL)
	{
	  previous = (kernelPageTable *) listItemPointer->previous;

	  status = deletePageTable(listItemPointer);
	  
	  if (status < 0)
	    return (status);

	  listItemPointer = previous;
	}
    }

  dir->firstTable = NULL;

  // Delete the directory.
  status = deletePageDirectory(dir);
  
  if (status < 0)
    return (status);

  // Return success
  return (status = 0);
}


int kernelPageMapToFree(int processId, void *physicalAddress, 
		  void **virtualAddress, unsigned int size)
{
  // This is a publicly accessible wrapper function for the map() function.
  // This one is used to add mapped pages into an address space.  Parameter
  // checking is done inside the map() function, not here.

  int status = 0;
  kernelPageDirectory *directory = NULL;
  void *mappedAddress = NULL;


  // Have we been initialized?
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Find the appropriate page directory
  directory = findPageDirectory(processId);

  if (directory == NULL)
    return (status = ERR_NOSUCHENTRY);

  status = map(directory, &physicalAddress, &mappedAddress, size, 1 /*add*/);

  if (status < 0)
    return (status);

  *virtualAddress = mappedAddress;

  // Return success
  return (status = 0);
}


int kernelPageUnmap(int processId, void **physicalAddress, 
		    void *virtualAddress, unsigned int size)
{
  // This is a publicly accessible wrapper function for the map() function.
  // This one is used to remove mapped pages from an address space.  Parameter
  // checking is done inside the map() function, not here.

  int status = 0;
  kernelPageDirectory *directory = NULL;
  void *unmappedAddress = NULL;


  // Have we been initialized?
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Find the appropriate page directory
  directory = findPageDirectory(processId);

  if (directory == NULL)
    return (status);

  status = map(directory, &unmappedAddress, &virtualAddress, size, 0 /*del*/);

  if (status < 0)
    return (status);

  *physicalAddress = unmappedAddress;

  // Return success
  return (status = 0);
}


void *kernelPageGetPhysical(int processId, void *virtualAddress)
{
  // This function is mostly a wrapper function for the getPhysical
  // function defined above.

  kernelPageDirectory *directory = NULL;


  // Have we been initialized?
  if (!initialized)
    return (NULL);
  
  // Find the appropriate page directory
  directory = findPageDirectory(processId);

  if (directory == NULL)
    return (NULL);

  return (getPhysical(directory, virtualAddress));
}
