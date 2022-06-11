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
//  libhtml.c
//

// This is the main entry point for our library of HTML functions

#include "libhtml.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/html.h>
#include <sys/xml.h>


#ifdef DEBUG

	#define DEBUG_OUTMAX	160
	int debugLibHtml = 0;
	static char debugOutput[DEBUG_OUTMAX];

	static void DEBUGMSG(const char *message, ...)
	{
		va_list list;

		if (debugLibHtml)
		{
			va_start(list, message);
			vsnprintf(debugOutput, DEBUG_OUTMAX, message, list);
			printf("%s", debugOutput);
		}
	}

	static void dumpRaw(const char *buffer, unsigned len)
	{
		while (len)
		{
			DEBUGMSG("%c", *buffer);
			buffer += 1;
			len -= 1;
		}
	}

	static void dumpLine(const char *buffer, unsigned len)
	{
		if (len)
		{
			dumpRaw(buffer, len);
			DEBUGMSG("\n");
		}
	}

	static void print(htmlDocument *doc)
	{
		htmlElement *element = NULL;
		int count;

		if (doc->element)
		{
			for (count = 0; count < doc->numElements; count ++)
			{
				element = &doc->element[count];

				switch (element->type)
				{
					case html_text:
						DEBUGMSG("%s", element->text.text);
						break;

					case html_image:
						DEBUGMSG("[[IMAGE]]");
						break;

					case html_link:
						DEBUGMSG(" _");
						break;

					case html_link_end:
						DEBUGMSG("_ ");
						break;

					default:
						break;
				}
			}

			DEBUGMSG("\n");
		}
	}

#else
	#define DEBUGMSG(message, arg...) do { } while (0)
	#define dumpRaw(buffer, len) do { } while (0)
	#define dumpLine(buffer, len) do { } while (0)
	#define print(doc) do { } while (0)
#endif


static inline int isTag(const char *t1, int t1Len, const char *t2)
{
	if ((t1Len == (int) strlen(t2)) && !strncasecmp(t1, t2, t1Len))
		return (1);
	else
		return (0);
}


static unsigned addTextAttrs(xmlElement *xml, unsigned textAttrs)
{
	if (isTag(xml->name, xml->nameLen, HTML_TAG_B))
		textAttrs |= HTML_TEXTATTR_B;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_BIG))
		textAttrs |= HTML_TEXTATTR_BIG;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_BLOCKQUOTE))
		textAttrs |= HTML_TEXTATTR_BLOCKQUOTE;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_CENTER))
		textAttrs |= HTML_TEXTATTR_CENTER;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_CODE))
		textAttrs |= HTML_TEXTATTR_CODE;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_DEL))
		textAttrs |= HTML_TEXTATTR_DEL;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_EM))
		textAttrs |= HTML_TEXTATTR_EM;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_H1))
		textAttrs |= HTML_TEXTATTR_H1;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_H2))
		textAttrs |= HTML_TEXTATTR_H2;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_H3))
		textAttrs |= HTML_TEXTATTR_H3;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_H4))
		textAttrs |= HTML_TEXTATTR_H4;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_H5))
		textAttrs |= HTML_TEXTATTR_H5;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_H6))
		textAttrs |= HTML_TEXTATTR_H6;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_I))
		textAttrs |= HTML_TEXTATTR_I;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_INS))
		textAttrs |= HTML_TEXTATTR_INS;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_KBD))
		textAttrs |= HTML_TEXTATTR_KBD;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_MARK))
		textAttrs |= HTML_TEXTATTR_MARK;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_PRE))
		textAttrs |= HTML_TEXTATTR_PRE;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_Q))
		textAttrs |= HTML_TEXTATTR_Q;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_S))
		textAttrs |= HTML_TEXTATTR_S;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_SAMP))
		textAttrs |= HTML_TEXTATTR_SAMP;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_SMALL))
		textAttrs |= HTML_TEXTATTR_SMALL;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_STRIKE))
		textAttrs |= HTML_TEXTATTR_STRIKE;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_STRONG))
		textAttrs |= HTML_TEXTATTR_STRONG;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_SUB))
		textAttrs |= HTML_TEXTATTR_SUB;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_SUP))
		textAttrs |= HTML_TEXTATTR_SUP;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_TIME))
		textAttrs |= HTML_TEXTATTR_TIME;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_TT))
		textAttrs |= HTML_TEXTATTR_TT;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_U))
		textAttrs |= HTML_TEXTATTR_U;
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_VAR))
		textAttrs |= HTML_TEXTATTR_VAR;

	return (textAttrs);
}


static htmlElement *newElement(htmlDocument *doc)
{
	doc->element = realloc(doc->element, ((doc->numElements + 1) *
		sizeof(htmlElement)));
	if (!doc->element)
	{
		DEBUGMSG("Memory error\n");
		return (NULL);
	}

	memset(&doc->element[doc->numElements], 0, sizeof(htmlElement));

	return (&doc->element[doc->numElements++]);
}


static int addSimpleElement(htmlDocument *doc, htmlElementType type)
{
	htmlElement *element = NULL;

	element = newElement(doc);
	if (!element)
		return (-1);

	element->type = type;

	return (0);
}


static char *collapseString(const char *text, int len)
{
	char *string = NULL;
	int newLen = 0;
	int count;

	for (count = 0; count < len; count ++)
	{
		if (isspace(text[count]))
		{
			if (!count || isspace(text[count - 1]))
				continue;
		}

		newLen += 1;
	}

	if (newLen < 0)
		return (string = NULL);

	string = calloc((newLen + 1), 1);
	if (!string)
	{
		DEBUGMSG("Memory error\n");
		return (string);
	}

	if (!newLen)
	{
		// It can be an empty string (just a NULL character)
		return (string);
	}

	newLen = 0;

	for (count = 0; count < len; count ++)
	{
		if (isspace(text[count]))
		{
			if (!count || isspace(text[count - 1]))
				continue;

			string[newLen++] = ' ';
		}
		else
		{
			string[newLen++] = text[count];
		}
	}

	if (isspace(string[newLen - 1]))
		string[--newLen] = '\0';

	return (string);
}


static int addTextElement(htmlDocument *doc, const char *text, int len,
	unsigned attrs)
{
	char *string = NULL;
	int stringLen = 0;
	htmlElement *element = NULL;

	string = collapseString(text, len);
	if (!string)
		return (-1);

	stringLen = strlen(string);

	if (!stringLen)
	{
		// Empty
		free(string);
		return (0);
	}

	element = newElement(doc);
	if (!element)
		return (-1);

	element->type = html_text;
	element->text.text = string;
	element->text.len = stringLen;
	element->text.attrs = attrs;

	return (0);
}


static int addImageElement(htmlDocument *doc, xmlElement *xml)
{
	htmlElement *element = NULL;
	xmlAttribute *attr = NULL;
	int count;

	element = newElement(doc);
	if (!element)
		return (-1);

	element->type = html_image;

	for (count = 0; count < xml->numAttributes; count ++)
	{
		attr = &xml->attribute[count];

		if (!strncasecmp(attr->name, "src", attr->nameLen))
		{
			element->image.srcLen = attr->valueLen;
			element->image.src = calloc((element->image.srcLen + 1), 1);
			if (!element->image.src)
				return (errno);

			strncpy(element->image.src, attr->value, element->image.srcLen);
		}
		else if (!strncasecmp(attr->name, "width", attr->nameLen))
		{
			element->image.width = atoi(attr->value);
		}
		else if (!strncasecmp(attr->name, "height", attr->nameLen))
		{
			element->image.height = atoi(attr->value);
		}
	}

	return (0);
}


static int processElement(htmlDocument *doc, xmlElement *xml,
	unsigned textAttrs)
{
	const char *ptr = xml->body;
	int len = xml->bodyLen;
	int divLen = 0;
	xmlElement *child = xml->child;

	// Add any text attributes from the current tag
	textAttrs = addTextAttrs(xml, textAttrs);

	// Some tag types that add their own elements

	if (isTag(xml->name, xml->nameLen, HTML_TAG_DIV))
	{
		// This is a div (<div>) tag.  Only add elements for it if the body
		// has content.
		if (len)
		{
			addSimpleElement(doc, html_div);
			divLen = len;
		}
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_A))
	{
		// This is a link (<a>) tag
		addSimpleElement(doc, html_link);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_BLOCKQUOTE))
	{
		// This is a block quote (<blockquote>) tag
		addSimpleElement(doc, html_blockquote);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_BR))
	{
		// This is a line break (<br>) tag
		addSimpleElement(doc, html_linebreak);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_IMG))
	{
		// This is an image (<img>) tag
		addImageElement(doc, xml);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_P))
	{
		// This is a paragraph (<p>) tag
		addSimpleElement(doc, html_paragraph);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_TABLE))
	{
		// This is a table (<table>) tag
		addSimpleElement(doc, html_table);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_TD))
	{
		// This is a table cell (<td>) tag
		addSimpleElement(doc, html_table_cell);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_TH))
	{
		// This is a table header (<th>) tag
		addSimpleElement(doc, html_table_header);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_TR))
	{
		// This is a table row (<tr>) tag
		addSimpleElement(doc, html_table_row);
	}

	// Process the body of the tag, if any
	while (len > 0)
	{
		if (!child)
		{
			addTextElement(doc, ptr, len, textAttrs);
			len = 0;
		}
		else if (ptr < child->start)
		{
			addTextElement(doc, ptr, (child->start - ptr), textAttrs);

			len -= (child->start - ptr);
			ptr += (child->start - ptr);
		}
		else
		{
			processElement(doc, child, textAttrs);

			ptr += child->len;
			len -= child->len;
			child = child->next;
		}
	}

	// Some tag types that add their own element pairs that we need to close

	if (isTag(xml->name, xml->nameLen, HTML_TAG_BLOCKQUOTE))
	{
		addSimpleElement(doc, html_blockquote_end);
	}
	if (isTag(xml->name, xml->nameLen, HTML_TAG_DIV))
	{
		if (divLen)
			addSimpleElement(doc, html_div_end);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_A))
	{
		addSimpleElement(doc, html_link_end);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_P))
	{
		addSimpleElement(doc, html_paragraph_end);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_TABLE))
	{
		addSimpleElement(doc, html_table_end);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_TD))
	{
		addSimpleElement(doc, html_table_cell_end);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_TH))
	{
		addSimpleElement(doc, html_table_header_end);
	}
	else if (isTag(xml->name, xml->nameLen, HTML_TAG_TR))
	{
		addSimpleElement(doc, html_table_row_end);
	}

	return (0);
}


static int processBody(htmlDocument *doc)
{
	xmlElement *body = NULL;

	// Process the body
	body = xmlFindElement(&doc->xml, (const char *[]){ HTML_TAG,
		HTML_TAG_BODY, NULL});

	if (!body)
	{
		DEBUGMSG("HTML body not found\n");
		return (-1);
	}

	return (processElement(doc, body, 0 /* textAttrs */));
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int htmlParse(htmlDocument *doc, const char *buffer, unsigned len)
{
	xmlElement *element = NULL;

	// Check params
	if (!doc || !buffer || !len)
		return (-1);

	DEBUGMSG("Parse HTML (%u bytes)\n", len);

	if (xmlParse(&doc->xml, buffer, len) < 0)
	{
		DEBUGMSG("XML parse error\n");
		return (-1);
	}

	// Set some default rendering parameters
	doc->params = HTML_DEFAULT_RENDERING_PARAMS;

	// Try to find the title
	element = xmlFindElement(&doc->xml, (const char *[]){ HTML_TAG,
		HTML_TAG_HEAD, HTML_TAG_TITLE, NULL});
	if (element)
	{
		doc->title = element->body;
		doc->titleLen = element->bodyLen;

		dumpLine(doc->title, doc->titleLen);
	}

	// Process the body
	if (processBody(doc) < 0)
	{
		htmlFree(doc);
		return (-1);
	}

	print(doc);

	return (0);
}


void htmlFree(htmlDocument *doc)
{
	int count;

	if (doc)
	{
		xmlFree(&doc->xml);

		if (doc->element)
		{
			for (count = 0; count < doc->numElements; count ++)
			{
				switch (doc->element[count].type)
				{
					case html_image:
						if (doc->element[count].image.src)
							free(doc->element[count].image.src);
						break;

					case html_text:
						if (doc->element[count].text.text)
							free(doc->element[count].text.text);
						break;

					default:
						break;
				}
			}

			free(doc->element);
		}

		memset(doc, 0, sizeof(htmlDocument));
	}
}

