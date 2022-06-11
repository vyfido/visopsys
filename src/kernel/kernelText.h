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
//  kernelText.h
//

#if !defined(_KERNELTEXT_H)

#include "kernelStream.h"
#include "kernelFont.h"
#include "kernelGraphic.h"

// Definitions
#define TEXTSTREAMSIZE 32768
#define DEFAULT_TAB 8
#define DEFAULT_SCROLLBACKLINES 256

// Colours for the text console
// 0  = black
// 1  = blue
// 2  = green
// 3  = cyan
// 4  = red
// 5  = magenta
// 6  = brown
// 7  = light grey
// 8  = dark grey
// 9  = light blue
// 10 = light green
// 11 = light cyan
// 12 = light red
// 13 = light magenta
// 14 = yellow
// 15 = white
#define DEFAULTFOREGROUND       7
#define DEFAULTBACKGROUND       1
#define DEFAULTERRORFOREGROUND  6

// A data structure to represent a text area on the screen which gets drawn
// by the appropriate driver functions
typedef volatile struct
{
  int xCoord;
  int yCoord;
  int columns;
  int rows;
  int bytesPerChar;
  int cursorColumn;
  int cursorRow;
  int cursorState;
  int maxBufferLines;
  int scrollBackLines;
  int scrolledBackLines;
  int hidden;
  color foreground;
  color background;
  void *inputStream;
  void *outputStream;
  unsigned char *bufferData;
  unsigned char *visibleData;
  kernelAsciiFont *font;
  kernelGraphicBuffer *graphicBuffer;
  unsigned char *savedScreen;
  unsigned savedCursorColumn;
  unsigned savedCursorRow;

} kernelTextArea;

// This structure contains pointers to all the appropriate functions
// to output text from a given text stream
typedef struct
{
  int (*driverInitialize) (void);
  void (*setCursor) (kernelTextArea *, int);
  int (*getCursorAddress) (kernelTextArea *);
  int (*setCursorAddress) (kernelTextArea *, int, int);
  int (*getForeground) (kernelTextArea *);
  int (*setForeground) (kernelTextArea *, int);
  int (*getBackground) (kernelTextArea *);
  int (*setBackground) (kernelTextArea *, int);
  int (*print) (kernelTextArea *, const char *);
  int (*delete) (kernelTextArea *);
  int (*screenDraw) (kernelTextArea *);
  int (*screenClear) (kernelTextArea *);
  int (*screenSave) (kernelTextArea *);
  int (*screenRestore) (kernelTextArea *);

} kernelTextOutputDriver;

// A text input stream.  In single user operation there is only one, and it's
// where all keyboard input goes.
typedef volatile struct 
{
  stream s;
  int ownerPid;
  int echo;

} kernelTextInputStream;

// This structure is used to refer to a stream made up of text.
typedef volatile struct 
{
  kernelTextOutputDriver *outputDriver;
  kernelTextArea *textArea;

} kernelTextOutputStream;

// The default driver initializations
int kernelTextConsoleInitialize(void);
int kernelGraphicConsoleInitialize(void);

// Functions from kernelText.c

int kernelTextInitialize(int, int);
kernelTextArea *kernelTextAreaNew(int, int, int);
void kernelTextAreaDestroy(kernelTextArea *);
int kernelTextSwitchToGraphics(kernelTextArea *);
kernelTextInputStream *kernelTextGetConsoleInput(void);
kernelTextOutputStream *kernelTextGetConsoleOutput(void);
int kernelTextSetConsoleInput(kernelTextInputStream *);
int kernelTextSetConsoleOutput(kernelTextOutputStream *);
kernelTextInputStream *kernelTextGetCurrentInput(void);
int kernelTextSetCurrentInput(kernelTextInputStream *);
kernelTextOutputStream *kernelTextGetCurrentOutput(void);
int kernelTextSetCurrentOutput(kernelTextOutputStream *);
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
void kernelTextStreamScroll(kernelTextOutputStream *, int);
void kernelTextScroll(int);
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
void kernelTextStreamSetCursor(kernelTextOutputStream *, int);
void kernelTextSetCursor(int);
void kernelTextStreamScreenClear(kernelTextOutputStream *);
void kernelTextScreenClear(void);
int kernelTextScreenSave(void);
int kernelTextScreenRestore(void);

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
