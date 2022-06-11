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
//  kernelGraphicConsoleDriver.h
//

#if !defined(_KERNELGRAPHICCONSOLEDRIVER_H)

#include "kernelText.h"

// Functions exported from kernelGraphicConsoleDriver.c
int kernelGraphicConsoleInitialize(kernelTextArea *);
int kernelGraphicConsoleGetCursorAddress(kernelTextArea *);
int kernelGraphicConsoleSetCursorAddress(kernelTextArea *, int, int);
int kernelGraphicConsoleSetForeground(kernelTextArea *, int);
int kernelGraphicConsoleSetBackground(kernelTextArea *, int);
int kernelGraphicConsolePrint(kernelTextArea *, const char *);
int kernelGraphicConsoleClearScreen(kernelTextArea *);

#define _KERNELGRAPHICCONSOLEDRIVER_H
#endif
