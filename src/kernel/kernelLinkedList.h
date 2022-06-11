//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  kernelLinkedList.h
//

#if !defined(_KERNELLINKEDLIST_H)

#include "kernelLock.h"

typedef struct _kernelLinkedListItem {
  void *data;
  struct _kernelLinkedListItem *next;
  struct _kernelLinkedListItem *prev;

} kernelLinkedListItem;

typedef struct {
  kernelLinkedListItem *first;
  kernelLinkedListItem *iter;
  int numItems;
  lock lock;

} kernelLinkedList;

int kernelLinkedListAdd(kernelLinkedList *, void *);
int kernelLinkedListRemove(kernelLinkedList *, void *);
int kernelLinkedListClear(kernelLinkedList *);
void *kernelLinkedListIterStart(kernelLinkedList *);
void *kernelLinkedListIterNext(kernelLinkedList *);
int kernelLinkedListIterEnd(kernelLinkedList *);

#define _KERNELLINKEDLIST_H
#endif
