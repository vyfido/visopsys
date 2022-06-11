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
//  kernelLoaderClass.c
//

// This file contains miscellaneous functions for various classes of files
// that don't have their own source files.

#include "kernelLoader.h"
#include <stdio.h>


static int textDetect(const char *fileName, void *dataPtr, int size,
		      loaderFileClass *class)
{
  // This function returns the percentage of characters that are plain text
  // if we think the supplied file data represents a text file.

  unsigned char *data = dataPtr;
  int textChars = 0;
  int count;

  if ((fileName == NULL) || (dataPtr == NULL) || !size || (class == NULL))
    return (0);

  // Loop through the supplied data.  If it's at least 90% (this is an
  // arbitrary number) printable ascii, etc, say yes
  for (count = 0; count < size; count ++)
    if ((data[count] == 0x0a) || (data[count] == 0x0d) ||
	(data[count] == 0x09) ||
	((data[count] >= 0x20) && (data[count] <= 0x7e)))
      textChars += 1;

  if (((textChars * 100) / size) >= 90)
    {
      // We will call this a text file.
      sprintf(class->className, "%s %s", FILECLASS_NAME_TEXT,
	      FILECLASS_NAME_DATA);
      class->flags = (LOADERFILECLASS_TEXT | LOADERFILECLASS_DATA);
      return (1);
    }
  else
    // No
    return (0);
}


static int configDetect(const char *fileName, void *data, int size,
			loaderFileClass *class)
{
  // Detect whether we think this is a Visopsys configuration file.

  // Config files are text
  char *dataPtr = data;
  int totalLines = 0;
  int configLines = 0;
  int haveEquals = 0;
  int count;

  if (!textDetect(fileName, dataPtr, size, class))
    return (0);

  // Loop through the lines.  Each one should either start with a comment,
  // or a newline, or be of the form variable=value.
  dataPtr = data;
  for (count = 0; count < size; count ++)
    {
      totalLines += 1;

      if (dataPtr[count] == '\n')
	{
	  configLines += 1;
	  continue;
	}

      if (dataPtr[count] == '#')
	{
	  // Go to the end of the line
	  while ((count < size) && (dataPtr[count] != '\n') &&
		 (dataPtr[count] != '\0'))
	    count ++;

	  configLines += 1;
	  continue;
	}

      // It doesn't start with a comment or newline.  It should be of the
      // form variable=value
      haveEquals = 0;
      while (count < size)
	{
	  if (dataPtr[count] == '=')
	    haveEquals += 1;

	  if ((dataPtr[count] == '\n') || (dataPtr[count] == '\0'))
	    {
	      if (haveEquals == 1)
		configLines += 1;
	      break;
	    }

	  count ++;
	}
    }

  if (((configLines * 100) / totalLines) >= 95)
    {
      sprintf(class->className, "%s %s", FILECLASS_NAME_CONFIG,
	      FILECLASS_NAME_DATA);
      class->flags =
	(LOADERFILECLASS_CONFIG | LOADERFILECLASS_TEXT | LOADERFILECLASS_DATA);
      return (1);
    }
  else
    return (0);
}


static int binaryDetect(const char *fileName, void *dataPtr, int size,
			loaderFileClass *class)
{
  // If it's not text, it's binary

  if (!textDetect(fileName, dataPtr, size, class))
    {
      sprintf(class->className, "%s %s", FILECLASS_NAME_BIN,
	      FILECLASS_NAME_DATA);
      class->flags = (LOADERFILECLASS_BIN | LOADERFILECLASS_DATA);
      return (1);
    }
  else
    return (0);
}


static int bootDetect(const char *fileName, void *dataPtr, int size,
		      loaderFileClass *class)
{
  // Must be binary, and have the boot signature 'AA55' in the last 2 bytes

  unsigned short *sig = (dataPtr + 510);

  if (binaryDetect(fileName, dataPtr, size, class) && (*sig == 0xAA55))
    {
      sprintf(class->className, "%s %s", FILECLASS_NAME_BOOT,
	      FILECLASS_NAME_EXEC);
      class->flags = (LOADERFILECLASS_BIN | LOADERFILECLASS_STATIC |
		      LOADERFILECLASS_EXEC | LOADERFILECLASS_BOOT);
      return (1);
    }
  else
    return (0);
}


// Config files.	
kernelFileClass configFileClass = {
  FILECLASS_NAME_CONFIG,
  &configDetect,
  { }
};

// Boot files.	
kernelFileClass bootFileClass = {
  FILECLASS_NAME_BOOT,
  &bootDetect,
  { }
};

// Text files.	
kernelFileClass textFileClass = {
  FILECLASS_NAME_TEXT,
  &textDetect,
  { }
};

// Binary files.	
kernelFileClass binaryFileClass = {
  FILECLASS_NAME_BIN,
  &binaryDetect,
  { }
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelFileClass *kernelFileClassConfig(void)
{
  // The loader will call this function so that we can return a structure
  // for managing config files
  return (&configFileClass);
}


kernelFileClass *kernelFileClassBoot(void)
{
  // The loader will call this function so that we can return a structure
  // for managing boot sector files
  return (&bootFileClass);
}


kernelFileClass *kernelFileClassText(void)
{
  // The loader will call this function so that we can return a structure
  // for managing text files
  return (&textFileClass);
}


kernelFileClass *kernelFileClassBinary(void)
{
  // The loader will call this function so that we can return a structure
  // for managing binary files
  return (&binaryFileClass);
}
