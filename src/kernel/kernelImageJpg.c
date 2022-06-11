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
//  kernelImageJpg.c
//

// This file contains code for loading, saving, and converting images in
// the JPEG (.jpg) format.

#include "kernelImage.h"
#include "kernelImageJpg.h"
#include "kernelLoader.h"
#include <stdio.h>


static int detect(const char *fileName, void *dataPtr, int dataLength,
		  loaderFileClass *class)
{
  // This function returns 1 and fills the fileClass structure if the data
  // points to an JPEG file.

  char jpgStart[] = { 0xFF, JPG_SOI, 0xFF, JPG_APP0 };
  jpgHeader *header = (dataPtr + 4);

  if ((fileName == NULL) || (dataPtr == NULL) || !dataLength ||
      (class == NULL))
    return (0);

  // See whether this file claims to be a JPEG file
  if (!memcmp(dataPtr, jpgStart, 4) &&
      !strncmp(header->identifier, JPG_APP0_MARK, 4))
    {
      // We will say this is a JPG file.
      sprintf(class->className, "%s %s", FILECLASS_NAME_JPG,
	      FILECLASS_NAME_IMAGE);
      class->flags = (LOADERFILECLASS_BIN | LOADERFILECLASS_IMAGE);
      return (1);
    }
  else
    return (0);
}


kernelFileClass jpgFileClass = {
  FILECLASS_NAME_JPG,
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


kernelFileClass *kernelFileClassJpg(void)
{
  // The loader will call this function so that we can return a structure
  // for managing JPEG files
  return (&jpgFileClass);
}
