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
//  uptime.c
//

// This is the UNIX-style command for viewing info about system uptime

#include <stdio.h>
#include <sys/api.h>


int main(int argc, char *argv[])
{
  int i = 0;

  // Get the number of "up" seconds
  i = rtcUptimeSeconds();

  printf("Up %d days, ", (i / (60 * 60 * 24)));

  i %= (60 * 60 * 24);
  if ((i / (60 * 60)) < 10)
    printf("0");
  printf("%d:", (i / (60 * 60)));

  i %= (60 * 60);
  if ((i / 60) < 10)
    printf("0");
  printf("%d:", (i / 60));

  i %= 60;
  if (i < 10)
    printf("0");
  printf("%d\n", i);

  // Done
  return (0);
}
