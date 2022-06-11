//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  fontutil.c
//

// A program for editing and converting Visopsys fonts.

/* This is the text that appears when a user requests help about this program
<help>

 -- fontutil --

A program for editing and converting Visopsys fonts.

Usage:
  fontutil [options] <VBF_file>

Example:

This command is of only marginal usefulness to most users.  It is primarily
intended for developers modifying font definitions.

Options:
-a <code>       : Add a glyph to the font using the supplied code number
                  Use with a mandatory -f flag to specify the image file
-c <codepage>   : Set the code page
-d [code]       : Dump (print) the font data, or else a representation of
                : the glyph with the supplied code number
-i <img_file>   : Import a new font from the specified image file, which will
                : be a table 16x12 glyphs representing codes 32-127 and
		: 160-255 read left-to-right and top-to-bottom.
-f <file_name>  : Used for supplying a file name to commands that require one
-n <font_name>  : Set the font name
-p <points>     : Set the number of points
-r <code>       : Remove the glyph with the supplied code number
-v              : Verbose; print out more information about what's happening
-x <font_file>  : Convert an old-style font file to VBF

</help>
*/

#include "errno.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/image.h>
#include <sys/vbf.h>

typedef enum {
	operation_none, operation_dump, operation_update,
	operation_import, operation_add, operation_remove,
	operation_convert

} operation_type;

static const char *cmdName = NULL;
static char *fontName = NULL;
static int points = 0;
static char *codePage = NULL;
static int verbose = 0;


static inline int glyphPosition(int *codes, int numGlyphs, int code)
{
	int pos = ERR_NOSUCHENTRY;
	int count;

	for (count = 0; count < numGlyphs; count ++)
	{
		if (codes[count] == code)
		{
			pos = count;
			break;
		}
	}

	return (pos);
}


static int readHeader(FILE *vbfFile, vbfFileHeader *vbfHeader)
{
	// Read the header of a VBF file

	int status = 0;

	status = fseek(vbfFile, 0, SEEK_SET);
	if (status < 0)
	{
		perror(cmdName);
		fprintf(stderr, "Can't seek %s\n", vbfFile->f.name);
		return (status = errno);
	}

	status = fread(vbfHeader, sizeof(vbfFileHeader), 1, vbfFile);
	if (status != 1)
	{
		perror(cmdName);
		fprintf(stderr, "Can't read %s\n", vbfFile->f.name);
		return (status = errno);
	}

	if (strncmp(vbfHeader->magic, VBF_MAGIC, 4))
	{
		fprintf(stderr, "%s is not a VBF font file\n", vbfFile->f.name);
		return (status = ERR_INVALID);
	}

	return (status = 0);
}


static int writeHeader(FILE *vbfFile, vbfFileHeader *vbfHeader)
{
	// Write the header of a VBF file

	int status = 0;

	status = fseek(vbfFile, 0, SEEK_SET);
	if (status < 0)
	{
		perror(cmdName);
		fprintf(stderr, "Can't seek %s\n", vbfFile->f.name);
		return (status = errno);
	}

	status = fwrite(vbfHeader, sizeof(vbfFileHeader), 1, vbfFile);
	if (status != 1)
	{
		perror(cmdName);
		fprintf(stderr, "Can't write %s\n", vbfFile->f.name);
		return (status = errno);
	}

	return (status = 0);
}


static int updateHeader(const char *vbfFileName)
{
	int status = 0;
	FILE *vbfFile = NULL;
	vbfFileHeader vbfHeader;

	printf("Update VBF header of %s\n", vbfFileName);
	
	bzero(&vbfHeader, sizeof(vbfHeader));

	vbfFile = fopen(vbfFileName, "r+");
	if (!vbfFile)
	{
		perror(cmdName);
		fprintf(stderr, "Can't open %s for reading/writing\n", vbfFileName);
		return (status = errno);
	}

	status = readHeader(vbfFile, &vbfHeader);
	if (status < 0)
	{
		fclose(vbfFile);
		return (status);
	}

	if (fontName)
	{
		printf("Font name: now %s (was %s)\n", fontName, vbfHeader.name);
		bzero(vbfHeader.name, 32);
		strncpy(vbfHeader.name, fontName, 32);
	}

	if (points)
	{
		printf("Points: now %d (was %d)\n", points, vbfHeader.points);
		vbfHeader.points = points;
	}

	if (codePage)
	{
		printf("Codepage: now %s (was %s)\n", codePage, vbfHeader.codePage);
		bzero(vbfHeader.codePage, 16);
		strncpy(vbfHeader.codePage, codePage, 16);
	}

	status = writeHeader(vbfFile, &vbfHeader);

	fclose(vbfFile);
	
	return (status);
}


static int readFont(FILE *vbfFile, vbfFileHeader *vbfHeader, int **codes,
	unsigned char **data)
{
	int status = 0;
	int bytesPerChar = 0;

	// Read the header
	status = readHeader(vbfFile, vbfHeader);
	if (status < 0)
		goto out;

	bytesPerChar = (((vbfHeader->glyphWidth * vbfHeader->glyphHeight) + 7) / 8);

	// Get memory for the codes and data
	*codes = malloc(vbfHeader->numGlyphs * sizeof(int));
	*data = malloc(vbfHeader->numGlyphs * bytesPerChar);
	if (!(*codes) || !(*data))
	{
		perror(cmdName);
		fprintf(stderr, "Error getting memory for character codes or data\n");
		status = errno;
		goto out;
	}

	// Read the code map
	status = fread(*codes, sizeof(int), vbfHeader->numGlyphs, vbfFile);
	if (status < vbfHeader->numGlyphs)
	{
		perror(cmdName);
		fprintf(stderr, "Error reading character codes of %s\n",
			vbfFile->f.name);
		status = errno;
		goto out;
	}

	// Read the character data
	status = fread(*data, bytesPerChar, vbfHeader->numGlyphs, vbfFile);
	if (status < vbfHeader->numGlyphs)
	{
		perror(cmdName);
		fprintf(stderr, "Error reading character data of %s\n",
			vbfFile->f.name);
		status = errno;
		goto out;
	}

	// Return success
	status = 0;

out:
	if (status)
	{
		if (*codes)
		{
			free(*codes);
			*codes = NULL;
		}
		if (*data)
		{
			free(*data);
			*data = NULL;
		}
	}
	return (status);
}


static int writeFont(FILE *vbfFile, vbfFileHeader *vbfHeader, int *codes,
	unsigned char *data)
{
	int status = 0;
	int bytesPerChar = 0;

	// Write the header
	status = writeHeader(vbfFile, vbfHeader);
	if (status < 0)
		return (status);

	bytesPerChar = (((vbfHeader->glyphWidth * vbfHeader->glyphHeight) + 7) / 8);

	// Write the code map
	status = fwrite(codes, sizeof(int), vbfHeader->numGlyphs, vbfFile);
	if (status < vbfHeader->numGlyphs)
	{
		perror(cmdName);
		fprintf(stderr, "Error writing character codes for %s\n",
			vbfFile->f.name);
		return (status = errno);
	}

	// Write the character data
	status = fwrite(data, bytesPerChar, vbfHeader->numGlyphs, vbfFile);
	if (status < vbfHeader->numGlyphs)
	{
		perror(cmdName);
		fprintf(stderr, "Error writing character data for %s\n",
			vbfFile->f.name);
		return (status = errno);
	}

	// Return success
	return (status = 0);
}


static void dumpHeader(vbfFileHeader *vbfHeader)
{
	char tmp[33];

	printf("VBF file header:\n");
	strncpy(tmp, vbfHeader->magic, 4); tmp[4] = '\0';
	printf(" magic=%s\n", tmp);
	printf(" version=%d.%d\n", ((vbfHeader->version & 0xFFFF0000) >> 16),
		(vbfHeader->version & 0xFFFF));
	strncpy(tmp, vbfHeader->name, 32); tmp[32] = '\0';
	printf(" name=%s\n", tmp);
	printf(" points=%d\n", vbfHeader->points);
	strncpy(tmp, vbfHeader->codePage, 16); tmp[16] = '\0';
	printf(" codePage=%s\n", tmp);
	printf(" numGlyphs=%d\n", vbfHeader->numGlyphs);
	printf(" glyphWidth=%d\n", vbfHeader->glyphWidth);
	printf(" glyphHeight=%d\n", vbfHeader->glyphHeight);
}


static int dumpChar(int code, vbfFileHeader *vbfHeader, int *codes,
	unsigned char *data)
{
	int status = 0;
	int pos = 0;
	int pixelsPerChar = 0;
	int bytesPerChar = 0;
	int count;

	pos = glyphPosition(codes, vbfHeader->numGlyphs, code);
	if (pos < 0)
	{
		fprintf(stderr, "Glyph %d does not exist in font file\n", code);
		return (status = ERR_NOSUCHENTRY);
	}

	pixelsPerChar = (vbfHeader->glyphWidth * vbfHeader->glyphHeight);
	bytesPerChar = ((pixelsPerChar + 7) / 8);
	data += (pos * bytesPerChar);

	for (count = 0; count < pixelsPerChar; count ++)
	{
		if (!(count % vbfHeader->glyphWidth))
			printf("\n");

		if (data[count / 8] & (0x80 >> (count % 8)))
			printf("#");
		else
			printf("_");
	}
	printf("\n");

	return (status = 0);
}


static int dump(const char *vbfFileName, int code)
{
	int status = 0;
	FILE *vbfFile = NULL;
	vbfFileHeader vbfHeader;
	int *codes = NULL;
	unsigned char *data = NULL;

	bzero(&vbfHeader, sizeof(vbfHeader));

	vbfFile = fopen(vbfFileName, "r");
	if (!vbfFile)
	{
		perror(cmdName);
		fprintf(stderr, "Can't open %s for reading\n", vbfFileName);
		return (status = errno);
	}

	status = readFont(vbfFile, &vbfHeader, &codes, &data);
	if (status < 0)
		goto out;

	status = 0;

	if (code >= 0)
		status = dumpChar(code, &vbfHeader, codes, data);
	else
		dumpHeader(&vbfHeader);

out:
	if (codes)
		free(codes);
	if (data)
		free(data);

	if (vbfFile)
		fclose(vbfFile);

	return (status);
}


static void image2Bitmap(pixel *srcPix, int imageWidth, int glyphWidth,
	int glyphHeight, unsigned char *destBytes)
{
	int pixelsPerChar = 0;
	int pixelCount = 0;
	int count;

	pixelsPerChar = (glyphWidth * glyphHeight);

	// Loop through the image data, setting bitmap bits for each black pixel
	// in the image.

	for (count = 0; count < pixelsPerChar; count ++)
	{
		// If it's black, set the corresponding bit in the new bitmap
		if (PIXELS_EQ(&srcPix[pixelCount], &((color) { 0, 0, 0 } )))
		{
			destBytes[count / 8] |= (0x80 >> (count % 8));

			if (verbose)
				printf("#");
		}
		else if (verbose)
			printf("_");

		pixelCount += 1;
		if (!(pixelCount % glyphWidth))
		{
			pixelCount += (imageWidth - glyphWidth);
			if (verbose)
				printf("\n");
		}
	}
}


static int import(const char *imageFileName, const char *vbfFileName)
{
	int status = 0;
	image importImage;
	FILE *vbfFile = NULL;
	vbfFileHeader vbfHeader;
	static int glyphColumns = 16;
	static int glyphRows = 12;
	int bytesPerChar = 0;
	int *codes = NULL;
	unsigned char *data = NULL;
	int startPixel = 0;
	int startByte = 0;
	int count, colCount, rowCount;
	
	printf("Import font from %s to VBF file %s\n", imageFileName, vbfFileName);
	
	bzero(&importImage, sizeof(image));
	bzero(&vbfHeader, sizeof(vbfHeader));

	// Try to get the kernel to load the image
	status = imageLoad(imageFileName, 0, 0, &importImage);
	if (status < 0)
	{
		errno = status;
		perror(cmdName);
		fprintf(stderr, "Error loading font image file %s\n", imageFileName);
		goto out;
	}

	if (importImage.width % glyphColumns)
	{
		fprintf(stderr, "Image width (%d) of %s is not a multiple of %d\n",
			importImage.width, imageFileName, glyphColumns);
		status = ERR_INVALID;
		goto out;
	}

	if (importImage.height % glyphRows)
	{
		fprintf(stderr, "Image height (%d) of %s is not a multiple of %d\n",
			importImage.height, imageFileName, glyphRows);
		status = ERR_INVALID;
		goto out;
	}

	// Open our output file
	vbfFile = fopen(vbfFileName, "w");
	if (!vbfFile)
	{
		perror(cmdName);
		fprintf(stderr, "Can't open font file %s for writing\n", vbfFileName);
		status = errno;
		goto out;
	}

	strncpy(vbfHeader.magic, VBF_MAGIC, 4);
	vbfHeader.version = VBF_VERSION;
	if (fontName)
		strncpy(vbfHeader.name, fontName, 32);
	else
		strncpy(vbfHeader.name, imageFileName, 32);
	if (points)
		vbfHeader.points = points;
	if (codePage)
		strncpy(vbfHeader.codePage, codePage, 16);
	vbfHeader.numGlyphs = (glyphColumns * glyphRows);
	vbfHeader.glyphWidth = (importImage.width / glyphColumns);
	vbfHeader.glyphHeight = (importImage.height / glyphRows);

	if (verbose)
		printf("%d glyphs size %dx%d\n", vbfHeader.numGlyphs,
			vbfHeader.glyphWidth, vbfHeader.glyphHeight);

	bytesPerChar = (((vbfHeader.glyphWidth * vbfHeader.glyphHeight) + 7) / 8);

	// Allocate memory for the codes and data
	codes = malloc(vbfHeader.numGlyphs * sizeof(int));
	data = malloc(vbfHeader.numGlyphs * bytesPerChar);
	if (!codes || !data)
	{
		perror(cmdName);
		fprintf(stderr, "Error allocating memory\n");
		status = errno;
		goto out;
	}

	// Loop through and put in the codes
	for (count = 0; count < 96; count ++)
		codes[count] = (count + 32);
	for (count = 0; count < 96; count ++)
		codes[count + 96] = (count + 160);

	// Loop through the characters in the image and add them
	for (rowCount = 0; rowCount < glyphRows; rowCount ++)
	{
		for (colCount = 0; colCount < glyphColumns; colCount ++)
		{
			// Calculate the starting pixel number of the image we're working
			// from
			startPixel = ((rowCount * glyphColumns * vbfHeader.glyphWidth *
				vbfHeader.glyphHeight) + (colCount * vbfHeader.glyphWidth));

			startByte = (((rowCount * glyphColumns) + colCount) * bytesPerChar);

			// Convert it to bitmap data
			image2Bitmap((importImage.data + (startPixel * sizeof(pixel))),
				importImage.width, vbfHeader.glyphWidth,
				vbfHeader.glyphHeight, (data + startByte));
		}
	}

	// Write out the font
	status = writeFont(vbfFile, &vbfHeader, codes, data);

out:
	if (codes)
		free(codes);
	if (data)
		free(data);
	if (vbfFile)
		fclose(vbfFile);

	return (status);
}


static int addGlyph(int code, const char *addFileName, const char *vbfFileName)
{
	int status = 0;
	image addImage;
	FILE *destFile = NULL;
	vbfFileHeader vbfHeader;
	int *oldCodes = NULL;
	unsigned char *oldData = NULL;
	int bytesPerChar = 0;
	int newNumGlyphs = 0;
	int *newCodes = NULL;
	unsigned char *newData = NULL;
	int pos = 0;
	int count;

	printf("Add glyph %d from %s to VBF file %s\n", code, addFileName,
		vbfFileName);
	
	bzero(&addImage, sizeof(image));
	bzero(&vbfHeader, sizeof(vbfHeader));

	// Try to get the kernel to load the image
	status = imageLoad(addFileName, 0, 0, &addImage);
	if (status < 0)
	{
		errno = status;
		perror(cmdName);
		fprintf(stderr, "Error loading glyph image file %s\n", addFileName);
		return (status);
	}

	// Open our output file
	destFile = fopen(vbfFileName, "r+");
	if (!destFile)
	{
		perror(cmdName);
		fprintf(stderr, "Can't open destination file %s for writing\n",
			vbfFileName);
		return (status = errno);
	}

	// Read in the font
	status = readFont(destFile, &vbfHeader, &oldCodes, &oldData);
	if (status < 0)
		goto out;

	bytesPerChar = (((vbfHeader.glyphWidth * vbfHeader.glyphHeight) + 7) / 8);

	newNumGlyphs = vbfHeader.numGlyphs;
	newCodes = oldCodes;
	newData = oldData;

	// Does the glyph already appear in the font, or are we replacing it?
	if ((pos = glyphPosition(oldCodes, vbfHeader.numGlyphs, code)) < 0)
	{
		// The glyph doesn't appear in the font.  Make space for it.

		newNumGlyphs += 1;

		// Get memory for the new codes and new data
		newCodes = malloc(newNumGlyphs * sizeof(int));
		newData = malloc(newNumGlyphs * bytesPerChar);
		if (!newCodes || !newData)
		{
			perror(cmdName);
			fprintf(stderr, "Error getting memory for character codes or "
				"data\n");
			status = errno;
			goto out;
		}

		// Find the correct (sorted) place in the map
		pos = vbfHeader.numGlyphs;
		for (count = 0; count < vbfHeader.numGlyphs; count ++)
		{
			if (oldCodes[count] > code)
			{
				pos = count;
				break;
			}
		}

		// Copy the codes from the 'before' part of the map
		memcpy(newCodes, oldCodes, (pos * sizeof(int)));
		// Copy the codes from the 'after' part of the map
		memcpy((newCodes + ((pos + 1) * sizeof(int))),
			(oldCodes + (pos * sizeof(int))),
			((vbfHeader.numGlyphs - pos) * sizeof(int)));

		// Copy the data from the 'before' glyphs
		memcpy(newData, oldData, (pos * bytesPerChar));
		// Copy the data from the 'after' glyphs
		memcpy((newData + ((pos + 1) * bytesPerChar)),
			(oldData + (pos * bytesPerChar)),
			((vbfHeader.numGlyphs - pos) * bytesPerChar));

		vbfHeader.numGlyphs = newNumGlyphs;
	}
	else
		// Clear the existing data
		memset((newData + (pos * bytesPerChar)), 0, bytesPerChar);

	// Set the code value in the map
	newCodes[pos] = code;

	// Convert the image data into font bitmap data
	image2Bitmap(addImage.data, addImage.width, vbfHeader.glyphWidth,
		vbfHeader.glyphHeight, (newData + (pos * bytesPerChar)));

	// Write the font back to disk
	status = writeFont(destFile, &vbfHeader, newCodes, newData);

out:
	if (destFile)
		fclose(destFile);
	if (oldCodes)
		free(oldCodes);
	if (oldData)
		free(oldData);
	if (newCodes && (newCodes != oldCodes))
		free(newCodes);
	if (newData && (newData != oldData))
		free(newData);

	return (status);
}


static int removeGlyph(int code, const char *vbfFileName)
{
	int status = 0;
	FILE *destFile = NULL;
	vbfFileHeader vbfHeader;
	int *codes = NULL;
	unsigned char *data = NULL;
	int pos = 0;
	int bytesPerChar = 0;
	unsigned newFileSize = 0;

	printf("Remove glyph %d from VBF file %s\n", code, vbfFileName);
	
	bzero(&vbfHeader, sizeof(vbfHeader));

	// Open our output file
	destFile = fopen(vbfFileName, "r+");
	if (!destFile)
	{
		perror(cmdName);
		fprintf(stderr, "Can't open destination file %s for writing\n",
			vbfFileName);
		return (status = errno);
	}

	// Read in the font
	status = readFont(destFile, &vbfHeader, &codes, &data);
	if (status < 0)
		goto out;

	// Find the position of the glyph in the map
	pos = glyphPosition(codes, vbfHeader.numGlyphs, code);
	if (pos < 0)
	{
		fprintf(stderr, "Glyph %d does not exist in font file %s\n", code,
			vbfFileName);
		status = ERR_NOSUCHENTRY;
		goto out;
	}
	
	if (pos < (vbfHeader.numGlyphs - 1))
	{
		bytesPerChar =
			(((vbfHeader.glyphWidth * vbfHeader.glyphHeight) + 7) / 8);

		// 'Erase' the code by copying the following ones over it
		memcpy(&codes[pos], &codes[pos + 1],
			((vbfHeader.numGlyphs - pos) * sizeof(int)));
		// 'Erase' the data
		memcpy((data + (pos * bytesPerChar)),
			(data + ((pos + 1) * bytesPerChar)),
			((vbfHeader.numGlyphs - pos) * bytesPerChar));
	}

	vbfHeader.numGlyphs -= 1;

	// Write the font back to disk
	status = writeFont(destFile, &vbfHeader, codes, data);

out:
	if (destFile)
	{
		newFileSize = destFile->offset;

		fclose(destFile);

		if (!status)
			// Truncate the file to the current file offset
			truncate(vbfFileName, newFileSize);
	}

	return (status);
}


static int convert(const char *convertFileName, const char *vbfFileName)
{
	// Given an old-style font file, which is really just a very oblong Windows
	// bitmap (.bmp) file, ask the kernel to load it, returning it as an image,
	// so we can steal the data from it and convert it into our new proprietary
	// bitmapped font file format (VBF)

	int status = 0;
	image convertImage;
	vbfFileHeader vbfHeader;
	FILE *destFile = NULL;
	int codes[ASCII_PRINTABLES];
	int pixelsPerChar = 0;
	int bytesPerChar = 0;
	int destBytes = 0;
	unsigned char *destBitmap = NULL;
	pixel *srcPix = NULL;
	int count1, count2;

	printf("Convert %s to VBF file %s\n", convertFileName, vbfFileName);

	bzero(&convertImage, sizeof(image));
	bzero(&vbfHeader, sizeof(vbfHeader));

	status = imageLoad(convertFileName, 0, 0, &convertImage);
	if (status < 0)
	{
		fprintf(stderr, "Error loading source font file %s\n", convertFileName);
		return (status);
	}

	// Open our output file
	destFile = fopen(vbfFileName, "w+");
	if (!destFile)
	{
		perror(cmdName);
		fprintf(stderr, "Can't open destination file %s for writing\n",
			vbfFileName);
		return (status = errno);
	}

	strncpy(vbfHeader.magic, VBF_MAGIC, 4);
	vbfHeader.version = VBF_VERSION;
	if (fontName)
		strncpy(vbfHeader.name, fontName, 32);
	else
		strncpy(vbfHeader.name, convertFileName, 32);
	if (points)
		vbfHeader.points = points;
	if (codePage)
		strncpy(vbfHeader.codePage, codePage, 16);
	vbfHeader.numGlyphs = ASCII_PRINTABLES;
	vbfHeader.glyphWidth = convertImage.width;
	vbfHeader.glyphHeight = (convertImage.height / ASCII_PRINTABLES);

	if (verbose)
		printf("Glyph size %dx%d\n", vbfHeader.glyphWidth, vbfHeader.glyphHeight);

	// Write out the header
	status = writeHeader(destFile, &vbfHeader);
	if (status < 0)
	{
		fclose(destFile);
		return (status);
	}

	// Old-style font files contain all 95 ASCII printables (32-126).  Write out
	// a code for each one.
	for (count1 = 0; count1 < ASCII_PRINTABLES; count1 ++)
		codes[count1] = (count1 + 32);

	status = fwrite(codes, sizeof(int), ASCII_PRINTABLES, destFile);
	if (status < ASCII_PRINTABLES)
	{
		perror(cmdName);
		fprintf(stderr, "Error writing destination file %s\n", vbfFileName);
		fclose(destFile);
		return (status = errno);
	}

	// Convert the image data rows into rows of mono bitmap image data

	pixelsPerChar = (vbfHeader.glyphWidth * vbfHeader.glyphHeight);
	bytesPerChar = ((pixelsPerChar + 7) / 8);
	destBytes = (ASCII_PRINTABLES * bytesPerChar);

	destBitmap = malloc(destBytes);
	if (!destBitmap)
	{
		perror(cmdName);
		fprintf(stderr, "Error allocating memory\n");
		fclose(destFile);
		return (status = errno);
	}

	// Now loop through the image data, setting bitmap bits for each black pixel
	// in the image.

	srcPix = convertImage.data;
	for (count1 = 0; count1 < ASCII_PRINTABLES; count1 ++)
	{
		for (count2 = 0; count2 < pixelsPerChar; count2 ++)
		{
			if (verbose)
			{
				//if (!(count2 % 8))
				//	printf(" ");

				if (!(count2 % vbfHeader.glyphWidth))
					printf("\n");
			}

			// If it's black, set the corresponding bit in the new bitmap
			if (PIXELS_EQ(srcPix, &((color) { 0, 0, 0 } )))
			{
				destBitmap[(count1 * bytesPerChar) + (count2 / 8)] |=
					(0x80 >> (count2 % 8));

				if (verbose)
					printf("#");
			}
			else if (verbose)
				printf("_");

			srcPix++;
		}

		if (verbose)
		{
			printf(" ");
			for (count2 = 0; count2 < bytesPerChar; count2 ++)
				printf("%02x ", destBitmap[(count1 * bytesPerChar) + count2]);
			printf("\n");
		}
	}

	printf("Writing %d bytes of bitmap data\n", destBytes);
	
	// Write out the bitmap data
	status = fwrite(destBitmap, 1, destBytes, destFile);

	free(destBitmap);
	fclose(destFile);

	if (status < destBytes)
	{
		fprintf(stderr, "Error writing destination file %s\n", vbfFileName);
		return (status = errno);
	}

	return (status = 0);
}


static void usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "  %s [options] <VBF_file>\n", cmdName);
	fprintf(stderr, "  (type 'help %s' for options help\n", cmdName);
	return;
}


int main(int argc, char *argv[])
{
	int status = 0;
	const char *vbfFileName = NULL;
	char opt;
	operation_type operation = operation_none;
	int code = -1;
	const char *convertFileName = NULL;
	const char *otherFileName = NULL;

	cmdName = argv[0];

	if (argc < 2)
	{
		usage();
		errno = ERR_ARGUMENTCOUNT;
		return (status = errno);
	}
	
	vbfFileName = argv[argc - 1];

	while (strchr("acdfinprvx:?",
		(opt = getopt(argc, argv, "a:c:d:f:i:n:p:r:vx:"))))
	{
		switch (opt)
		{
			case 'a':
				// Add a glyph
				if (!optarg)
				{
					fprintf(stderr, "Missing code argument for '-a' option\n");
					usage();
					return (status = ERR_NULLPARAMETER);
				}
				code = atoi(optarg);
				operation = operation_add;
				break;

			case 'c':
				// Set the codepage
				if (!optarg)
				{
					fprintf(stderr, "Missing codepage argument for '-c' option\n");
					usage();
					return (status = ERR_NULLPARAMETER);
				}
				codePage = optarg;
				if (operation == operation_none)
					operation = operation_update;
				break;

			case 'd':
				// Just dump out the font data
				if (optarg && (optarg != vbfFileName))
					code = atoi(optarg);
				operation = operation_dump;
				break;

			case 'f':
				// Another file name (depends on the operation)
				if (!optarg)
				{
					fprintf(stderr, "Missing filename argument for '-f' option\n");
					usage();
					return (status = ERR_NULLPARAMETER);
				}
				otherFileName = optarg;
				break;

			case 'i':
				// Import a new font from an image file.
				if (!optarg)
				{
					fprintf(stderr, "Missing image filename argument for '-i' "
						"option\n");
					usage();
					return (status = ERR_NULLPARAMETER);
				}
				otherFileName = optarg;
				operation = operation_import;
				break;

			case 'n':
				// Set the name
				if (!optarg)
				{
					fprintf(stderr, "Missing font name argument for '-n' option\n");
					usage();
					return (status = ERR_NULLPARAMETER);
				}
				fontName = optarg;
				if (operation == operation_none)
					operation = operation_update;
				break;

			case 'p':
				// Set the number of points
				if (!optarg)
				{
					fprintf(stderr, "Missing points argument for '-p' option\n");
					usage();
					return (status = ERR_NULLPARAMETER);
				}
				points = atoi(optarg);
				if (operation == operation_none)
					operation = operation_update;
				break;

			case 'r':
				// Remove a glyph
				if (!optarg)
				{
					fprintf(stderr, "Missing code argument for '-r' option\n");
					usage();
					return (status = ERR_NULLPARAMETER);
				}
				code = atoi(optarg);
				operation = operation_remove;
				break;

			case 'v':
				verbose = 1;
				break;

			case 'x':
				// Convert
				if (!optarg)
				{
					fprintf(stderr, "Missing filename argument for '-x' option\n");
					usage();
					return (status = ERR_NULLPARAMETER);
				}
				operation = operation_convert;
				convertFileName = optarg;
				break;

			case ':':
				fprintf(stderr, "Missing parameter for %s option\n",
					argv[optind - 1]);
				usage();
				return (status = ERR_NULLPARAMETER);

			case '?':
				fprintf(stderr, "Unknown option '%c'\n", optopt);
				usage();
				return (status = ERR_INVALID);
		}
	}

	switch (operation)
	{
		case operation_dump:
			status = dump(vbfFileName, code);
			break;

		case operation_convert:
			status = convert(convertFileName, vbfFileName);
			break;

		case operation_update:
			status = updateHeader(vbfFileName);
			break;

		case operation_import:
			status = import(otherFileName, vbfFileName);
			break;

		case operation_add:
			if (!otherFileName)
			{
				fprintf(stderr, "Missing image file (-f) argument to add (-a) "
					"operation\n");
				usage();
				return (status = ERR_NULLPARAMETER);
			}
			status = addGlyph(code, otherFileName, vbfFileName);
			break;

		case operation_remove:
			status = removeGlyph(code, vbfFileName);
			break;

		default:
		case operation_none:
			fprintf(stderr, "No operation specified\n");
			status = ERR_INVALID;
			break;
	}

	return (status);
}
