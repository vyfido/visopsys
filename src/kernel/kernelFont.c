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
//  kernelFont.c
//

// This contains utility functions for managing fonts.

#include "kernelFont.h"
#include "kernelMalloc.h"
#include "kernelMemoryManager.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>

static int initialized = 0;

// This specifies the default system font, built in.  Very simple.

static unsigned char chars[][8] = {
  { 0, 0, 0, 0, 0, 0, 0, 0 }, // space
  { 48, 48, 48, 48, 48, 0, 48, 0 }, // !
  { 40, 40, 0, 0, 0, 0, 0, 0 }, // "
  { 40, 40, 254, 40, 254, 40, 40, 0 }, // #
  { 16, 254, 144, 254, 18, 254, 16, 0 }, // $
  { 66, 164, 72, 16, 36, 74, 132, 0 }, // %
  { 112, 80, 48, 54, 76, 140, 114, 0 }, // &
  { 6, 12, 0, 0, 0, 0, 0, 0 }, // '
  { 8, 16, 32, 32, 32, 16, 8, 0 }, // (
  { 32, 16, 8, 8, 8, 16, 32, 0 }, // )
  { 16, 84, 56, 254, 56, 84, 16, 0 }, // *
  { 0, 16, 16, 124, 16, 16, 0, 0 }, // +
  { 0, 0, 0, 0, 0, 24, 48, 0 }, // ,
  { 0, 0, 0, 124, 0, 0, 0, 0 }, // -
  { 0, 0, 0, 0, 0, 24, 24, 0 }, // .
  { 2, 4, 8, 16, 32, 64, 128, 0 }, // /
  { 56, 68, 130, 130, 130, 68, 56, 0 }, // 0
  { 16, 48, 16, 16, 16, 16, 56, 0 }, // 1
  { 124, 130, 2, 124, 128, 128, 254, 0 }, // 2
  { 60, 66, 2, 12, 2, 66, 60, 0 }, // 3
  { 24, 40, 72, 136, 252, 8, 8, 0 }, // 4
  { 252, 128, 128, 252, 2, 2, 252, 0 }, // 5
  { 124, 128, 128, 252, 130, 130, 124, 0 }, // 6
  { 254, 2, 2, 4, 8, 16, 32, 0 }, // 7
  { 124, 130, 130, 124, 130, 130, 124, 0 }, // 8
  { 124, 130, 130, 124, 2, 2, 124, 0 }, // 9
  { 0, 0, 0, 24, 0, 24, 0, 0 }, // :
  { 0, 0, 0, 24, 0, 24, 48, 0 }, // ;
  { 6, 24, 96, 128, 96, 24, 6, 0 }, // <
  { 0, 0, 0, 124, 0, 124, 0, 0 }, // =
  { 192, 48, 12, 2, 12, 48, 192, 0 }, // >
  { 56, 70, 2, 4, 24, 0, 16, 0 }, // ?
  { 60, 66, 92, 84, 92, 64, 62, 0 }, // @
  { 16, 108, 130, 130, 254, 130, 130, 0 }, // A
  { 252, 130, 132, 248, 132, 130, 252, 0 }, // B
  { 124, 130, 128, 128, 128, 130, 124, 0 }, // C
  { 248, 68, 66, 66, 66, 68, 248, 0 }, // D
  { 254, 128, 128, 252, 128, 128, 254, 0 }, // E
  { 254, 128, 128, 252, 128, 128, 128, 0 }, // F
  { 126, 128, 128, 142, 130, 130, 124, 0 }, // G
  { 130, 130, 130, 254, 130, 130, 130, 0 }, // H
  { 124, 16, 16, 16, 16, 16, 124, 0 }, // I
  { 62, 4, 4, 4, 4, 68, 56, 0 }, // J
  { 130, 140, 144, 224, 144, 140, 130, 0 }, // K
  { 128, 128, 128, 128, 128, 128, 254, 0 }, // L
  { 198, 170, 146, 146, 130, 130, 130, 0 }, // M
  { 130, 194, 162, 146, 138, 134, 130, 0 }, // N
  { 124, 130, 130, 130, 130, 130, 124, 0 }, // O
  { 124, 130, 130, 252, 128, 128, 128, 0 }, // P
  { 120, 132, 132, 132, 140, 132, 122, 0 }, // Q
  { 252, 130, 130, 252, 132, 130, 130, 0 }, // R
  { 124, 130, 128, 124, 2, 2, 252, 0 }, // S
  { 254, 16, 16, 16, 16, 16, 16, 0 }, // T
  { 130, 130, 130, 130, 130, 130, 124, 0 }, // U
  { 130, 130, 130, 130, 68, 40, 16, 0 }, // V
  { 130, 130, 130, 130, 146, 170, 198, 0 }, // W
  { 130, 68, 40, 16, 40, 68, 130, 0 }, // X
  { 130, 68, 40, 16, 16, 16, 16, 0 }, // Y
  { 254, 4, 8, 16, 32, 64, 254, 0 }, // Z
  { 124, 64, 64, 64, 64, 64, 124, 0 }, // [
  { 2, 4, 8, 16, 32, 64, 128, 0 }, // /
  { 124, 4, 4, 4, 4, 4, 124, 0 }, // ]
  { 16, 40, 68, 0, 0, 0, 0, 0 }, // ^
  { 0, 0, 0, 0, 0, 0, 0, 254 }, // _
  { 96, 48, 0, 0, 0, 0, 0, 0 }, // `
  { 0, 0, 120, 4, 124, 132, 126, 0 }, // a
  { 128, 128, 252, 130, 130, 130, 124, 0 }, // b
  { 0, 0, 124, 128, 128, 128, 124, 0 }, // c
  { 2, 2, 126, 130, 130, 130, 126, 0 }, // d
  { 0, 0, 124, 130, 254, 128, 126, 0 }, // e
  { 60, 66, 64, 64, 240, 64, 64, 0 }, // f
  { 0, 0, 124, 130, 130, 126, 2, 124 }, // g
  { 128, 128, 252, 130, 130, 130, 130, 0 }, // h
  { 16, 0, 48, 16, 16, 16, 56, 0 }, // i
  { 8, 0, 24, 8, 8, 8, 72, 48 }, // j
  { 0, 128, 140, 240, 136, 132, 132, 0 }, // k
  { 16, 16, 16, 16, 16, 16, 56, 0 }, // l
  { 0, 0, 68, 170, 146, 146, 130, 0 }, // m
  { 0, 0, 252, 130, 130, 130, 130, 0 }, // n
  { 0, 0, 124, 130, 130, 130, 124, 0 }, // o
  { 0, 0, 124, 130, 130, 252, 128, 128 }, // p
  { 0, 0, 124, 130, 130, 126, 2, 2 }, // q
  { 0, 0, 124, 130, 128, 128, 128, 0 }, // r
  { 0, 0, 124, 128, 124, 2, 252, 0 }, // s
  { 32, 32, 252, 32, 32, 34, 28, 0 }, // t
  { 0, 0, 132, 132, 132, 132, 122, 0 }, // u
  { 0, 0, 68, 68, 68, 40, 16, 0 }, // v
  { 0, 0, 130, 146, 146, 170, 68, 0 }, // w
  { 0, 0, 68, 40, 16, 40, 68, 0 }, // x
  { 0, 0, 68, 68, 68, 56, 4, 120 }, // y
  { 0, 0, 124, 8, 16, 32, 124, 0 }, // z
  { 24, 32, 32, 96, 32, 32, 24, 0 }, // {
  { 16, 16, 16, 16, 16, 16, 16, 0 }, // |
  { 48, 8, 8, 12, 8, 8, 48, 0 },  // }
  { 64, 168, 168, 16, 0, 0, 0, 0 }  // ~
};

static kernelAsciiFont systemFont;

static kernelAsciiFont *defaultFont = NULL;
static kernelAsciiFont *fontList[MAX_FONTS];
static int numFonts = 0;

static inline int asciiToIndex(int ascii)
{
  return (ascii - 32);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelFontInitialize(void)
{
  // Initialize the font functions

  int status = 0;
  int count;

  // Clear out our font list
  kernelMemClear(&fontList, (sizeof(kernelAsciiFont *) * MAX_FONTS));

  // Create the default system font

  strcpy(systemFont.name, "system");

  for (count = 0; count < ASCII_PRINTABLES; count ++)
    {
      systemFont.chars[count].type = IMAGETYPE_MONO;
      systemFont.chars[count].pixels = 64;
      systemFont.chars[count].width = 8;
      systemFont.chars[count].height = 8;
      systemFont.chars[count].dataLength = 8;
      systemFont.chars[count].data = chars[count];
    }
  
  systemFont.charWidth = 8;
  systemFont.charHeight = 8;

  // Set the system font to be our default
  defaultFont = &systemFont;

  initialized = 1;

  return (status = 0);
}


int kernelFontGetDefault(kernelAsciiFont **pointer)
{
  // Return a pointer to the default system font
  
  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (pointer == NULL)
    return (status = ERR_NULLPARAMETER);

  else
    *pointer = defaultFont;

  return (status = 0);
}


int kernelFontSetDefault(const char *name)
{
  // Sets the default font to one we've already got in our list
  
  int status = 0;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (name == NULL)
    return (status = ERR_NULLPARAMETER);

  for (count = 0; count < numFonts; count ++)
    if (!strcmp(fontList[count]->name, name))
      {
	defaultFont = fontList[count];
	break;
      }
  
  if (count == numFonts)
    {
      kernelError(kernel_error, "Unable to find the font \"%s\"", name);
      return (status = ERR_NOSUCHENTRY);
    }
  else
    return (status = 0);
}


int kernelFontLoad(const char* filename, const char *fontname,
		   kernelAsciiFont **pointer, int fixedWidth)
{
  // Takes the name of a bitmap file containing a font definition and turns
  // it into our internal representation of a kernelAsciiFont.  The bitmap
  // should have pure green as its background; every other color gets turned
  // 'on' in our mono font scheme.  If the operation is successful the
  // supplied pointer is set to point to the new font.

  int status = 0;
  image fontImage;
  unsigned charWidth, charHeight, charBytes;
  int pixels;
  kernelAsciiFont *newFont = NULL;
  pixel *imageData = NULL;
  unsigned sourcePixel = 0;
  unsigned char *fontData = NULL;
  unsigned firstOnPixel, lastOnPixel, currentPixel;
  int count1, count2, count3;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((filename == NULL) || (fontname == NULL) || (pointer == NULL))
    return (status = ERR_NULLPARAMETER);
  
  // Until we've accomplished everything successfully...
  *pointer = NULL;

  // Check to see if its been loaded already.  If not, just return it.
  for (count1 = 0; count1 < numFonts; count1 ++)
    if (!strcmp(fontList[count1]->name, fontname))
      {
	// Already a font by that name.
	*pointer = fontList[count1];
	return (status = 0);
      }

  // Don't exceed MAX_FONTS
  if (numFonts >= MAX_FONTS)
    return (status = ERR_NOFREE);

  // Try to load the font bitmap
  status = kernelImageLoadBmp(filename, &fontImage);
  if (status < 0)
    return (status = ERR_NOSUCHFILE);

  // The font file is a "vertical" concatenation of the character images.
  // The width of the bitmap image describes the width of a character, whereas
  // the height of a character is the division of the bitmap height and the
  // ASCII_PRINTABLES value
  charWidth = fontImage.width;
  charHeight = (fontImage.height / ASCII_PRINTABLES);
  // How many bytes per char in our mono version?
  charBytes = ((charWidth * charHeight) / 8);
  if ((charWidth * charHeight) % 8)
    charBytes += 1;

  // Get memory for the font structure and the images data.
  newFont = kernelMalloc(sizeof(kernelAsciiFont));
  fontData = kernelMalloc(charBytes * ASCII_PRINTABLES);
  if ((newFont == NULL) || (fontData == NULL))
    {
      kernelError(kernel_error, "Unable to get memory to hold the font data");
      return (status = ERR_MEMORY);
    }

  // Set some values in the new font
  strcpy(newFont->name, fontname);
  newFont->charWidth = charWidth;
  newFont->charHeight = charHeight;

  // Loop through all of the composite image data, turning it into a mono
  // bitmap.
  
  imageData = fontImage.data;

  // Now we do a loop to separate the individual characters from the composite
  // image data, turning them into mono bitmaps as we go.
  for (count1 = 0; count1 < ASCII_PRINTABLES; count1 ++)
    {
      // Stuff that won't change in the rest of the code for this character,
      // below (things like width can change -- see below)
      newFont->chars[count1].type = IMAGETYPE_MONO;
      newFont->chars[count1].width = charWidth;
      newFont->chars[count1].height = charHeight;
      newFont->chars[count1].pixels = (charWidth * charHeight);
      newFont->chars[count1].dataLength = charBytes;
      newFont->chars[count1].data = (fontData + (count1 * charBytes));

      pixels = (charWidth * charHeight);

      // These allow us to keep track of the leftmost and rightmost 'on'
      // pixels for this character.  We can use these for narrowing the
      // image if we want a variable-width font
      firstOnPixel = (charWidth - 1);
      lastOnPixel = 0;

      // Loop through the pixels of the color image, mapping them to
      // 'on' or 'off' pixels.  Anything in pure green is off, everything
      // else is on.
      for (count2 = 0; count2 < pixels; count2 ++)
	{
	  sourcePixel = ((count1 * pixels) + count2);

	  if ((imageData[sourcePixel].red == 0) &&
	      (imageData[sourcePixel].green == 255) &&
	      (imageData[sourcePixel].blue == 0))
	    // Off
	    continue;

	  // Otherwise, 'on'.

	  // Watch for leftmost or rightmost 'on' pixel
	  currentPixel = (count2 % charWidth);
	  if (currentPixel < firstOnPixel)
	    firstOnPixel = currentPixel;
	  if (currentPixel > lastOnPixel)
	    lastOnPixel = currentPixel;

	  // Set the bit.
	  ((unsigned char *) newFont->chars[count1].data)[count2 / 8] |=
	    (((unsigned char) 0x80) >> (count2 % 8));
	}

      if (!fixedWidth)
	{
	  // For variable-width fonts, we want no empty columns before the
	  // character data, and only one after.  Make sure we don't get
	  // buggered up by anything with no 'on' pixels such as the space
	  // character

	  if ((firstOnPixel > 0) || (lastOnPixel < (charWidth - 2)))
	    {
	      if (firstOnPixel > lastOnPixel)
		{
		  // This has no pixels.  Probably a space character.  Give it
		  // a width of approximately 1/5th the char width
		  firstOnPixel = 0;
		  lastOnPixel = ((charWidth / 5) - 1);
		}

	      // We will strip bits from each row of the character image.  This
	      // is a little bit of bit bashing.  The count2 counter counts
	      // through all of the bits.  The count3 one only counts bits that
	      // aren't being skipped, and sets/clears them.

	      count3 = 0;
	      for (count2 = 0; count2 < pixels; count2 ++)
		{
		  currentPixel = (count2 % charWidth);
		  if ((currentPixel < firstOnPixel) ||
		      (currentPixel > (lastOnPixel + 1)))
		    // Skip this pixel.  It's from a column we're deleting.
		    continue;
		  
		  if (((unsigned char *) newFont
		       ->chars[count1].data)[count2 / 8] &
		      (((unsigned char) 0x80) >> (count2 % 8)))
		    // The bit is on
		    ((unsigned char *) newFont
		     ->chars[count1].data)[count3 / 8] |=
		      (((unsigned char) 0x80) >> (count3 % 8));
		  else
		    // The bit is off
		    ((unsigned char *) newFont
		     ->chars[count1].data)[count3 / 8] &=
		      ~(((unsigned char) 0x80) >> (count3 % 8));
		  
		  count3++;
		}

	      // Adjust the character image information
	      newFont->chars[count1].width -=
		(firstOnPixel + (((charWidth - 2) - lastOnPixel)));
	      newFont->chars[count1].pixels =
		(newFont->chars[count1].width * charHeight);
	    }
	}
      // Finished with this character.
    }	
					    				    
  // Release the memory from our composite font image
  kernelMemoryRelease(fontImage.data);

  // Success.  Add the font to our list
  fontList[numFonts++] = newFont;
      
  // Set the pointer to the new font.
  *pointer = newFont;

  // Return success
  return (status = 0);
} 


int kernelFontGetPrintedWidth(kernelAsciiFont *font, const char *string)
{
  // This function takes a font pointer and a pointer to a string, and
  // calculates/returns the width of screen real-estate that the string
  // will consume if printed.  Use with variable-width fonts, of course, or
  // you're wasting your time.
  
  int printedWidth = 0;
  int stringLength = 0;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (printedWidth = -1);
  
  // Check parameters
  if ((font == NULL) || (string == NULL))
    return (printedWidth = -1);
  
  stringLength = strlen(string);

  // Loop through the characters of the string, adding up their individual
  // character widths
  for (count = 0; count < stringLength; count ++)
    printedWidth += font->chars[((unsigned) string[count]) - 32].width;

  return (printedWidth);
}
