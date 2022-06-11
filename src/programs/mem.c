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
//  mem.c
//

// This is the DOS-style command for viewing memory usage statistics

/* This is the text that appears when a user requests help about this program
<help>

 -- mem --

A command to display current memory utilization

Usage:
  mem [-k]

This command prints a listing of memory allocations, plus a summary at the
end.  If the (optional) '-k' parameter is supplied, then 'mem' will display
system (kernel) memory usage instead.

Options:
-k  : Show kernel memory usage

</help>
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/memory.h>


int main(int argc, char *argv[])
{
  // This command will dump lists of memory allocations and usage statistics

  int status = 0;
  int kernelMem = 0;
  memoryStats stats;
  memoryBlock *blocksArray = NULL;
  unsigned totalFree = 0;
  unsigned percentUsed = 0;
  unsigned count;

  // Want kernel memory stats?
  if (getopt(argc, argv, "k") == 'k')
    kernelMem = 1;

  // Get overall statistics so we know how large our blocks array needs
  // to be
  bzero(&stats, sizeof(memoryStats));
  status = memoryGetStats(&stats, kernelMem);
  if (status < 0)
    {
      errno = status;
      perror(argv[0]);
      return (status);
    }

  // Get memory for the blocks memory
  blocksArray = malloc(stats.usedBlocks * sizeof(memoryBlock));
  if (blocksArray == NULL)
    {
      errno = ERR_MEMORY;
      perror(argv[0]);
      return (errno);
    }

  // Get memory blocks information
  status =
    memoryGetBlocks(blocksArray, (stats.usedBlocks * sizeof(memoryBlock)),
		    kernelMem);
  if (status >= 0)
    {
      printf(" --- %s usage information by block ---\n",
	     (kernelMem? "Kernel heap" : "Memory"));
      for (count = 0; count < stats.usedBlocks; count ++)
	{
	  printf(" proc=%d", blocksArray[count].processId);
	  textSetColumn(10);
	  printf("%u->%u (size %u)", blocksArray[count].startLocation,
		 blocksArray[count].endLocation,
		 (blocksArray[count].endLocation - 
		  blocksArray[count].startLocation + 1));
	  textTab();
	  printf("%s\n", blocksArray[count].description);
	}
    }

  // Print memory usage information

  if (stats.totalMemory != 0)
    {
      // Switch raw bytes numbers to kilobytes.  This will also prevent
      // overflow when we calculate percentage, below.
      stats.totalMemory >>= 10;
      stats.usedMemory >>= 10;

      totalFree = (stats.totalMemory - stats.usedMemory);
      percentUsed = ((stats.usedMemory * 100) / stats.totalMemory);

      // Print out the percent usage information
      printf(" --- Usage totals ---\nUsed blocks: %d\nUsed memory: "
	     "%u Kb - %d%%\nFree memory: %u Kb - %d%%\n", stats.usedBlocks,
	     stats.usedMemory, percentUsed, totalFree, (100 - percentUsed));
    }

  return (status = 0);
}
