//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  abspath.c
//

// This is code that can be shared between multiple libraries.  It won't
// compile on its own; it needs to be #included inside a function
// definition.


// Prototype:
//   void abspath(const char *orig, char *new)
{
  char cwd[MAX_PATH_LENGTH];

  // Get the current directory
  multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);

  if ((orig[0] != '/') && (orig[0] != '\\'))
    {
      strcpy(new, cwd);

      if ((new[strlen(new) - 1] != '/') &&
          (new[strlen(new) - 1] != '\\'))
        strncat(new, "/", 1);
      
      strcat(new, orig);
    } 
  else
    strcpy(new, orig);

  return;
}
