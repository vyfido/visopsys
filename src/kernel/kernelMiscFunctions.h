//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  kernelMiscFunctions.h
//
	
#if !defined(_KERNELMISCFUNCTIONS_H)

#include "kernelFileStream.h"
#include "kernelVariableList.h"
#include <time.h>

#define MAX_SYMBOL_LENGTH 80

typedef struct {
  unsigned address;
  char symbol[MAX_SYMBOL_LENGTH];

} kernelSymbol;

typedef struct {
  union {
    struct {
      unsigned timeLow;
      unsigned short timeMid;
      unsigned short timeHighVers;
      unsigned char clockSeqRes;
      unsigned char clockSeqLow;
      unsigned char node[6];
    } fields;
    unsigned char bytes[16];
  };
} kernelGuid;

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

const char *kernelVersion(void);
void kernelMemCopy(const void *, void *, unsigned);
void kernelMemClear(void *, unsigned);
int kernelMemCmp(const void *, const void *, unsigned);
void kernelStackTrace(void *, void *);
void kernelConsoleLogin(void);
int kernelConfigurationReader(const char *, variableList *);
int kernelConfigurationWriter(const char *, variableList *);
int kernelReadSymbols(const char *);
time_t kernelUnixTime(void);
int kernelGuidGenerate(kernelGuid *);

#define _KERNELMISCFUNCTIONS_H
#endif
