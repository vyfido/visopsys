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
//  vis.h
//

// This file contains definitions and structures for general library
// functionality shared between the kernel and userspace in Visopsys

#ifndef _VIS_H
#define _VIS_H

#include <sys/debug.h>
#include <sys/errors.h>
#include <sys/lock.h>
#include <sys/memory.h>

// Definitions for variable lists
#define VARIABLE_INITIAL_MEMORY		MEMORY_PAGE_SIZE
#define VARIABLE_INITIAL_NUMBER		32
#define VARIABLE_INITIAL_DATASIZE \
	(VARIABLE_INITIAL_MEMORY - (2 * VARIABLE_INITIAL_NUMBER * sizeof(char *)))

// Structures for linked lists

// A linked list item structure
typedef struct _linkedListItem {
	void *data;
	struct _linkedListItem *next;
	struct _linkedListItem *prev;

} linkedListItem;

// A linked list structure
typedef struct {
	linkedListItem *first;
	linkedListItem *last;
	int numItems;
	spinLock lock;

} linkedList;

// A variable list structure
typedef struct {
	int procId;
	int numVariables;
	int maxVariables;
	unsigned usedData;
	unsigned maxData;
	void *memory;
	unsigned memorySize;
	spinLock lock;
	int system;

} variableList;

typedef struct {
	void (*debug)(const char *, const char *, int, debug_category,
		const char *, ...) __attribute__((format(printf, 5, 6)));
	void (*error)(const char *, const char *, int, kernelErrorKind,
		const char *, ...) __attribute__((format(printf, 5, 6)));
	int (*_free)(void *, const char *);
	int (*lockGet)(spinLock *);
	int (*lockRelease)(spinLock *);
	void *(*_malloc)(unsigned, const char *);
	void *(*memoryGet)(unsigned, const char *);
	void *(*memoryGetSystem)(unsigned, const char *);
	int (*memoryRelease)(void *);
	int (*memoryReleaseSystem)(void *);
	int (*multitaskerGetCurrentProcessId)(void);

} kernelLibOps;

extern kernelLibOps kernLibOps;

// Functions

// Linked lists
int linkedListAddFront(linkedList *, void *);
int linkedListAddBack(linkedList *, void *);
int linkedListRemove(linkedList *, void *);
int linkedListClear(linkedList *);
void *linkedListIterStart(linkedList *, linkedListItem **);
void *linkedListIterNext(linkedList *, linkedListItem **);
void linkedListDebug(linkedList *);

// Variable lists
int variableListCreate(variableList *);
int variableListCreateSystem(variableList *);
int variableListDestroy(variableList *);
const char *variableListGetVariable(variableList *, int);
const char *variableListGet(variableList *, const char *);
int variableListSet(variableList *, const char *, const char *);
int variableListUnset(variableList *, const char *);
int variableListClear(variableList *);

#endif

