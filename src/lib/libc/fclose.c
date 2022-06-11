// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  fclose.c
//

// This is the standard "fclose" function, as found in standard C libraries

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>


int fclose(FILE *theStream)
{
  // Given a file stream pointer, close the file stream.

  int status = 0;

  if (visopsys_in_kernel)
    return (errno = ERR_BUG);

  // Check params
  if (theStream == NULL)
    return (errno = ERR_NULLPARAMETER);

  status = fileStreamClose(theStream);
  free(theStream);

  return (status);
}
