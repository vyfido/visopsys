// 
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  vshCursorMenu.c
//

// This contains some useful functions written for the shell

//#include <string.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/api.h>


_X_ int vshCursorMenu(const char *prompt, int numItems, char *items[],
		      int defaultSelection)
{
  // Desc: This will create a pretty cursor-changeable text menu with the supplied 'prompt' string at the stop.  Returns the integer (zero-based) selected item number, or else negative on error or no selection.

  int foregroundColor = textGetForeground();
  int backgroundColor = textGetBackground();
  int itemWidth = 0;
  int selectedOption = defaultSelection;
  char character;
  int count1, count2;

  // Get the width of the widest item and set our item width
  for (count1 = 0; count1 < numItems; count1 ++)
    if (strlen(items[count1]) > itemWidth)
      itemWidth = strlen(items[count1]);

  // Print prompt message
  printf("\n%s\n", prompt);

  int column = textGetColumn();
  int row = textGetRow();

  while(1)
    {
      textSetColumn(column);
      textSetRow(row);
      textSetCursor(0);
      
      for (count1 = 0; count1 < numItems; count1 ++)
	{
	  printf(" ");

	  if (selectedOption == count1)
	    {
	      // Reverse the colors
	      textSetForeground(backgroundColor);
	      textSetBackground(foregroundColor);
	    }
	  
	    printf(" %s ", items[count1]);
	    for (count2 = 0; count2 < (itemWidth - strlen(items[count1]));
		 count2 ++)
	      printf(" ");
	    printf("\n");
	  
	    if (selectedOption == count1)
	      {
		// Restore the colors
		textSetForeground(foregroundColor);
		textSetBackground(backgroundColor);
	      }
	}

      printf("\n  [Cursor up/down to change, Enter to select, 'Q' to quit]\n");
      
      textInputSetEcho(0);
      character = getchar();
      
      if (character == (unsigned char) 17)
	{
	  if (selectedOption > 0)
	    // Cursor up.
	    selectedOption -= 1;
	}

      else if (character == (unsigned char) 20)
	{
	  // Cursor down.
	  if (selectedOption < (numItems - 1))
	    selectedOption += 1;
	}

      else if ((character == (unsigned char) 10) ||
	       (character == 'Q') || (character == 'q'))
	{
	  // Enter
	  textSetCursor(1);
	  textInputSetEcho(1);
	  printf("\n");
	  if (character == (unsigned char) 10)
	    return (selectedOption);
	  else
	    return (ERR_CANCELLED);
	}
    }  
}
