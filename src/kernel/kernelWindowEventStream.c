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
//  kernelWindowEventStream.c
//

// This file contains the kernel's facilities for reading and writing
// window events using a 'streams' abstraction.

#include "kernelWindowEventStream.h"
#include "kernelStream.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>
#include <sys/window.h>
#include <sys/errors.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelWindowEventStreamNew(volatile windowEventStream *newStream)
{
  // This function initializes the new window event stream structure.
  // Returns 0 on success, negative otherwise.

  int status = 0;

  // Check arguments
  if (newStream == NULL)
    {
      kernelError(kernel_error, "NULL stream parameter");
      return (status = ERR_NULLPARAMETER);
    }

  // Clear out the windowEventStream structure
  kernelMemClear((void *) newStream, sizeof(windowEventStream));

  // We need to get a new stream and attach it to the window event stream
  // structure
  status = kernelStreamNew((stream *) &(newStream->s),
			   (WINDOW_MAX_EVENTS * (sizeof(windowEvent) /
						 sizeof(unsigned))),
			   itemsize_dword);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to create the window event stream");
      return (status);
    }

  // Otherwise, simply clear the stream regardless of whether we are
  // doing a read or a write.
  newStream->s.clear((stream *) &(newStream->s));

  // Yahoo, all set. 
  return (status = 0);
}


int kernelWindowEventStreamRead(volatile windowEventStream *theStream,
				windowEvent *event)
{
  // This function will read a window event from the window event stream
  // into the supplied windowEvent structure

  int dwords = 0;

  // Check arguments
  if ((theStream == NULL) || (event == NULL))
    {
      kernelError(kernel_error, "NULL event stream or event argument");
      return (ERR_NULLPARAMETER);
    }

  if (theStream->s.count > 0)
    dwords = theStream
      ->s.popN((stream *) &(theStream->s), (sizeof(windowEvent) /
					    sizeof(unsigned)), event);

  // Read the requisite number of dwords from the stream
  return (dwords);
}


int kernelWindowEventStreamWrite(volatile windowEventStream *theStream,
				 windowEvent *event)
{
  // This function will write the data from the supplied windowEvent structure
  // to the window event stream 

  // Check arguments
  if ((theStream == NULL) || (event == NULL))
    {
      kernelError(kernel_error, "NULL event stream or event argument");
      return (ERR_NULLPARAMETER);
    }

  // Append the requisite number of unsigneds to the stream
  return (theStream->s.appendN((stream *) &(theStream->s),
			       (sizeof(windowEvent) / sizeof(unsigned)),
			       event));
}
