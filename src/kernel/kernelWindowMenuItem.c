//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMisc.h"
#include "kernelError.h"
#include <string.h>

static kernelAsciiFont *menuItemFont = NULL;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewMenuItem(kernelWindowComponent
					       *menuComponent,
					       const char *text,
					       componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowMenuItem

  kernelWindowComponent *component = NULL;
  kernelWindowMenu *menu = NULL;
  listItemParameters itemParams;
  extern color kernelDefaultBackground;

  // Check params
  if ((menuComponent == NULL) || (text == NULL) || (params == NULL))
    {
      kernelError(kernel_error, "NULL parameter for menu item");
      return (component = NULL);
    }
  if (menuComponent->type != menuComponentType)
    {
      kernelError(kernel_error, "Can only add a menu item to a menu");
      return (component = NULL);
    }

  if (menuItemFont == NULL)
    {
      // Try to load a nice-looking font
      if (kernelFontLoad(WINDOW_DEFAULT_VARFONT_SMALL_FILE,
			 WINDOW_DEFAULT_VARFONT_SMALL_NAME,
			 &menuItemFont, 0) < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&menuItemFont);
    }

  if (params->font == NULL)
    {
      componentParameters tmpParams;
      kernelMemCopy(params, &tmpParams, sizeof(componentParameters));
      params = &tmpParams;
      params->font = menuItemFont;
    }

  menu = menuComponent->data;

  // Get the superclass list item component
  kernelMemClear(&itemParams, sizeof(listItemParameters));
  strncpy(itemParams.text, text, WINDOW_MAX_LABEL_LENGTH);
  component = kernelWindowNewListItem(menu->container, windowlist_textonly,
				      &itemParams, params);
  if (component == NULL)
    return (component);

  if (!(component->parameters.flags & WINDOW_COMPFLAG_CUSTOMBACKGROUND))
    // We use a different default background color than the window list
    // item component that the menu item is based upon
    kernelMemCopy(&(kernelDefaultBackground), (color *)
		  &(component->parameters.background), sizeof(color));

  return (component);
}
