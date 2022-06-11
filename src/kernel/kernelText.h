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
//  kernelText.h
//

#if !defined(_KERNELTEXT_H)

#include "kernelStream.h"
#include "kernelFontFunctions.h"
#include "kernelGraphicFunctions.h"

// Definitions
#define TEXTSTREAMSIZE 1024
#define DEFAULT_TAB 8

// A data structure to represent a text area on the screen which gets drawn
// by the appropriate driver functions
typedef volatile struct
{
  int xCoord;
  int yCoord;
  int columns;
  int rows;
  int cursorColumn;
  int cursorRow;
  color foreground;
  color background;
  void *inputStream;
  void *outputStream;
  unsigned char *data;
  kernelAsciiFont *font;
  kernelGraphicBuffer *graphicBuffer;
  int lock;

} kernelTextArea;

// This structure contains pointers to all the appropriate functions
// to output text from a given text stream
typedef struct
{
  int (*driverInitialize) (kernelTextArea *);
  int (*getCursorAddress) (kernelTextArea *);
  int (*setCursorAddress) (kernelTextArea *, int, int);
  int (*setForeground) (kernelTextArea *, int);
  int (*setBackground) (kernelTextArea *, int);
  int (*print) (kernelTextArea *, const char *);
  int (*clearScreen) (kernelTextArea *);

} kernelTextOutputDriver;

// A text input stream.  In single user operation there is only one, and it's
// where all keyboard input goes.
typedef volatile struct 
{
  int ownerPid;
  stream *s;
  int echo;

} kernelTextInputStream;

// This structure is used to refer to a stream made up of text.
typedef volatile struct 
{
  int ownerPid;
  kernelTextOutputDriver *outputDriver;
  kernelTextArea *textArea;
  int foreground;
  int background;

} kernelTextOutputStream;

// Functions from kernelText.c

int kernelTextInitialize(int, int);
int kernelTextSwitchToGraphics(kernelTextArea *);
kernelTextInputStream *kernelTextGetConsoleInput(void);
kernelTextOutputStream *kernelTextGetConsoleOutput(void);
int kernelTextSetConsoleInput(kernelTextInputStream *);
int kernelTextSetConsoleOutput(kernelTextOutputStream *);
int kernelTextNewInputStream(kernelTextInputStream *);
int kernelTextNewOutputStream(kernelTextOutputStream *);
int kernelTextGetForeground(void);
int kernelTextSetForeground(int);
int kernelTextGetBackground(void);
int kernelTextSetBackground(int);
int kernelTextStreamPutc(kernelTextOutputStream *, int);
int kernelTextPutc(int);
int kernelTextStreamPrint(kernelTextOutputStream *, const char *);
int kernelTextPrint(const char *, ...);
int kernelTextStreamPrintLine(kernelTextOutputStream *, const char *);
int kernelTextPrintLine(const char *, ...);
void kernelTextStreamNewline(kernelTextOutputStream *);
void kernelTextNewline(void);
void kernelTextStreamBackSpace(kernelTextOutputStream *);
void kernelTextBackSpace(void);
void kernelTextStreamTab(kernelTextOutputStream *);
void kernelTextTab(void);
void kernelTextStreamCursorUp(kernelTextOutputStream *);
void kernelTextCursorUp(void);
void kernelTextStreamCursorDown(kernelTextOutputStream *);
void kernelTextCursorDown(void);
void kernelTextStreamCursorLeft(kernelTextOutputStream *);
void kernelTextCursorLeft(void);
void kernelTextStreamCursorRight(kernelTextOutputStream *);
void kernelTextCursorRight(void);
int kernelTextStreamGetNumColumns(kernelTextOutputStream *);
int kernelTextGetNumColumns(void);
int kernelTextStreamGetNumRows(kernelTextOutputStream *);
int kernelTextGetNumRows(void);
int kernelTextStreamGetColumn(kernelTextOutputStream *);
int kernelTextGetColumn(void);
void kernelTextStreamSetColumn(kernelTextOutputStream *, int);
void kernelTextSetColumn(int);
int kernelTextStreamGetRow(kernelTextOutputStream *);
int kernelTextGetRow(void);
void kernelTextStreamSetRow(kernelTextOutputStream *,int);
void kernelTextSetRow(int);
void kernelTextStreamClearScreen(kernelTextOutputStream *);
void kernelTextClearScreen(void);

int kernelTextInputStreamCount(kernelTextInputStream *);
int kernelTextInputCount(void);
int kernelTextInputStreamGetc(kernelTextInputStream *, char *);
int kernelTextInputGetc(char *);
int kernelTextInputStreamPeek(kernelTextInputStream *, char *);
int kernelTextInputPeek(char *);
int kernelTextInputStreamReadN(kernelTextInputStream *, int, char *);
int kernelTextInputReadN(int, char *);
int kernelTextInputStreamReadAll(kernelTextInputStream *, char *);
int kernelTextInputReadAll(char *);
int kernelTextInputStreamAppend(kernelTextInputStream *, int);
int kernelTextInputAppend(int);
int kernelTextInputStreamAppendN(kernelTextInputStream *, int, char *);
int kernelTextInputAppendN(int, char *);
int kernelTextInputStreamRemove(kernelTextInputStream *);
int kernelTextInputRemove(void);
int kernelTextInputStreamRemoveN(kernelTextInputStream *, int);
int kernelTextInputRemoveN(int);
int kernelTextInputStreamRemoveAll(kernelTextInputStream *);
int kernelTextInputRemoveAll(void);
void kernelTextInputStreamSetEcho(kernelTextInputStream *, int);
void kernelTextInputSetEcho(int);

#define _KERNELTEXT_H
#endif
