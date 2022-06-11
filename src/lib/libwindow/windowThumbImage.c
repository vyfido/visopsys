// 
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  windowThumbImage.c
//

// This contains functions for user programs to operate GUI components.

#include <errno.h>
#include <sys/api.h>
#include <sys/window.h>

extern int libwindow_initialized;
extern void libwindowInitialize(void);


static int getImage(const char *fileName, image *imageData, unsigned maxWidth,
		    unsigned maxHeight)
{
  int status = 0;
  float scale = 0;
  unsigned thumbWidth = 0;
  unsigned thumbHeight = 0;

  if (fileName)
    status = imageLoad(fileName, 0, 0, imageData);
  else
    status = imageNew(imageData, maxWidth, maxHeight);
  if (status < 0)
    return (status);

  // Scale the image
  thumbWidth = imageData->width;
  thumbHeight = imageData->height;

  // Presumably we need to shrink it?
  if (thumbWidth > maxWidth)
    {
      scale = ((float) maxWidth / (float) thumbWidth);
      thumbWidth = (unsigned) ((float) thumbWidth * scale);
      thumbHeight = (unsigned) ((float) thumbHeight * scale);
    }
  if (thumbHeight > maxHeight)
    {
      scale = ((float) maxHeight / (float) thumbHeight);
      thumbWidth = (unsigned) ((float) thumbWidth * scale);
      thumbHeight = (unsigned) ((float) thumbHeight * scale);
    }

  if ((thumbWidth != imageData->width) || (thumbHeight != imageData->height))
    status = imageResize(imageData, thumbWidth, thumbHeight);
  else
    status = 0;

  return (status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ objectKey windowNewThumbImage(objectKey parent, const char *fileName, unsigned maxWidth, unsigned maxHeight, componentParameters *params)
{
  // Desc: Create a new window image component from the supplied image file name 'fileName', with the given 'parent' window or container, and component parameters 'params'.  Dimension values 'maxWidth' and 'maxHeight' constrain the maximum image size.  The resulting image will be scaled down, if necessary, with the aspect ratio intact.  If 'fileName' is NULL, a blank image will be created.

  int status = 0;
  image imageData;
  objectKey thumbImage = NULL;

  if (!libwindow_initialized)
    libwindowInitialize();

  // Check params.  File name can be NULL.
  if ((parent == NULL) || !maxWidth || !maxHeight || (params == NULL))
    {
      errno = ERR_NULLPARAMETER;
      return (thumbImage = NULL);
    }

  status = getImage(fileName, &imageData, maxWidth, maxHeight);

  if (status >= 0)
    {
      thumbImage = windowNewImage(parent, &imageData, draw_normal, params);
      if (thumbImage == NULL)
	errno = ERR_NOCREATE;
    }

  imageFree(&imageData);
  return (thumbImage);
}


_X_ int windowThumbImageUpdate(objectKey thumbImage, const char *fileName, unsigned maxWidth, unsigned maxHeight)
{
  // Desc: Update an existing window image component 'thumbImage', previously created with a call to windowNewThumbImage(), from the supplied image file name 'fileName'.  Dimension values 'maxWidth' and 'maxHeight' constrain the maximum image size.  The resulting image will be scaled down, if necessary, with the aspect ratio intact.  If 'fileName' is NULL, the image will become blank.

  int status = 0;
  image imageData;

  if (!libwindow_initialized)
    libwindowInitialize();

  // Check params.  File name can be NULL.
  if ((thumbImage == NULL) || !maxWidth || !maxHeight)
    return (status = ERR_NULLPARAMETER);

  status = getImage(fileName, &imageData, maxWidth, maxHeight);

  if (status >= 0)
    status = windowComponentSetData(thumbImage, &imageData, sizeof(image));

  imageFree(&imageData);
  return (status);
}
