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
//  shutdown.c
//

// This is the UNIX-style command for shutting down the system

#include <stdio.h>
#include <string.h>
#include <sys/api.h>

typedef enum 
{  
  halt, reboot

} shutdownType;


int main(int argc, char *argv[])
{
  // There's a nice system function for doing this.

  int status = 0;
  int count;

  // Make sure none of our args are NULL
  for (count = 0; count < argc; count ++)
    if (argv[count] == NULL)
      return (status = ERR_NULLPARAMETER);

  if ((argc > 1) && (strcmp(argv[1], "-f") == 0))
    // Do a nasty shutdown
    status = shutdown(halt, 1);

  else
    {
      // Try to do a nice shutdown
      status = shutdown(halt, 0);
      
      if (status < 0)
	printf("Use \"%s -f\" to force.", argv[0]);
    }

  return (status);
}
