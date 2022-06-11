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
//  kernelFileStream.c
//

// This file contains the kernel's facilities for reading and writing
// files using a 'streams' abstraction.  It's a convenience for dealing
// with files.

#include "kernelFileStream.h"
#include "kernelFile.h"
#include "kernelStream.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include <string.h>
#include <stdlib.h>


static int readBlock(fileStream *theStream)
{
  // This function is internal, and is used to read the requested file block
  // into the stream.

  int status = 0;

  // Make sure we're not at the end of the file
  if (theStream->block >= theStream->f.blocks)
    {
      kernelError(kernel_error, "Can't read beyond the end of file %s"
		  "(block %d > %d)", theStream->f.name, theStream->block,
		  (theStream->f.blocks - 1));
      return (status = ERR_NODATA);
    }

  // Clear the stream
  theStream->s.clear(&(theStream->s));

  // Read the next block of the file, and put it into the stream.  The
  // way we do this is a bit of a cheat as far as streams are concerned, as
  // we are subverting the normal procedure for putting things into it.
  status =
    kernelFileRead(&(theStream->f), theStream->block, 1, theStream->s.buffer);
  if (status < 0)
    return (status);

  // The stream is clean
  theStream->dirty = 0;

  // Set some values on the stream.
  
  // The first is always 0 when we read a block
  theStream->s.first = 0;

  // If this is the last block of the file, then we need to set the
  // 'last' position based on the file size mod the block size. Otherwise,
  // we consider the stream full.
  if ((theStream->block < (theStream->f.blocks - 1)) ||
      ((theStream->f.size % theStream->f.blockSize) == 0))
    {
      // We made a full stream.  Appending anything to the stream at this
      // point would begin to overwrite the data at the beginning of
      // the buffer, by the way.
      theStream->s.last = 0;
      theStream->s.count = theStream->f.blockSize;
    }
  else
    {
      // The stream is only partially full.
      theStream->s.last = (theStream->f.size % theStream->f.blockSize);
      theStream->s.count = theStream->s.last;
    }

  // Return success
  return (status = 0);
}


static int writeBlock(fileStream *theStream)
{
  // This function is internal, and is used to write a file's next block
  // from the supplied stream.

  int status = 0;
  unsigned newSize = 0;

  // Write the requested block of the file from the stream.
  status =
    kernelFileWrite(&(theStream->f), theStream->block, 1, theStream->s.buffer);
  if (status < 0)
    return (status);

  // The stream is now clean
  theStream->dirty = 0;

  // If this is the last block of the file, we should update the file size
  if (theStream->block >= (theStream->f.blocks - 1))
    {
      newSize =
	((theStream->block * theStream->f.blockSize) + theStream->s.count);
      kernelFileSetSize(theStream->f.handle, newSize);
    }

  // Return success
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

  // Clear out the fileStream structure
  kernelMemClear((void *) newStream, sizeof(fileStream));

  // Attempt to open the file with the requested name.  Supply a pointer
  // to the file structure in the new stream structure
  status = kernelFileOpen(name, openMode, &(newStream->f));
  if (status < 0)
    return (status);

  // We need to get a new stream and attach it to the file stream structure
  // using the blockSize information from the file structure
  status =
    kernelStreamNew(&(newStream->s), newStream->f.blockSize, itemsize_byte);
  if (status < 0)
    {
      kernelFileClose(&(newStream->f));
      return (status);
    }

  if (newStream->f.blocks > 0)
    {
      if (newStream->f.openMode & OPENMODE_WRITE)
	// Since we are opening in a write mode, we read the last
	// block of the file into the stream
	newStream->block = (newStream->f.blocks - 1);

      else
	// We are not opening in write mode, so we read the first block
	// of the file into the stream.
	newStream->block = 0;

      status = readBlock(newStream);
      if (status < 0)
	{
	  kernelFileClose(&(newStream->f));
	  return (status);
	}
    }
  else
    {
      // Otherwise, simply clear the stream regardless of whether we are
      // doing a read or a write.
      newStream->s.clear(&(newStream->s));
    }

  // Yahoo, all set. 
  return (status = 0);
}


int kernelFileStreamSeek(fileStream *theStream, int offset)
{
  // This function will position the virtual 'head' of the stream at the
  // requested location, so that the next 'read' or 'write' operation will
  // fetch or change data starting at the offset supplied to this seek
  // command.

  int status = 0;
  unsigned newBlock = 0;

  // Check arguments
  
  if (theStream == NULL)
    {
      kernelError(kernel_error, "NULL file stream argument");
      return (status = ERR_NULLPARAMETER);
    }

  // We need to calculate what will be the new block number based on the
  // supplied offset.
  newBlock = ((offset + 1) / theStream->f.blockSize);

  if (newBlock != theStream->block)
    {
      // If we're dirty, flush any existing stuff
      if (theStream->dirty)
	{
	  status = kernelFileStreamFlush(theStream);
	  if (status < 0)
	    return (status);
	}

      theStream->block = newBlock;

      if (theStream->block < theStream->f.blocks)
	{
	  // Read the block from the file, and put it into the stream.
	  status = readBlock(theStream);
	  if (status < 0)
	    return (status);
	}
      else
	{
	  // Write an empty block at the new end of the file and proceed from
	  // there
	  kernelMemClear(theStream->s.buffer, theStream->f.blockSize);
	  status = writeBlock(theStream);
	  if (status < 0)
	    return (status);

	  theStream->s.first = 0;
	  theStream->s.last = 0;
	  theStream->s.count = 0;
	}
    }

  if (theStream->f.openMode & OPENMODE_WRITE)
    {
      theStream->s.last = (offset % theStream->f.blockSize);
      theStream->s.count = theStream->s.last;
    }
  else
    {
      theStream->s.first = (offset % theStream->f.blockSize);
      if (theStream->s.count > 0)
	{
	  if (theStream->s.last > 0)
	    theStream->s.count =
	      max((theStream->s.last - theStream->s.first), 0);
	  else
	    theStream->s.count = (theStream->f.blockSize - theStream->s.first);
	}
    }

  // Return success
  return (status = 0);
}


int kernelFileStreamRead(fileStream *theStream, unsigned readBytes,
			 char *buffer)
{
  // This function will read the requested number of bytes from the file
  // stream into the supplied buffer 

  int status = 0;
  int bytes = 0;
  unsigned doneBytes = 0;

  // Check arguments
  if ((theStream == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "NULL file stream or buffer argument");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the number of bytes to read is greater than 0
  if (readBytes <= 0)
    // Don't make an error, just do nothing
    return (status = 0);

  // Make sure this file is open in a read mode
  if (!(theStream->f.openMode & OPENMODE_READ))
    {
      kernelError(kernel_error, "file not open in read mode");
      return (status = ERR_INVALID);
    }

  while (doneBytes < readBytes)
    {
      // How many bytes are in the stream currently?  We will grab either
      // readBytes bytes, or all the bytes depending on which is greater

      if (theStream->s.count == 0)
	{
	  // The stream is empty.  We need to read another block from the file
	  theStream->block += 1;

	  if (theStream->block >= theStream->f.blocks)
	    // File finished
	    break;

	  status = readBlock(theStream);
	  if (status < 0)
	    return (status);
	}

      bytes = min(theStream->s.count, readBytes);

      // Get 'bytes' bytes from the stream, and put them in the buffer
      status = theStream->s.popN(&(theStream->s), bytes, (buffer + doneBytes));
      if (status < 0)
	return (status);

      doneBytes += bytes;
    }

  buffer[doneBytes] = '\0';
  return (doneBytes);
}


int kernelFileStreamReadLine(fileStream *theStream, unsigned maxBytes,
			     char *buffer)
{
  // This function will read bytes from the file stream into the supplied
  // buffer until it hits a newline, or until the buffer is full, or until
  // the file is finished

  int status = 0;
  unsigned doneBytes = 0;

  // Check arguments
  
  if ((theStream == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "NULL file stream or buffer argument");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure this file is open in a read mode
  if (!(theStream->f.openMode & OPENMODE_READ))
    {
      kernelError(kernel_error, "file not open in read mode");
      return (status = ERR_INVALID);
    }

  while (doneBytes < (maxBytes - 1))
    {
      // How many bytes are in the stream currently?  We will grab either
      // readBytes bytes, or all the bytes depending on which is greater

      if (theStream->s.count == 0)
	{
	  // The stream is empty.  We need to read another block from the file
	  theStream->block += 1;

	  if (theStream->block >= theStream->f.blocks)
	    // File finished
	    break;

	  status = readBlock(theStream);
	  if (status < 0)
	    return (status);
	}
      
      // Get a byte from the stream, and put it in the buffer
      status = theStream->s.pop(&(theStream->s), (buffer + doneBytes));
      if (status < 0)
	return (status);

      if (buffer[doneBytes] == '\n')
	break;

      doneBytes++;
    }

  buffer[doneBytes] = '\0';
  return (doneBytes);
}


int kernelFileStreamWrite(fileStream *theStream, unsigned writeBytes,
			  char *buffer)
{
  // This function will write the requested number of bytes from the
  // supplied buffer to the file stream at the current offset.

  int status = 0;
  int bytes = 0;
  unsigned doneBytes = 0;

  // Check arguments
  
  if ((theStream == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "NULL file stream or buffer argument");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure this file is open in a write mode
  if (!(theStream->f.openMode & OPENMODE_WRITE))
    {
      kernelError(kernel_error, "file not open in write mode");
      return (status = ERR_INVALID);
    }

  // Make sure the number of bytes to write is greater than 0
  if (writeBytes <= 0)
    // Don't make an error, just do nothing
    return (status = 0);

  while (doneBytes < writeBytes)
    {
      // How many bytes are currently in the stream?  If the number of
      // bytes we are writing won't fit into the existing stream, we
      // need to do multiple partial operations.
      bytes = min((writeBytes - doneBytes),
		  (theStream->f.blockSize - theStream->s.count));

      // Append 'bytes' bytes to the stream from the buffer
      status =
	theStream->s.appendN(&(theStream->s), bytes, (buffer + doneBytes));
      if (status < 0)
	return (status);

      doneBytes += bytes;
      theStream->dirty = 1;

      // Did we fill the stream?
      if (theStream->s.count >= theStream->f.blockSize)
	{
	  // The stream is now full.  We will need to flush it and possibly
	  // read in the next block of the file

	  // Flush it so the block gets written to disk
	  status = kernelFileStreamFlush(theStream);
	  if (status < 0)
	    return (status);

	  // We are now using the next block
	  theStream->block++;

	  // If we are writing the last (possibly partial) block, and if
	  // the file is longer than the block we're writing, we need to
	  // read the current block first.

	  if (((writeBytes - doneBytes) < theStream->f.blockSize) &&
	      (theStream->block < theStream->f.blocks))
	    {
	      status = readBlock(theStream);
	      if (status < 0)
		return (status);
	    }
	  else
	    // Simply clear the stream
	    theStream->s.clear(&(theStream->s));
	}
    }

  // Return success
  return (status = 0);
}


int kernelFileStreamWriteStr(fileStream *theStream, char *buffer)
{
  // This is just a wrapper function for the Write function, above, except
  // that it saves the caller the trouble of specifying the length of the
  // string to write; it calls strlen() and passes the information along
  // to the write routine.

  return (kernelFileStreamWrite(theStream, strlen(buffer), buffer));
}


int kernelFileStreamWriteLine(fileStream *theStream, char *buffer)
{
  // This is just a wrapper function for the Write function, above, and
  // is just like the kernelFileStreamWriteStr function above except that
  // it adds a newline to the stream after the line has been added.

  int status = 0;

  status = kernelFileStreamWrite(theStream, strlen(buffer), buffer);
  if (status < 0)
    return (status);

  // Add a newline to the end of the stream
  return (theStream->s.append(&(theStream->s), '\n'));
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

  // Flush the file stream
  status = kernelFileStreamFlush(theStream);
  if (status < 0)
    return (status);
 
  // Close the file
  status = kernelFileClose(&(theStream->f));
  if (status < 0)
    return (status);

  // Destroy the stream
  status = kernelStreamDestroy(&(theStream->s));

  return (status);
}
