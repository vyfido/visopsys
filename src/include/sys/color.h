//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  color.h
//

// This file contains definitions and structures for using and manipulating
// colors in Visopsys

#ifndef _COLOR_H
#define _COLOR_H

// Default color values for the system
#define COLOR_DEFAULT_FOREGROUND_RED	11
#define COLOR_DEFAULT_FOREGROUND_GREEN	21
#define COLOR_DEFAULT_FOREGROUND_BLUE	49
#define COLOR_DEFAULT_BACKGROUND_RED	175
#define COLOR_DEFAULT_BACKGROUND_GREEN	175
#define COLOR_DEFAULT_BACKGROUND_BLUE	190
#define COLOR_DEFAULT_DESKTOP_RED		36
#define COLOR_DEFAULT_DESKTOP_GREEN		46
#define COLOR_DEFAULT_DESKTOP_BLUE		89

// Names for color settings
#define COLOR_SETTING_FOREGROUND		"foreground"
#define COLOR_SETTING_BACKGROUND		"background"
#define COLOR_SETTING_DESKTOP			"desktop"

// A structure to represent an RGB color value
typedef struct {
	unsigned char blue;
	unsigned char green;
	unsigned char red;

} color;

// Default color macros
#define COLOR_DEFAULT_FOREGROUND \
	((color){ \
		COLOR_DEFAULT_FOREGROUND_BLUE, \
		COLOR_DEFAULT_FOREGROUND_GREEN, \
		COLOR_DEFAULT_FOREGROUND_RED } )

#define COLOR_DEFAULT_BACKGROUND \
	((color){ \
		COLOR_DEFAULT_BACKGROUND_BLUE, \
		COLOR_DEFAULT_BACKGROUND_GREEN, \
		COLOR_DEFAULT_BACKGROUND_RED } )

#define COLOR_DEFAULT_DESKTOP \
	((color){ \
		COLOR_DEFAULT_DESKTOP_BLUE, \
		COLOR_DEFAULT_DESKTOP_GREEN, \
		COLOR_DEFAULT_DESKTOP_RED } )

// Color copying macro
#define COLOR_COPY(destColor, srcColor) { \
	(destColor)->blue = (srcColor)->blue; \
	(destColor)->green = (srcColor)->green; \
	(destColor)->red = (srcColor)->red; \
}

// Color adjustment macro
#define COLOR_ADJUST(destColor, srcColor, numerator, denominator) { \
	(destColor)->blue = min(255, (((int)(srcColor)->blue * numerator) / \
		denominator)); \
	(destColor)->green = min(255, (((int)(srcColor)->green * numerator) / \
		denominator)); \
	(destColor)->red = min(255, (((int)(srcColor)->red * numerator) / \
		denominator)); \
}

// Standard PC (text-mode) color values, and our real-color translations
// of them
#define COLOR_BLACK			((color){ 0, 0, 0 } )		// 0=Black
#define COLOR_BLUE			((color){ 170, 0, 0 } )		// 1=Blue
#define COLOR_GREEN			((color){ 0, 170, 0 } )		// 2=Green
#define COLOR_CYAN			((color){ 170, 170, 0 } )	// 3=Cyan
#define COLOR_RED			((color){ 0, 0, 170 } )		// 4=Red
#define COLOR_MAGENTA		((color){ 170, 0, 170 } )	// 5=Magenta
#define COLOR_BROWN			((color){ 0, 85, 170 } )	// 6=Brown
#define COLOR_LIGHTGRAY		((color){ 170, 170, 170 } )	// 7=Light gray
#define COLOR_DARKGRAY		((color){ 85, 85, 85 } )	// 8=Dark gray
#define COLOR_LIGHTBLUE		((color){ 255, 85, 85 } )	// 9=Light blue
#define COLOR_LIGHTGREEN	((color){ 85, 255, 85 } )	// 10=Light green
#define COLOR_LIGHTCYAN		((color){ 255, 255, 85 } )	// 11=Light cyan
#define COLOR_LIGHTRED		((color){ 85, 85, 255 } )	// 12=Light red
#define COLOR_LIGHTMAGENTA	((color){ 255, 85, 255 } )	// 13=Light magenta
#define COLOR_YELLOW		((color){ 85, 255, 255 } )	// 14=Yellow
#define COLOR_WHITE			((color){ 255, 255, 255 } )	// 15=White

#endif

