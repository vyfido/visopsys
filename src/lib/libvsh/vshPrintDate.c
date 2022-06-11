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
//  vshPrintDate.c
//

// This contains some useful functions written for the shell

#include <stdio.h>
#include <sys/vsh.h>


_X_ void vshPrintDate(unsigned unformattedDate)
{
  // Desc: Print the packed date value, specified by the unsigned integer 'unformattedDate' -- such as that found in the file.modifiedDate field -- in a (for now, arbitrary) human-readable format.

  int day = 0;
  int month = 0;
  int year = 0;

  day = (unformattedDate & 0x0000001F);
  month = ((unformattedDate & 0x000001E0) >> 5);
  year = ((unformattedDate & 0xFFFFFE00) >> 9);

  switch(month)
    {
    case 1:
      printf("Jan");
      break;
    case 2:
      printf("Feb");
      break;
    case 3:
      printf("Mar");
      break;
    case 4:
      printf("Apr");
      break;
    case 5:
      printf("May");
      break;
    case 6:
      printf("Jun");
      break;
    case 7:
      printf("Jul");
      break;
    case 8:
      printf("Aug");
      break;
    case 9:
      printf("Sep");
      break;
    case 10:
      printf("Oct");
      break;
    case 11:
      printf("Nov");
      break;
    case 12:
      printf("Dec");
      break;
    default:
      printf("???");
      break;
    }

  putchar(' ');

  if (day < 10)
    putchar('0');
  printf("%u %u", day, year);

  return;
}
