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
//  libxml.c
//

// This is the main entry point for our library of XML functions

#include "libxml.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/html.h>
#include <sys/xml.h>


#ifdef DEBUG

	#define DEBUG_OUTMAX	160
	int debugLibXml = 0;
	static char debugOutput[DEBUG_OUTMAX];

	static void DEBUGMSG(const char *message, ...)
	{
		va_list list;

		if (debugLibXml)
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

	static void dumpTags(xmlDocument *doc)
	{
		const char *tmp = NULL;
		int count;

		for (count = 0; count < doc->numTags; count ++)
		{
			switch(doc->tag[count].type)
			{
				case tag_single:
					tmp = "single";
					break;

				case tag_open:
					tmp = "open";
					break;

				case tag_close:
					tmp = "close";
					break;

				case tag_unknown:
				default:
					tmp = "unknown";
					break;
			}

			DEBUGMSG("tag %d ", count);
			dumpRaw(doc->tag[count].name, doc->tag[count].nameLen);
			DEBUGMSG(":%s \t", tmp);
			dumpLine(doc->tag[count].start, min(doc->tag[count].len, 60));
		}
	}

	static void dumpElementsRecursive(xmlElement *element, int level)
	{
		int count;

		for (count = 0; count < level; count ++)
			DEBUGMSG("  ");

		DEBUGMSG("element <");
		dumpRaw(element->name, element->nameLen);
		DEBUGMSG("> len %d body %d\n", element->len, element->bodyLen);

		if (element->child)
			dumpElementsRecursive(element->child, (level + 1));
		if (element->next)
			dumpElementsRecursive(element->next, level);
	}

	static void dumpElements(xmlDocument *doc)
	{
		dumpElementsRecursive(&doc->rootElement, 0);
	}

#else
	#define DEBUGMSG(message, arg...) do { } while (0)
	#define dumpRaw(buffer, len) do { } while (0)
	#define dumpLine(buffer, len) do { } while (0)
	#define dumpTags(doc) do { } while (0)
	#define dumpElementsRecursive(element, level) do { } while (0)
	#define dumpElements(doc) do { } while (0)
#endif


static const char *firstNonSpace(const char *buffer, unsigned len)
{
	while ((len > 0) && isspace(*buffer))
	{
		buffer += 1;
		len -= 1;
	}

	if ((len > 0) && !isspace(*buffer))
		return (buffer);
	else
		return (NULL);
}


static const char *lastNonSpace(const char *buffer, unsigned len)
{
	buffer = (buffer + (len - 1));

	while ((len > 0) && isspace(*buffer))
	{
		buffer -= 1;
		len -= 1;
	}

	if ((len > 0) && !isspace(*buffer))
		return (buffer);
	else
		return (NULL);
}


static xmlTagType tagType(const xmlTag *tag)
{
	const char *tmp = (tag->start + 1);
	int len = (tag->len - 2);

	if (len > 0)
	{
		tmp = firstNonSpace(tmp, len);
		if (tmp)
		{
			if (*tmp == '!')
				return (tag_single);

			if (*tmp == '/')
				return (tag_close);
		}

		tmp = (tag->start + 1);

		tmp = lastNonSpace(tmp, len);
		if (tmp)
		{
			if (*tmp == '/')
				return (tag_single);
			else
				return (tag_open);
		}
	}

	return (tag_unknown);
}


static void tagName(xmlTag *tag)
{
	const char *tmp = (tag->start + 1);
	int len = (tag->len - 2);

	if (len > 0)
	{
		tmp = firstNonSpace(tmp, len);
		if (tmp)
		{
			if (((*tmp == '!') && (tag->type == tag_single)) ||
				((*tmp == '/') && (tag->type == tag_close)))
			{
				tmp += 1;
				len -= ((tmp - tag->start) - 1);

				tmp = firstNonSpace(tmp, len);
				if (!tmp)
					return;
			}

			tag->name = tmp;

			while ((len > 0) && !isspace(*tmp))
			{
				tag->nameLen += 1;
				tmp += 1;
				len -= 1;
			}
		}
	}
}


static int enumerateTags(xmlDocument *doc, const char *buffer, unsigned len)
{
	xmlTag *openTag = NULL;
	int inComment = 0;

	while (len)
	{
		if (inComment)
		{
			if (!strncasecmp(buffer, XML_TAG_COMMENT_CLOSE, min(len, 3)))
			{
				buffer += min(len, 2);
				len -= min(len, 2);
				inComment = 0;
			}
			else
			{
				buffer += 1;
				len -= 1;
				continue;
			}
		}

		if (*buffer == '<')
		{
			if (openTag)
			{
				DEBUGMSG("Unterminated tag open\n");
				dumpLine(buffer, min(len, 16));
				return (-1);
			}

			doc->tag = realloc(doc->tag, ((doc->numTags + 1) *
				sizeof(xmlTag)));
			if (!doc->tag)
			{
				DEBUGMSG("Memory error\n");
				return (-1);
			}

			openTag = &doc->tag[doc->numTags];
			doc->numTags += 1;

			memset(openTag, 0, sizeof(xmlTag));
			openTag->start = buffer;

			// Watch out for the special case of comments, which can enclose
			// other tags.  We want to skip their contents entirely.
			if (!strncasecmp(buffer, XML_TAG_COMMENT_OPEN, min(len, 4)))
			{
				buffer += min(len, 3);
				len -= min(len, 3);
				inComment = 1;
			}
		}
		else if (*buffer == '>')
		{
			if (!openTag)
			{
				DEBUGMSG("Orphan tag close\n");
				dumpLine(buffer, min(len, 16));
				return (-1);
			}

			openTag->len = ((buffer - openTag->start) + 1);
			openTag->type = tagType(openTag);

			if (openTag->type == tag_unknown)
			{
				DEBUGMSG("Unknown tag type: ");
				dumpLine(openTag->start, min(openTag->len, 16));
				return (-1);
			}

			tagName(openTag);
			if (!openTag->name)
			{
				DEBUGMSG("Unknown tag name: ");
				dumpLine(openTag->start, min(openTag->len, 16));
				return (-1);
			}

			openTag = NULL;
		}

		buffer += 1;
		len -= 1;
	}

	return (0);
}


static int pairTags(xmlDocument *doc)
{
	xmlTag *tag1 = NULL, *tag2 = NULL;
	int opens = 0;
	int count1, count2;

	// Loop through all the tags, looking for 'open' tags that require a
	// corresponding 'close'
	for (count1 = 0; count1 < doc->numTags; count1 ++)
	{
		tag1 = &doc->tag[count1];

		if (tag1->type == tag_open)
		{
			opens = 1;

			// Look for 'close' tags with the same name, skipping nested
			// pairs of the same name.
			for (count2 = (count1 + 1); count2 < doc->numTags; count2 ++)
			{
				tag2 = &doc->tag[count2];

				if ((tag1->nameLen == tag2->nameLen) && !strncmp(tag1->name,
					tag2->name, tag1->nameLen))
				{
					if (tag2->type == tag_open)
					{
						// Nested 'open'
						opens += 1;
					}
					else if (tag2->type == tag_close)
					{
						opens -= 1;

						if (!opens)
						{
							// This is the corresponding 'close'
							tag1->closeTag = tag2;
							break;
						}
					}
				}
			}

			if (!tag1->closeTag)
			{
				// Some tags in e.g. HTML can look like opens, but are
				// technically allowed to be single (<a>, <br>, ...)
				tag1->type = tag_single;
			}
		}
	}

	return (0);
}


static void elementAttrs(xmlElement *element)
{
	const char *tmp = (element->openTag->name + element->openTag->nameLen);
	int len = (element->openTag->len - ((tmp - element->openTag->start) + 1));
	xmlAttribute *attribute = NULL;

	while (len > 0)
	{
		// Skip any whitespace
		while ((len > 0) && isspace(*tmp))
		{
			tmp += 1;
			len -= 1;
		}

		if (len <= 0)
			break;

		if (*tmp == '/')
			break;

		element->attribute = realloc(element->attribute,
			((element->numAttributes + 1) * sizeof(xmlAttribute)));
		if (!element->attribute)
		{
			DEBUGMSG("Memory error\n");
			return;
		}

		attribute = &element->attribute[element->numAttributes];
		element->numAttributes += 1;

		memset(attribute, 0, sizeof(xmlAttribute));
		attribute->name = tmp;

		// Find the end of the name ('=' or a whitespace character)
		while ((len > 0) && (*tmp != '=') && !isspace(*tmp))
		{
			attribute->nameLen += 1;
			tmp += 1;
			len -= 1;
		}

		if (len <= 0)
			break;

		// Find the beginning of the attribute value, if any

		while ((len > 0) && isspace(*tmp))
		{
			tmp += 1;
			len -= 1;
		}

		if (len <= 0)
			break;

		if (*tmp != '=')
			continue;

		tmp += 1;
		len -= 1;

		while ((len > 0) && isspace(*tmp))
		{
			tmp += 1;
			len -= 1;
		}

		if (len <= 0)
			break;

		if (*tmp != '"')
		{
			DEBUGMSG("Attribute value missing open \"\n");
			return;
		}

		tmp += 1;
		len -= 1;

		attribute->value = tmp;

		// Find the end of the value ('"')
		while ((len > 0) && (*tmp != '"'))
		{
			attribute->valueLen += 1;
			tmp += 1;
			len -= 1;
		}

		if (len <= 0)
			break;

		if (*tmp != '"')
		{
			DEBUGMSG("Attribute value missing close \"\n");
			return;
		}

		tmp += 1;
		len -= 1;
	}
}


static void makeElementTreeRecursive(xmlDocument *doc, xmlElement *parent,
	xmlElement *previous, int *nextIndex)
{
	xmlElement *current = NULL;

	while (*nextIndex < doc->numElements)
	{
		current = &doc->element[*nextIndex];

		// If the current element is not inside the parent, exit
		if (current->start >= (parent->start + parent->len))
			return;

		// Is the current element inside the previous one?
		if (current->start < (previous->start + previous->len))
		{
			// The current element is inside the previous - recurse
			previous->child = current;
			*nextIndex += 1;
			makeElementTreeRecursive(doc, previous, current, nextIndex);
		}
		else
		{
			// The current element is not inside the previous - chain
			previous->next = current;
			previous = current;
			*nextIndex += 1;
		}
	}
}


static void makeElementTree(xmlDocument *doc, xmlElement *root)
{
	int nextIndex = 1;

	root->child = &doc->element[0];

	makeElementTreeRecursive(doc, root, root->child, &nextIndex);
}


static int composeElements(xmlDocument *doc)
{
	xmlTag *tag = NULL;
	int numElements = 0;
	xmlElement *element = NULL;
	int count;

	// The number of elements will be the number of 'single' and 'open' tags
	for (count = 0; count < doc->numTags; count ++)
	{
		tag = &doc->tag[count];

		if ((tag->type == tag_single) || (tag->type == tag_open))
			numElements += 1;
	}

	// Allocate memory for all of the elements
	doc->element = calloc(numElements, sizeof(xmlElement));
	if (!doc->element)
	{
		DEBUGMSG("Memory error\n");
		return (-1);
	}

	// Make elements corresponding to all 'single' and 'open' tags
	for (count = 0; count < doc->numTags; count ++)
	{
		tag = &doc->tag[count];

		if ((tag->type == tag_single) || (tag->type == tag_open))
		{
			element = &doc->element[doc->numElements];

			element->openTag = tag;

			element->start = tag->start;
			if (tag->type == tag_single)
			{
				element->len = tag->len;
			}
			else
			{
				element->len = ((tag->closeTag->start - tag->start) +
					tag->closeTag->len);
			}

			element->name = tag->name;
			element->nameLen = tag->nameLen;

			if (tag->type == tag_open)
			{
				element->body = (tag->start + tag->len);
				element->bodyLen = (tag->closeTag->start - element->body);
			}

			elementAttrs(element);

			doc->numElements += 1;
		}
	}

	makeElementTree(doc, &doc->rootElement);

	return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int xmlParse(xmlDocument *doc, const char *buffer, unsigned len)
{
	// Check params
	if (!doc || !buffer || !len)
		return (-1);

	DEBUGMSG("Parse XML (%u bytes)\n", len);

	// Set up the document's root element
	doc->rootElement.start = buffer;
	doc->rootElement.len = len;
	doc->rootElement.name = "root";
	doc->rootElement.nameLen = strlen(doc->rootElement.name);

	// First pass: find all the tags and classify them (open, close, single)
	if (enumerateTags(doc, buffer, len) < 0)
	{
		DEBUGMSG("Error enumerating tags\n");
		xmlFree(doc);
		return (-1);
	}

	if (0) dumpTags(doc);

	// Second pass: pair open tags with close tags
	if (pairTags(doc) < 0)
	{
		DEBUGMSG("Error pairing tags\n");
		xmlFree(doc);
		return (-1);
	}

	// Third pass: build an array (and tree) of elements
	if (composeElements(doc) < 0)
	{
		DEBUGMSG("Error composing elements\n");
		xmlFree(doc);
		return (-1);
	}

	dumpElements(doc);

	return (0);
}


void xmlFree(xmlDocument *doc)
{
	int count;

	if (doc)
	{
		if (doc->tag)
			free(doc->tag);

		if (doc->element)
		{
			for (count = 0; count < doc->numElements; count ++)
			{
				if (doc->element[count].attribute)
					free(doc->element[count].attribute);
			}

			free(doc->element);
		}

		memset(doc, 0, sizeof(xmlDocument));
	}
}


xmlElement *xmlFindElement(xmlDocument *doc, const char *names[])
{
	const char *name = NULL;
	xmlElement *element = NULL;

	// Check params
	if (!doc || !names)
		return (element = NULL);

	element = doc->rootElement.child;
	if (!element)
		return (element = NULL);

	name = names[0];

	while (name)
	{
		DEBUGMSG("Find element <%s> ... ", name);

		while (element)
		{
			if (!strncasecmp(element->name, name, max(element->nameLen,
				(int) strlen(name))))
			{
				DEBUGMSG("found\n");

				name = *(++names);

				if (!name)
					// Finished
					goto out;

				element = element->child;
				break;
			}

			element = element->next;
		}

		if (!element)
		{
			DEBUGMSG("not found\n");
			break;
		}
	}

out:
	return (element);
}

