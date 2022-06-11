//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  kernelPipe.c
//

// This file contains the kernel's facilities for reading and writing
// inter-process communication pipes using a 'streams' abstraction

#include "kernelPipe.h"
#include "kernelError.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelStream.h"
#include <stdlib.h>
#include <sys/vis.h>

linkedList *pipes = NULL;


static int destroy(kernelPipe *pipe)
{
	// Destroys a pipe structure (without a permissions check)

	int status = 0;

	// Destroy the stream
	status = kernelStreamDestroy(&pipe->s);
	if (status < 0)
		return (status);

	// Remove it from our list
	status = linkedListRemove(pipes, pipe);
	if (status < 0)
		return (status);

	// Free memory
	kernelFree(pipe);

	return (status = 0);
}


static void purgePipes(void)
{
	// Iterate through our list of pipes and ensure that the processes using
	// them are still alive

	linkedListItem *listItem = NULL;
	kernelPipe *pipe = NULL;

	pipe = linkedListIterStart(pipes, &listItem);

	while (pipe)
	{
		if (!kernelMultitaskerProcessIsAlive(pipe->creatorPid) &&
			!kernelMultitaskerProcessIsAlive(pipe->readerPid) &&
			!kernelMultitaskerProcessIsAlive(pipe->writerPid))
		{
			destroy(pipe);
		}

		pipe = linkedListIterNext(pipes, &listItem);
	}
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

kernelPipe *kernelPipeNew(unsigned num, unsigned size)
{
	// Initializes and returns a pointer to a new pipe structure

	kernelPipe *pipe = NULL;

	// Check params
	if (!num || !size)
	{
		kernelError(kernel_error, "NULL parameter");
		return (pipe = NULL);
	}

	if (!pipes)
	{
		// Get memory for our list of pipes
		pipes = kernelMalloc(sizeof(linkedList));
		if (!pipes)
		{
			kernelError(kernel_error, "Memory error creating pipe");
			return (pipe = NULL);
		}
	}
	else
	{
		// Get rid of any old pipes
		purgePipes();
	}

	// Get memory for the pipe itself
	pipe = kernelMalloc(sizeof(kernelPipe));
	if (!pipe)
	{
		kernelError(kernel_error, "Memory error creating pipe");
		return (pipe = NULL);
	}

	pipe->itemSize = size;

	// Set all the PIDs to the caller
	pipe->creatorPid = pipe->readerPid = pipe->writerPid =
		kernelCurrentProcess->processId;

	pipe->streamSize = itemsize_byte;
	if (!(pipe->itemSize & 3))
	{
		// More efficiency if the size is a multiple of dwords
		pipe->itemSize >>= 2;
		pipe->streamSize = itemsize_dword;
	}

	// Get a new stream and attach it to the pipe structure
	if (kernelStreamNew(&pipe->s, (num * pipe->itemSize), pipe->streamSize)
		< 0)
	{
		kernelError(kernel_error, "Unable to create the pipe stream");
		kernelFree(pipe);
		return (pipe = NULL);
	}

	// Add the pipe to our list
	if (linkedListAddBack(pipes, pipe) < 0)
	{
		kernelError(kernel_error, "Couldn't add pipe to list");
		kernelStreamDestroy(&pipe->s);
		kernelFree(pipe);
		return (pipe = NULL);
	}

	return (pipe);
}


int kernelPipeDestroy(kernelPipe *pipe)
{
	// Destroys a pipe structure

	int status = 0;

	// Check params
	if (!pipe)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check permissions.  Only the creator can destroy this way.
	if (kernelCurrentProcess->processId != pipe->creatorPid)
	{
		kernelError(kernel_error, "Pipe permission denied");
		return (status = ERR_PERMISSION);
	}

	return (status = destroy(pipe));
}


int kernelPipeSetReader(kernelPipe *pipe, int pid)
{
	// Set the ID of the process that is allowed to read from the pipe

	int status = 0;

	// Check params
	if (!pipe)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check permissions.  Only the creator can set this.
	if (kernelCurrentProcess->processId != pipe->creatorPid)
	{
		kernelError(kernel_error, "Pipe permission denied");
		return (status = ERR_PERMISSION);
	}

	pipe->readerPid = pid;

	return (status = 0);
}


int kernelPipeSetWriter(kernelPipe *pipe, int pid)
{
	// Set the ID of the process that is allowed to write to the pipe

	int status = 0;

	// Check params
	if (!pipe)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check permissions.  Only the creator can set this.
	if (kernelCurrentProcess->processId != pipe->creatorPid)
	{
		kernelError(kernel_error, "Pipe permission denied");
		return (status = ERR_PERMISSION);
	}

	pipe->writerPid = pid;

	return (status = 0);
}


int kernelPipeClear(kernelPipe *pipe)
{
	// Clear data from the pipe

	// Check params
	if (!pipe)
	{
		kernelError(kernel_error, "NULL parameter");
		return (ERR_NULLPARAMETER);
	}

	// Check permissions
	if (kernelCurrentProcess->processId != pipe->writerPid)
	{
		kernelError(kernel_error, "Pipe permission denied");
		return (ERR_PERMISSION);
	}

	// Clear the data from the stream
	return (pipe->s.clear(&pipe->s));
}


int kernelPipeRead(kernelPipe *pipe, unsigned num, void *buffer)
{
	// Read data from the pipe

	int status = 0;

	// Check params
	if (!pipe || !num || !buffer)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check permissions
	if (kernelCurrentProcess->processId != pipe->readerPid)
	{
		kernelError(kernel_error, "Pipe permission denied");
		return (status = ERR_PERMISSION);
	}

	// Only read complete items
	num = min(num, (pipe->s.count / pipe->itemSize));

	// Read the data from the stream
	status = pipe->s.popN(&pipe->s, (num * pipe->itemSize), buffer);
	if (status <= 0)
		return (status);

	return (status / pipe->itemSize);
}


int kernelPipeWrite(kernelPipe *pipe, unsigned num, void *buffer)
{
	// Write data to the pipe

	int status = 0;

	// Check params
	if (!pipe || !num || !buffer)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check permissions
	if (kernelCurrentProcess->processId != pipe->writerPid)
	{
		kernelError(kernel_error, "Pipe permission denied");
		return (status = ERR_PERMISSION);
	}

	// Write the data to the stream
	status = pipe->s.appendN(&pipe->s, (num * pipe->itemSize), buffer);

	return (status);
}

