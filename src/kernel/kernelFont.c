//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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

// This contains utility functions for managing fonts

#include "kernelFont.h"
#include "kernelCharset.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelFile.h"
#include "kernelLoader.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ascii.h>
#include <sys/file.h>
#include <sys/image.h>
#include <sys/paths.h>
#include <sys/vis.h>

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
	{ 64, 168, 168, 16, 0, 0, 0, 0 }			// ~
};

static kernelFont *systemFont = NULL;
static linkedList fontList;


static int search(const char *family, unsigned flags, int points,
	const char *charSet, char *foundFileName)
{
	// Takes the name of a desired font family name, style flags, size in
	// points, and an optional character set.  The function will search the
	// system fonts directory for the appropriate font file.

	int status = 0;
	char *fileName = NULL;
	file theFile;
	loaderFileClass loaderClass;
	kernelFileClass *fileClassDriver = NULL;
	kernelFont font;
	int found = 0;
	int count;

	kernelDebug(debug_font, "Searching for %s font with flags=0x%x, "
		"points=%d, charset=%s", family, flags, points, charSet);

	fileName = kernelMalloc(MAX_PATH_NAME_LENGTH + 1);
	if (!fileName)
		return (status = ERR_MEMORY);

	// Loop through the files in the font directory
	for (count = 0; ; count ++)
	{
		if (count)
			status = kernelFileNext(PATH_SYSTEM_FONTS, &theFile);
		else
			status = kernelFileFirst(PATH_SYSTEM_FONTS, &theFile);

		if (status < 0)
			break;

		if (theFile.type != fileT)
			continue;

		snprintf(fileName, MAX_PATH_NAME_LENGTH, "%s/%s", PATH_SYSTEM_FONTS,
			theFile.name);

		kernelDebug(debug_font, "Checking %s", fileName);

		fileClassDriver = kernelLoaderClassifyFile(fileName, &loaderClass);
		if (!fileClassDriver)
			continue;

		if (!(loaderClass.type & LOADERFILECLASS_FONT))
			continue;

		if (!fileClassDriver->font.getInfo)
			continue;

		// Get info about the font file
		status = fileClassDriver->font.getInfo(fileName, &font.info);
		if (status < 0)
			continue;

		kernelDebug(debug_font, "Family %s, flags=0x%x, points=%d, "
			"charset=%s", font.info.family, font.info.flags, font.info.points,
			font.info.charSet);

		if (!strcmp(font.info.family, family) &&
			(font.info.flags == (flags & ~FONT_STYLEFLAG_FIXED)) &&
			(font.info.points == points) &&
			!strcmp(font.info.charSet, charSet))
		{
			// This is the one we're looking for
			strcpy(foundFileName, fileName);
			found = 1;
			break;
		}
	}

	kernelFree(fileName);

	if (found)
	{
		kernelDebug(debug_font, "Found");
		return (status = 0);
	}
	else
	{
		kernelDebug(debug_font, "Not found");
		return (status = ERR_NOSUCHFILE);
	}
}


static kernelFont *load(kernelFont *font, const char *fileName,
	int fixedWidth)
{
	int status = 0;
	int allocated = 0;
	file fontFile;
	unsigned char *fileData = NULL;
	loaderFileClass loaderClass;
	kernelFileClass *fileClassDriver = NULL;

	kernelDebug(debug_font, "Loading %s", fileName);

	if (!font)
	{
		font = kernelMalloc(sizeof(kernelFont));
		if (!font)
			return (font);

		allocated = 1;
	}

	// Load the font file data into memory
	fileData = kernelLoaderLoad(fileName, &fontFile);
	if (!fileData)
	{
		status = ERR_BADDATA;
		goto out;
	}

	// Get the file class of the file
	fileClassDriver = kernelLoaderClassify(fileName, fileData, fontFile.size,
		&loaderClass);
	if (!fileClassDriver)
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
	status = fileClassDriver->font.load(fileData, fontFile.size, font,
		fixedWidth);

	if (status >= 0)
	{
		if (fixedWidth)
			font->info.flags |= FONT_STYLEFLAG_FIXED;

		if (allocated)
		{
			status = linkedListAddBack(&fontList, font);
			if (status < 0)
				goto out;
		}
	}

out:
	if (fileData)
		kernelMemoryRelease(fileData);

	if (status < 0)
	{
		if (allocated)
			kernelFree(font);

		font = NULL;
	}

	kernelDebug(debug_font, "Loading %s %s", fileName, ((status < 0)?
		"failed" : "successful"));

	return (font);
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
	memset(&fontList, 0, sizeof(linkedList));

	// Create the default system font

	systemFont = kernelMalloc(sizeof(kernelFont));
	if (!systemFont)
	{
		kernelError(kernel_error, "Couldn't get memory for system font");
		status = ERR_MEMORY;
		goto out;
	}

	systemFont->glyphs = kernelMalloc(ASCII_PRINTABLES * sizeof(kernelGlyph));
	if (!systemFont->glyphs)
	{
		kernelError(kernel_error, "Couldn't get memory for system font");
		status = ERR_MEMORY;
		goto out;
	}

	strcpy(systemFont->info.family, "system");
	systemFont->info.points = 8;

	strcpy(systemFont->info.charSet, CHARSET_NAME_ASCII);
	status = linkedListAddBack(&systemFont->charSet,
		systemFont->info.charSet);
	if (status < 0)
		goto out;

	systemFont->numGlyphs = ASCII_CHARS;
	systemFont->glyphWidth = 8;
	systemFont->glyphHeight = 8;

	for (count = 0; count < ASCII_PRINTABLES; count ++)
	{
		systemFont->glyphs[count].unicode = (CHARSET_CTRL_CODES + count);

		systemFont->glyphs[count].img.type = IMAGETYPE_MONO;
		systemFont->glyphs[count].img.pixels = (systemFont->glyphWidth *
			systemFont->glyphHeight);
		systemFont->glyphs[count].img.width = systemFont->glyphWidth;
		systemFont->glyphs[count].img.height = systemFont->glyphHeight;
		systemFont->glyphs[count].img.dataLength = 8;
		systemFont->glyphs[count].img.data = glyphs[count];
	}

	initialized = 1;
	status = 0;

out:
	if (status < 0)
	{
		if (systemFont)
		{
			if (systemFont->glyphs)
				kernelFree(systemFont->glyphs);

			kernelFree(systemFont);
		}
	}

	return (status);
}


kernelFont *kernelFontGetSystem(void)
{
	// Return a pointer to the default system font

	// Make sure we've been initialized
	if (!initialized)
		return (NULL);

	return (systemFont);
}


int kernelFontHasCharSet(kernelFont *font, const char *charSet)
{
	// Returns 1 if the supplied font has the requested character set loaded.
	// 0 otherwise.

	linkedListItem *iter = NULL;
	char *haveCharSet = NULL;

	// Make sure we've been initialized
	if (!initialized)
		return (0);

	// Check params
	if (!font || !charSet)
	{
		kernelError(kernel_error, "NULL parameter");
		return (0);
	}

	haveCharSet = linkedListIterStart(&font->charSet, &iter);

	while (haveCharSet)
	{
		if (!strcmp(haveCharSet, charSet))
			// We've got it
			return (1);

		haveCharSet = linkedListIterNext(&font->charSet, &iter);
	}

	// Don't have it
	return (0);
}


kernelFont *kernelFontGet(const char *family, unsigned flags, int points,
	const char *charSet)
{
	// Takes the name of a desired font family name, style flags, size in
	// points, and an optional character set.  The function will check whether
	// the required information is already in memory, and if not, search the
	// system fonts directory for the appropriate font file.  If found, it
	// will call the driver to load it into memory.

	int status = 0;
	linkedListItem *iter = NULL;
	kernelFont *font = NULL;
	char *fileName = NULL;

	// Make sure we've been initialized
	if (!initialized)
		return (font = NULL);

	// Check params
	if (!family || !points)
	{
		kernelError(kernel_error, "NULL parameter");
		return (font = NULL);
	}

	if (!charSet)
		charSet = CHARSET_NAME_DEFAULT;

	kernelDebug(debug_font, "Getting %s font with flags=0x%x, points=%d, "
		"charset=%s", family, flags, points, charSet);

	fileName = kernelMalloc(MAX_PATH_NAME_LENGTH + 1);
	if (!fileName)
		return (font = NULL);

	// Check to see if it's been loaded already

	font = linkedListIterStart(&fontList, &iter);

	while (font)
	{
		kernelDebug(debug_font, "Checking %s, flags=0x%x, points=%d",
			font->info.family, font->info.flags, font->info.points);

		if (!strcmp(font->info.family, family) &&
			(font->info.flags == flags) && (font->info.points == points))
		{
			// The font is already loaded
			kernelDebug(debug_font, "Font already loaded, checking charset");

			// Do we already have the required character set?
			if (kernelFontHasCharSet(font, charSet))
			{
				// We've got it
				kernelDebug(debug_font, "Charset already loaded");
				goto out;
			}

			// We don't have this charset yet
			kernelDebug(debug_font, "Charset not yet loaded");

			// Try to find it
			status = search(family, flags, points, charSet, fileName);
			if (status < 0)
			{
				// Not found, or load error
				font = NULL;
				goto out;
			}

			// Try to load it
			font = load(font, fileName, (flags & FONT_STYLEFLAG_FIXED));
			goto out;
		}

		font = linkedListIterNext(&fontList, &iter);
	}

	if (!font)
	{
		// We don't have this font yet
		kernelDebug(debug_font, "Font not yet loaded");

		// Try to find it
		status = search(family, flags, points, charSet, fileName);
		if (status < 0)
			// Not found, or load error
			goto out;

		// Try to load it
		font = load(font, fileName, (flags & FONT_STYLEFLAG_FIXED));
	}

out:

	if (fileName)
		kernelFree(fileName);

	return (font);
}


int kernelFontGetPrintedWidth(kernelFont *font, const char *charSet,
	const char *string)
{
	// This function takes a font pointer and a pointer to a string, and
	// calculates/returns the width of screen real-estate that the string will
	// consume if printed.  Use with variable-width fonts, of course, or
	// you're wasting your time.

	int printedWidth = 0;
	int length = 0;
	unsigned unicode = 0;
	int multiByte = 0;
	int count1, count2;

	// Make sure we've been initialized
	if (!initialized)
		return (printedWidth = -1);

	// Check params.  'charSet' can be NULL.
	if (!font || !string)
		return (printedWidth = -1);

	// How long is the string?
	length = strlen(string);

	if (!charSet)
		charSet = CHARSET_NAME_DEFAULT;

	// Loop through the characters of the string, adding up their individual
	// character widths
	for (count1 = 0; count1 < length; count1 ++)
	{
		if ((unsigned char) string[count1] < CHARSET_IDENT_CODES)
		{
			unicode = string[count1];
		}
		else if (!strcmp(charSet, CHARSET_NAME_UTF8))
		{
			multiByte = mbtowc(&unicode, (string + count1), (length -
				count1));
			if (multiByte < 0)
				continue;
			else if (multiByte > 1)
				count1 += (multiByte - 1);
		}
		else
		{
			unicode = kernelCharsetToUnicode(charSet, (unsigned char)
				string[count1]);
		}

		for (count2 = 0; count2 < font->numGlyphs; count2 ++)
		{
			if (font->glyphs[count2].unicode == unicode)
			{
				printedWidth += font->glyphs[count2].img.width;
				break;
			}
		}
	}

	return (printedWidth);
}


int kernelFontGetWidth(kernelFont *font)
{
	// Returns the character width of the supplied font.  Only useful when the
	// font is fixed-width.

	if (!initialized)
		return (-1);

	// Check params
	if (!font)
		return (-1);

	return (font->glyphWidth);
}


int kernelFontGetHeight(kernelFont *font)
{
	// Returns the character height of the supplied font

	if (!initialized)
		return (-1);

	// Check params
	if (!font)
		return (-1);

	return (font->glyphHeight);
}

