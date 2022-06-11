//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  kernelWindowMenuItem.c
//

// This code is for managing kernelWindowMenuItem objects.  These are
// selectable items that occur inside of kernelWindowMenu components.

#include "kernelWindowManager.h"     // Our prototypes are here
#include <string.h>

static kernelAsciiFont *menuItemFont = NULL;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewMenuItem(volatile void *parent,
					       const char *text,
					       componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowMenuItem

  kernelWindowComponent *component = NULL;
  extern color kernelDefaultBackground;
 
  if (menuItemFont == NULL)
    {
      // Try to load a nice-looking font
      if (kernelFontLoad(DEFAULT_VARIABLEFONT_SMALL_FILE,
			 DEFAULT_VARIABLEFONT_SMALL_NAME, &menuItemFont) < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&menuItemFont);
    }

  // Get the superclass list item component
  component = kernelWindowNewListItem(parent, menuItemFont, text, params);
  if (component == NULL)
    return (component);

  if (component->parameters.useDefaultBackground)
    {
      // We use a different default background color than the window
      // list item component that the menu item is based upon
      component->parameters.background.red = kernelDefaultBackground.red;
      component->parameters.background.green = kernelDefaultBackground.green;
      component->parameters.background.blue = kernelDefaultBackground.blue;
    }

  return (component);
}
