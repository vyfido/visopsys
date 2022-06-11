//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  libhtml.h
//

// This file contains public definitions and structures used by the HTML
// library.

#ifndef _LIBHTML_H
#define _LIBHTML_H

#include <sys/font.h>
#include <sys/html.h>

// Default rendering parameters
#define HTML_DEFAULT_RENDERING_PARAMS \
	((htmlRenderParameters){ \
		/* Normal font */ \
		{ { FONT_FAMILY_LIBMONO, FONT_STYLEFLAG_NORMAL, 8  /* heading6 */ }, \
		  { FONT_FAMILY_LIBMONO, FONT_STYLEFLAG_BOLD, 8    /* heading5 */ }, \
		  { FONT_FAMILY_LIBMONO, FONT_STYLEFLAG_NORMAL, 8  /* small */ }, \
		  { FONT_FAMILY_LIBMONO, FONT_STYLEFLAG_NORMAL, 10 /* normal */ }, \
		  { FONT_FAMILY_LIBMONO, FONT_STYLEFLAG_BOLD, 10   /* bold */ }, \
		  { FONT_FAMILY_ARIAL, FONT_STYLEFLAG_BOLD, 10     /* heading4 */ }, \
		  { FONT_FAMILY_ARIAL, FONT_STYLEFLAG_BOLD, 10     /* heading3 */ }, \
		  { FONT_FAMILY_ARIAL, FONT_STYLEFLAG_BOLD, 12     /* big */ }, \
		  { FONT_FAMILY_ARIAL, FONT_STYLEFLAG_BOLD, 12     /* heading2 */ }, \
		  { FONT_FAMILY_ARIAL, FONT_STYLEFLAG_BOLD, 20     /* heading1 */ } } \
	} )

// Library debugging flags
extern int debugLibHtml;

#endif

