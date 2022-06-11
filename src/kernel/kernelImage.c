//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
#include "kernelError.h"
#include <sys/file.h>
#include <string.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelImageLoad(const char *fileName, int reqWidth, int reqHeight,
		    image *loadImage)
{
  int status = 0;
  file theFile;
  unsigned char *imageFileData = NULL;
  loaderFileClass loaderClass;
  kernelFileClass *fileClassDriver = NULL;
  
  // Check params
  if ((fileName == NULL) || (loadImage == NULL))
    return (status = ERR_NULLPARAMETER);

  // Load the image file into memory
  imageFileData = kernelLoaderLoad(fileName, &theFile);
  if (imageFileData == NULL)
    return (status = ERR_NOSUCHENTRY);

  // Get the file class of the file.
  fileClassDriver =
    kernelLoaderClassify(fileName, imageFileData, theFile.size, &loaderClass);
  if (fileClassDriver == NULL)
    {
      kernelError(kernel_error, "File type of %s is unknown", fileName);
      return (status = ERR_INVALID);
    }

  // Is it an image?
  if (!(loaderClass.flags & LOADERFILECLASS_IMAGE))
    {
      kernelError(kernel_error, "%s is not a recognized image format",
		  fileName);
      return (status = ERR_INVALID);
    }

  // Call the appropriate 'load' function
  if (fileClassDriver->image.load)
    return (fileClassDriver->image.load(imageFileData, theFile.size, reqWidth,
					reqHeight, loadImage));
  else
    return (status = ERR_NOTIMPLEMENTED);
}


int kernelImageSave(const char *fileName, int format, image *saveImage)
{
  int status = 0;
  const char *fileClassName = NULL;
  kernelFileClass *fileClassDriver = NULL;

  // Check params
  if ((fileName == NULL) || (saveImage == NULL))
    return (status = ERR_NULLPARAMETER);

  switch (format)
    {
    case IMAGEFORMAT_BMP:
      fileClassName = FILECLASS_NAME_BMP;
      break;
    default:
      kernelError(kernel_error, "Image format %d is unknown", format);
      return (status = ERR_NULLPARAMETER);
      break;
    }

  // Get the file class for the specified format
  fileClassDriver = kernelLoaderGetFileClass(fileClassName);
  if (fileClassDriver == NULL)
    return (status = ERR_INVALID);

  // Call the appropriate 'save' function
  if (fileClassDriver->image.save)
    return (fileClassDriver->image.save(fileName, saveImage));
  else
    return (status = ERR_NOTIMPLEMENTED);
}
