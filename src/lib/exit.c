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
//  exit.c
//

// This is the standard "exit" function, as found in standard C libraries

#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>


void exit(int status)
{
  // We don't set errno in this function
  errno = 0;

  // I'm sure there will be lots of shutdown things to do here in the
  // future

  // Shut down
  multitaskerTerminate(status);

  // Now, there's nothing else we can do except wait to be killed
  while(1);
}
