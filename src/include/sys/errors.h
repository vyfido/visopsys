// 
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  errors.h
//

// This file contains all of the standard error numbers returned by
// calls to the Visopsys kernel (and by applications programs, if so 
// desired).

#if !defined(_ERRORS_H)

// This is the generic error
#define ERR_ERROR          -1  // No additional error information

// These are the most basic, standard, catch-all error codes.  They're not
// very specific or informative.  They're similar in name to some of the UNIX
// error codes.
#define ERR_INVALID        -2  // Invalid idea, generally
#define ERR_PERMISSION     -3  // Permission denied
#define ERR_MEMORY         -4  // Memory allocation or freeing error
#define ERR_BUSY           -5  // The resource is in use
#define ERR_NOSUCHENTRY    -6  // Generic things that don't exist
#define ERR_BADADDRESS     -7  // Bad pointers

// These are a little bit more specific
#define ERR_NOTINITIALIZED -8  // The resource hasn't been initialized
#define ERR_NOTIMPLEMENTED -9  // Functionality that hasn't been implemented
#define ERR_NULLPARAMETER  -10 // NULL pointer passsed as a parameter
#define ERR_NODATA         -11 // There's no data on which to operate
#define ERR_BADDATA        -12 // The data being operated on is corrupt
#define ERR_ALIGN          -13 // Memory alignment errors
#define ERR_NOFREE         -14 // No free (whatever is being requested)
#define ERR_DEADLOCK       -15 // The action would result in a deadlock
#define ERR_PARADOX        -16 // The requested action is paradoxical
#define ERR_NOLOCK         -17 // The requested resource could not be locked
#define ERR_NOVIRTUAL      -18 // Virtual address space error
#define ERR_EXECUTE        -19 // Could not execute a command or program
#define ERR_NOTEMPTY       -20 // Attempt to remove something that has content
#define ERR_NOCREATE       -21 // Could not create an item
#define ERR_NODELETE       -22 // Could not delete an item
#define ERR_IO             -23 // Input/Output error
#define ERR_BOUNDS         -24 // Array bounds exceeded, etc
#define ERR_ARGUMENTCOUNT  -25 // Incorrect number of arguments to a function
#define ERR_ALREADY        -26 // The action has already been performed
#define ERR_DIVIDEBYZERO   -27 // You're not allowed to do this!
#define ERR_DOMAIN         -28 // Argument is out of the domain of math func
#define ERR_RANGE          -29 // Result is out of the range of the math func
#define ERR_CANCELLED      -30 // Operation was explicitly cancelled
#define ERR_KILLED         -31 // Process or operation was unexpectedly killed

// Things to do with files
#define ERR_NOSUCHFILE     -32 // No such file
#define ERR_NOSUCHDIR      -33 // No such directory
#define ERR_NOTAFILE       -34 // The item is not a regular file
#define ERR_NOTADIR        -35 // The item is not a directory
#define ERR_NOWRITE        -36 // The item cannot be written

// Other things that don't exist
#define ERR_NOSUCHUSER     -37 // The used ID is unknown
#define ERR_NOSUCHPROCESS  -38 // The process in question does not exist
#define ERR_NOSUCHDRIVER   -39 // There is no driver to perform an action
#define ERR_NOSUCHFUNCTION -40 // The requested function does not exist

// Oops, it's the kernel's fault...
#define ERR_BUG            -41 // An internal bug has been detected   

#define _ERRORS_H
#endif
