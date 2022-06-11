// 
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  vsh.h
//

// These are functions written for vsh which can be used by other programs

#if !defined(_VSH_H)

// Functions
void vshPrintTime(unsigned);
void vshPrintDate(unsigned);
int vshFileList(const char *);
int vshDumpFile(const char *);
int vshDeleteFile(const char *);
int vshCopyFile(const char *, const char *);
int vshRenameFile(const char *, const char *);
void vshMakeAbsolutePath(const char *, char *);
void vshCompleteFilename(char *);
int vshSearchPath(const char *, char *);

#define _VSH_H
#endif