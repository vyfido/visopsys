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
//  perror.c
//

// This "perror" function is similar in function to the one found in
// other standard C libraries

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


void perror(const char *prefix)
{
  // Prints the appropriate error message corresponding to the error
  // number that we were passed.  Saves application programs from having
  // to necessarily know what all these numbers mean.

  char *stringToPrint = NULL;


  if (prefix != NULL)
    textPrint((char *) prefix);
  else
    textPrint("(NULL)");

  textPrint(": ");

  switch (errno)
    {
    case 0:
      stringToPrint = "No error.";
      break;
    case ERR_INVALID:
      stringToPrint = "Invalid operation.";
      break;
    case ERR_PERMISSION:
      stringToPrint = "Permission denied.";
      break;
    case ERR_MEMORY:
      stringToPrint = "Memory allocation or freeing error.";
      break;
    case ERR_BUSY:
      stringToPrint = "The resource is busy.";
      break;
    case ERR_NOSUCHENTRY:
      stringToPrint = "Object does not exist.";
      break;
    case ERR_BADADDRESS:
      stringToPrint = "Invalid memory address.";
      break;
    case ERR_NOTINITIALIZED:
      stringToPrint = "Resource has not been initialized.";
      break;
    case ERR_NOTIMPLEMENTED:
      stringToPrint = "Requested functionality not implemented.";
      break;
    case ERR_NULLPARAMETER:
      stringToPrint = "Required parameter was NULL.";
      break;
    case ERR_NODATA:
      stringToPrint = "No data supplied.";
      break;
    case ERR_BADDATA:
      stringToPrint = "Corrupt data encountered.";
      break;
    case ERR_ALIGN:
      stringToPrint = "Memory alignment error.";
      break;
    case ERR_NOFREE:
      stringToPrint = "No free resources.";
      break;
    case ERR_DEADLOCK:
      stringToPrint = "Deadlock situation avoided.";
      break;
    case ERR_PARADOX:
      stringToPrint = "Requested action is paradoxical.";
      break;
    case ERR_NOLOCK:
      stringToPrint = "Resource lock could not be obtained.";
      break;
    case ERR_NOVIRTUAL:
      stringToPrint = "Virtual memory error.";
      break;
    case ERR_EXECUTE:
      stringToPrint = "Command could not be executed.";
      break;
    case ERR_NOTEMPTY:
      stringToPrint = "Object is not empty.";
      break;
    case ERR_NOCREATE:
      stringToPrint = "Cannot create.";
      break;
    case ERR_NODELETE:
      stringToPrint = "Cannot delete.";
      break;
    case ERR_IO:
      stringToPrint = "Device input/output error.";
      break;
    case ERR_BOUNDS:
      stringToPrint = "Out of bounds error.";
      break;
    case ERR_ARGUMENTCOUNT:
      stringToPrint = "Incorrect number of parameters.";
      break;
    case ERR_ALREADY:
      stringToPrint = "Requested action is unnecessary.";
      break;
    case ERR_DIVIDEBYZERO:
      stringToPrint = "Divide by zero error.";
      break;
    case ERR_DOMAIN:
      stringToPrint = "Math operation is not in the domain";
      break;
    case ERR_RANGE:
      stringToPrint = "Math operation is out of range";
      break;
    case ERR_KILLED:
      stringToPrint = "Process killed";
      break;
    case ERR_NOSUCHFILE:
      stringToPrint = "No such file.";
      break;
    case ERR_NOSUCHDIR:
      stringToPrint = "No such directory.";
      break;
    case ERR_NOTAFILE:
      stringToPrint = "Object is not a file.";
      break;
    case ERR_NOTADIR:
      stringToPrint = "Object is not a directory.";
      break;
    case ERR_NOWRITE:
      stringToPrint = "Cannot write data.";
      break;
    case ERR_NOSUCHUSER:
      stringToPrint = "No such user.";
      break;
    case ERR_NOSUCHPROCESS:
      stringToPrint = "No such process.";
      break;
    case ERR_NOSUCHDRIVER:
      stringToPrint = "There is no driver for this operation.";
      break;
    case ERR_NOSUCHFUNCTION:
      stringToPrint = "Operation not supported.";
      break;
    case ERR_BUG:
      stringToPrint = "Internal error (bug).";
      break;
    default:
      stringToPrint = "Unknown error.";
      break;
    }

  textPrintLine(stringToPrint);
 
  // Don't change errno.
  return;
}
