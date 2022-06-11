//
//  Visopsys
//  Copyright (C) 1998-2015 J. Andrew McLaughlin
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
//  kernelImageIco.c
//

// This file contains code for manipulating windows .ico format icon files.

#include "kernelImageIco.h"
#include "kernelError.h"
#include "kernelGraphic.h"
#include "kernelImage.h"
#include "kernelImageBmp.h"
#include "kernelLoader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int detect(const char *fileName, void *dataPtr, unsigned dataSize,
	loaderFileClass *class)
{
	// This function returns 1 and fills the fileClass structure if the data
	// points to an ICO file.

	icoHeader *header = dataPtr;

	if ((fileName == NULL) || (dataPtr == NULL) || (class == NULL))
		return (0);

	// Make sure there's enough data here for our detection
	if (dataSize < (sizeof(icoHeader) + sizeof(icoEntry)))
		return (0);

	// See whether this file seems to be an .ico file
	if ((header->reserved == 0) &&
		(header->type == 1) &&
		(header->numIcons) &&
		(header->entries[0].width) &&
		(header->entries[0].height) &&
		(header->entries[0].reserved == 0) &&
		(header->entries[0].planes == 1) &&
		((header->entries[0].bitCount == BMP_BPP_MONO) ||
			(header->entries[0].bitCount == BMP_BPP_16) ||
			(header->entries[0].bitCount == BMP_BPP_256) ||
			(header->entries[0].bitCount == BMP_BPP_16BIT) ||
			(header->entries[0].bitCount == BMP_BPP_24BIT) ||
			(header->entries[0].bitCount == BMP_BPP_32BIT)))
	{
			// We will say this is an ICO file.
			sprintf(class->className, "%s %s", FILECLASS_NAME_ICO,
				FILECLASS_NAME_IMAGE);
			class->class = (LOADERFILECLASS_BIN | LOADERFILECLASS_IMAGE);
			return (1);
	}
	else
		return (0);
}


static int load(unsigned char *imageFileData, int dataSize, int reqWidth,
	int reqHeight, image *loadImage)
{
	// Processes the data from a raw .ico file and returns it as an image in
	// the closest possible dimensions to those requested (or else, the biggest
	// image if no dimensions are specified).  The memory  must be freed by
	// the caller.

	int status = 0;
	icoHeader *fileHeader = NULL;
	unsigned width = 0;
	unsigned height = 0;
	unsigned dataStart = 0;
	icoEntry *entry = NULL;
	unsigned size = 0;
	icoInfoHeader *infoHeader = NULL;
	unsigned char *palette = NULL;
	pixel *imageData = NULL;
	unsigned fileOffset = 0;
	unsigned fileLineWidth = 0;
	int compression = 0;
	unsigned pixelRowCounter = 0;
	unsigned char colorIndex = 0;
	int colors = 0;
	unsigned pixelCounter = 0;
	int count;

	if ((imageFileData == NULL) || !dataSize || (loadImage == NULL))
		return (status = ERR_NULLPARAMETER);

	// Point our header pointer at the start of the file
	fileHeader = (icoHeader *) imageFileData;

	// Loop through the icon entries.  If a desired height and width was
	// specified, pick the one that's closest.  Otherwise, pick the largest
	// ("highest res") one
	entry = &(fileHeader->entries[0]);
	size = (entry->width * entry->height);
	for (count = 0; count < fileHeader->numIcons; count ++)
	{
		if (reqWidth && reqHeight)
		{
			if (abs((fileHeader->entries[0].width *
					fileHeader->entries[0].height) - (reqWidth * reqHeight)) <
				abs((entry->width * entry->height) - (reqWidth * reqHeight)))
			{
				entry = &(fileHeader->entries[count]);
				size = (entry->width * entry->height);
			}
		}
		else
		{
			if (((unsigned) fileHeader->entries[0].width *
				(unsigned) fileHeader->entries[0].height) > size)
			{
				entry = &(fileHeader->entries[count]);
				size = (entry->width * entry->height);
			}
		}
	}

	infoHeader = ((void *) imageFileData + entry->fileOffset);

	width = entry->width;
	height = entry->height;
	dataStart = (entry->fileOffset + sizeof(icoInfoHeader) +
		(entry->colorCount * 4));
	compression = infoHeader->compression;
	colors = entry->colorCount;

	palette = ((void *) infoHeader + infoHeader->headerSize);

	// Get a blank image of sufficient size
	status = kernelImageNew(loadImage, width, height);
	if (status < 0)
		return (status);

	imageData = loadImage->data;

	// Ok.  Now we need to loop through the bitmap data and turn each bit of
	// data into a pixel.  Note that bitmap data is "upside down" in the file.

	if (infoHeader->bitsPerPixel == BMP_BPP_32BIT)
	{
		// 32-bit bitmap.  Pretty simple, since our image structure's data
		// is a 24-bit bitmap (but the right way up).

		fileLineWidth = (width * 4);

		// This outer loop is repeated once for each row of pixels
		for (count = (height - 1); count >= 0; count --)
		{
			fileOffset = (dataStart + (count * fileLineWidth));

			// This inner loop is repeated for each pixel in a row
			for (pixelRowCounter = 0; pixelRowCounter < width;
				pixelRowCounter++)
			{
				imageData[pixelCounter].blue =
					imageFileData[fileOffset + (pixelRowCounter * 4)];
				imageData[pixelCounter].green =
					imageFileData[fileOffset + (pixelRowCounter * 4) + 1];
				imageData[pixelCounter++].red =
					imageFileData[fileOffset + (pixelRowCounter * 4) + 2];
			}
		}
	}

	else if (infoHeader->bitsPerPixel == BMP_BPP_24BIT)
	{
		// 24-bit bitmap.  Very simple, since our image structure's data
		// is a 24-bit bitmap (but the right way up).

		// There might be padding bytes at the end of a line in the file to
		// make each one have a multiple of 4 bytes
		fileLineWidth = (width * 3);
		if (fileLineWidth % 4)
			fileLineWidth = (fileLineWidth + (4 - (fileLineWidth % 4)));

		// This outer loop is repeated once for each row of pixels
		for (count = (height - 1); count >= 0; count --)
		{
			fileOffset = (dataStart + (count * fileLineWidth));

			// Copy a line of data from the file to our image
			memcpy((((void *) imageData) + ((height - count - 1) *
				(width * 3))), ((void *) imageFileData + fileOffset),
				(width * 3));
		}
	}

	else if (infoHeader->bitsPerPixel == BMP_BPP_256)
	{
		// 8-bit bitmap.  (256 colors)

		if (compression == BMP_COMP_NONE)
		{
			// No compression.  Each sequential byte of data in the file is an
			// index into the color palette (at the end of the header)

			// There might be padding bytes at the end of a line in the file to
			// make each one have a multiple of 4 bytes
			fileLineWidth = width;
			if (fileLineWidth % 4)
			fileLineWidth = (fileLineWidth + (4 - (fileLineWidth % 4)));

			// This outer loop is repeated once for each row of pixels
			for (count = (height - 1); count >= 0; count --)
			{
				fileOffset = (dataStart + (count * fileLineWidth));

				// This inner loop is repeated for each pixel in a row
				for (pixelRowCounter = 0; pixelRowCounter < width;
					pixelRowCounter++)
				{
					// Get the byte that indexes the color
					colorIndex = imageFileData[fileOffset + pixelRowCounter];

					if (colorIndex >= colors)
					{
						kernelError(kernel_error, "Illegal color index %d",
							colorIndex);
						kernelImageFree((image *) &loadImage);
						return (status = ERR_INVALID);
					}

					// Convert it to a pixel
					imageData[pixelCounter].blue = palette[colorIndex * 4];
					imageData[pixelCounter].green =
					palette[(colorIndex * 4) + 1];
					imageData[pixelCounter++].red =
					palette[(colorIndex * 4) + 2];
				}
			}
		}
		else
		{
			// Not supported.  Release the image data memory
			kernelError(kernel_error, "RLE compression not supported");
			kernelImageFree((image *) &loadImage);
			return (status = ERR_INVALID);
		}
	}

	else
	{
		// Not supported.  Release the image data memory
		kernelError(kernel_error, "Unsupported bit depth %d",
			infoHeader->bitsPerPixel);
		kernelImageFree((image *) &loadImage);
		return (status = ERR_INVALID);
	}

	// Set the image's info fields
	loadImage->width = width;
	loadImage->height = height;

	// Assign the image data to the image
	loadImage->data = imageData;

	return (0);
}


kernelFileClass icoFileClass = {
	FILECLASS_NAME_ICO,
	&detect,
	{}
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

kernelFileClass *kernelFileClassIco(void)
{
	// The loader will call this function so that we can return a structure
	// for managing ICO files

	static int filled = 0;

	if (!filled)
	{
		icoFileClass.image.load = &load;
		filled = 1;
	}

	return (&icoFileClass);
}

