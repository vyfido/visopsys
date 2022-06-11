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
//  kernelFont.c
//

// This contains utility functions for managing fonts.

#include "kernelFont.h"
#include "kernelError.h"
#include "kernelFile.h"
#include "kernelImage.h"
#include "kernelLoader.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelVariableList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/paths.h>

static int initialized = 0;

// This specifies the default system font, built in.  Very simple.

static unsigned char glyphs[ASCII_PRINTABLES][8] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 },					// space
	{ 48, 48, 48, 48, 48, 0, 48, 0 },			// !
	{ 40, 40, 0, 0, 0, 0, 0, 0 },				// "
	{ 40, 40, 254, 40, 254, 40, 40, 0 },		// #
	{ 16, 254, 144, 254, 18, 254, 16, 0 },		// $
	{ 66, 164, 72, 16, 36, 74, 132, 0 },		// %
	{ 112, 80, 48, 54, 76, 140, 114, 0 },		// &
	{ 6, 12, 0, 0, 0, 0, 0, 0 },				// '
	{ 8, 16, 32, 32, 32, 16, 8, 0 },			// (
	{ 32, 16, 8, 8, 8, 16, 32, 0 },				// )
	{ 16, 84, 56, 254, 56, 84, 16, 0 },			// *
	{ 0, 16, 16, 124, 16, 16, 0, 0 },			// +
	{ 0, 0, 0, 0, 0, 24, 48, 0 },				// ,
	{ 0, 0, 0, 124, 0, 0, 0, 0 },				// -
	{ 0, 0, 0, 0, 0, 24, 24, 0 },				// .
	{ 2, 4, 8, 16, 32, 64, 128, 0 },			// /
	{ 56, 68, 130, 130, 130, 68, 56, 0 },		// 0
	{ 16, 48, 16, 16, 16, 16, 56, 0 },			// 1
	{ 124, 130, 2, 124, 128, 128, 254, 0 },		// 2
	{ 60, 66, 2, 12, 2, 66, 60, 0 },			// 3
	{ 24, 40, 72, 136, 252, 8, 8, 0 },			// 4
	{ 252, 128, 128, 252, 2, 2, 252, 0 },		// 5
	{ 124, 128, 128, 252, 130, 130, 124, 0 },	// 6
	{ 254, 2, 2, 4, 8, 16, 32, 0 },				// 7
	{ 124, 130, 130, 124, 130, 130, 124, 0 },	// 8
	{ 124, 130, 130, 124, 2, 2, 124, 0 },		// 9
	{ 0, 0, 0, 24, 0, 24, 0, 0 },				// :
	{ 0, 0, 0, 24, 0, 24, 48, 0 },				// ;
	{ 6, 24, 96, 128, 96, 24, 6, 0 },			// <
	{ 0, 0, 0, 124, 0, 124, 0, 0 },				// =
	{ 192, 48, 12, 2, 12, 48, 192, 0 },			// >
	{ 56, 70, 2, 4, 24, 0, 16, 0 },				// ?
	{ 60, 66, 92, 84, 92, 64, 62, 0 },			// @
	{ 16, 108, 130, 130, 254, 130, 130, 0 },	// A
	{ 252, 130, 132, 248, 132, 130, 252, 0 },	// B
	{ 124, 130, 128, 128, 128, 130, 124, 0 },	// C
	{ 248, 68, 66, 66, 66, 68, 248, 0 },		// D
	{ 254, 128, 128, 252, 128, 128, 254, 0 },	// E
	{ 254, 128, 128, 252, 128, 128, 128, 0 },	// F
	{ 126, 128, 128, 142, 130, 130, 124, 0 },	// G
	{ 130, 130, 130, 254, 130, 130, 130, 0 },	// H
	{ 124, 16, 16, 16, 16, 16, 124, 0 },		// I
	{ 62, 4, 4, 4, 4, 68, 56, 0 },				// J
	{ 130, 140, 144, 224, 144, 140, 130, 0 },	// K
	{ 128, 128, 128, 128, 128, 128, 254, 0 },	// L
	{ 198, 170, 146, 146, 130, 130, 130, 0 },	// M
	{ 130, 194, 162, 146, 138, 134, 130, 0 },	// N
	{ 124, 130, 130, 130, 130, 130, 124, 0 },	// O
	{ 124, 130, 130, 252, 128, 128, 128, 0 },	// P
	{ 120, 132, 132, 132, 140, 132, 122, 0 },	// Q
	{ 252, 130, 130, 252, 132, 130, 130, 0 },	// R
	{ 124, 130, 128, 124, 2, 2, 252, 0 },		// S
	{ 254, 16, 16, 16, 16, 16, 16, 0 },			// T
	{ 130, 130, 130, 130, 130, 130, 124, 0 },	// U
	{ 130, 130, 130, 130, 68, 40, 16, 0 },		// V
	{ 130, 130, 130, 130, 146, 170, 198, 0 },	// W
	{ 130, 68, 40, 16, 40, 68, 130, 0 },		// X
	{ 130, 68, 40, 16, 16, 16, 16, 0 },			// Y
	{ 254, 4, 8, 16, 32, 64, 254, 0 },			// Z
	{ 124, 64, 64, 64, 64, 64, 124, 0 },		// [
	{ 2, 4, 8, 16, 32, 64, 128, 0 },			// /
	{ 124, 4, 4, 4, 4, 4, 124, 0 },				// ]
	{ 16, 40, 68, 0, 0, 0, 0, 0 },				// ^
	{ 0, 0, 0, 0, 0, 0, 0, 254 },				// _
	{ 96, 48, 0, 0, 0, 0, 0, 0 },				// `
	{ 0, 0, 120, 4, 124, 132, 126, 0 },			// a
	{ 128, 128, 252, 130, 130, 130, 124, 0 },	// b
	{ 0, 0, 124, 128, 128, 128, 124, 0 },		// c
	{ 2, 2, 126, 130, 130, 130, 126, 0 },		// d
	{ 0, 0, 124, 130, 254, 128, 126, 0 },		// e
	{ 60, 66, 64, 64, 240, 64, 64, 0 },			// f
	{ 0, 0, 124, 130, 130, 126, 2, 124 },		// g
	{ 128, 128, 252, 130, 130, 130, 130, 0 },	// h
	{ 16, 0, 48, 16, 16, 16, 56, 0 },			// i
	{ 8, 0, 24, 8, 8, 8, 72, 48 },				// j
	{ 0, 128, 140, 240, 136, 132, 132, 0 },		// k
	{ 16, 16, 16, 16, 16, 16, 56, 0 },			// l
	{ 0, 0, 68, 170, 146, 146, 130, 0 },		// m
	{ 0, 0, 252, 130, 130, 130, 130, 0 },		// n
	{ 0, 0, 124, 130, 130, 130, 124, 0 },		// o
	{ 0, 0, 124, 130, 130, 252, 128, 128 },		// p
	{ 0, 0, 124, 130, 130, 126, 2, 2 },			// q
	{ 0, 0, 124, 130, 128, 128, 128, 0 },		// r
	{ 0, 0, 124, 128, 124, 2, 252, 0 },			// s
	{ 32, 32, 252, 32, 32, 34, 28, 0 },			// t
	{ 0, 0, 132, 132, 132, 132, 122, 0 },		// u
	{ 0, 0, 68, 68, 68, 40, 16, 0 },			// v
	{ 0, 0, 130, 146, 146, 170, 68, 0 },		// w
	{ 0, 0, 68, 40, 16, 40, 68, 0 },			// x
	{ 0, 0, 68, 68, 68, 56, 4, 120 },			// y
	{ 0, 0, 124, 8, 16, 32, 124, 0 },			// z
	{ 24, 32, 32, 96, 32, 32, 24, 0 },			// {
	{ 16, 16, 16, 16, 16, 16, 16, 0 },			// |
	{ 48, 8, 8, 12, 8, 8, 48, 0 },				// }
	{ 64, 168, 168, 16, 0, 0, 0, 0 },			// ~
	{ 0, 0, 0, 0, 0, 0, 0, 0 }					// DEL
};

static asciiFont *systemFont = NULL;
static asciiFont *defaultFont = NULL;
static asciiFont *fontList[FONTS_MAX];
static int numFonts = 0;


static int load(const char *fileName, const char *fontName, int kernel,
	asciiFont **pointer, int fixedWidth)
{
	// Takes the name of a file containing a font definition and turns it into
	// our internal representation of an asciiFont.  The image should have pure
	// green as its background; every other color gets turned 'on' in our mono
	// font scheme.  If the operation is successful the supplied pointer is set
	// to point to the new font.

	int status = 0;
	char *fixedName = NULL;
	file fontFile;
	unsigned char *fileData = NULL;
	loaderFileClass loaderClass;
	kernelFileClass *fileClassDriver = NULL;
	int count;

	// Make sure we've been initialized
	if (!initialized)
		return (status = ERR_NOTINITIALIZED);

	// Check parameters
	if (!fileName || (kernel && !fontName) || !pointer)
		return (status = ERR_NULLPARAMETER);

	// Until we've accomplished everything successfully...
	*pointer = NULL;

	if (kernel)
	{
		// Check to see if it's been loaded already.  If not, just return it.
		for (count = 0; count < numFonts; count ++)
		{
			if (!strcmp(fontList[count]->name, fontName))
			{
				// Already a font by that name.
				*pointer = fontList[count];
				return (status = 0);
			}
		}

		// Don't exceed FONTS_MAX
		if (numFonts >= FONTS_MAX)
			return (status = ERR_NOFREE);
	}

	fixedName = kernelMalloc(MAX_PATH_NAME_LENGTH);
	if (!fixedName)
		return (status = ERR_MEMORY);

	strncpy(fixedName, fileName, MAX_PATH_NAME_LENGTH);

	// 'Fix' the file name if neccessary.
	status = kernelFileFind(fixedName, &fontFile);
	if (status < 0)
	{
		// If the filename is not absolute, try prepending the name of the
		// system font directory
		if (fixedName[0] != '/')
		{
			snprintf(fixedName, MAX_PATH_NAME_LENGTH, PATH_SYSTEM_FONTS "/%s",
				fileName);
			status = kernelFileFind(fixedName, &fontFile);
		}

		if (status < 0)
		{
			kernelError(kernel_error, "Font file \"%s\" not found", fileName);
			status = ERR_NOSUCHENTRY;
			goto out;
		}
	}

	// Load the font file data into memory
	fileData = kernelLoaderLoad(fixedName, &fontFile);
	if (!fileData)
	{
		status = ERR_BADDATA;
		goto out;
	}

	// Get the file class of the file.
	fileClassDriver = kernelLoaderClassify(fixedName, fileData, fontFile.size,
		&loaderClass);
	if (!fileClassDriver)
	{
		status = ERR_INVALID;
		goto out;
	}

	if (!(loaderClass.class & LOADERFILECLASS_FONT))
	{
		status = ERR_INVALID;
		goto out;
	}

	if (!fileClassDriver->font.load)
	{
		status = ERR_NOTIMPLEMENTED;
		goto out;
	}

	// Call the appropriate 'load' function
	status = fileClassDriver->font.load(fileData, fontFile.size, kernel,
		pointer, fixedWidth);
	if (status < 0)
		goto out;

	if (kernel)
	{
		strcpy((*pointer)->name, fontName);

		// Add the font to our list
		fontList[numFonts++] = *pointer;
	}

	// Success
	status = 0;

out:
	if ((status < 0) && fileData)
		kernelFree(fileData);

	if (fixedName)
		kernelFree(fixedName);

	return (status);
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
	memset(&fontList, 0, (sizeof(asciiFont *) * FONTS_MAX));

	// Create the default system font

	systemFont = kernelMalloc(sizeof(asciiFont));
	if (!systemFont)
	{
		kernelError(kernel_error, "Couldn't get memory for system font");
		return (status = ERR_MEMORY);
	}

	strcpy(systemFont->name, "system");
	systemFont->glyphWidth = 8;
	systemFont->glyphHeight = 8;

	for (count = 0; count < ASCII_CHARS; count ++)
	{
		systemFont->glyphs[count].type = IMAGETYPE_MONO;
		systemFont->glyphs[count].pixels = (systemFont->glyphWidth *
			systemFont->glyphHeight);
		systemFont->glyphs[count].width = systemFont->glyphWidth;
		systemFont->glyphs[count].height = systemFont->glyphHeight;

		if ((count >= 32) && (count < (ASCII_PRINTABLES + 32)))
		{
			systemFont->glyphs[count].dataLength = 8;
			systemFont->glyphs[count].data = glyphs[count - 32];
		}
	}

	// Set the system font to be our default
	defaultFont = systemFont;

	initialized = 1;

	return (status = 0);
}


int kernelFontGetDefault(asciiFont **pointer)
{
	// Return a pointer to the default system font

	int status = 0;

	// Make sure we've been initialized
	if (!initialized)
		return (status = ERR_NOTINITIALIZED);

	// Check parameters
	if (!pointer)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	*pointer = defaultFont;
	return (status = 0);
}


int kernelFontLoadSystem(const char *fileName, const char *fontName,
	asciiFont **pointer, int fixedWidth)
{
	return (load(fileName, fontName, 1 /* kernel */, pointer, fixedWidth));
}


int kernelFontLoadUser(const char *fileName, asciiFont **pointer,
	int fixedWidth)
{
	return (load(fileName, NULL /* no font name */, 0 /* not kernel */,
		pointer, fixedWidth));
}


int kernelFontGetPrintedWidth(asciiFont *font, const char *string)
{
	// This function takes a font pointer and a pointer to a string, and
	// calculates/returns the width of screen real-estate that the string
	// will consume if printed.  Use with variable-width fonts, of course, or
	// you're wasting your time.

	int printedWidth = 0;
	int length = 0;
	unsigned idx = 0;
	int count;

	// Make sure we've been initialized
	if (!initialized)
		return (printedWidth = -1);

	// Check parameters
	if (!font || !string)
		return (printedWidth = -1);

	// How long is the string?
	length = strlen(string);

	// Loop through the characters of the string, adding up their individual
	// character widths
	for (count = 0; count < length; count ++)
	{
		idx = (unsigned char) string[count];
		printedWidth += font->glyphs[idx].width;
	}

	return (printedWidth);
}


int kernelFontGetWidth(asciiFont *font)
{
	// Returns the character width of the supplied font.  Only useful when the
	// font is fixed-width.

	if (!initialized)
		return (-1);

	// Check parameters
	if (!font)
		return (-1);

	return (font->glyphWidth);
}


int kernelFontGetHeight(asciiFont *font)
{
	// Returns the character height of the supplied font.

	if (!initialized)
		return (-1);

	// Check parameters
	if (!font)
		return (-1);

	return (font->glyphHeight);
}

