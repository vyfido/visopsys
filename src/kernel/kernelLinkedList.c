//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  kernelLinkedList.c
//

// This file contains the C functions belonging to the kernel's implementation
// of linked lists.

#include "kernelLinkedList.h"
#include "kernelError.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////



int kernelLinkedListAdd(kernelLinkedList *list, void *data)
{
  // Add the specified data value to the linked list.

  int status = 0;
  kernelLinkedListItem *new = NULL;

  if ((list == NULL) || (data == NULL))
    return (status = ERR_NULLPARAMETER);

  new = kernelMalloc(sizeof(kernelLinkedListItem));
  if (new == NULL)
    return (status = ERR_MEMORY);

  new->data = data;

  status = kernelLockGet(&list->lock);
  if (status < 0)
    {
      kernelFree(new);
      return (status);
    }

  if (list->first)
    list->first->prev = new;

  new->next = list->first;
  list->first = new;

  list->numItems += 1;

  kernelLockRelease(&list->lock);
  return (status = 0);
}


int kernelLinkedListRemove(kernelLinkedList *list, void *data)
{
  // Remove the linked list item with the specified data value

  int status = 0;
  kernelLinkedListItem *iter = NULL;
  
  if ((list == NULL) || (data == NULL))
    return (status = ERR_NULLPARAMETER);

  status = kernelLockGet(&list->lock);
  if (status < 0)
    return (status);

  iter = list->first;

  while (iter)
    {
      if (iter->data == data)
	{
	  if (iter->prev)
	    {
	      // If the item we're removing is the current one in an active
	      // iteration, point the iteration at the previous item now.
	      if (iter == list->iter)
		list->iter = iter->prev;

	      iter->prev->next = iter->next;
	    }

	  if (iter->next)
	    iter->next->prev = iter->prev;

	  if (iter == list->first)
	    list->first = iter->next;

	  list->numItems -= 1;

	  kernelFree(iter);
	  kernelLockRelease(&list->lock);
	  return (status = 0);
	}
      
      iter = iter->next;
    }

  kernelLockRelease(&list->lock);
  return (status = ERR_NOSUCHENTRY);
}


int kernelLinkedListClear(kernelLinkedList *list)
{
  // Remove everything in the linked list.

  int status = 0;
  kernelLinkedListItem *iter = NULL;
  kernelLinkedListItem *next = NULL;

  if (list == NULL)
    return (status = ERR_NULLPARAMETER);

  status = kernelLockGet(&list->lock);
  if (status < 0)
    return (status);

  iter = list->first;

  while (iter)
    {
      next = iter->next;
      kernelFree(iter);
      iter = next;
    }

  list->numItems = 0;

  kernelLockRelease(&list->lock);
  kernelMemClear(list, sizeof(kernelLinkedList));
  return (status = 0);
}


void *kernelLinkedListIterStart(kernelLinkedList *list)
{
  // Starts an iteration through the linked list.  Locks the list and doesn't
  // release the lock until kernelLinkedListIterEnd() is called, since the
  // state of the iteration is held in the list.  Returns the data value from
  // the first item, if applicable.

  if (list == NULL)
    return (NULL);

  if (list->iter)
    {
      kernelError(kernel_error, "List iteration is already active");
      return (NULL);
    }
  
  list->iter = list->first;

  if (!list->iter)
    return (NULL);

  return (list->iter->data);
}


void *kernelLinkedListIterNext(kernelLinkedList *list)
{
  // Returns the data value from the next item in the list, if applicable.

  if (list == NULL)
    return (NULL);
  
  if (!list->iter)
    return (NULL);

  list->iter = list->iter->next;

  if (!list->iter)
    return (NULL);

  return (list->iter->data);
}


int kernelLinkedListIterEnd(kernelLinkedList *list)
{
  // Terminate an interation through the list.  Unlock the list and clear
  // the iteration state.

  list->iter = NULL;
  return (0);
}
