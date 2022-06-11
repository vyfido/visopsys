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
//  kernelLoader.h
//
	
// This is the header file to go with the kernel's loader

#if !defined(_KERNELLOADER_H)

#include <sys/loader.h>
#include <sys/file.h>
#include <sys/image.h>
#include <sys/process.h>

#define FILECLASS_NAME_EMPTY    "empty"
#define FILECLASS_NAME_TEXT     "text"
#define FILECLASS_NAME_BIN      "binary"
#define FILECLASS_NAME_STATIC   "static"
#define FILECLASS_NAME_DYNAMIC  "dynamic"
#define FILECLASS_NAME_EXEC     "executable"
#define FILECLASS_NAME_OBJ      "object"
#define FILECLASS_NAME_LIB      "library"
#define FILECLASS_NAME_CORE     "core"
#define FILECLASS_NAME_IMAGE    "image"
#define FILECLASS_NAME_DATA     "data"

#define FILECLASS_NAME_ELF      "ELF"
#define FILECLASS_NAME_BMP      "bitmap"
#define FILECLASS_NAME_ICO      "icon"
#define FILECLASS_NAME_JPG      "JPEG"
#define FILECLASS_NAME_CONFIG   "configuration"
#define FILECLASS_NAME_BOOT     "boot"
#define LOADER_NUM_FILECLASSES  8

// A generic structure to represent a relocation entry
typedef struct {
  void *offset;     // Virtual offset in image
  char *symbolName; // Index into symbol table
  int info;         // Driver-specific
  unsigned addend;  // Not used (yet)

} kernelRelocation;

// A collection of kernelRelocation entries
typedef struct {
  int numRelocs;
  int tableSize;
  kernelRelocation relocations[];

} kernelRelocationTable;

// The structure that describes a dynamic library ready for use by the loader
typedef struct {
  char name[MAX_NAME_LENGTH];
  void *code;
  void *codeVirtual;
  void *codePhysical;
  unsigned codeSize;
  void *data;
  void *dataVirtual;
  unsigned dataSize;
  unsigned imageSize;
  loaderSymbolTable *symbolTable;
  kernelRelocationTable *relocationTable;
  void *next;
  
} kernelDynamicLibrary;

// This is a structure for a file class.  It contains a standard name for
// the file class and function pointers for managing that class of file.
typedef struct {
  char *className;
  int (*detect)(const char *, void *, int, loaderFileClass *);
  union {
    struct {
      loaderSymbolTable * (*getSymbols)(void *, int, int);
      int (*layoutLibrary)(void *, kernelDynamicLibrary *);
      int (*layoutExecutable)(void *, processImage *);
      int (*link)(int, void *, processImage *);
    } executable;
    struct {
      int (*load)(unsigned char *, int, int, int, image *);
      int (*save)(const char *, image *);
    } image;
  };
  
} kernelFileClass;

// Functions exported by kernelLoader.c
void *kernelLoaderLoad(const char *, file *);
kernelFileClass *kernelLoaderGetFileClass(const char *);
kernelFileClass *kernelLoaderClassify(const char *, void *, int,
				      loaderFileClass *);
kernelFileClass *kernelLoaderClassifyFile(const char *, loaderFileClass *);
loaderSymbolTable *kernelLoaderGetSymbols(const char *, int);
int kernelLoaderLoadProgram(const char *, int);
int kernelLoaderLoadLibrary(const char *);
kernelDynamicLibrary *kernelLoaderGetLibrary(const char *);
int kernelLoaderExecProgram(int, int);
int kernelLoaderLoadAndExec(const char *, int, int);

// These are format-specific file class functions
kernelFileClass *kernelFileClassElf(void);
kernelFileClass *kernelFileClassBmp(void);
kernelFileClass *kernelFileClassIco(void);
kernelFileClass *kernelFileClassJpg(void);
kernelFileClass *kernelFileClassConfig(void);
kernelFileClass *kernelFileClassBoot(void);
kernelFileClass *kernelFileClassText(void);
kernelFileClass *kernelFileClassBinary(void);

#define _KERNELLOADER_H
#endif
