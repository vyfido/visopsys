//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  kernelWindowTextFieldComponent.c
//

// This code is for managing kernelWindowTextFieldComponent objects.
// These are just kernelWindowTextAreaComponents that consist of a single
// line


#include "kernelWindowManager.h"     // Our prototypes are here
#include <string.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewTextFieldComponent(kernelWindow *window,
					 int columns, kernelAsciiFont *font)
{
  // Formats a kernelWindowComponent as a kernelWindowTextFieldComponent.
  // Really it just returns a kernelWindowTextAreaComponent with only
  // one row.

  kernelWindowComponent *textAreaComponent = NULL;

  textAreaComponent =
    kernelWindowNewTextAreaComponent(window, columns, 1, font);

  // Except change the type to a text field
  textAreaComponent->type = windowTextFieldComponent;

  return (textAreaComponent);
}
