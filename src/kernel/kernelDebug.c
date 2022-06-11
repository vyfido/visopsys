//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  kernelDebug.c
//

#if defined(DEBUG)

#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelInterrupt.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelPic.h"
#include "kernelText.h"
#include <stdio.h>
#include <sys/cdefs.h>

static kernelDebugCategory categories[MAX_DEBUG_CATEGORIES];
static int numDebugCategories = 0;
static const char *fileNames[MAX_DEBUG_FILENAMES];
static int numDebugFileNames = 0;
static int debugAll = 0;
static int showProcess = 0;
static int showFile = 0;
static int showFunction = 0;


static int isCategory(kernelDebugCategory category)
{
  // Returns 1 if we're debugging a particular category
  
  int count;

  for (count = 0; count < numDebugCategories; count ++)
    if (categories[count] == category)
      return (1);

  // Fell through -- not there.
  return (0);
}


static int isFileName(const char *fileName)
{
  // Returns 1 if we're debugging a particular file
  
  int count;

  for (count = 0; count < numDebugFileNames; count ++)
    if (fileNames[count] == fileName)
      return (1);

  // Fell through -- not there.
  return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelDebugInitialize(void)
{
  // Here is where we enable any flags/categories/files for debugging.
  // No effect unless DEBUG is specified in the Makefile.  Here are some
  // examples:
  // kernelDebugFlags(DEBUG_SHOWPROCESS | DEBUG_SHOWFILE | DEBUG_SHOWFUNCTION);
  // kernelDebugAddCategory(debug_all);
  // kernelDebugAddCategory(debug_api);
  // kernelDebugAddCategory(debug_fs);
  // kernelDebugAddCategory(debug_gui);
  // kernelDebugAddCategory(debug_misc);
  // kernelDebugAddCategory(debug_scsi);
  // kernelDebugAddCategory(debug_usb);
  // kernelDebugAddFile("kernelWindow.c");

  return;
}


void kernelDebugFlags(int flags)
{
  // Sets the amount of information that gets displayed with each
  // line of debugging information

  if (flags & DEBUG_SHOWPROCESS)
    showProcess = 1;
  if (flags & DEBUG_SHOWFILE)
    showFile = 1;
  if (flags & DEBUG_SHOWFUNCTION)
    showFunction = 1;

  return;
}


void kernelDebugAddCategory(kernelDebugCategory category)
{
  // Used to turn on a category of debug messages

  if (category == debug_all)
    debugAll = 1;

  else if (!isCategory(category))
    {
      if (numDebugCategories >= MAX_DEBUG_CATEGORIES)
	kernelError(kernel_error, "Max debug categories (%d) already "
		    "registered", MAX_DEBUG_CATEGORIES);
      else
	categories[numDebugCategories++] = category;
    }

  return;
}


void kernelDebugAddFile(const char *fileName)
{
  // Used to turn on debug messages for a source file

  if (!isFileName(fileName))
    {
      if (numDebugFileNames >= MAX_DEBUG_FILENAMES)
	kernelError(kernel_error, "Max debug file names (%d) already "
		    "registered", MAX_DEBUG_FILENAMES);
      else
	fileNames[numDebugFileNames++] = fileName;
    }

  return;
}


void kernelDebugOutput(const char *fileName, const char *function, int line,
		       kernelDebugCategory category, const char *message, ...)
{
  // This routine takes a bunch of parameters and outputs the message,
  // depending on a couple of filtering parameters.

  va_list list;
  char *debugText = NULL;
  kernelTextOutputStream *console = kernelTextGetConsoleOutput();

  // See whether we should skip this message
  if (!debugAll && !isCategory(category) && !isFileName(fileName))
    return;

  debugText = kernelMalloc(MAX_DEBUGTEXT_LENGTH);
  if (debugText == NULL)
    return;

  if (strlen(message) > (MAX_DEBUGTEXT_LENGTH - 80))
    message = "<<<debug message too long>>>";

  strcpy(debugText, "DEBUG " );
  if (showProcess)
    {
      if (kernelProcessingInterrupt)
	sprintf((debugText + strlen(debugText)), "interrupt %x",
		kernelPicGetActive());
      else
	sprintf((debugText + strlen(debugText)), "%s:",
		kernelCurrentProcess->processName);
    }
  if (showFile)
    sprintf((debugText + strlen(debugText)), "%s(%d):", fileName, line);
  if (showFunction)
    sprintf((debugText + strlen(debugText)), "%s:", function);

  kernelTextStreamPrint(console, debugText);

  // Initialize the argument list
  va_start(list, message);

  // Expand the message if there were any parameters
  _expandFormatString(debugText, message, list);

  va_end(list);

  kernelTextStreamPrintLine(console, debugText);
  kernelFree(debugText);

  return;
}


#endif  // defined(DEBUG)
