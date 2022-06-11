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
//  kernelApi.c
//
	
// This is the part of the kernel's API that sorts out which functions
// get called from external locations.

#include "kernelApi.h"
#include "kernelParameters.h"
#include "kernelText.h"
#include "kernelFile.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelLoader.h"
#include "kernelRtcFunctions.h"
#include "kernelShutdown.h"
#include "kernelMiscFunctions.h"
#include "kernelWindowManager.h"
#include "kernelRandom.h"
#include "kernelError.h"
#include <sys/errors.h>

// We do this so that <sys/api.h> won't complain about being included
// in a kernel file
#undef KERNEL
#include <sys/api.h>
#define KERNEL

static kernelFunctionIndex functionIndex[] =
{
  // Text input/output functions

  { _fnum_textGetForeground, kernelTextGetForeground, 0, PRIVILEGE_USER },
  { _fnum_textSetForeground, kernelTextSetForeground, 1, PRIVILEGE_USER },
  { _fnum_textGetBackground, kernelTextGetBackground, 0, PRIVILEGE_USER },
  { _fnum_textSetBackground, kernelTextSetBackground, 1, PRIVILEGE_USER },
  { _fnum_textPutc, kernelTextPutc, 1, PRIVILEGE_USER },
  { _fnum_textPrint, kernelTextPrint, 1, PRIVILEGE_USER },
  { _fnum_textPrintLine, kernelTextPrintLine, 1, PRIVILEGE_USER },
  { _fnum_textNewline, kernelTextNewline, 0, PRIVILEGE_USER },
  { _fnum_textBackSpace, kernelTextBackSpace, 0, PRIVILEGE_USER },
  { _fnum_textTab, kernelTextTab, 0, PRIVILEGE_USER },
  { _fnum_textCursorUp, kernelTextCursorUp, 0, PRIVILEGE_USER },
  { _fnum_textCursorDown, kernelTextCursorDown, 0, PRIVILEGE_USER },
  { _fnum_ternelTextCursorLeft, kernelTextCursorLeft, 0, PRIVILEGE_USER },
  { _fnum_textCursorRight, kernelTextCursorRight, 0, PRIVILEGE_USER },
  { _fnum_textGetNumColumns, kernelTextGetNumColumns, 0, PRIVILEGE_USER },
  { _fnum_textGetNumRows, kernelTextGetNumRows, 0, PRIVILEGE_USER },
  { _fnum_textGetColumn, kernelTextGetColumn, 0, PRIVILEGE_USER },
  { _fnum_textSetColumn, kernelTextSetColumn, 1, PRIVILEGE_USER },
  { _fnum_textGetRow, kernelTextGetRow, 0, PRIVILEGE_USER },
  { _fnum_textSetRow, kernelTextSetRow, 1, PRIVILEGE_USER },
  { _fnum_textClearScreen, kernelTextClearScreen, 0, PRIVILEGE_USER },
  { _fnum_textInputCount, kernelTextInputCount, 0, PRIVILEGE_USER },
  { _fnum_textInputGetc, kernelTextInputGetc, 1, PRIVILEGE_USER },
  { _fnum_textInputReadN, kernelTextInputReadN, 2, PRIVILEGE_USER },
  { _fnum_textInputReadAll, kernelTextInputReadAll, 1, PRIVILEGE_USER },
  { _fnum_textInputAppend, kernelTextInputAppend, 1, PRIVILEGE_USER },
  { _fnum_textInputAppendN, kernelTextInputAppendN, 2, PRIVILEGE_USER },
  { _fnum_textInputRemove, kernelTextInputRemove, 0, PRIVILEGE_USER },
  { _fnum_textInputRemoveN, kernelTextInputRemoveN, 1, PRIVILEGE_USER },
  { _fnum_textInputRemoveAll, kernelTextInputRemoveAll, 0, PRIVILEGE_USER },
  { _fnum_textInputSetEcho, kernelTextInputSetEcho, 1,  PRIVILEGE_USER },

  
  // Disk functions

  { _fnum_diskFunctionsGetBoot, kernelDiskFunctionsGetBoot,
    0, PRIVILEGE_USER },
  { _fnum_diskFunctionsGetCount, kernelDiskFunctionsGetCount,
    0, PRIVILEGE_USER },
  { _fnum_diskFunctionsGetInfo, kernelDiskFunctionsGetInfo,
    2, PRIVILEGE_USER },
  { _fnum_diskFunctionsMotorOn, kernelDiskFunctionsMotorOn,
    1, PRIVILEGE_SUPERVISOR },
  { _fnum_diskFunctionsMotorOff, kernelDiskFunctionsMotorOff,
    1, PRIVILEGE_SUPERVISOR },
  { _fnum_diskFunctionsDiskChanged, kernelDiskFunctionsDiskChanged,
    1, PRIVILEGE_SUPERVISOR },
  { _fnum_diskFunctionsReadSectors, kernelDiskFunctionsReadSectors,
    4, PRIVILEGE_SUPERVISOR },
  { _fnum_diskFunctionsWriteSectors, kernelDiskFunctionsWriteSectors,
    4, PRIVILEGE_SUPERVISOR },
  { _fnum_diskFunctionsReadAbsoluteSectors,
    kernelDiskFunctionsReadAbsoluteSectors, 4, PRIVILEGE_SUPERVISOR },
  { _fnum_diskFunctionsWriteAbsoluteSectors,
    kernelDiskFunctionsWriteAbsoluteSectors, 4, PRIVILEGE_SUPERVISOR },


  // Filesystem functions

  { _fnum_filesystemCheck, kernelFilesystemCheck,3, PRIVILEGE_USER },
  { _fnum_filesystemDefragment, kernelFilesystemDefragment,1, PRIVILEGE_USER },
  { _fnum_filesystemMount, kernelFilesystemMount,2, PRIVILEGE_USER },
  { _fnum_filesystemSync, kernelFilesystemSync,1, PRIVILEGE_USER },
  { _fnum_filesystemUnmount, kernelFilesystemUnmount,1, PRIVILEGE_USER },
  { _fnum_filesystemNumberMounted, kernelFilesystemNumberMounted,
    0, PRIVILEGE_USER },
  { _fnum_filesystemFirstFilesystem, kernelFilesystemFirstFilesystem,
    1, PRIVILEGE_USER },
  { _fnum_filesystemNextFilesystem, kernelFilesystemNextFilesystem,
    1, PRIVILEGE_USER },
  { _fnum_filesystemGetFree, kernelFilesystemGetFree, 1, PRIVILEGE_USER },
  { _fnum_filesystemGetBlockSize, kernelFilesystemGetBlockSize,
    1, PRIVILEGE_USER },


  // File functions

  { _fnum_fileFixupPath, kernelFileFixupPath, 2, PRIVILEGE_USER },
  { _fnum_fileFirst, kernelFileFirst, 2, PRIVILEGE_USER },
  { _fnum_fileNext, kernelFileNext, 2, PRIVILEGE_USER },
  { _fnum_fileFind, kernelFileFind, 2, PRIVILEGE_USER },
  { _fnum_fileOpen, kernelFileOpen, 3, PRIVILEGE_USER },
  { _fnum_fileClose, kernelFileClose, 1, PRIVILEGE_USER },
  { _fnum_fileRead, kernelFileRead, 4, PRIVILEGE_USER },
  { _fnum_fileWrite, kernelFileWrite, 4, PRIVILEGE_USER },
  { _fnum_fileDelete, kernelFileDelete, 1, PRIVILEGE_USER },
  { _fnum_fileDeleteSecure, kernelFileDeleteSecure, 1, PRIVILEGE_USER },
  { _fnum_fileMakeDir, kernelFileMakeDir, 1, PRIVILEGE_USER },
  { _fnum_fileRemoveDir, kernelFileRemoveDir, 1, PRIVILEGE_USER },
  { _fnum_fileCopy, kernelFileCopy, 2, PRIVILEGE_USER },
  { _fnum_fileCopyRecursive, kernelFileCopyRecursive, 2, PRIVILEGE_USER },
  { _fnum_fileMove, kernelFileMove, 2, PRIVILEGE_USER },
  { _fnum_fileTimestamp, kernelFileTimestamp, 1, PRIVILEGE_USER },
  { _fnum_fileStreamOpen, kernelFileStreamOpen, 3, PRIVILEGE_USER },
  { _fnum_fileStreamSeek, kernelFileStreamSeek, 2, PRIVILEGE_USER },
  { _fnum_fileStreamRead, kernelFileStreamRead, 3, PRIVILEGE_USER },
  { _fnum_fileStreamWrite, kernelFileStreamWrite, 3, PRIVILEGE_USER },
  { _fnum_fileStreamFlush, kernelFileStreamFlush, 1, PRIVILEGE_USER },
  { _fnum_fileStreamClose, kernelFileStreamClose, 1, PRIVILEGE_USER },


  // Memory manager functions

  { _fnum_memoryPrintUsage, kernelMemoryPrintUsage, 0, PRIVILEGE_USER },
  { _fnum_memoryRequestBlock, kernelMemoryRequestBlock, 3, PRIVILEGE_USER },
  { _fnum_memoryRequestPhysicalBlock, kernelMemoryRequestPhysicalBlock,
    3, PRIVILEGE_SUPERVISOR },
  { _fnum_memoryReleaseBlock, kernelMemoryReleaseBlock, 1, PRIVILEGE_USER },
  { _fnum_memoryReleaseAllByProcId, kernelMemoryReleaseAllByProcId,
    1, PRIVILEGE_USER },
  { _fnum_memoryChangeOwner, kernelMemoryChangeOwner,
    4, PRIVILEGE_SUPERVISOR },


  // Multitasker functions.

  { _fnum_multitaskerCreateProcess, kernelMultitaskerCreateProcess,
    5, PRIVILEGE_USER },
  { _fnum_multitaskerSpawn, kernelMultitaskerSpawn, 4, PRIVILEGE_USER },
  { _fnum_multitaskerGetCurrentProcessId, 
    kernelMultitaskerGetCurrentProcessId, 0, PRIVILEGE_USER },
  { _fnum_multitaskerGetProcessOwner, kernelMultitaskerGetProcessOwner,
    1, PRIVILEGE_USER },
  { _fnum_multitaskerGetProcessName, kernelMultitaskerGetProcessName,
    1, PRIVILEGE_USER },
  { _fnum_multitaskerGetProcessState, kernelMultitaskerGetProcessState,
    2, PRIVILEGE_USER },
  { _fnum_multitaskerSetProcessState, kernelMultitaskerSetProcessState,
    2, PRIVILEGE_USER },
  { _fnum_multitaskerGetProcessPriority, kernelMultitaskerGetProcessPriority,
    1, PRIVILEGE_USER },
  { _fnum_multitaskerSetProcessPriority, kernelMultitaskerSetProcessPriority,
    2, PRIVILEGE_USER },
  { _fnum_multitaskerGetProcessPrivilege,
    kernelMultitaskerGetProcessPrivilege, 1, PRIVILEGE_USER },
  { _fnum_multitaskerGetCurrentDirectory, 
    kernelMultitaskerGetCurrentDirectory, 2, PRIVILEGE_USER },
  { _fnum_multitaskerSetCurrentDirectory, 
    kernelMultitaskerSetCurrentDirectory, 1, PRIVILEGE_USER },
  { _fnum_multitaskerGetTextInput, kernelMultitaskerGetTextInput,
    0, PRIVILEGE_USER },
  { _fnum_multitaskerSetTextInput, kernelMultitaskerSetTextInput,
    2, PRIVILEGE_USER },
  { _fnum_multitaskerGetTextOutput, kernelMultitaskerGetTextOutput,
    0, PRIVILEGE_USER },
  { _fnum_multitaskerSetTextOutput, kernelMultitaskerSetTextOutput,
    2, PRIVILEGE_USER },
  { _fnum_multitaskerGetProcessorTime, kernelMultitaskerGetProcessorTime, 
    1, PRIVILEGE_USER },
  { _fnum_multitaskerYield, kernelMultitaskerYield, 0, PRIVILEGE_USER },
  { _fnum_multitaskerWait, kernelMultitaskerWait, 1, PRIVILEGE_USER },
  { _fnum_multitaskerBlock, kernelMultitaskerBlock, 1, PRIVILEGE_USER },
  { _fnum_multitaskerKillProcess, kernelMultitaskerKillProcess,
    2, PRIVILEGE_USER },
  { _fnum_multitaskerTerminate, kernelMultitaskerTerminate,
    1, PRIVILEGE_USER },
  { _fnum_multitaskerDumpProcessList, kernelMultitaskerDumpProcessList,
    0, PRIVILEGE_USER },


  // Loader functions

  { _fnum_loaderLoad, kernelLoaderLoad, 2, PRIVILEGE_USER },
  { _fnum_loaderLoadProgram, kernelLoaderLoadProgram, 4, PRIVILEGE_USER },
  { _fnum_loaderExecProgram, kernelLoaderExecProgram, 2, PRIVILEGE_USER },
  { _fnum_loaderLoadAndExec, kernelLoaderLoadAndExec,  5, PRIVILEGE_USER },


  // Real-time clock functions

  { _fnum_rtcReadSeconds, kernelRtcReadSeconds, 0, PRIVILEGE_USER },
  { _fnum_rtcReadMinutes, kernelRtcReadMinutes, 0, PRIVILEGE_USER },
  { _fnum_rtcReadHours, kernelRtcReadHours, 0, PRIVILEGE_USER },
  { _fnum_rtcReadDayOfWeek, kernelRtcReadDayOfWeek, 0, PRIVILEGE_USER },
  { _fnum_rtcReadDayOfMonth, kernelRtcReadDayOfMonth, 0, PRIVILEGE_USER },
  { _fnum_rtcReadMonth, kernelRtcReadMonth, 0, PRIVILEGE_USER },
  { _fnum_rtcReadYear, kernelRtcReadYear, 0, PRIVILEGE_USER },
  { _fnum_rtcUptimeSeconds, kernelRtcUptimeSeconds, 0, PRIVILEGE_USER },
  { _fnum_rtcDateTime, kernelRtcDateTime, 1, PRIVILEGE_USER },


  // Random number functions

  { _fnum_randomUnformatted, kernelRandomUnformatted, 0, PRIVILEGE_USER },
  { _fnum_randomFormatted, kernelRandomFormatted, 2, PRIVILEGE_USER },
  { _fnum_randomSeededUnformatted, kernelRandomSeededUnformatted,
    1, PRIVILEGE_USER },
  { _fnum_randomSeededFormatted, kernelRandomSeededFormatted,
    3, PRIVILEGE_USER },


  // Environment functions

  { _fnum_environmentGet, kernelEnvironmentGet, 3, PRIVILEGE_USER },
  { _fnum_environmentSet, kernelEnvironmentSet, 2, PRIVILEGE_USER },
  { _fnum_environmentUnset, kernelEnvironmentUnset, 1, PRIVILEGE_USER },
  { _fnum_environmentDump, kernelEnvironmentDump, 0, PRIVILEGE_USER },


  // Raw graphics functions

  { _fnum_graphicsAreEnabled, kernelGraphicsAreEnabled, 0, PRIVILEGE_USER },
  { _fnum_graphicGetScreenWidth, kernelGraphicGetScreenWidth,
    0, PRIVILEGE_USER },
  { _fnum_graphicGetScreenHeight, kernelGraphicGetScreenHeight,
    0, PRIVILEGE_USER },
  { _fnum_graphicCalculateAreaBytes, kernelGraphicCalculateAreaBytes,
    2, PRIVILEGE_USER },
  { _fnum_graphicClearScreen, kernelGraphicClearScreen, 1, PRIVILEGE_USER },
  { _fnum_graphicDrawPixel, kernelGraphicDrawPixel, 5, PRIVILEGE_USER },
  { _fnum_graphicDrawLine, kernelGraphicDrawLine, 7, PRIVILEGE_USER },
  { _fnum_graphicDrawRect, kernelGraphicDrawRect, 9, PRIVILEGE_USER },
  { _fnum_graphicDrawOval, kernelGraphicDrawOval, 9, PRIVILEGE_USER },
  { _fnum_graphicDrawImage, kernelGraphicDrawImage, 4, PRIVILEGE_USER },
  { _fnum_graphicGetImage, kernelGraphicGetImage, 6, PRIVILEGE_USER },
  { _fnum_graphicDrawText, kernelGraphicDrawText, 7, PRIVILEGE_USER },
  { _fnum_graphicCopyArea, kernelGraphicCopyArea, 7, PRIVILEGE_USER },
  { _fnum_graphicClearArea, kernelGraphicClearArea, 6, PRIVILEGE_USER },
  { _fnum_graphicRenderBuffer, kernelGraphicRenderBuffer, 7, PRIVILEGE_USER },


  // Window manager functions

  { _fnum_windowManagerStart, kernelWindowManagerStart, 0, PRIVILEGE_USER },
  { _fnum_windowManagerLogin, kernelWindowManagerLogin, 1,
    PRIVILEGE_SUPERVISOR },
  { _fnum_windowManagerLogout, kernelWindowManagerLogout, 0, PRIVILEGE_USER },
  { _fnum_windowManagerNewWindow, kernelWindowManagerNewWindow,
    6, PRIVILEGE_USER },
  { _fnum_windowManagerDestroyWindow, kernelWindowManagerDestroyWindow,
    1, PRIVILEGE_USER },
  { _fnum_windowManagerUpdateBuffer, kernelWindowManagerUpdateBuffer,
    5, PRIVILEGE_USER },
  { _fnum_windowSetTitle, kernelWindowSetTitle, 2, PRIVILEGE_USER },
  { _fnum_windowGetSize, kernelWindowGetSize, 3, PRIVILEGE_USER },
  { _fnum_windowSetSize, kernelWindowSetSize, 3, PRIVILEGE_USER },
  { _fnum_windowAutoSize, kernelWindowAutoSize, 1, PRIVILEGE_USER },
  { _fnum_windowGetLocation, kernelWindowGetLocation, 3, PRIVILEGE_USER },
  { _fnum_windowSetLocation, kernelWindowSetLocation, 3, PRIVILEGE_USER },
  { _fnum_windowSetHasBorder, kernelWindowSetHasBorder, 2, PRIVILEGE_USER },
  { _fnum_windowSetHasTitleBar, kernelWindowSetHasTitleBar,
    2, PRIVILEGE_USER },
  { _fnum_windowSetMovable, kernelWindowSetMovable, 2, PRIVILEGE_USER },
  { _fnum_windowSetHasCloseButton, kernelWindowSetHasCloseButton,
    2, PRIVILEGE_USER },
  { _fnum_windowLayout, kernelWindowLayout, 1, PRIVILEGE_USER },
  { _fnum_windowSetVisible, kernelWindowSetVisible, 2, PRIVILEGE_USER },
  { _fnum_windowAddComponent, kernelWindowAddComponent, 3, PRIVILEGE_USER },
  { _fnum_windowAddClientComponent, kernelWindowAddClientComponent,
    3, PRIVILEGE_USER },
  { _fnum_windowComponentGetWidth, kernelWindowComponentGetWidth,
    1, PRIVILEGE_USER },
  { _fnum_windowComponentGetHeight, kernelWindowComponentGetHeight,
    1, PRIVILEGE_USER },
  { _fnum_windowManagerRedrawArea, kernelWindowManagerRedrawArea,
    4, PRIVILEGE_USER },
  { _fnum_windowManagerProcessMouseEvent,
    kernelWindowManagerProcessMouseEvent, 1, PRIVILEGE_USER },
  { _fnum_windowManagerTileBackground, kernelWindowManagerTileBackground,
    1, PRIVILEGE_USER },
  { _fnum_windowManagerCenterBackground, kernelWindowManagerCenterBackground,
    1, PRIVILEGE_USER },
  { _fnum_windowManagerScreenShot, kernelWindowManagerScreenShot, 
    1, PRIVILEGE_USER },
  { _fnum_windowManagerSaveScreenShot, kernelWindowManagerSaveScreenShot, 
    1, PRIVILEGE_USER },
  { _fnum_windowManagerSetTextOutput, kernelWindowManagerSetTextOutput, 
    1, PRIVILEGE_USER },
  { _fnum_windowNewButtonComponent, kernelWindowNewButtonComponent, 
    6, PRIVILEGE_USER },
  { _fnum_windowNewIconComponent, kernelWindowNewIconComponent, 
    4, PRIVILEGE_USER },
  { _fnum_windowNewImageComponent, kernelWindowNewImageComponent, 
    2, PRIVILEGE_USER },
  { _fnum_windowNewTextAreaComponent, kernelWindowNewTextAreaComponent, 
    4, PRIVILEGE_USER },
  { _fnum_windowNewTextFieldComponent, kernelWindowNewTextFieldComponent, 
    3, PRIVILEGE_USER },
  { _fnum_windowNewTextLabelComponent, kernelWindowNewTextLabelComponent, 
    3, PRIVILEGE_USER },
  { _fnum_windowNewTitleBarComponent, kernelWindowNewTitleBarComponent, 
    4, PRIVILEGE_USER },


  // Miscellaneous functions

  { _fnum_fontGetDefault, kernelFontGetDefault, 1, PRIVILEGE_USER },
  { _fnum_fontSetDefault, kernelFontSetDefault, 1, PRIVILEGE_USER },
  { _fnum_fontLoad, kernelFontLoad, 3, PRIVILEGE_USER },
  { _fnum_fontGetPrintedWidth, kernelFontGetPrintedWidth, 2, PRIVILEGE_USER },
  { _fnum_imageLoadBmp, kernelImageLoadBmp, 2, PRIVILEGE_USER },
  { _fnum_imageSaveBmp, kernelImageSaveBmp, 2, PRIVILEGE_USER },
  { _fnum_shutdown, kernelShutdown, 2, PRIVILEGE_USER },
  { _fnum_version, kernelVersion, 0, PRIVILEGE_USER },


  // List terminator
  { 0, NULL, 0, 0 }
};


int kernelApi(unsigned *argList)
{
  int status = 0;
  int argCount = 0;
  unsigned functionNumber = 0;
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
  argCount--;

  if (argCount > API_MAX_ARGS)
    {
      kernelError(kernel_error, "Illegal number of arguments (%u) to API "
		  "call", argCount);
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
      kernelError(kernel_error, "No such API function %d", functionNumber);
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Do the number of args match the number expected?
  if (argCount != functionEntry->argCount)
    {
      kernelError(kernel_error, "Incorrect number of arguments (%d) to API "
		  "function %u (%d)", argCount, functionEntry->functionNumber,
		  functionEntry->argCount);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Does the caller have the adequate privilege level to call this
  // function?
  currentProc = kernelMultitaskerGetCurrentProcessId();
  currentPriv = kernelMultitaskerGetProcessPrivilege(currentProc);
  if (currentPriv < 0)
    {
      kernelError(kernel_error, "Couldn't determine current privilege level "
		  "for call to API function %d",
		  functionEntry->functionNumber);
      return (status = ERR_BUG);
    }
  else if (currentPriv > functionEntry->privilege)
    {
      kernelError(kernel_error, "Insufficient privilege to invoke API "
		  "function %d", functionEntry->functionNumber);
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

