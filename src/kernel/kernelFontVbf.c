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
//  kernelFontVbf.c
//

// This file contains code for loading, saving, and converting fonts
// in the Visopsys Bitmap Font (.vbf) format.  VBF is a very simple,
// proprietary format that allows for simple bitmapped fonts in a 'sparse'
// list (i.e. the list of glyph codes can contain as many or as few entries
// as desired, in any order, etc.).  Existing popular bitmap formats are much
// more rigid and complicated, don't allow for sparseness, and thus aren't
// amenable to our usual disk-space stinginess.

#include "kernelFontVbf.h"
#include "kernelError.h"
#include "kernelLoader.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include <stdio.h>
#include <string.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Standard font driver functions
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

static int detect(const char *fileName, void *dataPtr, unsigned size,
	loaderFileClass *class)
{
	// This function returns 1 and fills the fileClass structure if the data
	// points to a VBF file.

	vbfFileHeader *vbfHeader = dataPtr;

	// Check params
	if (!fileName || !dataPtr || !class)
		return (0);

	// Make sure there's enough data here for our detection
	if (size < sizeof(vbfFileHeader))
		return (0);

	// See whether this file claims to be a VBF file.
	if (!strncmp(vbfHeader->magic, VBF_MAGIC, VBF_MAGIC_LEN))
	{
		// We'll accept that.
		sprintf(class->className, "%s %s", FILECLASS_NAME_VBF,
			FILECLASS_NAME_FONT);
		class->class = (LOADERFILECLASS_BIN | LOADERFILECLASS_FONT);
		class->subClass = LOADERFILESUBCLASS_VBF;
		return (1);
	}
	else
	{
		return (0);
	}
}


static int load(unsigned char *fontFileData, int dataLength, int kernel,
	asciiFont **pointer, int fixedWidth)
{
	// Loads a VBF file and returns it as a font.  The memory for this and
	// its data must be freed by the caller.

	int status = 0;
	vbfFileHeader *vbfHeader = (vbfFileHeader *) fontFileData;
	int glyphWidth = 0, glyphHeight = 0, glyphBytes = 0;
	asciiFont *newFont = NULL;
	unsigned char *fontData = NULL;
	unsigned char *charData = 0;
	int firstOnPixel = 0, lastOnPixel = 0, currentPixel = 0;
	int count1, count2, count3;

	// Check params
	if (!fontFileData || !dataLength || !pointer)
		return (status = ERR_NULLPARAMETER);

	// How many bytes per char?
	glyphWidth = vbfHeader->glyphWidth;
	glyphHeight = vbfHeader->glyphHeight;
	glyphBytes = (((glyphWidth * glyphHeight) + 7) / 8);

	// Get memory for the font structure and the images data.
	if (kernel)
	{
		newFont = kernelMalloc(sizeof(asciiFont));
		fontData = kernelMalloc(glyphBytes * vbfHeader->numGlyphs);
	}
	else
	{
		newFont = kernelMemoryGet(sizeof(asciiFont), "user font");
		fontData = kernelMemoryGet((glyphBytes * vbfHeader->numGlyphs),
			"user font data");
	}

	if (!newFont || !fontData)
	{
		kernelError(kernel_error, "Unable to get memory to hold the font data");
		return (status = ERR_MEMORY);
	}

	// Copy the basic font info
	strncpy(newFont->name, vbfHeader->name, 32);
	newFont->glyphWidth = glyphWidth;
	newFont->glyphHeight = glyphHeight;

	// Copy the bitmap data directory from the file into the font memory
	memcpy(fontData, &(vbfHeader->codes[vbfHeader->numGlyphs]),
		(glyphBytes * vbfHeader->numGlyphs));

	// Loop through the all the images (whether they're implemented of not)
	for (count1 = 0; count1 < ASCII_CHARS; count1 ++)
	{
		// Stuff that won't change in the rest of the code for this character,
		// below (things like width can change -- see below)
		newFont->glyphs[count1].type = IMAGETYPE_MONO;
		newFont->glyphs[count1].width = glyphWidth;
		newFont->glyphs[count1].height = glyphHeight;
		newFont->glyphs[count1].pixels = (glyphWidth * glyphHeight);

		// Does this character appear in our character map?
		for (count2 = 0; count2 < vbfHeader->numGlyphs; count2 ++)
		{
			if (vbfHeader->codes[count2] == count1)
			{
				// The character is implemented.  'count2' is the index into
				// the character codes.
				newFont->glyphs[count1].dataLength = glyphBytes;
				newFont->glyphs[count1].data = (fontData + (count2 * glyphBytes));
				break;
			}
		}

		if (!newFont->glyphs[count1].data)
			continue;

		// If a variable-width font has been requested, then we need to do some
		// bit-bashing to remove surplus space on either side of each character.
		if (!fixedWidth)
		{
			charData = newFont->glyphs[count1].data;

			// These allow us to keep track of the leftmost and rightmost 'on'
			// pixels for this character.  We can use these for narrowing the
			// image if we want a variable-width font
			firstOnPixel = (glyphWidth - 1);
			lastOnPixel = 0;

			for (count2 = 0; count2 < glyphHeight; count2 ++)
			{
				// Find the first-on pixel
				for (count3 = 0; count3 < firstOnPixel; count3 ++)
				{
					if (charData[((count2 * glyphWidth) + count3) / 8] &
						(0x80 >> (((count2 * glyphWidth) + count3) % 8)))
					{
						firstOnPixel = count3;
						break;
					}
				}

				// Find the last-on pixel
				for (count3 = (glyphWidth - 1); count3 > lastOnPixel; count3 --)
				{
					if (charData[((count2 * glyphWidth) + count3) / 8] &
						(0x80 >> (((count2 * glyphWidth) + count3) % 8)))
					{
						lastOnPixel = count3;
						break;
					}
				}
			}

			// For variable-width fonts, we want no empty columns before the
			// character data, and only one after.  Make sure we don't get
			// buggered up by anything with no 'on' pixels such as the space
			// character

			if ((firstOnPixel > 0) || (lastOnPixel < (glyphWidth - 2)))
			{
				if (firstOnPixel > lastOnPixel)
				{
					// This has no pixels.  Probably a space character.  Give
					// it a width of approximately 1/5th the char width
					firstOnPixel = 0;
					lastOnPixel = ((glyphWidth / 5) - 1);
				}

				// We will strip bits from each row of the character image.
				// This is the little bit of bit bashing.  The count2 counter
				// counts through all of the bits.  The count3 one only counts
				// bits that aren't being skipped, and sets/clears them.

				count3 = 0;
				for (count2 = 0; count2 < (glyphWidth * glyphHeight); count2 ++)
				{
					currentPixel = (count2 % glyphWidth);
					if ((currentPixel < firstOnPixel) ||
						(currentPixel > (lastOnPixel + 1)))
					{
						// Skip this pixel.  It's from a column we're deleting.
						continue;
					}

					if (charData[count2 / 8] & (0x80 >> (count2 % 8)))
						// The bit is on
						charData[count3 / 8] |= (0x80 >> (count3 % 8));
					else
						// The bit is off
						charData[count3 / 8] &= ~(0x80 >> (count3 % 8));

					count3++;
				}

				// Adjust the character image information
				newFont->glyphs[count1].width -=
					(firstOnPixel + (((glyphWidth - 2) - lastOnPixel)));
				newFont->glyphs[count1].pixels =
					(newFont->glyphs[count1].width * glyphHeight);
			}
		}
	}

	// Set the pointer to the new font.
	*pointer = newFont;

	return (status = 0);
}


kernelFileClass vbfFileClass = {
	FILECLASS_NAME_VBF,
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

kernelFileClass *kernelFileClassVbf(void)
{
	// The loader will call this function so that we can return a structure
	// for managing VBF files

	static int filled = 0;

	if (!filled)
	{
		vbfFileClass.font.load = &load;
		filled = 1;
	}

	return (&vbfFileClass);
}

