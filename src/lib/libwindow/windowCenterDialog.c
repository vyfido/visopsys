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
//  windowCenterDialog.c
//

// This contains functions for user programs to operate GUI components.

#include <sys/api.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ void windowCenterDialog(objectKey parentWindow, objectKey dialogWindow)
{
  // Desc: Center a dialog window.  The first object key is the parent window, and the second is the dialog window.  This function can be used to center a regular window on the screen if the first objectKey argument is NULL.
  
  int parentX = 0, parentY = 0;
  unsigned myWidth = 0, myHeight = 0, parentWidth = 0, parentHeight = 0;
  int diffWidth, diffHeight;

  if (parentWindow)
    {
      // Get the size and location of the parent window
      windowGetLocation(parentWindow, &parentX, &parentY);
      windowGetSize(parentWindow, &parentWidth, &parentHeight);
    }
  else
    {
      parentWidth = graphicGetScreenWidth();
      parentHeight = graphicGetScreenHeight();
    }

  // Get our size
  windowGetSize(dialogWindow, &myWidth, &myHeight);

  diffWidth = (parentWidth - myWidth);
  diffHeight = (parentHeight - myHeight);

  // Set our location
  windowSetLocation(dialogWindow, (parentX + (diffWidth / 2)),
		    (parentY + (diffHeight / 2)));
}
