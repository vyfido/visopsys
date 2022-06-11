//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelFileStream.c
//

// This file contains the kernel's facilities for reading and writing
// files using a 'streams' abstraction.  It's a convenience for dealing
// with files.

#include "kernelFileStream.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelFile.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include <stdlib.h>
#include <string.h>


static int readBlock(fileStream *theStream)
{
  // This function reads the current file block into the fileStream's buffer.

  int status = 0;

  kernelDebug(debug_io, "Read fileStream %s block %u", theStream->f.name,
  	      theStream->block);

  // Make sure we're not at the end of the file
  if (theStream->block >= theStream->f.blocks)
    {
      kernelError(kernel_error, "Can't read beyond the end of file %s "
		  "(block %d > %d)", theStream->f.name, theStream->block,
		  (theStream->f.blocks - 1));
      return (status = ERR_NODATA);
    }

  // Read the block of the file, and put it into the stream.
  status =
    kernelFileRead(&(theStream->f), theStream->block, 1, theStream->buffer);
  if (status < 0)
    return (status);

  // The stream is clean
  theStream->dirty = 0;

  // Return success
  return (status = 0);
}


static int writeBlock(fileStream *theStream)
{
  // This function writes the buffer into the fileStream's current block.

  int status = 0;
  unsigned oldSize = theStream->f.size;

  kernelDebug(debug_io, "Write fileStream %s block %u", theStream->f.name,
  	      theStream->block);

  // Write the requested block of the file from the stream.
  status =
    kernelFileWrite(&(theStream->f), theStream->block, 1, theStream->buffer);
  if (status < 0)
    return (status);

  // The stream is now clean
  theStream->dirty = 0;

  // If we have enlarged the file, we should set the file size to the most
  // recent file 
  if (theStream->size > oldSize)
    {
      kernelDebug(debug_io, "FileStream %s size %u", theStream->f.name,
		  theStream->size);
      kernelFileEntrySetSize(theStream->f.handle, theStream->size);
    }

  // Return success
  return (status = 0);
}


static int attachToFile(fileStream *newStream)
{
  // Given a fileStream structure with a valid file inside it, start up the
  // stream.

  int status = 0;

  kernelDebug(debug_io, "Attach to fileStream %s", newStream->f.name);

  // Get memory for the buffer
  newStream->buffer = kernelMalloc(newStream->f.blockSize);
  if (!newStream->buffer)
    return (status = ERR_MEMORY);

  // If there's existing data in the file, read the first block into the buffer
  if (newStream->f.blocks)
    {
      status = readBlock(newStream);
      if (status < 0)
	return (status);
    }

  newStream->size = newStream->f.size;

  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelFileStreamOpen(const char *name, int openMode, fileStream *newStream)
{
  // This function initializes the new filestream structure, opens the
  // requested file using the supplied mode number, and attaches it to the
  // stream.  Returns 0 on success, negative otherwise.

  int status = 0;

  // Check arguments
  if ((name == NULL) || (newStream == NULL))
    {
      kernelError(kernel_error, "NULL name or stream parameter");
      return (status = ERR_NULLPARAMETER);
    }

  kernelDebug(debug_io, "Open fileStream %s mode %x", name, openMode);

  // Clear out the fileStream structure
  kernelMemClear(newStream, sizeof(fileStream));

  // Attempt to open the file with the requested name.  Supply a pointer
  // to the file structure in the new stream structure
  status = kernelFileOpen(name, openMode, &(newStream->f));
  if (status < 0)
    return (status);

  status = attachToFile(newStream);
  if (status < 0)
    {
      kernelFileClose(&(newStream->f));
      return (status);
    }

  // Yahoo, all set. 
  return (status = 0);
}


int kernelFileStreamSeek(fileStream *theStream, unsigned offset)
{
  // This function will position the virtual 'head' of the stream at the
  // requested location, so that the next 'read' or 'write' operation will
  // fetch or change data starting at the offset supplied to this seek
  // command.

  int status = 0;

  // Check arguments

  if (theStream == NULL)
    {
      kernelError(kernel_error, "NULL file stream argument");
      return (status = ERR_NULLPARAMETER);
    }

  if (theStream->f.openMode & OPENMODE_WRITE)
    {
      // Can't seek past the last byte of the stream + 1
      if (offset > theStream->size)
	{
	  kernelError(kernel_error, "Can't seek past the end of the file "
		  "(%u > %u)", offset, theStream->size);
	  return (status = ERR_RANGE);
	}
    }
  else
    {
      // Can't seek past the last byte of the stream
      if (offset >= theStream->size)
	{
	  kernelError(kernel_error, "Can't seek past the end of the file "
		  "(%u >= %u)", offset, theStream->size);
	  return (status = ERR_RANGE);
	}
    }

  kernelDebug(debug_io, "Seek fileStream %s to %u", theStream->f.name, offset);

  // Does the new block differ from the new one?
  if ((offset / theStream->f.blockSize) != theStream->block)
    {
      // If we're dirty, flush any existing stuff
      if (theStream->dirty)
	{
	  status = kernelFileStreamFlush(theStream);
	  if (status < 0)
	    return (status);
	}

      theStream->offset = offset;
      theStream->block = (theStream->offset / theStream->f.blockSize);

      if (theStream->block < theStream->f.blocks)
      	{
	  // Read the block from the file, and put it into the buffer
	  status = readBlock(theStream);
	  if (status < 0)
	    return (status);
	}
      else if (theStream->f.openMode & OPENMODE_WRITE)
	// Simply clear the buffer
	kernelMemClear(theStream->buffer, theStream->f.blockSize);
    }
  else
    theStream->offset = offset;

  // Return success
  return (status = 0);
}


int kernelFileStreamRead(fileStream *theStream, unsigned readBytes,
			 char *buffer)
{
  // This function will read the requested number of bytes from the file
  // stream into the supplied buffer 

  int status = 0;
  unsigned blockOffset = 0;
  unsigned remainder = 0;
  unsigned bytes = 0;
  unsigned doneBytes = 0;

  // Check arguments
  if ((theStream == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "NULL file stream or buffer argument");
      return (status = ERR_NULLPARAMETER);
    }

  kernelDebug(debug_io, "Read %u at %u from fileStream %s", readBytes,
	      theStream->offset, theStream->f.name);

  // Make sure this file is open in a read mode
  if (!(theStream->f.openMode & OPENMODE_READ))
    {
      kernelError(kernel_error, "File not open in read mode");
      return (status = ERR_INVALID);
    }

  // Don't read past the end of the stream
  if (theStream->offset >= theStream->size)
    return (status = ERR_NODATA);

  while ((doneBytes < readBytes) && (theStream->offset < theStream->size))
    {
      // How many bytes remain in the buffer currently?  We will grab either
      // readBytes bytes, or all the bytes depending on which is greater
      blockOffset = (theStream->offset % theStream->f.blockSize);
      remainder = (theStream->f.blockSize - blockOffset);

      bytes = min(remainder, (readBytes - doneBytes));

      // Don't read past the end of the stream
      if ((theStream->size - theStream->offset) < bytes)
	bytes = (theStream->size - theStream->offset);

      // Copy 'bytes' bytes from the stream buffer to the output buffer
      kernelMemCopy((theStream->buffer + blockOffset), (buffer + doneBytes),
		    bytes);

      doneBytes += bytes;
      theStream->offset += bytes;

      if ((theStream->offset / theStream->f.blockSize) != theStream->block)
	{
	  // The stream is empty.  Can we read another block from the file?
	  theStream->block = (theStream->offset / theStream->f.blockSize);

	  if (theStream->block < theStream->f.blocks)
	    {
	      status = readBlock(theStream);
	      if (status < 0)
		return (status);
	    }
	  else if (theStream->f.openMode & OPENMODE_WRITE)
	    // Simply clear the buffer
	    kernelMemClear(theStream->buffer, theStream->f.blockSize);
	}
    }

  kernelDebug(debug_io, "Read %u", doneBytes);
  return (doneBytes);
}


int kernelFileStreamReadLine(fileStream *theStream, unsigned maxBytes,
			     char *buffer)
{
  // This function will read bytes from the file stream into the supplied
  // buffer until it hits a newline, or until the buffer is full, or until
  // the file is finished

  int status = 0;
  unsigned blockOffset = 0;
  unsigned doneBytes = 0;

  // Check arguments
  
  if ((theStream == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "NULL file stream or buffer argument");
      return (status = ERR_NULLPARAMETER);
    }

  kernelDebug(debug_io, "ReadLine %u from fileStream %s", maxBytes,
  	      theStream->f.name);

  // Make sure this file is open in a read mode
  if (!(theStream->f.openMode & OPENMODE_READ))
    {
      kernelError(kernel_error, "file not open in read mode");
      return (status = ERR_INVALID);
    }

  // Don't read past the end of the stream
  if (theStream->offset >= theStream->size)
    return (status = ERR_NODATA);

  while ((doneBytes < (maxBytes - 1)) && (theStream->offset < theStream->size))
    {
      blockOffset = (theStream->offset % theStream->f.blockSize);

      // Get a byte from the stream buffer, and put it in the output buffer
      buffer[doneBytes] = theStream->buffer[blockOffset];

      doneBytes += 1;
      theStream->offset += 1;

      if ((theStream->offset / theStream->f.blockSize) != theStream->block)
	{
	  // The stream is empty.  Can we read another block from the file?
	  theStream->block = (theStream->offset / theStream->f.blockSize);

	  if (theStream->block < theStream->f.blocks)
	    {
	      status = readBlock(theStream);
	      if (status < 0)
		return (status);
	    }
	  else if (theStream->f.openMode & OPENMODE_WRITE)
	    // Simply clear the buffer
	    kernelMemClear(theStream->buffer, theStream->f.blockSize);
	}

      if (buffer[doneBytes - 1] == '\n')
	{
	  buffer[doneBytes - 1] = '\0';
	  doneBytes -= 1;
	  break;
	}
    }

  kernelDebug(debug_io, "ReadLine %d:%d: %s", theStream->block,
  	      theStream->offset, buffer);

  buffer[maxBytes - 1] = '\0';
  return (doneBytes);
}


int kernelFileStreamWrite(fileStream *theStream, unsigned writeBytes,
			  const char *buffer)
{
  // This function will write the requested number of bytes from the
  // supplied buffer to the file stream at the current offset.

  int status = 0;
  unsigned blockOffset = 0;
  unsigned remainder = 0;
  unsigned bytes = 0;
  unsigned doneBytes = 0;

  // Check arguments
  if ((theStream == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "NULL file stream or buffer argument");
      return (status = ERR_NULLPARAMETER);
    }

  kernelDebug(debug_io, "Write %u to fileStream %s", writeBytes,
	      theStream->f.name);

  // Make sure this file is open in a write mode
  if (!(theStream->f.openMode & OPENMODE_WRITE))
    {
      kernelError(kernel_error, "file not open in write mode");
      return (status = ERR_INVALID);
    }

  while (doneBytes < writeBytes)
    {
      // How many bytes remain in the buffer currently?  We will insert either
      // writeBytes bytes, or all the bytes depending on which is greater
      blockOffset = (theStream->offset % theStream->f.blockSize);
      remainder = (theStream->f.blockSize - blockOffset);

      bytes = min(remainder, (writeBytes - doneBytes));
      
      //kernelDebug(debug_io, "Loop bytes=%d at %d", bytes, theStream->offset);

      // Copy 'bytes' bytes from the output buffer to the stream buffer
      theStream->dirty = 1;
      kernelMemCopy((buffer + doneBytes), (theStream->buffer + blockOffset),
		    bytes);

      doneBytes += bytes;

      theStream->offset += bytes;
      if (theStream->offset > theStream->size)
	theStream->size = theStream->offset;

      if ((theStream->offset / theStream->f.blockSize) != theStream->block)
	{
	  // The buffer is now full.  We will need to flush it and possibly
	  // read in the next block of the file

	  // Write the current block of the stream to the file.
	  status = writeBlock(theStream);
	  if (status < 0)
	    return (status);

	  theStream->block = (theStream->offset / theStream->f.blockSize);

	  // If we are writing the last (possibly partial) block, and if
	  // the file is longer than the block we're writing, we need to
	  // read the current block first.

	  if (theStream->block < theStream->f.blocks)
	    {
	      status = readBlock(theStream);
	      if (status < 0)
		return (status);
	    }
	  else
	    // Simply clear the buffer
	    kernelMemClear(theStream->buffer, theStream->f.blockSize);
	}
    }

  return (doneBytes);
}


int kernelFileStreamWriteStr(fileStream *theStream, const char *buffer)
{
  // This is just a wrapper function for the Write function, above, except
  // that it saves the caller the trouble of specifying the length of the
  // string to write; it calls strlen() and passes the information along
  // to the write routine.

  return (kernelFileStreamWrite(theStream, strlen(buffer), buffer));
}


int kernelFileStreamWriteLine(fileStream *theStream, const char *buffer)
{
  // This is just a wrapper function for the Write function, above, and
  // is just like the kernelFileStreamWriteStr function above except that
  // it adds a newline to the stream after the line has been added.

  int status = 0;

  if ((theStream == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "NULL file stream or buffer argument");
      return (status = ERR_NULLPARAMETER);
    }

  kernelDebug(debug_io, "WriteLine to fileStream %s", theStream->f.name);

  status = kernelFileStreamWrite(theStream, strlen(buffer), buffer);
  if (status < 0)
    return (status);

  // Add a newline to the end of the stream
  status = kernelFileStreamWrite(theStream, 1, "\n");

  return (status);
}


int kernelFileStreamFlush(fileStream *theStream)
{
  // If the file corresponding to the supplied stream is open in a writable
  // mode, this function will cause any unwritten data to get flushed from
  // the stream to the disk.

  int status = 0;

  // Check arguments
  
  if (theStream == NULL)
    {
      kernelError(kernel_error, "NULL file stream argument");
      return (status = ERR_NULLPARAMETER);
    }

  if (theStream->dirty)
    {
      kernelDebug(debug_io, "Flush fileStream %s", theStream->f.name);

      // Write the current block of the stream to the file.
      status = writeBlock(theStream);
      if (status < 0)
	return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFileStreamClose(fileStream *theStream)
{
  // This function is just a wrapper around a couple of other functions.
  // Basically, this will flush the stream to disk (if the file is open in a 
  // writable mode), close the file, and deallocate the stream memory
  // that gets allocated by the kernelFileStreamNew function.

  int status = 0;

  // Check arguments
  
  if (theStream == NULL)
    {
      kernelError(kernel_error, "NULL file stream argument");
      return (status = ERR_NULLPARAMETER);
    }

  kernelDebug(debug_io, "Close fileStream %s", theStream->f.name);

  // Flush the file stream
  status = kernelFileStreamFlush(theStream);
  if (status < 0)
    return (status);
 
  // Close the file
  status = kernelFileClose(&(theStream->f));
  if (status < 0)
    return (status);

  // Free the buffer and clear the structure.
  kernelFree(theStream->buffer);
  kernelMemClear(theStream, sizeof(fileStream));

  return (status = 0);
}


int kernelFileStreamGetTemp(fileStream *newStream)
{
  // This function initializes the new filestream structure, opens a temporary
  // file, and attaches it to the stream.  Returns 0 on success, negative
  // otherwise.

  int status = 0;

  // Check args
  if (newStream == NULL)
    {
      kernelError(kernel_error, "NULL stream parameter");
      return (status = ERR_NULLPARAMETER);
    }

  // Clear out the fileStream structure
  kernelMemClear((void *) newStream, sizeof(fileStream));

  // Attempt to open a temporary file.  Supply a pointer to the file
  // structure in the new stream structure.
  status = kernelFileGetTemp(&(newStream->f));
  if (status < 0)
    return (status);

  status = attachToFile(newStream);
  if (status < 0)
    {
      kernelFileClose(&(newStream->f));
      return (status);
    }

  // Yahoo, all set. 
  return (status = 0);
}
