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
//  kernelMisc.h
//
	
#if !defined(_KERNELMISC_H)

#include "kernelFileStream.h"
#include "kernelMultitasker.h"
#include "kernelVariableList.h"
#include <time.h>
#include <sys/guid.h>
#include <sys/utsname.h>

#define MAX_SYMBOL_LENGTH 80

typedef struct {
  unsigned value;
  char name[MAX_SYMBOL_LENGTH];

} kernelSymbol;

static inline int POW(int x, int y)
{
  int ret = 0;
  int count;

  if (y == 0)
    ret = 1;
  else
    {
      ret = x;
      for (count = 1; count < y; count ++)
	ret *= x;
    }
  return (ret);
}

void kernelGetVersion(char *, int);
int kernelSystemInfo(struct utsname *);
void kernelMemCopy(const void *, void *, unsigned);
void kernelMemSet(void *, unsigned char, unsigned);
#define kernelMemClear(ptr, num) kernelMemSet(ptr, 0, num)
int kernelMemCmp(const void *, const void *, unsigned);
int kernelStackTrace(kernelProcess *, char *, int);
void kernelConsoleLogin(void);
int kernelConfigRead(const char *, variableList *);
int kernelConfigWrite(const char *, variableList *);
int kernelConfigGet(const char *, const char *, char *, unsigned);
int kernelConfigSet(const char *, const char *, const char *);
int kernelConfigUnset(const char *, const char *);
int kernelReadSymbols(const char *);
time_t kernelUnixTime(void);
int kernelGuidGenerate(guid *);
unsigned kernelCrc32(void *, unsigned, unsigned *);

#define _KERNELMISC_H
#endif
