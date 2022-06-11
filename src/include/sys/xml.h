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
//  xml.h
//

// This file contains public definitions and structures used by the XML
// library.

#ifndef _XML_H
#define _XML_H

#define XML_TAG_COMMENT_OPEN	"<!--"
#define XML_TAG_COMMENT_CLOSE	"-->"

typedef enum {
	tag_unknown,
	tag_single,
	tag_open,
	tag_close

} xmlTagType;

typedef struct _xmlTag {
	const char *start;
	int len;
	xmlTagType type;
	const char *name;
	int nameLen;
	struct _xmlTag *closeTag;

} xmlTag;

typedef struct {
	const char *name;
	int nameLen;
	const char *value;
	int valueLen;

} xmlAttribute;

typedef struct _xmlElement {
	xmlTag *openTag;
	const char *start;
	int len;
	const char *name;
	int nameLen;
	const char *body;
	int bodyLen;
	xmlAttribute *attribute;
	int numAttributes;
	struct _xmlElement *child;
	struct _xmlElement *next;

} xmlElement;

typedef struct {
	xmlTag *tag;
	int numTags;
	xmlElement rootElement;
	xmlElement *element;
	int numElements;

} xmlDocument;

int xmlParse(xmlDocument *, const char *, unsigned);
void xmlFree(xmlDocument *);
xmlElement *xmlFindElement(xmlDocument *, const char *[]);

#endif

