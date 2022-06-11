//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelPipe.h
//

// This file describes the kernel's facilities for reading and writing
// inter-process communication pipes using a 'streams' abstraction.

#ifndef _KERNELPIPE_H
#define _KERNELPIPE_H

#include <sys/stream.h>

typedef struct {
	unsigned itemSize;
	int creatorPid;
	int readerPid;
	int writerPid;
	streamItemSize streamSize;
	stream s;

} kernelPipe;

// Functions exported by kernelPipe.c
kernelPipe *kernelPipeNew(unsigned, unsigned);
int kernelPipeDestroy(kernelPipe *);
int kernelPipeSetReader(kernelPipe *, int);
int kernelPipeSetWriter(kernelPipe *, int);
int kernelPipeClear(kernelPipe *);
int kernelPipeRead(kernelPipe *, unsigned, void *);
int kernelPipeWrite(kernelPipe *, unsigned, void *);

#endif

