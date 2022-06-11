// 
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  abort.c
//

// This is the standard "abort" function, as found in standard C libraries

#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>


void abort(void)
{
  // From GNU: The  abort()  function causes abnormal program termination
  // unless the signal SIGABRT is caught and the signal handler does not
  // return.  If the abort() function causes program termination, all open
  // streams are closed and flushed.  If the SIGABRT signal is blocked or
  // ignored,  the  abort() function will still override it.

  // For now, terminate with an error code
  multitaskerTerminate(1);

  // Now, there's nothing else we can do except wait to be killed
  while(1);
}
