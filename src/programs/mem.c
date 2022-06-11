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
//  mem.c
//

// This is the DOS-style command for viewing memory usage statistics

#include <string.h>
#include <sys/api.h>


int main(int argc, char *argv[])
{
  // This command will prompt the memory manager to dump a list of memory
  // allocations.  This is a temporary hack until the system has an api for
  // allowing a user process to access the data by itself.  In other words,
  // the kernel should not print this stuff on the screen by itself.

  int kernelMem = 0;

  if ((argc > 1) && !strcmp(argv[1], "-k"))
    kernelMem = 1;

  // Print memory usage information
  memoryPrintUsage(kernelMem);

  return (0);
}
