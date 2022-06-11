//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  testio.c
//
//  Program to test IO ports & related stuff! Davide Airaghi

#include <stdio.h>
#include <sys/api.h>
#include <sys/vsh.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define InPort8(port, data) \
  __asm__ __volatile__ ("inb %%dx, %%al" :  "=a" (data) : "d" (port))    


int main(int argc,char **argv)
{
  int pid = 0;
  int setClear = 0;
  int portN = 0;
  int res = 0;
  unsigned char ch;
  
  pid = multitaskerGetCurrentProcessId();

  if (argc >= 2)
    {
      portN = atoi(argv[argc - 1]);

      if (argc > 2)
	if (argv[1][0] == '1')
	  setClear = 1;
    }
  else
    {
      printf("Usage:\n");
      printf("portio <port>   : read from <port>.\n");
      printf("portio 1 <port> : Allow IO to <port> and read.\n");
      printf("portio 0 <port> : Disallow IO to <port> and read.\n");  
      return (-1);
    }

  if (argc > 2)
    {
      res = multitaskerSetIOPerm(pid, portN, setClear);	
      if (res < 0)
	{
	  printf("ERRCODE: %d\n",res);
	  return (res);
	}
      printf("%s permission on port %d\n", (setClear? "Set" : "Cleared"),
	     portN);
    }

  printf("Press a key to read port #%d ",portN);
  (void) getchar();
  printf("Reading port!\n");
  InPort8(portN, ch);

  return 0;
}
