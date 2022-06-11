//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  nm.c
//

// This is the UNIX-style command for reporting information about a shared
// library

/* This is the text that appears when a user requests help about this program
<help>

 -- nm --

Show information about a dynamic library file.

Usage:
  nm <file1> [file2] [...]

This command is useful to software developers.  For each name listed after
the command, representing a shared library file (usually ending with a .so
extension) or dynamically-linked executable, nm will print a catalogue of
information about its symbols.  Data symbols, functions, sections, and other
items are shown, along with their bindings (such as 'local', 'global', or
'weak').

</help>
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/vsh.h>


static void usage(char *name)
{
  printf("usage:\n%s <file1> [file2] [...]\n", name);
  return;
}


int main(int argc, char *argv[])
{
  void *fileData = NULL;
  file theFile;
  loaderFileClass class;
  loaderSymbolTable *symTable = NULL;
  int count1, count2;

  static char *bindings[] = {
    "local", "global", "weak"
  };
  static char *types[] = {
    "none", "object", "function", "section", "file"
  };

  // Need at least one argument
  if (argc < 2)
    {
      usage(argv[0]);
      return (errno = ERR_ARGUMENTCOUNT);
    }

  errno = 0;

  for (count1 = 1; count1 < argc; count1 ++)
    {
      // Initialize the file and file class structures
      bzero(&theFile, sizeof(file));
      bzero(&class, sizeof(loaderFileClass));

      // Load the file
      fileData = loaderLoad(argv[count1], &theFile);
      if (fileData == NULL)
	{
	  errno = ERR_NODATA;
	  printf("Can't load file \"%s\"\n", argv[count1]);
	  continue;
	}

      // Make sure it's a dynamic library or executable
      if (loaderClassify(argv[count1], fileData, theFile.size, &class) == NULL)
	{
	  printf("File type of \"%s\" is unknown\n", argv[count1]);
	  continue;
	}
      if (!(class.class & (LOADERFILECLASS_EXEC | LOADERFILECLASS_LIB)) ||
	  !(class.subClass & LOADERFILESUBCLASS_DYNAMIC))
	{
	  errno = ERR_INVALID;
	  printf("\"%s\" is not a dynamic library or executable\n",
		 argv[count1]);
	  continue;
	}

      // Free the file data now.  We want the symbol information.
      memoryRelease(fileData);
      fileData = NULL;

      // Get the symbol table
      symTable = loaderGetSymbols(argv[count1]);
      if (symTable == NULL)
	{
	  printf("Unable to get dynamic symbols from \"%s\".\n", argv[count1]);
	  continue;
	}

      for (count2 = 0; count2 < symTable->numSymbols; count2 ++)
	{
	  if (symTable->symbols[count2].name[0])
	    printf("%08x  %s  %s,%s\n", (unsigned)
		   symTable->symbols[count2].value,
		   symTable->symbols[count2].name,
		   bindings[symTable->symbols[count2].binding],
		   types[symTable->symbols[count2].type]);
	}
    }

  return (errno);
}
