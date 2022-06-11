//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  date.c
//

// This is the UNIX-style command for viewing the current time/date

/* This is the text that appears when a user requests help about this program
<help>

 -- date --

Print the date and time.

Usage:
  date

</help>
*/

#include <stdio.h>
#include <time.h>
#include <sys/api.h>

int main(int argc, char *argv[])
{
  int status = 0;
  struct tm theTime;
  
  // We don't use argc.  This keeps the compiler happy
  argc = 0;

  // Get the current date and time structure
  status = rtcDateTime(&theTime);
  if (status < 0)
    {
      perror(argv[0]);
      return (status);
    }

  // Turn it into an ascii string and print it
  printf("%s\n", asctime(&theTime));

  // Done
  return (0);
}
