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
//  kernelWindowPasswordField.c
//

// This code is for managing kernelWindowPasswordField components.
// These are just kernelWindowTextFields that echo '*' instead of the
// typed text.

#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
#include <string.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewPasswordField(volatile void *parent,
				    int columns, kernelAsciiFont *font,
				    componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowPasswordField.
  // Really it just returns a modified kernelWindowTextField that only
  // echoes asterisks.

  kernelWindowComponent *component = NULL;

  component = kernelWindowNewTextField(parent, columns, font, params);
  if (component == NULL)
    return (component = NULL);

  ((kernelWindowTextField *) component->data)->hidden = 1;

  return (component);
}