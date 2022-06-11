//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
//  kernelText.h
//

#if !defined(_KERNELTEXT_H)

#include "kernelStream.h"

// Definitions
#define TEXTSTREAMSIZE 1024


// This structure contains pointers to all the appropriate functions
// to output text from a given text stream
typedef struct
{
  void (*initialize) (void *, int, int, int, int);
  int (*getCursorAddress) (void);
  void (*setCursorAddress) (int);
  void (*setForeground) (int);
  void (*setBackground) (int);
  void (*print) (const char *);
  void (*clearScreen) (void);

} kernelTextOutputDriver;


// This structure is used to refer to a stream made up of text.
typedef volatile struct 
{
  stream *s;
  streamFunctions *sFn;

  // Only applicable when doing output from a stream
  kernelTextOutputDriver *outputDriver;
  int foregroundColour;
  int backgroundColour;

} kernelTextStream;


#define DEFAULT_TAB 8

// Functions from kernelText.c

int kernelTextInitialize(int, int);
kernelTextStream *kernelTextStreamGetConsoleInput(void);
kernelTextStream *kernelTextStreamGetConsoleOutput(void);
int kernelTextStreamGetMode(void);
int kernelTextStreamSetMode(int);
int kernelTextStreamGetColumns(void);
int kernelTextStreamGetRows(void);
int kernelTextStreamGetForeground(void);
int kernelTextStreamSetForeground(int);
int kernelTextStreamGetBackground(void);
int kernelTextStreamSetBackground(int);
int kernelTextPutc(int);
int kernelTextPrint(const char *, ...);
int kernelTextPrintLine(const char *, ...);
void kernelTextNewline(void);
void kernelTextBackSpace(void);
void kernelTextTab(void);
void kernelTextCursorUp(void);
void kernelTextCursorDown(void);
void kernelTextCursorLeft(void);
void kernelTextCursorRight(void);
int kernelTextGetNumColumns(void);
int kernelTextGetNumRows(void);
int kernelTextGetColumn(void);
void kernelTextSetColumn(int);
int kernelTextGetRow(void);
void kernelTextSetRow(int);
void kernelTextClearScreen(void);

int kernelTextInputCount(void);
int kernelTextInputGetc(char *);
int kernelTextInputPeek(char *);
int kernelTextInputReadN(int, char *);
int kernelTextInputReadAll(char *);
int kernelTextInputAppend(int);
int kernelTextInputAppendN(int, char *);
int kernelTextInputRemove(void);
int kernelTextInputRemoveN(int);
int kernelTextInputRemoveAll(void);

#define _KERNELTEXT_H
#endif
