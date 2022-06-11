//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
#include "kernelMemory.h"
#include "kernelMultitasker.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelMisc.h"
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


static Elf32SectionHeader *getSectionHeaderByNumber(void *data, int number)
{
  // Look up an ELF section header by number and return the pointer to it.
  Elf32Header *header = data;
  Elf32SectionHeader *sectionHeaders = NULL;

  if (!header->e_shoff)
    // No section headers
    return (NULL);

  // Store a pointer to the start of the section headers
  sectionHeaders = (Elf32SectionHeader *) (data + header->e_shoff);

  return (&sectionHeaders[number]);
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


static int getLibraryDependencies(void *loadAddress, elfLibraryArray *array)
{
  // Look in the dynamic section and allocate an elfLibraryArray with copies
  // of the libraries which are dependencies

  int status = 0;
  Elf32SectionHeader *dynamicHeader = NULL;
  Elf32SectionHeader *stringHeader = NULL;
  Elf32Dyn *dynArray = NULL;
  int numLibraries = 0;
  char *string = NULL;
  kernelDynamicLibrary *library = NULL;
  int count;

  // Get the section header for the 'dynamic' section
  dynamicHeader = getSectionHeader(loadAddress, ".dynamic");
  if (dynamicHeader == NULL)
    {
      kernelError(kernel_error, "ELF image has no dynamic linking section");
      return (status = ERR_INVALID);
    }

  // The string table header used by the 'dynamic' section
  stringHeader = getSectionHeaderByNumber(loadAddress, dynamicHeader->sh_link);
  if (stringHeader == NULL)
    {
      kernelError(kernel_error, "Can't find ELF image dynamic string header");
      return (status = ERR_INVALID);
    }

  dynArray = (Elf32Dyn *) (loadAddress + dynamicHeader->sh_offset);

  // Loop through the 'dynamic' entries, and count up the number and length
  // of 'needed' entries
  array->numLibraries = 0;
  array->libraries = NULL;
  for (count = 0; (dynArray[count].d_tag != 0); count ++)
    {
      if (dynArray[count].d_tag == ELFDT_NEEDED)
	{
	  string = (loadAddress + stringHeader->sh_offset +
		    dynArray[count].d_un.d_val);
	  numLibraries += 1;
	}
    }

  // If no dependencies, stop here
  if (numLibraries == 0)
    return (status = 0);

  // Get the memory
  array->libraries = kernelMalloc(numLibraries * sizeof(kernelDynamicLibrary));
  if (array->libraries == NULL)
    return (status = ERR_MEMORY);

  // Go through the headers again, and make copies of all the needed
  // library structures.
  for (count = 0; (dynArray[count].d_tag != 0) ; count ++)
    {
      if (dynArray[count].d_tag == ELFDT_NEEDED)
	{
	  string = (loadAddress + stringHeader->sh_offset +
		    dynArray[count].d_un.d_val);

	  library = kernelLoaderGetLibrary(string);
	  if (library == NULL)
	    return (status = ERR_NOTINITIALIZED);

	  kernelMemCopy(library, &(array->libraries[array->numLibraries]),
			sizeof(kernelDynamicLibrary));
	  array->numLibraries += 1;
	}
    }

  return (status = 0);
}


static loaderSymbol *getSymbol(const char *name, loaderSymbolTable *symbols)
{
  // Returns a pointer to a symbol if it exists in the table and is defined
  // there
  
  loaderSymbol *symbol = NULL;
  int count;

  for (count = 0; count < symbols->numSymbols; count ++)
    {
      if (!strcmp(symbols->symbols[count].name, name))
	{
	  symbol = &(symbols->symbols[count]);
	  break;
	}
    }

  return (symbol);
}


static int resolveLibrarySymbols(loaderSymbolTable **symbolTable,
				 kernelDynamicLibrary *library)
{
  // Given 2 symbol tables, replace the first one with a version that
  // combines the 2, with any resolveable symbols resolved and table2
  // symbols adjusted by the value 'relocAddress'

  int status = 0;
  int newTableSize = 0;
  loaderSymbolTable *newTable = NULL;
  char *newTableData = NULL;
  loaderSymbol *symbol = NULL;
  loaderSymbol *newSymbol = NULL;
  int count;

  // First get memory for the new combined table
  newTableSize = ((*symbolTable)->tableSize + library->symbolTable->tableSize);
  newTable = kernelMalloc(newTableSize);
  if (newTable == NULL)
    return (status = ERR_MEMORY);

  newTable->tableSize = newTableSize;
  newTableData = (void *)
    (((unsigned) newTable + sizeof(loaderSymbolTable)) +
     (((*symbolTable)->numSymbols + library->symbolTable->numSymbols) *
      sizeof(loaderSymbol)));

  // Copy over the symbols of the first table
  for (count = 0; count < (*symbolTable)->numSymbols; count ++)
    {
      symbol = &((*symbolTable)->symbols[count]);
      if (!symbol->name[0])
	continue;

      newSymbol = &(newTable->symbols[newTable->numSymbols]);
      kernelMemCopy(symbol, newSymbol, sizeof(loaderSymbol));
      strcpy(newTableData, newSymbol->name);
      newSymbol->name = newTableData;
      newTableData += (strlen(newSymbol->name) + 1);
      newTable->numSymbols += 1;
    }

  // Loop through the symbols of the library.  If a symbol is undefined
  // in the new table and defined in the library, define it.  If a symbol
  // doesn't exist in the new table and defined in the library, add it.
  for (count = 0; count < library->symbolTable->numSymbols; count ++)
    {
      symbol = &(library->symbolTable->symbols[count]);

      // Skip undefined symbols, and local ones we're not exporting from
      // this library
      if (!symbol->name[0] || !symbol->defined ||
	  (symbol->binding != LOADERSYMBOLBIND_GLOBAL))
	continue;

      // Get any symbol entry with the same name from the new table
      newSymbol = getSymbol(symbol->name, newTable);

      if (newSymbol)
	{
	  if (!newSymbol->defined)
	    {
	      newSymbol->defined = symbol->defined;
	      newSymbol->value =
		(symbol->value + (unsigned) library->codeVirtual);
	      newSymbol->size = symbol->size;
	      newSymbol->binding = symbol->binding;
	      newSymbol->type = symbol->type;
	    }
	}

      else
	{
	  // Put the symbol in the new table
	  newSymbol = &(newTable->symbols[newTable->numSymbols]);
	  kernelMemCopy(symbol, newSymbol, sizeof(loaderSymbol));
	  strcpy(newTableData, newSymbol->name);
	  newSymbol->name = newTableData;
	  newSymbol->value += (unsigned) library->codeVirtual;
	  newTableData += (strlen(newSymbol->name) + 1);
	  newTable->numSymbols += 1;
	}
    }

  // Deallocate the first table, and assign the new one to the pointer
  kernelFree(*symbolTable);
  *symbolTable = newTable;

  return (status = 0);
}


static kernelRelocationTable *getRelocations(void *loadAddress,
					     loaderSymbolTable *symbols,
					     void *baseAddress)
{
  // Returns a table of generic kernelRelocation entries (used when filling
  // out a kernelDynamicLibrary structure, for example)

  kernelRelocationTable *table = NULL;
  Elf32SectionHeader *dynamicHeader = NULL;
  Elf32SectionHeader *stringHeader = NULL;
  int numTotalRelocs = 0;
  int tableSize = 0;
  Elf32Symbol *symArray = NULL;
  Elf32Rel *relArray = NULL;
  int count1, count2, count3;

  #define RELOC_SECTIONS 2

  // To optimize for doing RELOC_SECTIONS relocation sections, we do it in a
  // little loop using this structure.
  struct {
    const char *sectionName;
    Elf32SectionHeader *relocHeader;
    Elf32SectionHeader *symbolHeader;
    int numRelocs;
  } relocSection[RELOC_SECTIONS];

  // Get all the section headers we need

  // The 'dynamic' section header
  dynamicHeader = getSectionHeader(loadAddress, ".dynamic");
  if (dynamicHeader == NULL)
    {
      kernelError(kernel_error, "ELF image has no dynamic linking section");
      return (table = NULL);
    }

  // The string table header used by the 'dynamic' section
  stringHeader = getSectionHeaderByNumber(loadAddress, dynamicHeader->sh_link);
  if (stringHeader == NULL)
    {
      kernelError(kernel_error, "Can't find ELF dynamic string header");
      return (table = NULL);
    }

  // The names of the RELOC_SECTIONS relocation sections we're doing
  relocSection[0].sectionName = ".rel.dyn";
  relocSection[1].sectionName = ".rel.plt";

  // Get the section headers and count the number of relocations in each
  for (count1 = 0; count1 < RELOC_SECTIONS; count1 ++)
    {
      // The dynamic-linking relocations section header
      relocSection[count1].relocHeader =
	getSectionHeader(loadAddress, relocSection[count1].sectionName);
      if (relocSection[count1].relocHeader == NULL)
	continue;

      // The symbols header for this relocation section
      relocSection[count1].symbolHeader =
	getSectionHeaderByNumber(loadAddress,
				 relocSection[count1].relocHeader->sh_link);
      if (relocSection[count1].symbolHeader == NULL)
	{
	  kernelError(kernel_error, "Can't find ELF %s section symbols "
		      "header", relocSection[count1].sectionName);
	  return (table = NULL);
	}

      relocSection[count1].numRelocs =
	(relocSection[count1].relocHeader->sh_size / (int) sizeof(Elf32Rel));

      numTotalRelocs += relocSection[count1].numRelocs;
    }

  // Allocate memory for the relocation array

  tableSize = (sizeof(kernelRelocationTable) +
	       (numTotalRelocs * sizeof(kernelRelocation)));

  table = kernelMalloc(tableSize);
  if (table == NULL)
    return (table);

  table->tableSize = tableSize;

  // Now get the relocations for each section
  for (count1 = 0; count1 < RELOC_SECTIONS; count1 ++)
    {
      if (relocSection[count1].relocHeader == NULL)
	continue;

      relArray = (Elf32Rel *)
	(loadAddress + relocSection[count1].relocHeader->sh_offset);

      symArray = (Elf32Symbol *)
	(loadAddress + relocSection[count1].symbolHeader->sh_offset);
      
      for (count2 = 0; count2 < relocSection[count1].numRelocs; count2 ++)
	{
	  table->relocations[table->numRelocs].offset =
	    (void *) (relArray[count2].r_offset - (unsigned) baseAddress);
	  table->relocations[table->numRelocs].symbolName = NULL;
	  table->relocations[table->numRelocs].info =
	    (int) relArray[count2].r_info;

	  // Is there a symbol associated with this relocation?
	  if (ELF32_R_SYM(relArray[count2].r_info))
	    {
	      Elf32Symbol *sym =
		&symArray[ELF32_R_SYM(relArray[count2].r_info)];

	      char *symName =
		(loadAddress + stringHeader->sh_offset + sym->st_name);

	      // Find the symbol in our symbol table
	      for (count3 = 0; count3 < symbols->numSymbols; count3 ++)
		if (!strcmp(symbols->symbols[count3].name, symName))
		  {
		    table->relocations[table->numRelocs].symbolName =
		      symbols->symbols[count3].name;
		    break;
		  }
	      
	      if (table->relocations[table->numRelocs].symbolName == NULL)
		{
		  kernelError(kernel_error, "Unrecognized symbol name %s in "
			      "ELF image", symName);
		  kernelFree(table);
		  return (table = NULL);
		}
	    }

	  table->relocations[table->numRelocs].addend = 0;
	  table->numRelocs += 1;
	}
    }

  return (table);
}


static int doRelocations(void *dataAddress, void *codeVirtualAddress,
			 void *dataVirtualAddress,
			 loaderSymbolTable *globalSymTable,
			 kernelRelocationTable *relocTable,
			 elfLibraryArray *libArray)
{
  // Given the data address and virtual address it will be loaded, and the
  // symbol and relocation tables, do the relocations, baby.

  int status = 0;
  int dataOffset = (dataVirtualAddress - codeVirtualAddress);
  int *relocation = NULL;
  int type = 0;
  loaderSymbol *symbol = NULL;
  loaderSymbol *copySymbol = NULL;
  int count1, count2;

  // Loop for each relocation
  for (count1 = 0; count1 < relocTable->numRelocs; count1 ++)
    {
      // Get the address of the relocation
      relocation = (dataAddress + (unsigned)
		    (relocTable->relocations[count1].offset - dataOffset));

      // Perform the relocation.  The calculation depends upon the type of
      // relocation

      type = (int) ELF32_R_TYPE(relocTable->relocations[count1].info);
      if (relocTable->relocations[count1].symbolName)
	{
	  symbol = getSymbol(relocTable->relocations[count1].symbolName,
			     globalSymTable);
	  if (symbol == NULL)
	    {
	      kernelError(kernel_error, "Symbol %s not found",
			  relocTable->relocations[count1].symbolName);
	      return (status = ERR_NOSUCHENTRY);
	    }
	  if (!symbol->defined)
	    {
	      kernelError(kernel_error, "Undefined symbol %s", symbol->name);
	      return (status = ERR_NOSUCHENTRY);
	    }
	}

      switch (type)
	{
	case ELFR_386_32:
	  // A + S: Add the value of the symbol
	  *relocation += (int) symbol->value;
	  break;

	case ELFR_386_COPY:
	  // [S]: Copy the data value at the symbol address.  Look for the
	  // library that has this symbol defined
	  for (count2 = 0; count2 < libArray->numLibraries; count2 ++)
	    {
	      copySymbol = getSymbol(symbol->name,
				     libArray->libraries[count2].symbolTable);
	      if (copySymbol && copySymbol->defined)
		break;
	    }
	  if (copySymbol && copySymbol->defined)
	    {
	      *relocation =
	      	*((int *)((unsigned) libArray->libraries[count2].code +
	      		  (unsigned) copySymbol->value));
	    }
	  else
	    kernelError(kernel_warn, "Relocation %s type ELFR_386_COPY not "
			"initialized", symbol->name);
	  break;

	case ELFR_386_GLOB_DAT:
	case ELFR_386_JMP_SLOT:
	  // S: The value of the symbol
	  *relocation = (int) symbol->value;
	  break;

	case ELFR_386_RELATIVE:
	  // A + B: Add the base address
	  *relocation += (int) codeVirtualAddress;
	  break;

	default:
	  if (symbol)
	    kernelError(kernel_error, "Unsupported relocation type %d for "
			"symbol %s", type, symbol->name);
	  else
	    kernelError(kernel_error, "Unsupported relocation type %d", type);
	  return (status = ERR_NOTIMPLEMENTED);
	}
    }

  return (status = 0);
}


static int layoutLibrary(void *loadAddress, kernelDynamicLibrary *library)
{
  // This function is for preparing an ELF shared library for dynamic linking

  int status = 0;
  Elf32SectionHeader *dynamicHeader = NULL;
  Elf32SectionHeader *stringHeader = NULL;
  Elf32Dyn *dynArray = NULL;
  int numDynamic = 0;
  processImage libImage;
  int count;

  // We will assume we this function is not called unless the loader is
  // already sure that this file is both ELF and a shared library.  Thus, we
  // will not check the magic number stuff at the head of the file.

  kernelMemClear(&libImage, sizeof(processImage));

  // Get the section header for the 'dynamic' section
  dynamicHeader = getSectionHeader(loadAddress, ".dynamic");
  if (dynamicHeader == NULL)
    {
      kernelError(kernel_error, "Library has no dynamic linking section");
      return (status = ERR_INVALID);
    }

  // The string table header used by the 'dynamic' section
  stringHeader = getSectionHeaderByNumber(loadAddress, dynamicHeader->sh_link);
  if (stringHeader == NULL)
    {
      kernelError(kernel_error, "Can't find library dynamic string header");
      return (status = ERR_INVALID);
    }

  dynArray = (Elf32Dyn *) (loadAddress + dynamicHeader->sh_offset);

  // Loop through the 'dynamic' entries
  for (count = 0; (dynArray[count].d_tag != 0); count ++)
    {
      // Does the library need another library?
      if (dynArray[count].d_tag == ELFDT_NEEDED)
	{
	  kernelError(kernel_error, "Library %s needs library %s",
		      library->name,
		      (char *) (loadAddress + stringHeader->sh_offset +
				dynArray[count].d_un.d_val));
	  return (status = ERR_NOTIMPLEMENTED);
	}

      // Is there a library name stored here?
      if (dynArray[count].d_tag == ELFDT_SONAME)
	{
	  strncpy(library->name, (loadAddress + stringHeader->sh_offset +
				  dynArray[count].d_un.d_val),
		  MAX_NAME_LENGTH);
	}

      if (dynArray[count].d_tag == ELFDT_PLTREL)
	{
	  if (dynArray[count].d_un.d_val != ELFDT_REL)
	    {
	      kernelError(kernel_error, "PLT relocations need explicit "
			  "addends (not supported)");
	      return (status = ERR_NOTIMPLEMENTED);
	    }
	}

      numDynamic += 1;
    }

  status = layoutCodeAndData(loadAddress, &libImage, 1 /* kernel memory */);
  if (status < 0)
    return (status);

  library->code = libImage.code;
  library->codeVirtual = libImage.virtualAddress;
  library->codePhysical = kernelPageGetPhysical(KERNELPROCID, library->code);
  library->codeSize = libImage.codeSize;
  library->data = libImage.data;
  library->dataVirtual =
    (library->codeVirtual + (library->data - library->code));
  library->dataSize = libImage.dataSize;
  library->imageSize = libImage.imageSize;
  library->symbolTable =
    getSymbols(loadAddress, 1 /* dynamic */, 1 /* kernel */);
  library->relocationTable =
    getRelocations(loadAddress, library->symbolTable, 0);

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


static int pullInLibrary(int processId, kernelDynamicLibrary *library,
			 loaderSymbolTable **symbols)
{
  // Load the named dynamic library, augment the supplied symbol table with
  // the symbols from the library, and return a pointer to the library
  
  int status = 0;
  unsigned dataOffset = 0;
  void *dataMem = NULL;
  void *libraryDataPhysical = NULL;

  // Get memory for a copy of the library's data
  dataMem = kernelMemoryGet(kernelPageRoundUp(library->dataSize),
			    "dynamic library data");
  if (dataMem == NULL)
    return (status = ERR_MEMORY);

  // Get the physical address of the data memory
  libraryDataPhysical =
    kernelPageGetPhysical(kernelCurrentProcess->processId, dataMem);
  if (libraryDataPhysical == NULL)
    {
      kernelMemoryRelease(dataMem);
      return (status = ERR_MEMORY);
    }

  // Calculate the offset of the data start within its memory page
  dataOffset = ((library->dataVirtual - library->codeVirtual) -
		kernelPageRoundUp(library->codeSize));

  // Make a copy of the data
  kernelMemCopy(library->data, (dataMem + dataOffset), library->dataSize);

  // Find enough free pages for the whole library image
  library->codeVirtual = kernelPageFindFree(processId, library->imageSize);
  if (library->codeVirtual == NULL)
    {
      kernelMemoryRelease(dataMem);
      return (status = ERR_MEMORY);
    }

  library->dataVirtual += (unsigned) library->codeVirtual;

  // Map the kernel's library code into the process' address space
  status =
    kernelPageMap(processId, library->codePhysical, library->codeVirtual,
		  kernelPageRoundUp(library->codeSize));
  if (status < 0)
    {
      kernelMemoryRelease(dataMem);
      return (status);
    }

  // Map the data memory into the process' address space, right after the
  // end of the code.
  status = kernelPageMap(processId, libraryDataPhysical,
			 (library->dataVirtual - dataOffset),
			 kernelPageRoundUp(library->dataSize));
  if (status < 0)
    {
      kernelMemoryRelease(dataMem);
      return (status);
    }

  // Adjust the library's data pointer, so that it points to our copy (plus
  // the offset to the actual data start)
  library->data = (dataMem + dataOffset);

  // Code should be read-only
  kernelPageSetAttrs(processId, 0, PAGEFLAG_WRITABLE, library->codeVirtual,
		     kernelPageRoundUp(library->codeSize));
  if (status < 0)
    {
      kernelMemoryRelease(dataMem);
      return (status);
    }

  // Resolve symbols
  status = resolveLibrarySymbols(symbols, library);
  if (status < 0)
    {
      kernelMemoryRelease(library->code);
      return (status);
    }

  return (status = 0);
}


static int resolveLibraryDependencies(int processId,
				      loaderSymbolTable **symbols,
				      elfLibraryArray *libArray)
{
  // Given an array of library dependencies, load in each one, resolve all
  // the symbols, and do the relocations.

  int status = 0;
  kernelDynamicLibrary *library = NULL;
  int count;

  // For each library in our list, 
  for (count = 0; count < libArray->numLibraries; count ++)
    {
      library = &(libArray->libraries[count]);
      status = pullInLibrary(processId, library, symbols);
      if (status < 0)
	return (status);
    }

  // Do relocations for each library.  All symbols should now be resolved
  for (count = 0; count < libArray->numLibraries; count ++)
    {
      library = &(libArray->libraries[count]);
      status = doRelocations(library->data,  library->codeVirtual,
			     library->dataVirtual, *symbols,
			     library->relocationTable, libArray);
      if (status < 0)
	return (status);
    }

  return (status = 0);
}


static int link(int processId, void *loadAddress, processImage *execImage)
{
  // This function does runtime linking for dynamically-linked executables.
  // 'loadAddress' is the raw file data, and 'execImage' describes the
  // laid-out (using 'layoutExecutable()') version.

  int status = 0;
  loaderSymbolTable *symbols = NULL;
  elfLibraryArray libArray;
  kernelRelocationTable *relocations = NULL;
  int count;

  // We will assume we this function is not called unless the loader is
  // already sure that this file is both ELF and a shared library.  Thus, we
  // will not check the magic number stuff at the head of the file.

  // Get the dynamic symbols for the program
  symbols = getSymbols(loadAddress, 1 /* dynamic */, 1 /* kernel */);
  if (symbols == NULL)
    return (status = ERR_NODATA);

  // Get any library dependencies
  status = getLibraryDependencies(loadAddress, &libArray);
  if (status < 0)
    {
      kernelFree(symbols);
      return (status);
    }

  // Resolve the dependencies
  status = resolveLibraryDependencies(processId, &symbols, &libArray);
  if (status < 0)
    {
      kernelFree(symbols);
      kernelFree(libArray.libraries);
      return (status);
    }

  // Get the relocations for the program code
  relocations =
    getRelocations(loadAddress, symbols, execImage->virtualAddress);
  if (relocations == NULL)
    {
      kernelFree(symbols);
      kernelFree(libArray.libraries);
      return (status = ERR_NODATA);
    }

  // Do relocation for the program
  status = doRelocations(execImage->data, execImage->virtualAddress,
			 (execImage->virtualAddress +
			  (execImage->data - execImage->code)),
			 symbols, relocations, &libArray);
  if (status < 0)
    {
      kernelFree(symbols);
      kernelFree(libArray.libraries);
      return (status);
    }

  // Make the process own the memory for each library, and unmap it from the
  // memory of this process.
  for (count = 0; count < libArray.numLibraries; count ++)
    {
      kernelMemoryChangeOwner(kernelCurrentProcess->processId, processId, 0,
	      (void *) kernelPageRoundDown(libArray.libraries[count].data),
	      NULL);

      kernelPageUnmap(kernelCurrentProcess->processId, (void *)
		      kernelPageRoundDown(libArray.libraries[count].data),
		      kernelPageRoundUp(libArray.libraries[count].dataSize));
    }

  kernelFree(symbols);
  kernelFree(libArray.libraries);
  kernelFree(relocations);
  return (status);
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
      elfFileClass.executable.layoutLibrary = layoutLibrary;
      elfFileClass.executable.layoutExecutable = layoutExecutable;
      elfFileClass.executable.link = link;
      filled = 1;
    }

  return (&elfFileClass);
}
