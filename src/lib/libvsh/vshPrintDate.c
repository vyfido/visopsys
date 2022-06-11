// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  vshPrintDate.c
//

// This contains some useful functions written for the shell

#include <stdio.h>
#include <sys/vsh.h>


_X_ void vshPrintDate(char *buffer, unsigned unformattedDate)
{
  // Desc: Print the packed date value, specified by the unsigned integer 'unformattedDate' -- such as that found in the file.modifiedDate field -- into 'buffer' in a (for now, arbitrary) human-readable format.

  int day = 0;
  int month = 0;
  int year = 0;

  const char *monthString = NULL;
  day = (unformattedDate & 0x0000001F);
  month = ((unformattedDate & 0x000001E0) >> 5);
  year = ((unformattedDate & 0xFFFFFE00) >> 9);

  switch(month)
    {
    case 1:
      monthString = "Jan";
      break;
    case 2:
      monthString = "Feb";
      break;
    case 3:
      monthString = "Mar";
      break;
    case 4:
      monthString = "Apr";
      break;
    case 5:
      monthString = "May";
      break;
    case 6:
      monthString = "Jun";
      break;
    case 7:
      monthString = "Jul";
      break;
    case 8:
      monthString = "Aug";
      break;
    case 9:
      monthString = "Sep";
      break;
    case 10:
      monthString = "Oct";
      break;
    case 11:
      monthString = "Nov";
      break;
    case 12:
      monthString = "Dec";
      break;
    default:
      monthString = "???";
      break;
    }

  sprintf(buffer, "%s %02u %u", monthString, day, year);

  return;
}
