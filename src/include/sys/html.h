//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  html.h
//

// This file contains public definitions and structures used by the HTML
// library

#ifndef _HTML_H
#define _HTML_H

#include <sys/xml.h>

// HTML tags
#define HTML_TAG					"html"
#define HTML_TAG_A					"a"
#define HTML_TAG_ABBR				"abbr"
#define HTML_TAG_ACRONYM			"acronym"		// !HTML5
#define HTML_TAG_ADDRESS			"address"
#define HTML_TAG_APPLET				"applet"		// !HTML5
#define HTML_TAG_AREA				"area"
#define HTML_TAG_ARTICLE			"article"		// HTML5
#define HTML_TAG_ASIDE				"aside"			// HTML5
#define HTML_TAG_AUDIO				"audio"			// HTML5
#define HTML_TAG_B					"b"
#define HTML_TAG_BASE				"base"
#define HTML_TAG_BASEFONT			"basefont"		// !HTML5
#define HTML_TAG_BDI				"bdi"			// HTML5
#define HTML_TAG_BDO				"bdo"
#define HTML_TAG_BIG				"big"			// !HTML5
#define HTML_TAG_BLOCKQUOTE			"blockquote"
#define HTML_TAG_BODY				"body"
#define HTML_TAG_BR					"br"
#define HTML_TAG_BUTTON				"button"
#define HTML_TAG_CANVAS				"canvas"		// HTML5
#define HTML_TAG_CAPTION			"caption"
#define HTML_TAG_CENTER				"center"		// !HTML5
#define HTML_TAG_CITE				"cite"
#define HTML_TAG_CODE				"code"
#define HTML_TAG_COL				"col"
#define HTML_TAG_COLGROUP			"colgroup"
#define HTML_TAG_DATA				"data"			// HTML5
#define HTML_TAG_DATALIST			"datalist"		// HTML5
#define HTML_TAG_DD					"dd"
#define HTML_TAG_DEL				"del"
#define HTML_TAG_DETAILS			"details"		// HTML5
#define HTML_TAG_DFN				"dfn"
#define HTML_TAG_DIALOG				"dialog"		// HTML5
#define HTML_TAG_DIR				"dir"			// !HTML5
#define HTML_TAG_DIV				"div"
#define HTML_TAG_DL					"dl"
#define HTML_TAG_DT					"dt"
#define HTML_TAG_EM					"em"
#define HTML_TAG_EMBED				"embed"			// HTML5
#define HTML_TAG_FIELDSET			"fieldset"
#define HTML_TAG_FIGCAPTION			"figcaption"	// HTML5
#define HTML_TAG_FIGURE				"figure"		// HTML5
#define HTML_TAG_FONT				"font"			// !HTML5
#define HTML_TAG_FOOTER				"footer"		// HTML5
#define HTML_TAG_FORM				"form"
#define HTML_TAG_FRAME				"frame"			// !HTML5
#define HTML_TAG_FRAMESET			"frameset"		// !HTML5
#define HTML_TAG_H1					"h1"
#define HTML_TAG_H2					"h2"
#define HTML_TAG_H3					"h3"
#define HTML_TAG_H4					"h4"
#define HTML_TAG_H5					"h5"
#define HTML_TAG_H6					"h6"
#define HTML_TAG_HEAD				"head"
#define HTML_TAG_HEADER				"header"		// HTML5
#define HTML_TAG_HR					"hr"
#define HTML_TAG_I					"i"
#define HTML_TAG_IFRAME				"iframe"
#define HTML_TAG_IMG				"img"
#define HTML_TAG_INPUT				"input"
#define HTML_TAG_INS				"ins"
#define HTML_TAG_KBD				"kbd"
#define HTML_TAG_LABEL				"label"
#define HTML_TAG_LEGEND				"legend"
#define HTML_TAG_LI					"li"
#define HTML_TAG_LINK				"link"
#define HTML_TAG_MAIN				"main"			// HTML5
#define HTML_TAG_MAP				"map"
#define HTML_TAG_MARK				"mark"			// HTML5
#define HTML_TAG_META				"meta"
#define HTML_TAG_METER				"meter"			// HTML5
#define HTML_TAG_NAV				"nav"			// HTML5
#define HTML_TAG_NOFRAMES			"noframes"		// !HTML5
#define HTML_TAG_NOSCRIPT			"noscript"
#define HTML_TAG_OBJECT				"object"
#define HTML_TAG_OL					"ol"
#define HTML_TAG_OPTGROUP			"optgroup"
#define HTML_TAG_OPTION				"option"
#define HTML_TAG_OUTPUT				"output"		// HTML5
#define HTML_TAG_P					"p"
#define HTML_TAG_PARAM				"param"
#define HTML_TAG_PICTURE			"picture"		// HTML5
#define HTML_TAG_PRE				"pre"
#define HTML_TAG_PROGRESS			"progress"		// HTML5
#define HTML_TAG_Q					"q"
#define HTML_TAG_RP					"rp"			// HTML5
#define HTML_TAG_RT					"rt"			// HTML5
#define HTML_TAG_RUBY				"ruby"			// HTML5
#define HTML_TAG_S					"s"
#define HTML_TAG_SAMP				"samp"
#define HTML_TAG_SCRIPT				"script"
#define HTML_TAG_SECTION			"section"		// HTML5
#define HTML_TAG_SELECT				"select"
#define HTML_TAG_SMALL				"small"
#define HTML_TAG_SOURCE				"source"		// HTML5
#define HTML_TAG_SPAN				"span"
#define HTML_TAG_STRIKE				"strike"		// !HTML5
#define HTML_TAG_STRONG				"strong"
#define HTML_TAG_STYLE				"style"
#define HTML_TAG_SUB				"sub"
#define HTML_TAG_SUMMARY			"summary"		// HTML5
#define HTML_TAG_SUP				"sup"
#define HTML_TAG_SVG				"svg"
#define HTML_TAG_TABLE				"table"
#define HTML_TAG_TBODY				"tbody"
#define HTML_TAG_TD					"td"
#define HTML_TAG_TEMPLATE			"template"		// HTML5
#define HTML_TAG_TEXTAREA			"textarea"
#define HTML_TAG_TFOOT				"tfoot"
#define HTML_TAG_TH					"th"
#define HTML_TAG_THEAD				"thead"
#define HTML_TAG_TIME				"time"			// HTML5
#define HTML_TAG_TITLE				"title"
#define HTML_TAG_TR					"tr"
#define HTML_TAG_TRACK				"track"			// HTML5
#define HTML_TAG_TT					"tt"			// !HTML5
#define HTML_TAG_U					"u"
#define HTML_TAG_UL					"ul"
#define HTML_TAG_VAR				"var"
#define HTML_TAG_VIDEO				"video"			// HTML5
#define HTML_TAG_WBR				"wbr"			// HTML5

// Attributes of text
#define HTML_TEXTATTR_B				0x00000001
#define HTML_TEXTATTR_BIG			0x00000002
#define HTML_TEXTATTR_BLOCKQUOTE	0x00000004
#define HTML_TEXTATTR_CENTER		0x00000008
#define HTML_TEXTATTR_CODE			0x00000010
#define HTML_TEXTATTR_DEL			0x00000020
#define HTML_TEXTATTR_EM			0x00000040
#define HTML_TEXTATTR_H1			0x00000080
#define HTML_TEXTATTR_H2			0x00000100
#define HTML_TEXTATTR_H3			0x00000200
#define HTML_TEXTATTR_H4			0x00000400
#define HTML_TEXTATTR_H5			0x00000800
#define HTML_TEXTATTR_H6			0x00001000
#define HTML_TEXTATTR_I				0x00002000
#define HTML_TEXTATTR_INS			0x00004000
#define HTML_TEXTATTR_KBD			0x00008000
#define HTML_TEXTATTR_MARK			0x00010000
#define HTML_TEXTATTR_PRE			0x00020000
#define HTML_TEXTATTR_Q				0x00040000
#define HTML_TEXTATTR_S				0x00080000
#define HTML_TEXTATTR_SAMP			0x00100000
#define HTML_TEXTATTR_SMALL			0x00200000
#define HTML_TEXTATTR_STRIKE		0x00400000
#define HTML_TEXTATTR_STRONG		0x00800000
#define HTML_TEXTATTR_SUB			0x01000000
#define HTML_TEXTATTR_SUP			0x02000000
#define HTML_TEXTATTR_TIME			0x04000000
#define HTML_TEXTATTR_TT			0x08000000
#define HTML_TEXTATTR_U				0x10000000
#define HTML_TEXTATTR_VAR			0x20000000

#define HTML_TEXTATTR_HEADER		\
	( HTML_TEXTATTR_H1 | HTML_TEXTATTR_H2 | HTML_TEXTATTR_H3 | \
	  HTML_TEXTATTR_H4 | HTML_TEXTATTR_H5 | HTML_TEXTATTR_H6 )

typedef enum {
	html_unknown,
	html_blockquote,
	html_blockquote_end,
	html_div,
	html_div_end,
	html_image,
	html_linebreak,
	html_link,
	html_link_end,
	html_paragraph,
	html_paragraph_end,
	html_table,
	html_table_end,
	html_table_cell,
	html_table_cell_end,
	html_table_header,
	html_table_header_end,
	html_table_row,
	html_table_row_end,
	html_text

} htmlElementType;

typedef struct {
	char *src;
	int srcLen;
	int width;
	int height;

} htmlImageElement;

typedef struct {
	char *href;
	int hrefLen;

} htmlLinkElement;

typedef struct {
	char *text;
	int len;
	unsigned attrs;

} htmlTextElement;

typedef struct {
	htmlElementType type;
	union {
		htmlImageElement image;
		htmlLinkElement link;
		htmlTextElement text;
	};

} htmlElement;

typedef struct {
	const char *family;
	unsigned flags;
	int points;

} htmlFont;

typedef struct {
	struct {
		htmlFont heading6;
		htmlFont heading5;
		htmlFont small;
		htmlFont normal;
		htmlFont bold;
		htmlFont heading4;
		htmlFont heading3;
		htmlFont big;
		htmlFont heading2;
		htmlFont heading1;
	} font;

} htmlRenderParameters;

typedef struct {
	xmlDocument xml;
	htmlRenderParameters params;
	const char *title;
	int titleLen;
	htmlElement *element;
	int numElements;

} htmlDocument;

int htmlParse(htmlDocument *, const char *, unsigned);
void htmlFree(htmlDocument *);

#endif

