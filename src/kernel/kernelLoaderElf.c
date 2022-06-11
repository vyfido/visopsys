//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  kernelLoaderElf.c
//
	
// This file contains loader functions for dealing with ELF format executables
// and object files.

#include "kernelLoader.h"
#include "kernelLoaderElf.h"
#include "kernelMalloc.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelPageManager.h"
#include "kernelParameters.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>
#include <stdio.h>


static Elf32SectionHeader *getSectionHeader(void *data, const char *name)
{
  // Look up an ELF section header by name and return the pointer to it.
  Elf32Header *header = data;
  Elf32SectionHeader *sectionHeaders = NULL;
  Elf32SectionHeader *headerStringsHeader = NULL;
  Elf32SectionHeader *returnHeader = NULL;
  int count;

  if (!header->e_shoff || !header->e_shstrndx)
    // No section headers
    return (returnHeader = NULL);

  // Store a pointer to the start of the section headers
  sectionHeaders = (Elf32SectionHeader *) ((void *) data + header->e_shoff);

  // Store a pointer to the header for the 'header strings' section
  headerStringsHeader = &(sectionHeaders[header->e_shstrndx]);

  for (count = 1; count < header->e_shnum; count ++)
    {
      if (!strcmp((data + headerStringsHeader->sh_offset +
		   sectionHeaders[count].sh_name), name))
	{
	  returnHeader = &sectionHeaders[count];
	  break;
	}
    }

  return (returnHeader);
}


static int detect(const char *fileName, void *dataPtr, int size,
		  loaderFileClass *class)
{
  // This function returns 1 and fills the fileClass structure if the data
  // points to an ELF file.

  unsigned *magic = dataPtr;
  Elf32Header *header = dataPtr;
  Elf32SectionHeader *sectionHeaders = NULL;
  int count;

  if ((fileName == NULL) || (dataPtr == NULL) || !size || (class == NULL))
    return (0);

  // Look for the ELF magic number (0x7F + E + L + F)
  if (*magic == 0x464C457F)
    {
      // This is an ELF file.
      sprintf(class->className, "%s %s ", FILECLASS_NAME_ELF,
	      FILECLASS_NAME_BIN);
      class->flags = LOADERFILECLASS_BIN;

      // Is it an executable, object file, shared library, or core?
      switch (header->e_type)
	{
	case ELFTYPE_RELOC:
	  strcat(class->className, FILECLASS_NAME_OBJ);
	  class->flags |= LOADERFILECLASS_OBJ;
	  break;
	case ELFTYPE_EXEC:
	  sectionHeaders =
	    (Elf32SectionHeader *) ((void *) dataPtr + header->e_shoff);
	  for (count = 1; count < header->e_shnum; count ++)
	    if (sectionHeaders[count].sh_type == ELFSHT_DYNAMIC)
	      {
		strcat(class->className, FILECLASS_NAME_DYNAMIC " ");
		class->flags |= LOADERFILECLASS_DYNAMIC;
		break;
	      }
	  strcat(class->className, FILECLASS_NAME_EXEC);
	  class->flags |= LOADERFILECLASS_EXEC;
	  break;
	case ELFTYPE_SHARED:
	  strcat(class->className,
		 FILECLASS_NAME_DYNAMIC " " FILECLASS_NAME_LIB);
	  class->flags |= (LOADERFILECLASS_DYNAMIC | LOADERFILECLASS_LIB);
	  break;
	case ELFTYPE_CORE:
	  strcat(class->className, FILECLASS_NAME_CORE);
	  class->flags |= LOADERFILECLASS_DATA;
	  break;
	}

      return (1);
    }
  else
    return (0);
}


static loaderSymbolTable *getSymbols(void *data, int dynamic, int kernel)
{
  // Returns the symbol table of the file, dynamic or static symbols.

  Elf32Header *header = data;
  Elf32SectionHeader *sectionHeaders = NULL;
  Elf32SectionHeader *symbolTableHeader = NULL;
  Elf32SectionHeader *stringTableHeader = NULL;
  Elf32Symbol *symbols = NULL;
  int numSymbols = 0;
  loaderSymbolTable *symTable = NULL;
  int symTableSize = 0;
  void *symTableData = NULL;
  int count;

  if (!header->e_shoff)
    // So section headers, so no symbols
    return (symTable = NULL);

  // Store a pointer to the start of the section headers
  sectionHeaders = (Elf32SectionHeader *) (data + header->e_shoff);

  // Get the symbol and string tables
  if (dynamic)
    {
      // Symbol table
      symbolTableHeader = getSectionHeader(data, ".dynsym");
      // String table
      stringTableHeader = getSectionHeader(data, ".dynstr");
    }
  else
    {
      // Symbol table
      symbolTableHeader = getSectionHeader(data, ".symtab");
      // String table
      stringTableHeader = getSectionHeader(data, ".strtab");
    }

  if (!symbolTableHeader || !stringTableHeader)
    {
      // No symbols or no strings
      kernelError(kernel_error, "%s symbols or strings missing",
		  (dynamic? "Dynamic" : "Static")); 
      return (symTable = NULL);
    }

  symbols = (data + symbolTableHeader->sh_offset);
  numSymbols = (symbolTableHeader->sh_size / (int) sizeof(Elf32Symbol));
  symTableSize =
    (sizeof(loaderSymbolTable) + (numSymbols * sizeof(loaderSymbol)) +
     stringTableHeader->sh_size);

  // Get memory for the symbol table
  if (kernel)
    symTable = kernelMalloc(symTableSize);
  else
    symTable = kernelMemoryGet(symTableSize, "symbol table");
  if (symTable == NULL)
    return (symTable = NULL);

  // Set up the structure
  symTable->numSymbols = (numSymbols - 1);
  symTable->tableSize = symTableSize;
  symTableData = (void *) ((unsigned) symTable + sizeof(loaderSymbolTable) +
			   (numSymbols * sizeof(loaderSymbol)));

  // Copy the string table data
  kernelMemCopy((data + stringTableHeader->sh_offset), symTableData,
		stringTableHeader->sh_size);

  // Fill out the symbol array
  for (count = 1; count < numSymbols; count ++)
    {
      symTable->symbols[count - 1].name =
	(char *) ((int) symTableData + symbols[count].st_name);
      symTable->symbols[count - 1].defined = symbols[count].st_shndx;
      symTable->symbols[count - 1].value = symbols[count].st_value;
      symTable->symbols[count - 1].size = (unsigned) symbols[count].st_size;

      if (ELF32_ST_BIND(symbols[count].st_info) == ELFSTB_LOCAL)
	symTable->symbols[count - 1].binding = LOADERSYMBOLBIND_LOCAL;
      else if (ELF32_ST_BIND(symbols[count].st_info) == ELFSTB_GLOBAL)
	symTable->symbols[count - 1].binding = LOADERSYMBOLBIND_GLOBAL;
      else if (ELF32_ST_BIND(symbols[count].st_info) == ELFSTB_WEAK)
	symTable->symbols[count - 1].binding = LOADERSYMBOLBIND_WEAK;

      if (ELF32_ST_TYPE(symbols[count].st_info) == ELFSTT_NOTYPE)
	symTable->symbols[count - 1].type = LOADERSYMBOLTYPE_NONE;
      else if (ELF32_ST_TYPE(symbols[count].st_info) == ELFSTT_OBJECT)
	symTable->symbols[count - 1].type = LOADERSYMBOLTYPE_OBJECT;
      else if (ELF32_ST_TYPE(symbols[count].st_info) == ELFSTT_FUNC)
	symTable->symbols[count - 1].type = LOADERSYMBOLTYPE_FUNC;
      else if (ELF32_ST_TYPE(symbols[count].st_info) == ELFSTT_SECTION)
	symTable->symbols[count - 1].type = LOADERSYMBOLTYPE_SECTION;
      else if (ELF32_ST_TYPE(symbols[count].st_info) == ELFSTT_FILE)
	symTable->symbols[count - 1].type = LOADERSYMBOLTYPE_FILE;      
    }

  return (symTable);
}


static int layoutCodeAndData(void *loadAddress, processImage *execImage,
			     int kernel)
{
  // Given ELF executable or library file data, lay out the code and data
  // along the correct alignments.

  int status = 0;
  Elf32Header *header = (Elf32Header *) loadAddress;
  Elf32ProgramHeader *programHeader = NULL;
  int loadSegments = 0;
  unsigned imageSize = 0;
  void *imageMemory = NULL;
  static char *memoryDesc = "elf executable image";
  int count;

  execImage->entryPoint = header->e_entry;

  // Get the address of the program header
  programHeader = (Elf32ProgramHeader *) (loadAddress + header->e_phoff);

  for (count = 0; count < header->e_phnum; count ++)
    {
      if (programHeader[count].p_type == ELFPT_LOAD)
	{
	  // Code segment?
	  if (programHeader[count].p_flags & ELFPF_X)
	    {
	      // Make sure that any code segment size in the file is the same
	      // as the size in memory
	      if (programHeader[count].p_filesz !=
		  programHeader[count].p_memsz)
		{
		  kernelError(kernel_error, "Invalid ELF image (code file "
			      "size %d is not equal to code memory size %d)",
			      programHeader[count].p_filesz,
			      programHeader[count].p_memsz);
		  return (status = ERR_INVALID);
		}

	      execImage->virtualAddress = programHeader[count].p_vaddr;
	    }

	  // Check the alignment.  Must be the same as our page size
	  if (programHeader[count].p_align &&
	      (programHeader[count].p_align != MEMORY_PAGE_SIZE))
	    {
	      kernelError(kernel_error, "Illegal ELF program segment "
			  "alignment (%d != %d)", programHeader[count].p_align,
			  MEMORY_PAGE_SIZE);
	      return (status = ERR_INVALID);
	    }

	  // Add this program segment's memory size (rounded up to
	  // MEMORY_PAGE_SIZE) to our image size
	  imageSize += kernelPageRoundUp(programHeader[count].p_memsz);
	  loadSegments += 1;
	}
    }

  // Make sure there are 2 program header entries; 1 for code and 1 for data,
  // since this code is not sophisticated enough to handle other possibilities.
  if (loadSegments != 2)
    kernelError(kernel_warn, "Unexpected number of loadable ELF program "
		"header entries (%d)", loadSegments);

  // Get kernel or user memory based on the flag
  if (kernel)
    imageMemory = kernelMemoryGetSystem(imageSize, memoryDesc);
  else
    imageMemory = kernelMemoryGet(imageSize, memoryDesc);
  if (imageMemory == NULL)
    {
      kernelError(kernel_error, "Error getting memory for ELF image");
      return (status = ERR_MEMORY);
    }
  
  // Do layout for loadable program segments; the code and data segments
  for (count = 0; count < header->e_phnum; count ++)
    {
      if (programHeader[count].p_type == ELFPT_LOAD)
	{
	  void *srcAddr = (loadAddress + programHeader[count].p_offset);
	  void *destAddr = (imageMemory + (programHeader[count].p_vaddr -
					   execImage->virtualAddress));
	  kernelMemCopy(srcAddr, destAddr, programHeader[count].p_filesz);

	  // Code segment?
	  if (programHeader[count].p_flags & ELFPF_X)
	    {
	      execImage->code = imageMemory;
	      execImage->codeSize = programHeader[count].p_memsz;
	    }
	  else
	    {
	      // Data segment
	      execImage->data = destAddr;
	      execImage->dataSize = programHeader[count].p_memsz;
	    }
	}
    }

  // Set the rest of the info in the 'process image' structure
  execImage->imageSize = imageSize;

  // Success
  return (status = 0);
}


static int layoutExecutable(void *loadAddress, processImage *execImage)
{
  // This function is for preparing an ELF executable image to run.

  int status = 0;

  // We will assume that this function is not called unless the loader is
  // already sure that this file is both ELF and an executable.  Thus, we
  // will not check the magic number stuff at the head of the file.

  status =
    layoutCodeAndData(loadAddress, execImage, 0 /* not kernel memory */);
  if (status < 0)
    return (status);

  // Success
  return (status = 0);
}


kernelFileClass elfFileClass = {
  FILECLASS_NAME_ELF,
  &detect,
  { }
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelFileClass *kernelFileClassElf(void)
{
  // The loader will call this function so that we can return a structure
  // for managing ELF files

  static int filled = 0;

  if (!filled)
    {
      elfFileClass.executable.getSymbols = getSymbols;
      elfFileClass.executable.layoutExecutable = layoutExecutable;
      filled = 1;
    }

  return (&elfFileClass);
}
