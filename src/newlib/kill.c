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
//  kill.c
//

// Visopsys kill for Newlib

#include <errno.h>
#include <sys/api.h>
#include <sys/signal.h>

int kill(int pid, int sig)
{
  // For the moment we ignore the signal thing, since we really have no
  // signal implementation, except that a SIGKILL will set the force flag
  // to true (kill forcefully)

  int force = 0;

  if (sig == SIGKILL)
    force = 1;

  errno = multitaskerKillProcess(pid, force);

  if (errno)
    return (-1);
  else
    return (0);
}
