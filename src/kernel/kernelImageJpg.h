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
//  kernelImageJpg.h
//
	
// This defines things used by kernelImageJpg.c for manipulating JPEG format
// image files

#if !defined(_KERNELIMAGEJPG_H)

// Constants

#define JPG_SOF            0xC0
// SOF (Start Of Frame) markers, unused except to recognise unsupported
// formats
#define JPG_SOF1           0xC1
#define JPG_SOF2           0xC2
#define JPG_SOF3           0xC3
#define JPG_SOF5           0xC5
#define JPG_SOF6           0xC6
#define JPG_SOF7           0xC7
#define JPG_SOF9           0xC9
#define JPG_SOFA           0xCA
#define JPG_SOFB           0xCB
#define JPG_SOFD           0xCD
#define JPG_SOFE           0xCE
#define JPG_SOFF           0xCF
// End unsupported SOFs
#define JPG_DHT            0xC4
#define JPG_SOI            0xD8
#define JPG_EOI            0xD9
#define JPG_SOS            0xDA
#define JPG_DQT            0xDB
#define JPG_DRI            0xDD
#define JPG_APP0           0xE0
#define JPG_COM            0xFE
#define JPG_APP0_MARK      "JFIF"
#define JPG_UNITS_NONE     0
#define JPG_UNITS_DPI      1
#define JPG_UNITS_DPCM     2

#define JPG_HUFF_TABLES    4
#define JPG_HUFF_VALUES    64
#define JPG_QUANT_TABLES   2

// The 4 Huffman tables in our array
#define JPG_HUFF_DC_LUM    0 // DC of luminance (Y)
#define JPG_HUFF_AC_LUM    1 // AC of luminance (Y)
#define JPG_HUFF_DC_CHROM  2 // DC of chrominance (Cb, Cr)
#define JPG_HUFF_AC_CHROM  3 // AC of chrominance (Cb, Cr)

// The .jpg APP0 header

// This is the on-disk JPEG file header
typedef struct {
  unsigned short length;
  char identifier[5];  
  unsigned char versionMajor;
  unsigned char versionMinor;
  unsigned char units;
  unsigned short densityX;
  unsigned short densityY;
  unsigned char thumbX;
  unsigned char thumbY;
  unsigned char thumbData[];

} __attribute__((packed)) jpgHeader;

// This is the on-disk Huffman table header
typedef struct {
  unsigned short length;
  unsigned char classIdent;
  unsigned char codeCounts[16];
  unsigned char values[JPG_HUFF_VALUES];

} __attribute__((packed)) jpgHuffHeader;

// This is the on-disk quantization table
typedef struct {
  unsigned short length;
  unsigned char precisionIdent;
  unsigned char values[64];

} __attribute__((packed)) jpgQuantTable;

// This is the on-disk restart interval header
typedef struct {
  unsigned short length;
  unsigned short interval;

} __attribute__((packed)) jpgRestartHeader;

// This is the on-disk frame (image data) header
typedef struct {
  unsigned short length;
  unsigned char precision;
  unsigned short height;
  unsigned short width;
  unsigned char numComps;

  struct {
    unsigned char compId;
    unsigned char samplingFactor;
    unsigned char quantTable;
  } comp[4]; // Up to 4, most often 3 (Y, Cb, Cr)

} __attribute__((packed)) jpgFrameHeader;

// This pairs a Huffman code with a value from the on-disk Huffman table
typedef struct {
  unsigned short code;
  unsigned char value;

} jpgHuffCode;

// This is just a count along with a kernelMalloc'ed array of Huffman codes
typedef struct {
  int numCodes;
  jpgHuffCode *codes;

} jpgHuffTable;

#define _KERNELIMAGEJPG_H
#endif
