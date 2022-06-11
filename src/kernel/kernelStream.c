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
//  kernelStream.c
//

// This file contains all of the basic routines for dealing with generic
// data streams.  Data streams in Visopsys are implemented as circular
// buffers of variable size.

#include "kernelStream.h"
#include "kernelMemoryManager.h"
#include "kernelMiscAsmFunctions.h"
#include <sys/errors.h>
#include <string.h>


static int clear(stream *theStream)
{
  // Removes all data from the stream.  Returns 0 if successful,  
  // negative otherwise.

  int status = 0;

  // Make sure the stream pointer isn't NULL
  if (theStream == NULL)
    return (status = ERR_NULLPARAMETER);

  // We clear the buffer and set first, next, and count to zero
  kernelMemClear(theStream->buffer, theStream->size);

  theStream->first = 0;
  theStream->next = 0;
  theStream->count = 0;

  // Return success
  return (status = 0);
}


static int appendByte(stream *theStream, unsigned char byte)
{
  // Appends a single byte to the end of the stream.  Returns 0 if
  // successful, negative otherwise.

  int status = 0;

  // Make sure the stream pointer isn't NULL
  if (theStream == NULL)
    return (status = ERR_NULLPARAMETER);

  // Make sure we aren't about to overflow the buffer
  // if (theStream->count == theStream->size)
  //     return (status = ERR_BOUNDS);

  // Add the character
  theStream->buffer[theStream->next++] = byte;

  // Increase the count
  theStream->count++;

  // Watch for buffer-wrap
  if (theStream->next >= theStream->size)
    theStream->next = 0;

  // Return success
  return (status = 0);
}


static int appendBytes(stream *theStream, int number, unsigned char *buffer)
{
  // Appends the requested number of bytes to the end of the stream.  
  // Returns 0 on success, negative otherwise.

  int status = 0;
  int added = 0;

  // Make sure the stream pointer isn't NULL
  if (theStream == NULL)
    return (status = ERR_NULLPARAMETER);

  // Make sure the number of bytes to append is greater than zero
  if (number <= 0)
    return (status = ERR_INVALID);

  // Make sure the buffer pointer isn't NULL
  if (buffer == NULL)
    return (status = ERR_NULLPARAMETER);

  // Make sure we aren't about to overflow the buffer
  // if ((theStream->count + number) > theStream->size)
  //     return (status = ERR_BOUNDS);

  // Do a loop to add the characters
  while (added < number)
    {
      // Add 1 character
      theStream->buffer[theStream->next++] = buffer[added++];

      // Watch for buffer-wrap
      if (theStream->next >= theStream->size)
	theStream->next = 0;
    }

  // Increase the count
  theStream->count += number;

  // Return success
  return (status = 0);
}


static int pushByte(stream *theStream, unsigned char byte)
{
  // Adds a single byte to the beginning of the stream.  Returns 0 on
  // success, negative otherwise.

  int status = 0;

  // Make sure the stream pointer isn't NULL
  if (theStream == NULL)
    return (status = ERR_NULLPARAMETER);

  // Make sure we aren't about to overflow the buffer
  // if (theStream->count == theStream->size)
  //     return (status = ERR_BOUNDS);

  // Move the head of the buffer backwards
  theStream->first--;

  // Watch out for backwards wrap-around
  if (theStream->first < 0)
    theStream->first = (theStream->size - 1);
  
  // Add the byte to the head of the buffer
  theStream->buffer[theStream->first] = byte;

  // Increase the count
  theStream->count++;

  // Return success
  return (status = 0);
}


static int pushBytes(stream *theStream, int number,
		     unsigned char *buffer)
{
  // Adds the requested number of bytes to the beginning of the stream.
  // On success, it returns 0, negative otherwise

  int status = 0;
  int added = 0;

  // Make sure the stream pointer isn't NULL
  if (theStream == NULL)
    return (status = ERR_NULLPARAMETER);

  // Make sure the number of bytes to push is greater than zero
  if (number <= 0)
    return (status = ERR_INVALID);

  // Make sure the buffer pointer isn't NULL
  if (buffer == NULL)
    return (status = ERR_NULLPARAMETER);

  // Make sure we aren't about to overflow the buffer
  // if ((theStream->count + number) > theStream->size)
  //     return (status = ERR_BOUNDS);

  // Do a loop to add bytes
  while (number > 0)
    {
      // Move the head of the buffer backwards
      theStream->first--;

      // Watch out for backwards wrap-around
      if (theStream->first < 0)
	theStream->first = (theStream->size - 1);
  
      // Add the byte to the head of the buffer
      theStream->buffer[theStream->first] = buffer[--number];

      added++;
    }

  // Increase the count
  theStream->count += added;

  // Return success
  return (status = 0);
}


static int popByte(stream *theStream, unsigned char *byte)
{
  // Removes a single byte from the beginning of the stream and returns
  // it to the caller.  On error, it returns a NULL byte.

  int status = 0;

  // Make sure the stream pointer isn't NULL
  if (theStream == NULL)
    return (status = ERR_NULLPARAMETER);

  // Make sure the byte pointer isn't NULL
  if (byte == NULL)
    return (status = ERR_NULLPARAMETER);
    
  // Make sure the buffer isn't empty
  if (theStream->count == 0)
    return (status = ERR_NODATA);

  // Get the byte at the head of the buffer
  *byte = theStream->buffer[theStream->first];

  // Put a new NULL at the head of the buffer, and increment the head
  theStream->buffer[theStream->first++] = NULL;

  // Watch out for wrap-around
  if (theStream->first >= theStream->size)
    theStream->first = 0;
  
  // Decrease the count
  theStream->count--;

  // Return success
  return (status = 0);
}


static int popBytes(stream *theStream, int number, unsigned char *buffer)
{
  // Removes the requested number of bytes from the beginning of the stream
  // and returns them in the buffer provided.  On success, it returns the
  // number of characters it actually removed.  Returns negative on error.

  int removed = 0;

  // Make sure the stream pointer isn't NULL
  if (theStream == NULL)
    return (removed = ERR_NULLPARAMETER);

  // Make sure the number of bytes to pop is greater than zero
  if (number <= 0)
    return (removed = ERR_INVALID);

  // Make sure the buffer pointer isn't NULL
  if (buffer == NULL)
    return (removed = ERR_NULLPARAMETER);

  // Make sure the buffer isn't empty
  if (theStream->count == 0)
    return (removed = 0);

  // Do a loop to remove bytes and place them in the buffer
  while (removed < number)
    {
      // If the buffer is now empty, we stop here
      if (theStream->count == 0)
	break;

      // Get the byte at the head of the buffer
      buffer[removed++] = theStream->buffer[theStream->first];

      // Put a new NULL at this spot in the stream's buffer
      theStream->buffer[theStream->first++] = NULL;
      
      // Watch out for wrap-around
      if (theStream->first >= theStream->size)
	theStream->first = 0;

      // Decrease the count
      theStream->count--;
    }

  // For the sake of being polite, stick a NULL at the end of the
  // caller's buffer.  Hope this doesn't cause problems with the buffer
  // size being exactly large enough to hold the data bytes.
  buffer[removed] = NULL;

  // Return the number of bytes we copied
  return (removed);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


stream *kernelStreamNew(int items, streamItemSize itemSize)
{
  // Gets memory, initializes, clears out, and prepares the new stream.
  // Returns a pointer to the stream if successful, NULL otherwise

  int size = 0;
  stream *theStream = NULL;

  // Items should be greater than zero
  if (items <= 0)
    return (theStream = NULL);

  // What is the size, in bytes, of the requested stream?
  switch(itemSize)
    {
    case itemsize_char:
      size = (items * sizeof(char));
      break;
    case itemsize_int:
      size = (items * sizeof(int));
      break;
    default:
      return (theStream = NULL);
    }

  // We will allocate (size + sizeof(stream)) memory for the stream,
  // so that the whole thing can be obtained with one memory allocation.
  // Also, make it a multiple of the page size: otherwise the extra space
  // will just be wasted
  size += sizeof(stream);

  theStream = kernelMemoryRequestSystemBlock(size, 0, "data stream memory");

  // Were we successful?
  if (theStream == NULL)
    return (theStream);

  // Clear out the data
  kernelMemClear((void *) theStream, size);

  // Set up the stream's internal data.  All the other bits are zero
  theStream->buffer = (unsigned char *) (theStream + sizeof(stream));
  theStream->size = items;

  // Set the appropriate manipulation functions for this stream.

  switch(itemSize)
    {
    case itemsize_char:
      // Copy the byte stream functions
      theStream->clear = (int(*)(void *)) &clear;
      theStream->intercept = (int(*)(void *, ...)) NULL;
      theStream->append = (int(*)(void *, ...)) &appendByte;
      theStream->appendN = (int(*)(void *, int, ...)) &appendBytes;
      theStream->push = (int(*)(void *, ...)) &pushByte;
      theStream->pushN = (int(*)(void *, int, ...)) &pushBytes;
      theStream->pop = (int(*)(void *, ...)) &popByte;
      theStream->popN = (int(*)(void *, int, ...)) &popBytes;
      break;
    case itemsize_int:
      // Nothing implemented
      break;
    }

  // Return the pointer to the stream
  return (theStream);
}
