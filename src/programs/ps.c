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
//  ps.c
//

// This is the UNIX-style command for viewing a list of running processes

#include <sys/api.h>


int main(int argc, char *argv[])
{
  // This command will prompt the multitasker to dump a list of all active
  // processes.  This is a temporary hack until the system has an api for
  // allowing a user process to access the data by itself.  In other words,
  // the kernel should not print this stuff on the screen by itself.
  
  // This is easy
  multitaskerDumpProcessList();

  // Return success
  return (0);
}
