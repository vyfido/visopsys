// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  vshMakeAbsolutePath.c
//

// This contains some useful functions written for the shell

#include <string.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/api.h>


_X_ void vshMakeAbsolutePath(const char *orig, char *new)
{
  // Desc: Turns a filename, specified by 'orig', into an absolute pathname 'new'.  This basically just amounts to prepending the name of the current directory (plus a '/') to the supplied name.  'new' must be a buffer large enough to hold the entire filename.

  // Use shared code
  #include "../shared/abspath.c"
}
