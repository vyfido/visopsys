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
//  linkedList.c
//

// This file contains linked list code for general library functionality
// shared between the kernel and userspace in Visopsys

// Library debugging messages are off by default even in a debug build
#undef DEBUG

#include "libvis.h"


static int add(linkedList *list, void *data, int back)
{
	int status = 0;
	linkedListItem *new = NULL;

	if (!list || !data)
	{
		error("NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	new = memory_malloc(sizeof(linkedListItem));
	if (!new)
	{
		error("Memory allocation failure");
		return (status = ERR_MEMORY);
	}

	new->data = data;

	status = lock_get(&list->lock);
	if (status < 0)
	{
		error("Couldn't get lock");
		memory_free(new);
		return (status);
	}

	if (back)
	{
		if (list->last)
			list->last->next = new;

		new->prev = list->last;

		if (!list->first)
			list->first = new;

		list->last = new;
	}
	else
	{
		if (list->first)
			list->first->prev = new;

		new->next = list->first;

		if (!list->last)
			list->last = new;

		list->first = new;
	}

	list->numItems += 1;

	lock_release(&list->lock);

	return (status = 0);
}


static inline int inList(linkedList *list, linkedListItem *item)
{
	// Returns 1 if the item is in the list and the data matches, else 0

	linkedListItem *iter = list->first;

	while (iter)
	{
		if ((iter == item) && (iter->data == item->data))
			return (1);

		iter = iter->next;
	}

	return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

_X_ int linkedListAddFront(linkedList *list, void *data)
{
	// Desc : Add the specified value 'data' to the front of linked list 'list'.

	// This will check parameters
	return (add(list, data, 0 /* front */));
}


_X_ int linkedListAddBack(linkedList *list, void *data)
{
	// Desc : Add the specified value 'data' to the back of linked list 'list'.

	// This will check parameters
	return (add(list, data, 1 /* back */));
}


_X_ int linkedListRemove(linkedList *list, void *data)
{
	// Desc : Remove the item with the specified value 'data' from the linked list 'list'.

	int status = 0;
	linkedListItem *iter = NULL;

	if (!list || !data)
	{
		error("NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	status = lock_get(&list->lock);
	if (status < 0)
	{
		error("Couldn't get lock");
		return (status);
	}

	iter = list->first;

	while (iter)
	{
		if (iter->data == data)
		{
			if (iter->prev)
				iter->prev->next = iter->next;

			if (iter->next)
				iter->next->prev = iter->prev;

			if (iter == list->first)
				list->first = iter->next;

			if (iter == list->last)
				list->last = iter->prev;

			list->numItems -= 1;

			memory_free(iter);
			lock_release(&list->lock);

			return (status = 0);
		}

		iter = iter->next;
	}

	lock_release(&list->lock);

	error("Item not found");
	return (status = ERR_NOSUCHENTRY);
}


_X_ int linkedListClear(linkedList *list)
{
	// Desc : Remove everything from the linked list 'list'.

	int status = 0;
	linkedListItem *iter = NULL;
	linkedListItem *next = NULL;

	if (!list)
	{
		error("NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	status = lock_get(&list->lock);
	if (status < 0)
	{
		error("Couldn't get lock");
		return (status);
	}

	iter = list->first;

	while (iter)
	{
		next = iter->next;
		memory_free(iter);
		iter = next;
	}

	list->numItems = 0;

	lock_release(&list->lock);

	memset(list, 0, sizeof(linkedList));
	return (status = 0);
}


_X_ void *linkedListIterStart(linkedList *list, linkedListItem **iter)
{
	// Desc : Starts an iteration through the linked list 'list', using the supplied iterator pointer 'iter'.  Returns the data value from the first item, if applicable.

	// Check params
	if (!list || !iter)
	{
		error("NULL parameter");
		return (NULL);
	}

	*iter = list->first;

	if (!(*iter))
		return (NULL);
	else
		return ((*iter)->data);
}


_X_ void *linkedListIterNext(linkedList *list, linkedListItem **iter)
{
	// Desc : Returns the data value from the next item in the linked list 'list', if applicable.  Uses the iterator pointer 'iter' previously initialized via a call to linkedListIterStart().

	// Check params
	if (!list || !iter)
	{
		error("NULL parameter");
		return (NULL);
	}

	if (!(*iter))
	{
		error("Iterator not initialized");
		return (NULL);
	}

	// Ensure that the current item hasn't been removed from the list
	if (inList(list, *iter))
		*iter = (*iter)->next;
	else
		*iter = list->first; // restart

	if (!(*iter))
		return (NULL);
	else
		return ((*iter)->data);
}


void linkedListDebug(linkedList *list __attribute__((unused)))
{
#ifdef DEBUG
	linkedListItem *iter = NULL;
	void *data = NULL;

	data = linkedListIterStart(list, &iter);
	while (data)
	{
		debug("LIST data=%p next=%p prev=%p", data, iter->next, iter->prev);
		data = linkedListIterNext(list, &iter);
	}
#endif // DEBUG
}

