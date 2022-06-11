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
//  errno.h
//

// This is the Visopsys version of the standard header file errno.h

#if !defined(_ERRNO_H)

// This is where we get all the error numbers that the Visopsys kernel
// understands
#include <sys/errors.h>

// The global error code variable
extern int errno;

// This is where we translate the Visopsys kernel's error codes into
// names that are more familiar to unixy people.  If there is no Visopsys
// equivalent, we assign ERR_ERROR (which is just an error code with no
// additional information

#define EPERM   ERR_PERMISSION     // Not super-user
#define ENOENT  ERR_NOSUCHFILE     // No such file or directory
#define ESRCH   ERR_NOSUCHPROCESS  // No such process
#define EINTR   ERR_ERROR          // interrupted system call
#define EIO     ERR_IO             // I/O error
#define ENXIO   ERR_NOSUCHDRIVER   // No such device or address
#define E2BIG   ERR_ARGUMENTCOUNT  // Arg list too long
#define ENOEXEC ERR_EXECUTE        // Exec format error
#define EBADF   ERR_NOSUCHFILE     // Bad file number
#define ECHILD  ERR_NOSUCHPROCESS  // No children
#define EAGAIN  ERR_NOFREE         // Resource temporarily unavailable
#define ENOMEM  ERR_MEMORY         // Not enough memory
#define EACCES  ERR_PERMISSION     // Permission denied
#define EFAULT  ERR_NULLPARAMETER  // NULL address
#define ENOTBLK ERR_NOSUCHDRIVER   // Block device required
#define EBUSY   ERR_BUSY           // Resource busy
#define EEXIST  ERR_ALREADY        // File already exists
#define EXDEV   ERR_ERROR          // Cross-device link
#define ENODEV  ERR_NOSUCHDRIVER   // No such device
#define ENOTDIR ERR_NOTADIR        // Not a directory
#define EISDIR  ERR_NOTAFILE       // Is a directory
#define EINVAL  ERR_INVALID        // Invalid argument
#define ENFILE  ERR_ERROR          // File table overflow
#define EMFILE  ERR_ERROR          // Too many open files
#define ENOTTY  ERR_ERROR          // Inappropriate ioctl for device
#define ETXTBSY ERR_BUSY           // Text file busy
#define EFBIG   ERR_NOFREE         // File too large
#define ENOSPC  ERR_NOFREE         // No space left on device
#define ESPIPE  ERR_ERROR          // Illegal seek
#define EROFS   ERR_ERROR          // Read only file system
#define EMLINK  ERR_ERROR          // Too many links
#define EPIPE   ERR_ERROR          // Broken pipe
#define EDOM    ERR_DOMAIN         // Math arg out of domain of function
#define ERANGE  ERR_RANGE          // Math result not representable
#define ENOMSG  ERR_ERROR          // No message of desired type
#define EIDRM   ERR_ERROR          // Identifier removed
#define ECHRNG  ERR_ERROR          // Channel number out of range
#define EL2NSYNC ERR_ERROR         // Level 2 not synchronized
#define EL3HLT  ERR_ERROR          // Level 3 halted
#define EL3RST  ERR_ERROR          // Level 3 reset
#define ELNRNG  ERR_ERROR          // Link number out of range
#define EUNATCH ERR_ERROR          // Protocol driver not attached
#define ENOCSI  ERR_ERROR          // No CSI structure available
#define EL2HLT  ERR_ERROR          // Level 2 halted
#define EDEADLK ERR_DEADLOCK       // Deadlock condition.
#define ENOLCK  ERR_NOLOCK         // No record locks available.
#define ECANCELED ERR_ERROR        // Operation canceled
#define ENOTSUP ERR_NOTIMPLEMENTED // Operation not supported

#define _ERRNO_H
#endif
