//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>
#include <sys/errors.h>


static int readBlock(fileStream *theStream, int blockNumber)
{
  // This function is internal, and is used to read the requested file block
  // into the stream.

  int status = 0;

  // Make sure we're not at the end of the file
  if (blockNumber >= theStream->f.blocks)
    return (status = ERR_NODATA);

  // Clear the stream
  theStream->s.clear((stream *) &(theStream->s));

  // Read the next block of the file, and put it into the stream.  The
  // way we do this is a bit of a cheat as far as streams are concerned, as
  // we are subverting the normal procedure for putting things into it.
  status = kernelFileRead(&(theStream->f), blockNumber, 1,
			  theStream->s.buffer);
  if (status < 0)
    return (status);

  // The current block number has changed, and the stream is clean
  theStream->block = blockNumber;
  theStream->dirty = 0;

  // Set some values on the stream.
  
  // The first is always 0 when we read a block
  theStream->s.first = 0;

  // If this is the last block of the file, then we need to set the
  // 'next' position based on the file size mod the block size.
  // Otherwise, we consider the stream full.
  if (blockNumber == (theStream->f.blocks - 1))
    {
      // The stream is only partially full.
      theStream->s.next = (theStream->f.size % theStream->f.blockSize);
      theStream->s.count = theStream->s.next;

      // Make sure that there are NULLs after the valid data
      kernelMemClear((theStream->s.buffer + theStream->s.count),
		     (theStream->f.blockSize - theStream->s.count));
    }
  else
    {
      // We made a full stream.  Appending anything to the stream at this
      // point would begin to overwrite the data at the beginning of
      // the buffer, by the way.
      theStream->s.next = 0;
      theStream->s.count = theStream->f.blockSize;
    }

  // Return success
  return (status = 0);
}


static int writeBlock(fileStream *theStream, int blockNumber)
{
  // This function is internal, and is used to write a file's next block
  // from the supplied stream.

  int status = 0;
  unsigned newSize = 0;

  // Write the requested block of the file from the stream.
  status = kernelFileWrite(&(theStream->f), blockNumber, 1,
			   theStream->s.buffer);

  if (status < 0)
    return (status);

  // The stream is now clean
  theStream->dirty = 0;

  // If this is the last block of the file, we should update the file size
  if (blockNumber >= (theStream->f.blocks - 1))
    {
      newSize =
	((blockNumber * theStream->f.blockSize) + theStream->s.count);
      kernelFileSetSize(&(theStream->f), newSize);
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
  int blockToRead = 0;

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
  // Success?
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to open requested file");
      return (status);
    }

  // We need to get a new stream and attach it to the file stream structure
  // using the blockSize information from the file structure
  status = kernelStreamNew((stream *) &(newStream->s), newStream->f.blockSize,
			   itemsize_byte);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to create the file stream");
      kernelFileClose(&(newStream->f));
      return (status);
    }

  if (newStream->f.blocks > 0)
    {
      if (newStream->f.openMode & OPENMODE_WRITE)
	// Since we are opening in a write mode, we read the last
	// block of the file into the stream
	blockToRead = (newStream->f.blocks - 1);

      else
	// We are not opening in write mode, so we read the first block
	// of the file into the stream.
	blockToRead = 0;

      status = readBlock(newStream, blockToRead);
      if (status < 0)
	{
	  kernelError(kernel_error, "Error reading file");
	  kernelFileClose(&(newStream->f));
	  return (status);
	}
    }
  else
    {
      // Otherwise, simply clear the stream regardless of whether we are
      // doing a read or a write.
      newStream->s.clear((stream *) &(newStream->s));
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
  int newBlock = 0;

  // Check arguments
  
  if (theStream == NULL)
    {
      kernelError(kernel_error, "NULL file stream argument");
      return (status = ERR_NULLPARAMETER);
    }

  // We need to calculate what will be the new block number based on the
  // supplied offset.
  newBlock = (offset / theStream->f.blockSize);
  
  // Make sure we're not at the end of the file
  if (newBlock >= theStream->f.blocks)
    return (status = ERR_NODATA);

  // Read the block from the file, and put it into the stream.
  status = kernelFileRead(&(theStream->f), newBlock, 1,
			  theStream->s.buffer);
  if (status < 0)
    return (status);

  // We are now on a different block
  theStream->block = newBlock;

  // Set some stream values.
  theStream->s.first = (offset % theStream->f.blockSize);
  theStream->s.next = 0;

  if (theStream->f.openMode & OPENMODE_WRITE)
    theStream->s.count = 0;
  else
    theStream->s.count = (theStream->f.blockSize - theStream->s.first);
  
  theStream->dirty = 0;

  // Return success
  return (status = 0);
}


int kernelFileStreamRead(fileStream *theStream, int readBytes, char *buffer)
{
  // This function will read the requested number of bytes from the file
  // stream into the supplied buffer 

  int status = 0;
  int bytes = 0;
  int doneBytes = 0;

  // Check arguments
  
  if (theStream == NULL)
    {
      kernelError(kernel_error, "NULL file stream argument");
      return (status = ERR_NULLPARAMETER);
    }

  if (buffer == NULL)
    {
      kernelError(kernel_error, "NULL buffer argument");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure this file is open in a read mode
  if (!(theStream->f.openMode & OPENMODE_READ))
    {
      kernelError(kernel_error, "file not open in read mode");
      return (status = ERR_INVALID);
    }

  // Make sure the number of bytes to read is greater than 0
  if (readBytes <= 0)
    // Don't make an error, just do nothing
    return (status = 0);

  while (doneBytes < readBytes)
    {
      // How many bytes are in the stream currently?  We will grab either
      // readBytes bytes, or all the bytes depending on which is greater

      bytes = theStream->s.count;
      
      if (bytes == 0)
	{
	  // Oops, the stream is empty.  We need to read another block
	  // from the file
	  status = readBlock(theStream, ++(theStream->block));
	  if (status < 0)
	    return (status);
	  
	  bytes = theStream->s.count;
	}

      if (readBytes < bytes)
	bytes = readBytes;

      // Get 'bytes' bytes from the stream, and put them in the buffer
      status = theStream->s.popN((stream *) &(theStream->s), bytes,
				 (buffer + doneBytes));
      if (status < 0)
	return (status);

      doneBytes += bytes;
    }

  // Return success
  return (status = 0);
}


int kernelFileStreamReadLine(fileStream *theStream, int maxBytes, char *buffer)
{
  // This function will read bytes from the file stream into the supplied
  // buffer until it hits a newline, or until the buffer is full, or until
  // the file is finished

  int status = 0;
  int doneBytes = 0;

  // Check arguments
  
  if (theStream == NULL)
    {
      kernelError(kernel_error, "NULL file stream argument");
      return (status = ERR_NULLPARAMETER);
    }

  if (buffer == NULL)
    {
      kernelError(kernel_error, "NULL buffer argument");
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
	  // Oops, the stream is empty.  We need to read another block
	  // from the file
	  status = readBlock(theStream, ++(theStream->block));
	  if (status < 0)
	    // File finished?
	    break;
	}
      
      // Get a byte from the stream, and put it in the buffer
      status = theStream->s.pop((stream *) &(theStream->s),
				(buffer + doneBytes));
      
      if (status < 0)
	break;

      doneBytes++;

      if (buffer[doneBytes - 1] == '\n')
	break;
    }

  buffer[doneBytes] = '\0';
  return (doneBytes);
}


int kernelFileStreamWrite(fileStream *theStream, int writeBytes, char *buffer)
{
  // This function will write the requested number of bytes from the
  // supplied buffer to the file stream at the current offset.

  int status = 0;
  int bytes = 0;
  int doneBytes = 0;

  // Check arguments
  
  if (theStream == NULL)
    {
      kernelError(kernel_error, "NULL file stream argument");
      return (status = ERR_NULLPARAMETER);
    }

  if (buffer == NULL)
    {
      kernelError(kernel_error, "NULL buffer argument");
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
      
      // How many unused bytes are left in the stream?  Make sure we don't
      // overflow the stream
      bytes = (theStream->f.blockSize - theStream->s.count);

      if (writeBytes < bytes)
	bytes = (writeBytes - doneBytes);  

      // Append 'bytes' bytes to the stream from the buffer
      status = theStream->s
	.appendN((stream *) &(theStream->s), bytes, (buffer + doneBytes));
      if (status < 0)
	{
	  kernelError(kernel_error, "Error %d appending to file stream",
		      status);
	  return (status);
	}

      doneBytes += bytes;
      theStream->dirty = 1;

      // Did we fill the stream?
      if (theStream->s.count >= theStream->f.blockSize)
	{
	  // The stream is now full.  We will need to flush it and possibly
	  // read in the next block of the file

	  // Flush it so the block gets written to disk
	  status = kernelFileStreamFlush(theStream);

	  // We are now using the next block
	  theStream->block++;

	  // If we are writing the last (possibly partial) block, and if
	  // the file is longer than the block we're writing, we need to
	  // read the current block first.

	  if (((writeBytes - doneBytes) < theStream->f.blockSize) &&
	      (theStream->block < theStream->f.blocks))
	    {
	      status = readBlock(theStream, theStream->block);
	      if (status < 0)
		return (status);
	    }
	  else
	    // Simply clear the stream
	    theStream->s.clear((stream *) &(theStream->s));
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

  // Add a newline to the end of the stream
  theStream->s.append((stream *) &(theStream->s), '\n');

  return (kernelFileStreamWrite(theStream, strlen(buffer), buffer));
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
      status = writeBlock(theStream, theStream->block);

      if (status < 0)
	{
	  kernelError(kernel_error, "Error %d writing file block", status);
	  return (status);
	}
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
    kernelError(kernel_error, "Error flushing file");
 
  // Close the file
  status = kernelFileClose(&(theStream->f));

  if (status < 0)
    {
      kernelError(kernel_error, "Error closing file");
      return (status);
    }

  // Return success
  return (status = 0);
}
