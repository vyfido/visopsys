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
//  kernelApi.c
//
	
// This is the part of the kernel's API that sorts out which functions
// get called from external locations.

#include "kernelApi.h"
#include "kernelParameters.h"
#include "kernelText.h"
#include "kernelFilesystem.h"
#include "kernelFile.h"
#include "kernelFileStream.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelLoader.h"
#include "kernelRtcFunctions.h"
#include "kernelShutdown.h"
#include "kernelMiscFunctions.h"
#include "kernelRandom.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>

// We do this so that <sys/api.h> won't complain about being included
// in a kernel file
#undef KERNEL
#include <sys/api.h>
#define KERNEL

static kernelFunctionIndex functionIndex[] =
{
  // Text input/output functions

  { _fnum_textStreamGetColumns, kernelTextStreamGetColumns,
    "kernelTextStreamGetColumns", 0, PRIVILEGE_USER },

  { _fnum_textStreamGetRows, kernelTextStreamGetRows,
    "kernelTextStreamGetRows", 0, PRIVILEGE_USER },

  { _fnum_textStreamGetForeground, kernelTextStreamGetForeground,
    "kernelTextStreamGetForeground", 0, PRIVILEGE_USER },

  { _fnum_textStreamSetForeground, kernelTextStreamSetForeground,
    "kernelTextStreamSetForeground", 1, PRIVILEGE_USER },

  { _fnum_textStreamGetBackground, kernelTextStreamGetBackground,
    "kernelTextStreamGetBackground", 0, PRIVILEGE_USER },

  { _fnum_textStreamSetBackground, kernelTextStreamSetBackground,
    "kernelTextStreamSetBackground", 1, PRIVILEGE_USER },

  { _fnum_textPutc, kernelTextPutc, 
    "kernelTextPutc", 1, PRIVILEGE_USER },

  { _fnum_textPrint, kernelTextPrint, 
    "kernelTextPrint", 1, PRIVILEGE_USER },

  { _fnum_textPrintLine, kernelTextPrintLine,
    "kernelTextPrintLine", 1, PRIVILEGE_USER },

  { _fnum_textNewline, kernelTextNewline,
    "kernelTextNewline", 0, PRIVILEGE_USER },

  { _fnum_textBackSpace, kernelTextBackSpace, 
    "kernelTextBackSpace", 0, PRIVILEGE_USER },

  { _fnum_textTab, kernelTextTab, 
    "kernelTextTab", 0, PRIVILEGE_USER },

  { _fnum_textCursorUp, kernelTextCursorUp, 
    "kernelTextCursorUp", 0, PRIVILEGE_USER },

  { _fnum_textCursorDown, kernelTextCursorDown, 
    "kernelTextCursorDown", 0, PRIVILEGE_USER },

  { _fnum_ternelTextCursorLeft, kernelTextCursorLeft,
    "kernelTextCursorLeft", 0, PRIVILEGE_USER },

  { _fnum_textCursorRight, kernelTextCursorRight, 
    "kernelTextCursorRight", 0, PRIVILEGE_USER },

  { _fnum_textGetNumColumns, kernelTextGetNumColumns, 
    "kernelTextGetNumColumns", 0, PRIVILEGE_USER },

  { _fnum_textGetNumRows, kernelTextGetNumRows, 
    "kernelTextGetNumRows", 0, PRIVILEGE_USER },

  { _fnum_textGetColumn, kernelTextGetColumn, 
    "kernelTextGetColumn", 0, PRIVILEGE_USER },

  { _fnum_textSetColumn, kernelTextSetColumn, 
    "kernelTextSetColumn", 1, PRIVILEGE_USER },

  { _fnum_textGetRow, kernelTextGetRow, 
    "kernelTextGetRow", 0, PRIVILEGE_USER },

  { _fnum_textSetRow, kernelTextSetRow, 
    "kernelTextSetRow", 1, PRIVILEGE_USER },

  { _fnum_textClearScreen, kernelTextClearScreen, 
    "kernelTextClearScreen", 0, PRIVILEGE_USER },

  { _fnum_textInputCount, kernelTextInputCount, 
    "kernelTextInputCount", 0, PRIVILEGE_USER },

  { _fnum_textInputGetc, kernelTextInputGetc,
    "kernelTextInputGetc", 1, PRIVILEGE_USER },

  { _fnum_textInputReadN, kernelTextInputReadN, 
    "kernelTextInputReadN", 2, PRIVILEGE_USER },

  { _fnum_textInputReadAll, kernelTextInputReadAll, 
    "kernelTextInputReadAll", 1, PRIVILEGE_USER },

  { _fnum_textInputAppend, kernelTextInputAppend, 
    "kernelTextInputAppend", 1, PRIVILEGE_USER },

  { _fnum_textInputAppendN, kernelTextInputAppendN, 
    "kernelTextInputAppendN", 2, PRIVILEGE_USER },

  { _fnum_textInputRemove, kernelTextInputRemove, 
    "kernelTextInputRemove", 0, PRIVILEGE_USER },

  { _fnum_textInputRemoveN, kernelTextInputRemoveN, 
    "kernelTextInputRemoveN", 1, PRIVILEGE_USER },

  { _fnum_textInputRemoveAll, kernelTextInputRemoveAll, 
    "kernelTextInputRemoveAll", 0, PRIVILEGE_USER },


  // Filesystem functions

  { _fnum_filesystemSync, kernelFilesystemSync,
    "kernelFilesystemSync", 1, PRIVILEGE_USER },

  { _fnum_filesystemMount, kernelFilesystemMount,
    "kernelFilesystemMount", 2, PRIVILEGE_USER },

  { _fnum_filesystemUnmount, kernelFilesystemUnmount,
    "kernelFilesystemUnmount", 1, PRIVILEGE_USER },

  { _fnum_filesystemNumberMounted, kernelFilesystemNumberMounted, 
    "kernelFilesystemNumberMounted", 0, PRIVILEGE_USER },

  { _fnum_filesystemFirstFilesystem, kernelFilesystemFirstFilesystem, 
    "kernelFilesystemFirstFilesystem", 1, PRIVILEGE_USER },

  { _fnum_filesystemNextFilesystem, kernelFilesystemNextFilesystem, 
    "kernelFilesystemNextFilesystem", 1, PRIVILEGE_USER },

  { _fnum_filesystemGetFree, kernelFilesystemGetFree,
    "kernelFilesystemGetFree", 1, PRIVILEGE_USER },

  { _fnum_filesystemGetBlockSize, kernelFilesystemGetBlockSize,
    "kernelFilesystemGetBlockSize", 1, PRIVILEGE_USER },


  // File functions

  { _fnum_fileFixupPath, kernelFileFixupPath,
    "kernelFileFixupPath", 2, PRIVILEGE_USER },

  { _fnum_fileFirst, kernelFileFirst,
    "kernelFileFirst", 2, PRIVILEGE_USER },

  { _fnum_fileNext, kernelFileNext,
    "kernelFileNext", 2, PRIVILEGE_USER },

  { _fnum_fileFind, kernelFileFind,
    "kernelFileFind", 2, PRIVILEGE_USER },

  { _fnum_fileOpen, kernelFileOpen,
    "kernelFileOpen", 3, PRIVILEGE_USER },

  { _fnum_fileClose, kernelFileClose,
    "kernelFileClose", 1, PRIVILEGE_USER },

  { _fnum_fileRead, kernelFileRead,
    "kernelFileRead", 4, PRIVILEGE_USER },

  { _fnum_fileWrite, kernelFileWrite,
    "kernelFileWrite", 4, PRIVILEGE_USER },

  { _fnum_fileDelete, kernelFileDelete,
    "kernelFileDelete", 1, PRIVILEGE_USER },

  { _fnum_fileDeleteSecure, kernelFileDeleteSecure,
    "kernelFileDeleteSecure", 1, PRIVILEGE_USER },

  { _fnum_fileMakeDir, kernelFileMakeDir,
    "kernelFileMakeDir", 1, PRIVILEGE_USER },

  { _fnum_fileRemoveDir, kernelFileRemoveDir,
    "kernelFileRemoveDir", 1, PRIVILEGE_USER },

  { _fnum_fileCopy, kernelFileCopy,
    "kernelFileCopy", 2, PRIVILEGE_USER },

  { _fnum_fileMove, kernelFileMove,
    "kernelFileMove", 2, PRIVILEGE_USER },

  { _fnum_fileTimestamp, kernelFileTimestamp,
    "kernelFileTimestamp", 1, PRIVILEGE_USER },

  { _fnum_fileStreamOpen, kernelFileStreamOpen,
    "kernelFileStreamOpen", 3, PRIVILEGE_USER },

  { _fnum_fileStreamSeek, kernelFileStreamSeek,
    "kernelFileStreamSeek", 2, PRIVILEGE_USER },

  { _fnum_fileStreamRead, kernelFileStreamRead,
    "kernelFileStreamRead", 3, PRIVILEGE_USER },

  { _fnum_fileStreamWrite, kernelFileStreamWrite,
    "kernelFileStreamWrite", 3, PRIVILEGE_USER },

  { _fnum_fileStreamFlush, kernelFileStreamFlush,
    "kernelFileStreamFlush", 1, PRIVILEGE_USER },

  { _fnum_fileStreamClose, kernelFileStreamClose,
    "kernelFileStreamClose", 1, PRIVILEGE_USER },


  // Memory manager functions

  { _fnum_memoryPrintUsage, kernelMemoryPrintUsage,
    "kernelMemoryPrintUsage", 0, PRIVILEGE_USER },

  { _fnum_memoryRequestBlock, kernelMemoryRequestBlock,
    "kernelMemoryRequestBlock", 3, PRIVILEGE_USER },

  { _fnum_memoryRequestPhysicalBlock, kernelMemoryRequestPhysicalBlock, 
    "kernelMemoryRequestPhysicalBlock", 3, PRIVILEGE_SUPERVISOR },

  { _fnum_memoryReleaseByPointer, kernelMemoryReleaseByPointer, 
    "kernelMemoryReleaseByPointer", 1, PRIVILEGE_USER },

  { _fnum_memoryReleaseAllByProcId, kernelMemoryReleaseAllByProcId, 
    "kernelMemoryReleaseAllByProcId", 1, PRIVILEGE_USER },

  { _fnum_memoryChangeOwner, kernelMemoryChangeOwner, 
    "kernelMemoryChangeOwner", 4, PRIVILEGE_SUPERVISOR },


  // Multitasker functions.

  { _fnum_multitaskerSpawn, kernelMultitaskerSpawn,
    "kernelMultitaskerSpawn", 4, PRIVILEGE_USER },

  { _fnum_multitaskerYield, kernelMultitaskerYield,
    "kernelMultitaskerYield", 0, PRIVILEGE_USER },

  { _fnum_multitaskerWait, kernelMultitaskerWait,
    "kernelMultitaskerWait", 1, PRIVILEGE_USER },

  { _fnum_multitaskerCreateProcess, kernelMultitaskerCreateProcess, 
    "kernelMultitaskerCreateProcess", 5, PRIVILEGE_USER },

  { _fnum_multitaskerKillProcess, kernelMultitaskerKillProcess,
    "kernelMultitaskerKillProcess", 1, PRIVILEGE_USER },

  { _fnum_multitaskerTerminate, kernelMultitaskerTerminate,
    "kernelMultitaskerTerminate", 1, PRIVILEGE_USER },

  { _fnum_multitaskerGetCurrentProcessId, 
    kernelMultitaskerGetCurrentProcessId, 
    "kernelMultitaskerGetCurrentProcessId", 0, PRIVILEGE_USER },

  { _fnum_multitaskerGetProcessOwner, kernelMultitaskerGetProcessOwner, 
    "kernelMultitaskerGetProcessOwner", 1, PRIVILEGE_USER },

  { _fnum_multitaskerSetProcessPriority, kernelMultitaskerSetProcessPriority, 
    "kernelMultitaskerSetProcessPriority", 2, PRIVILEGE_USER },

  { _fnum_multitaskerGetProcessState, kernelMultitaskerGetProcessState, 
    "kernelMultitaskerGetProcessState", 2, PRIVILEGE_USER },

  { _fnum_multitaskerSetProcessState, kernelMultitaskerSetProcessState, 
    "kernelMultitaskerSetProcessState", 2, PRIVILEGE_USER },

  { _fnum_multitaskerGetCurrentDirectory, 
    kernelMultitaskerGetCurrentDirectory, 
    "kernelMultitaskerGetCurrentDirectory", 2, PRIVILEGE_USER },

  { _fnum_multitaskerSetCurrentDirectory, 
    kernelMultitaskerSetCurrentDirectory, 
    "kernelMultitaskerSetCurrentDirectory", 1, PRIVILEGE_USER },

  { _fnum_multitaskerDumpProcessList, kernelMultitaskerDumpProcessList, 
    "kernelMultitaskerDumpProcessList", 0, PRIVILEGE_USER },

  { _fnum_multitaskerGetProcessorTime, kernelMultitaskerGetProcessorTime, 
    "kernelMultitaskerGetProcessorTime", 1, PRIVILEGE_USER },


  // Loader functions

  { _fnum_loaderLoadAndExec, kernelLoaderLoadAndExec, 
    "kernelLoaderLoadAndExec", 4, PRIVILEGE_USER },


  // Real-time clock functions

  { _fnum_rtcReadSeconds, kernelRtcReadSeconds,
    "kernelRtcReadSeconds", 0, PRIVILEGE_USER },

  { _fnum_rtcReadMinutes, kernelRtcReadMinutes,
    "kernelRtcReadMinutes", 0, PRIVILEGE_USER },

  { _fnum_rtcReadHours, kernelRtcReadHours,
    "kernelRtcReadHours", 0, PRIVILEGE_USER },

  { _fnum_rtcReadDayOfWeek, kernelRtcReadDayOfWeek,
    "kernelRtcReadDayOfWeek", 0, PRIVILEGE_USER },

  { _fnum_rtcReadDayOfMonth, kernelRtcReadDayOfMonth,
    "kernelRtcReadDayOfMonth", 0, PRIVILEGE_USER },

  { _fnum_rtcReadMonth, kernelRtcReadMonth,
    "kernelRtcReadMonth", 0, PRIVILEGE_USER },

  { _fnum_rtcReadYear, kernelRtcReadYear,
    "kernelRtcReadYear", 0, PRIVILEGE_USER },

  { _fnum_rtcUptimeSeconds, kernelRtcUptimeSeconds,
    "kernelRtcUptimeSeconds", 0, PRIVILEGE_USER },

  { _fnum_rtcDateTime, kernelRtcDateTime,
    "kernelRtcDateTime", 1, PRIVILEGE_USER },


  // Random number functions

  { _fnum_randomUnformatted, kernelRandomUnformatted, 
    "kernelRandomUnformatted", 0, PRIVILEGE_USER },

  { _fnum_randomFormatted, kernelRandomFormatted, 
    "kernelRandomFormatted", 2, PRIVILEGE_USER },

  { _fnum_randomSeededUnformatted, kernelRandomSeededUnformatted, 
    "kernelRandomUnformatted", 1, PRIVILEGE_USER },

  { _fnum_randomSeededFormatted, kernelRandomSeededFormatted, 
    "kernelRandomSeededFormatted", 3, PRIVILEGE_USER },


  // Environment functions

  { _fnum_environmentGet, kernelEnvironmentGet, 
    "kernelEnvironmentGet", 3, PRIVILEGE_USER },

  { _fnum_environmentSet, kernelEnvironmentSet, 
    "kernelEnvironmentSet", 2, PRIVILEGE_USER },

  { _fnum_environmentUnset, kernelEnvironmentUnset, 
    "kernelEnvironmentUnset", 1, PRIVILEGE_USER },

  { _fnum_environmentDump, kernelEnvironmentDump, 
    "kernelEnvironmentDump", 0, PRIVILEGE_USER },


  // Miscellaneous functions

  { _fnum_shutdown, kernelShutdown, 
    "kernelShutdown", 2, PRIVILEGE_USER },

  { _fnum_version, kernelVersion, 
    "kernelVersion", 0, PRIVILEGE_USER },


  // List terminator
  { 0, NULL, NULL, 0, 0 }
};


int kernelApi(unsigned int *argList)
{
  int status = 0;
  int argCount = 0;
  int functionNumber = 0;
  kernelFunctionIndex *functionEntry = NULL;
  int currentProc = 0;
  int currentPriv = 0;
  int (*functionPointer)();
  int count;


  // Check arg
  if (argList == NULL)
    {
      kernelError(kernel_error, "No args supplied to API call");
      return (status = ERR_NULLPARAMETER);
    }

  // How many parameters are there?
  argCount = argList[0];
  argCount -= 1;

  if ((argCount < 0) || (argCount > API_MAX_ARGS))
    {
      kernelError(kernel_error, "Too many arguments to API call");
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Which function number are we being asked to call?
  functionNumber = argList[1];

  // Now we need to figure out which function we are being asked to call.
  for (count = 0; functionIndex[count].functionNumber != 0; count ++)
    if (functionIndex[count].functionNumber == functionNumber)
      {
	functionEntry = &functionIndex[count];
	break;
      }

  // Is there such a function?
  if (functionEntry == NULL)
    {
      kernelError(kernel_error, "No such API function");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Do the number of args match the number expected?
  if (argCount != functionEntry->argCount)
    {
      kernelError(kernel_error,
		  "Incorrect number of arguments to API function");
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Does the caller have the adequate privilege level to call this
  // function?
  currentProc = kernelMultitaskerGetCurrentProcessId();
  currentPriv = kernelMultitaskerGetProcessPrivilege(currentProc);
  if (currentPriv < 0)
    {
      kernelError(kernel_error, "Couldn't determine current privilege level");
      return (status = ERR_BUG);
    }
  else if (currentPriv > functionEntry->privilege)
    {
      kernelError(kernel_error,
		  "Insufficient privilege to invoke API function");
      return (status = ERR_PERMISSION);
    }

  // Make 'functionPointer' equal the address of the requested kernel
  // function.
  functionPointer = functionEntry->functionPointer;
  
  // Call the function, with the appropriate number of arguments.
  switch(argCount)
    {
    case 0:
      return (functionPointer());
    case 1:
      return (functionPointer(argList[2]));
    case 2:
      return (functionPointer(argList[2], argList[3]));
    case 3:
      return (functionPointer(argList[2], argList[3], argList[4]));
    case 4:
      return (functionPointer(argList[2], argList[3], argList[4], 
			      argList[5]));
    case 5:
      return (functionPointer(argList[2], argList[3], argList[4], 
			      argList[5], argList[6]));
    case 6:
      return (functionPointer(argList[2], argList[3], argList[4], 
			      argList[5], argList[6], argList[7]));
    case 7:
      return (functionPointer(argList[2], argList[3], argList[4], 
			      argList[5], argList[6], argList[7],
			      argList[8]));
    case 8:
      return (functionPointer(argList[2], argList[3], argList[4], 
			      argList[5], argList[6], argList[7],
			      argList[8], argList[9]));
    case 9:
      return (functionPointer(argList[2], argList[3], argList[4], 
			      argList[5], argList[6], argList[7],
			      argList[8], argList[9], argList[10]));
    default:
      return (status = ERR_ARGUMENTCOUNT);
    }
}

