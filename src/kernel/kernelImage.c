//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  kernelImage.c
//

// This file contains code for loading, saving, and converting images
// with various file formats

#include "kernelImage.h"
#include "kernelLoader.h"
#include "kernelMemoryManager.h"
#include "kernelMalloc.h"
#include "kernelFile.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>

// For the image file types that we support
#include "kernelImageFormatBmp.h" // Info about the .bmp format


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

// There is no initialization needed here.  These are standalone utility
// functions


int kernelImageLoadBmp(const char *filename, image *loadImage)
{
  // Loads a .bmp file and returns it as an image.  The memory
  // for this and its data must be freed by the caller.

  int status = 0;
  file theFile;
  unsigned char *imageFileData = NULL;
  bmpHeader *header = NULL;
  unsigned width = 0;
  unsigned height = 0;
  unsigned dataStart = 0;
  int compression = 0;
  int colors = 0;
  unsigned char *palette = NULL;
  unsigned fileOffset = 0;
  unsigned fileLineWidth = 0;
  unsigned pixelCounter = 0;
  unsigned pixelRowCounter = 0;
  unsigned char colorIndex = 0;
  pixel *imageData = NULL;
  int count;

  // Make sure the filename and image aren't NULL
  if ((filename == NULL) || (loadImage == NULL))
    return (status = ERR_NULLPARAMETER);

  // Load the image file into memory
  imageFileData = kernelLoaderLoad(filename, &theFile);
  if (imageFileData == NULL)
    return (status = ERR_NOSUCHENTRY);

  // Make sure that the file claims to be a bitmap file
  if (strncmp(imageFileData, "BM", 2))
    {
      // Guess it's not a bitmap
      // Release the file data memory
      kernelMemoryRelease(imageFileData);
      return (status = ERR_INVALID);
    }

  // Point our header pointer at the start of the file
  header = (bmpHeader *) (imageFileData + 2);

  width = header->width;
  height = header->height;
  dataStart = header->dataStart;
  compression = header->compression;
  colors = header->colors;

  palette = imageFileData + sizeof(bmpHeader) + 2;

  // Figure out how much memory we need for the array of pixels that
  // we'll attach to the image, and allocate it.  The size is a
  // product of the image height and width.
  loadImage->pixels = (width * height);
  loadImage->dataLength = (loadImage->pixels * sizeof(pixel));

  imageData = kernelMemoryGet(loadImage->dataLength, "image data");
  if (imageData == NULL)
    {
      // Release the file data memory
      kernelMemoryRelease(imageFileData);
      return (status = ERR_MEMORY);
    }

  // Ok.  Now we need to loop through the bitmap data and turn each bit
  // of data into a pixel.  The method we use will depend on whether
  // the image is compressed, and if so, the method used.  Note that bitmap
  // data is "upside down" in the file.

  if (header->bitsPerPixel == BMP_BPP_24BIT)
    {
      // 24-bit bitmap.  Very simple, since our image structure's data
      // is pretty much a bitmap (but the right way up).

      // There might be padding bytes at the end of a line in the file to make
      // each one have a multiple of 4 bytes
      fileLineWidth = (width * 3);
      if (fileLineWidth % 4)
	fileLineWidth = (fileLineWidth + (4 - (fileLineWidth % 4)));

      // This outer loop is repeated once for each row of pixels
      for (count = (height - 1); count >= 0; count --)
	{
	  fileOffset = (dataStart + (count * fileLineWidth));

	  // Copy a line of data from the file to our image
	  kernelMemCopy((imageFileData + fileOffset),
			(((void *) imageData) +
			 ((height - count - 1) * (width * 3))),
			(width * 3));
	}
    }

  else if (header->bitsPerPixel == BMP_BPP_256)
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
		      kernelMemoryRelease(imageFileData);
		      kernelMemoryRelease(imageData);
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
	  // Not supported.  Release the file data and image data memory
	  kernelMemoryRelease(imageFileData);
	  kernelMemoryRelease(imageData);
	  return (status = ERR_INVALID);
	}
    }
  else
    {
      // Not supported.  Release the file data and image data memory
      kernelMemoryRelease(imageFileData);
      kernelMemoryRelease(imageData);
      return (status = ERR_INVALID);
    }

  // Release the file data memory
  kernelMemoryRelease(imageFileData);

  // Set the image's info fields
  loadImage->width = width;
  loadImage->height = height;

  // Assign the image data to the image
  loadImage->data = imageData;

  // Success
  return (status = 0);
}


int kernelImageSaveBmp(const char *filename, image *saveImage)
{
  // Saves a kernel image format to a .bmp file

  int status = 0;
  int padBytes = 0;
  bmpHeader header;
  unsigned dataSize = 0;
  unsigned char *fileData = NULL;
  unsigned char *imageData = NULL;
  file theFile;
  int count;

  // Make sure the filename and image aren't NULL
  if ((filename == NULL) || (saveImage == NULL))
    return (status = ERR_NULLPARAMETER);

  // Do we need to pad each line of the image with extra bytes?  The file
  // data needs to be on doubleword boundaries.
  if ((saveImage->width * 3) % 4)
    padBytes = 4 - ((saveImage->width * 3) % 4);

  // The data size is number of lines, times line width + pad bytes
  dataSize = ((saveImage->width * 3) + padBytes) * saveImage->height;

  // Start filling in the bitmap header using the image information

  header.size = 2 + sizeof(bmpHeader) + dataSize;
  header.reserved = 0;
  header.dataStart = 2 + sizeof(bmpHeader);
  header.headerSize = 0x28;
  header.width = saveImage->width;
  header.height = saveImage->height;
  header.planes = 1;
  header.bitsPerPixel = BMP_BPP_24BIT;
  header.compression = BMP_COMP_NONE;
  header.dataSize = dataSize;
  header.hResolution = 7800;  // ?!? Whatever
  header.vResolution = 7800;  // ?!? Whatever
  header.importantColors = 0;

  // Get memory for the file
  fileData = kernelMalloc(header.size);
  if (fileData == NULL)
    {
      kernelError(kernel_error, "Unable to allocate memory for bitmap file");
      return (status = ERR_MEMORY);
    }

  // Set a pointer to the start of the image data
  imageData = fileData + header.dataStart;

  for (count = (saveImage->height - 1); count >= 0 ; count --)
    {
      kernelMemCopy((saveImage->data + (count * (saveImage->width * 3))),
		    imageData, (saveImage->width * 3));
      kernelMemClear((imageData + (saveImage->width * 3)), padBytes);

      // Move to the next line
      imageData += ((saveImage->width * 3) + padBytes);
    }

  // This needs to be set after we've processed the data
  header.colors = 0;

  // Now copy the 'magic number' into the file area
  fileData[0] = 'B'; fileData[1] = 'M';

  // Copy the header data into the file area
  kernelMemCopy(&header, (fileData + 2), sizeof(bmpHeader));

  // Now create/open the file stream for writing
  status = kernelFileOpen(filename, (OPENMODE_WRITE | OPENMODE_TRUNCATE |
	  			     OPENMODE_CREATE), &theFile);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to open %s for writing", filename);
      // Free the file data
      kernelFree(fileData);
      return (status);
    }

  // Write the file
  status = kernelFileWrite(&theFile, 0, (header.size / theFile.blockSize) +
	  		   ((header.size % theFile.blockSize)? 1 : 0),
	  		   fileData);

  // Free the file data
  kernelFree(fileData);

  if (status < 0)
    {
      kernelError(kernel_error, "Unable to write %s", filename);
      return (status);
    }

  // Close the file
  kernelFileClose(&theFile);

  // Done
  return (status = 0);
}
