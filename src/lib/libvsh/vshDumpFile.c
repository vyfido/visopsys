//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
//
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  vshDumpFile.c
//

// This contains some useful functions written for the shell

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/api.h>
#include <sys/vsh.h>


_X_ int vshDumpFile(const char *fileName)
{
	// Desc: Print the contents of the file, specified by 'fileName', to standard output.  'fileName' must be an absolute pathname, beginning with '/'.

	int status = 0;
	file theFile;
	char *fileBuffer = NULL;
	unsigned bytes = 0;
	unsigned char tmp = 0;
	unsigned count1, count2;

	// Make sure file name isn't NULL
	if (!fileName)
		return (errno = ERR_NULLPARAMETER);

	memset(&theFile, 0, sizeof(file));

	// Call the "find file" function to see if we can get the first file
	status = fileFind(fileName, &theFile);
	if (status < 0)
		return (errno = status);

	// Make sure the file isn't empty.  We don't want to try reading
	// data from a nonexistent place on the disk.
	if (!theFile.size)
		// It is empty, so just return
		return (status = 0);

	// The file exists and is non-empty.  That's all we care about (we don't
	// care at this point, for example, whether it's a file or a directory.
	// Read it into memory and print it on the screen.

	bytes = theFile.blockSize;

	// Allocate a buffer to store the file blocks in
	fileBuffer = malloc(bytes + 1);
	if (!fileBuffer)
		return (errno = ERR_MEMORY);

	status = fileOpen(fileName, OPENMODE_READ, &theFile);
	if (status < 0)
	{
		free(fileBuffer);
		return (errno = status);
	}

	for (count1 = 0; count1 < theFile.blocks; count1 ++)
	{
		// Read a block
		status = fileRead(&theFile, count1, 1 /* blocks */, fileBuffer);
		if (status < 0)
		{
			free(fileBuffer);
			return (errno = status);
		}

		// Is the last block partially-filled with data?
		if (count1 >= (theFile.blocks - 1))
		{
			if (theFile.size % theFile.blockSize)
				bytes = (theFile.size % theFile.blockSize);
		}

		// NULL-terminate the buffer
		fileBuffer[bytes] = '\0';

		// Print the block, in chunks if necessary
		for (count2 = 0; count2 < bytes; count2 += MAXSTRINGLENGTH)
		{
			if ((bytes - count2) > MAXSTRINGLENGTH)
			{
				tmp = fileBuffer[count2 + MAXSTRINGLENGTH];
				fileBuffer[count2 + MAXSTRINGLENGTH] = '\0';
			}

			textPrint(fileBuffer + count2);

			if ((bytes - count2) > MAXSTRINGLENGTH)
				fileBuffer[count2 + MAXSTRINGLENGTH] = tmp;
		}
	}

	// If the file did not end with a newline character...
	if (fileBuffer[bytes - 1] != '\n')
		textNewline();

	// Free the memory
	free(fileBuffer);

	return (status = 0);
}

