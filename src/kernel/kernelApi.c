//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
#include "kernelProcessorX86.h"
#include "kernelText.h"
#include "kernelFile.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelLoader.h"
#include "kernelRtc.h"
#include "kernelShutdown.h"
#include "kernelMiscFunctions.h"
#include "kernelWindowManager.h"
#include "kernelRandom.h"
#include "kernelUser.h"
#include "kernelEncrypt.h"
#include "kernelKeyboard.h"
#include "kernelError.h"
#include <sys/errors.h>

// We do this so that <sys/api.h> won't complain about being included
// in a kernel file
#undef KERNEL
#include <sys/api.h>
#define KERNEL

static kernelFunctionIndex textFunctionIndex[] = {

  // Text input/output functions (1000-1999 range)

  { _fnum_textGetConsoleInput, kernelTextGetConsoleInput, 0, PRIVILEGE_USER },
  { _fnum_textSetConsoleInput, kernelTextSetConsoleInput,
    1, PRIVILEGE_SUPERVISOR },
  { _fnum_textGetConsoleOutput, kernelTextGetConsoleOutput,
    0, PRIVILEGE_USER },
  { _fnum_textSetConsoleOutput, kernelTextSetConsoleOutput,
    1, PRIVILEGE_SUPERVISOR },
  { _fnum_textGetCurrentInput, kernelTextGetCurrentInput, 0, PRIVILEGE_USER },
  { _fnum_textSetCurrentInput, kernelTextSetCurrentInput, 1, PRIVILEGE_USER },
  { _fnum_textGetCurrentOutput, kernelTextGetCurrentOutput,
    0, PRIVILEGE_USER },
  { _fnum_textSetCurrentOutput, kernelTextSetCurrentOutput,
    1, PRIVILEGE_USER },
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
  { _fnum_textSetCursor, kernelTextSetCursor, 1, PRIVILEGE_USER },
  { _fnum_textScreenClear, kernelTextScreenClear, 0, PRIVILEGE_USER },
  { _fnum_textScreenSave, kernelTextScreenSave, 0, PRIVILEGE_USER },
  { _fnum_textScreenRestore, kernelTextScreenRestore, 0, PRIVILEGE_USER },
  { _fnum_textInputStreamCount, kernelTextInputStreamCount,
    1, PRIVILEGE_USER },
  { _fnum_textInputCount, kernelTextInputCount, 0, PRIVILEGE_USER },
  { _fnum_textInputStreamGetc, kernelTextInputStreamGetc, 2, PRIVILEGE_USER },
  { _fnum_textInputGetc, kernelTextInputGetc, 1, PRIVILEGE_USER },
  { _fnum_textInputStreamReadN, kernelTextInputStreamReadN,
    3, PRIVILEGE_USER },
  { _fnum_textInputReadN, kernelTextInputReadN, 2, PRIVILEGE_USER },
  { _fnum_textInputStreamReadAll, kernelTextInputStreamReadAll,
    2, PRIVILEGE_USER },
  { _fnum_textInputReadAll, kernelTextInputReadAll, 1, PRIVILEGE_USER },
  { _fnum_textInputStreamAppend, kernelTextInputStreamAppend,
    2, PRIVILEGE_USER },
  { _fnum_textInputAppend, kernelTextInputAppend, 1, PRIVILEGE_USER },
  { _fnum_textInputStreamAppendN, kernelTextInputStreamAppendN,
    3, PRIVILEGE_USER },
  { _fnum_textInputAppendN, kernelTextInputAppendN, 2, PRIVILEGE_USER },
  { _fnum_textInputStreamRemove, kernelTextInputStreamRemove,
    1, PRIVILEGE_USER },
  { _fnum_textInputRemove, kernelTextInputRemove, 0, PRIVILEGE_USER },
  { _fnum_textInputStreamRemoveN, kernelTextInputStreamRemoveN,
    2, PRIVILEGE_USER },
  { _fnum_textInputRemoveN, kernelTextInputRemoveN, 1, PRIVILEGE_USER },
  { _fnum_textInputStreamRemoveAll, kernelTextInputStreamRemoveAll,
    1, PRIVILEGE_USER },
  { _fnum_textInputRemoveAll, kernelTextInputRemoveAll, 0, PRIVILEGE_USER },
  { _fnum_textInputStreamSetEcho, kernelTextInputStreamSetEcho,
    2, PRIVILEGE_USER },
  { _fnum_textInputSetEcho, kernelTextInputSetEcho, 1,  PRIVILEGE_USER }
};

static kernelFunctionIndex diskFunctionIndex[] = {

  // Disk functions (2000-2999 range)

  { _fnum_diskReadPartitions, kernelDiskReadPartitions,
    0, PRIVILEGE_SUPERVISOR },
  { _fnum_diskSync, kernelDiskSync, 0, PRIVILEGE_USER },
  { _fnum_diskGetBoot, kernelDiskGetBoot, 1, PRIVILEGE_USER },
  { _fnum_diskGetCount, kernelDiskGetCount, 0, PRIVILEGE_USER },
  { _fnum_diskGetPhysicalCount, kernelDiskGetPhysicalCount,
    0, PRIVILEGE_USER },
  { _fnum_diskGetInfo, kernelDiskGetInfo, 1, PRIVILEGE_USER },
  { _fnum_diskGetPhysicalInfo, kernelDiskGetPhysicalInfo, 1, PRIVILEGE_USER },
  { _fnum_diskGetPartType, kernelDiskGetPartType, 2, PRIVILEGE_USER },
  { _fnum_diskGetPartTypes, kernelDiskGetPartTypes, 0, PRIVILEGE_USER },
  { _fnum_diskSetDoorState, kernelDiskSetDoorState, 2, PRIVILEGE_USER },
  { _fnum_diskReadSectors, kernelDiskReadSectors, 4, PRIVILEGE_SUPERVISOR },
  { _fnum_diskWriteSectors, kernelDiskWriteSectors, 4, PRIVILEGE_SUPERVISOR },
  { _fnum_diskReadAbsoluteSectors, kernelDiskReadAbsoluteSectors,
    4, PRIVILEGE_SUPERVISOR },
  { _fnum_diskWriteAbsoluteSectors, kernelDiskWriteAbsoluteSectors,
    4, PRIVILEGE_SUPERVISOR }
};

static kernelFunctionIndex filesystemFunctionIndex[] = {

  // Filesystem functions (3000-3999 range)

  { _fnum_filesystemFormat, kernelFilesystemFormat, 4, PRIVILEGE_SUPERVISOR },
  { _fnum_filesystemCheck, kernelFilesystemCheck,3, PRIVILEGE_USER },
  { _fnum_filesystemDefragment, kernelFilesystemDefragment,
    1, PRIVILEGE_USER },
  { _fnum_filesystemMount, kernelFilesystemMount, 2, PRIVILEGE_USER },
  { _fnum_filesystemUnmount, kernelFilesystemUnmount, 1, PRIVILEGE_USER },
  { _fnum_filesystemGetFree, kernelFilesystemGetFree, 1, PRIVILEGE_USER },
  { _fnum_filesystemGetBlockSize, kernelFilesystemGetBlockSize,
    1, PRIVILEGE_USER }
};

static kernelFunctionIndex fileFunctionIndex[] = {

  // File functions (4000-4999 range)

  { _fnum_fileFixupPath, kernelFileFixupPath, 2, PRIVILEGE_USER },
  { _fnum_fileGetDisk, kernelFileGetDisk, 2, PRIVILEGE_USER },
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
  { _fnum_fileStreamReadLine, kernelFileStreamReadLine, 3, PRIVILEGE_USER },
  { _fnum_fileStreamWrite, kernelFileStreamWrite, 3, PRIVILEGE_USER },
  { _fnum_fileStreamWriteStr, kernelFileStreamWriteStr, 2, PRIVILEGE_USER },
  { _fnum_fileStreamWriteLine, kernelFileStreamWriteLine, 2, PRIVILEGE_USER },
  { _fnum_fileStreamFlush, kernelFileStreamFlush, 1, PRIVILEGE_USER },
  { _fnum_fileStreamClose, kernelFileStreamClose, 1, PRIVILEGE_USER }
};

static kernelFunctionIndex memoryFunctionIndex[] = {

  // Memory manager functions (5000-5999 range)

  { _fnum_memoryPrintUsage, kernelMemoryPrintUsage, 1, PRIVILEGE_USER },
  { _fnum_memoryGet, kernelMemoryGet, 2, PRIVILEGE_USER },
  { _fnum_memoryGetPhysical, kernelMemoryGetPhysical,
    3, PRIVILEGE_SUPERVISOR },
  { _fnum_memoryRelease, kernelMemoryRelease, 1, PRIVILEGE_USER },
  { _fnum_memoryReleaseAllByProcId, kernelMemoryReleaseAllByProcId,
    1, PRIVILEGE_USER },
  { _fnum_memoryChangeOwner, kernelMemoryChangeOwner, 4, PRIVILEGE_SUPERVISOR }
};

static kernelFunctionIndex multitaskerFunctionIndex[] = {

  // Multitasker functions (6000-6999 range)

  { _fnum_multitaskerCreateProcess, kernelMultitaskerCreateProcess,
    5, PRIVILEGE_USER },
  { _fnum_multitaskerSpawn, kernelMultitaskerSpawn, 4, PRIVILEGE_USER },
  { _fnum_multitaskerGetCurrentProcessId, kernelMultitaskerGetCurrentProcessId,
    0, PRIVILEGE_USER },
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
  { _fnum_multitaskerGetProcessPrivilege, kernelMultitaskerGetProcessPrivilege,
    1, PRIVILEGE_USER },
  { _fnum_multitaskerGetCurrentDirectory, kernelMultitaskerGetCurrentDirectory,
    2, PRIVILEGE_USER },
  { _fnum_multitaskerSetCurrentDirectory, kernelMultitaskerSetCurrentDirectory,
    1, PRIVILEGE_USER },
  { _fnum_multitaskerGetTextInput, kernelMultitaskerGetTextInput,
    0, PRIVILEGE_USER },
  { _fnum_multitaskerSetTextInput, kernelMultitaskerSetTextInput,
    2, PRIVILEGE_USER },
  { _fnum_multitaskerGetTextOutput, kernelMultitaskerGetTextOutput,
    0, PRIVILEGE_USER },
  { _fnum_multitaskerSetTextOutput, kernelMultitaskerSetTextOutput,
    2, PRIVILEGE_USER },
  { _fnum_multitaskerDuplicateIO, kernelMultitaskerDuplicateIO,
    3, PRIVILEGE_USER },
  { _fnum_multitaskerGetProcessorTime, kernelMultitaskerGetProcessorTime, 
    1, PRIVILEGE_USER },
  { _fnum_multitaskerYield, kernelMultitaskerYield, 0, PRIVILEGE_USER },
  { _fnum_multitaskerWait, kernelMultitaskerWait, 1, PRIVILEGE_USER },
  { _fnum_multitaskerBlock, kernelMultitaskerBlock, 1, PRIVILEGE_USER },
  { _fnum_multitaskerDetach, kernelMultitaskerDetach, 0, PRIVILEGE_USER },
  { _fnum_multitaskerKillProcess, kernelMultitaskerKillProcess,
    2, PRIVILEGE_USER },
  { _fnum_multitaskerKillByName, kernelMultitaskerKillByName, 2,
    PRIVILEGE_USER },
  { _fnum_multitaskerTerminate, kernelMultitaskerTerminate,
    1, PRIVILEGE_USER },
  { _fnum_multitaskerDumpProcessList, kernelMultitaskerDumpProcessList,
    0, PRIVILEGE_USER }
};

static kernelFunctionIndex loaderFunctionIndex[] = {

  // Loader functions (7000-7999 range)

  { _fnum_loaderLoad, kernelLoaderLoad, 2, PRIVILEGE_USER },
  { _fnum_loaderLoadProgram, kernelLoaderLoadProgram, 4, PRIVILEGE_USER },
  { _fnum_loaderExecProgram, kernelLoaderExecProgram, 2, PRIVILEGE_USER },
  { _fnum_loaderLoadAndExec, kernelLoaderLoadAndExec,  5, PRIVILEGE_USER }
};

static kernelFunctionIndex rtcFunctionIndex[] = {

  // Real-time clock functions (8000-8999 range)

  { _fnum_rtcReadSeconds, kernelRtcReadSeconds, 0, PRIVILEGE_USER },
  { _fnum_rtcReadMinutes, kernelRtcReadMinutes, 0, PRIVILEGE_USER },
  { _fnum_rtcReadHours, kernelRtcReadHours, 0, PRIVILEGE_USER },
  { _fnum_rtcDayOfWeek, kernelRtcDayOfWeek, 3, PRIVILEGE_USER },
  { _fnum_rtcReadDayOfMonth, kernelRtcReadDayOfMonth, 0, PRIVILEGE_USER },
  { _fnum_rtcReadMonth, kernelRtcReadMonth, 0, PRIVILEGE_USER },
  { _fnum_rtcReadYear, kernelRtcReadYear, 0, PRIVILEGE_USER },
  { _fnum_rtcUptimeSeconds, kernelRtcUptimeSeconds, 0, PRIVILEGE_USER },
  { _fnum_rtcDateTime, kernelRtcDateTime, 1, PRIVILEGE_USER }
};

static kernelFunctionIndex randomFunctionIndex[] = {

  // Random number functions (9000-9999 range)

  { _fnum_randomUnformatted, kernelRandomUnformatted, 0, PRIVILEGE_USER },
  { _fnum_randomFormatted, kernelRandomFormatted, 2, PRIVILEGE_USER },
  { _fnum_randomSeededUnformatted, kernelRandomSeededUnformatted,
    1, PRIVILEGE_USER },
  { _fnum_randomSeededFormatted, kernelRandomSeededFormatted,
    3, PRIVILEGE_USER }
};

static kernelFunctionIndex environmentFunctionIndex[] = {
  
  // Environment functions (10000-10999 range)

  { _fnum_environmentGet, kernelEnvironmentGet, 3, PRIVILEGE_USER },
  { _fnum_environmentSet, kernelEnvironmentSet, 2, PRIVILEGE_USER },
  { _fnum_environmentUnset, kernelEnvironmentUnset, 1, PRIVILEGE_USER },
  { _fnum_environmentDump, kernelEnvironmentDump, 0, PRIVILEGE_USER }
};

static kernelFunctionIndex graphicFunctionIndex[] = {
  
  // Raw graphics functions (11000-11999 range)

  { _fnum_graphicsAreEnabled, kernelGraphicsAreEnabled, 0, PRIVILEGE_USER },
  { _fnum_graphicGetModes, kernelGraphicGetModes, 2, PRIVILEGE_USER },
  { _fnum_graphicGetMode, kernelGraphicGetMode, 1, PRIVILEGE_USER },
  { _fnum_graphicSetMode, kernelGraphicSetMode, 1, PRIVILEGE_SUPERVISOR },
  { _fnum_graphicGetScreenWidth, kernelGraphicGetScreenWidth,
    0, PRIVILEGE_USER },
  { _fnum_graphicGetScreenHeight, kernelGraphicGetScreenHeight,
    0, PRIVILEGE_USER },
  { _fnum_graphicCalculateAreaBytes, kernelGraphicCalculateAreaBytes,
    2, PRIVILEGE_USER },
  { _fnum_graphicClearScreen, kernelGraphicClearScreen, 1, PRIVILEGE_USER },
  { _fnum_graphicGetColor, kernelGraphicGetColor, 2, PRIVILEGE_SUPERVISOR },
  { _fnum_graphicSetColor, kernelGraphicSetColor, 2, PRIVILEGE_SUPERVISOR },
  { _fnum_graphicDrawPixel, kernelGraphicDrawPixel, 5, PRIVILEGE_USER },
  { _fnum_graphicDrawLine, kernelGraphicDrawLine, 7, PRIVILEGE_USER },
  { _fnum_graphicDrawRect, kernelGraphicDrawRect, 9, PRIVILEGE_USER },
  { _fnum_graphicDrawOval, kernelGraphicDrawOval, 9, PRIVILEGE_USER },
  { _fnum_graphicDrawImage, kernelGraphicDrawImage, 9, PRIVILEGE_USER },
  { _fnum_graphicGetImage, kernelGraphicGetImage, 6, PRIVILEGE_USER },
  { _fnum_graphicDrawText, kernelGraphicDrawText, 8, PRIVILEGE_USER },
  { _fnum_graphicCopyArea, kernelGraphicCopyArea, 7, PRIVILEGE_USER },
  { _fnum_graphicClearArea, kernelGraphicClearArea, 6, PRIVILEGE_USER },
  { _fnum_graphicRenderBuffer, kernelGraphicRenderBuffer, 7, PRIVILEGE_USER }
};

static kernelFunctionIndex windowFunctionIndex[] = {
  
  // Windowing system functions (12000-12999 range)

  { _fnum_windowLogin, kernelWindowLogin, 2, PRIVILEGE_SUPERVISOR },
  { _fnum_windowLogout, kernelWindowLogout, 0, PRIVILEGE_USER },
  { _fnum_windowNew, kernelWindowNew, 2, PRIVILEGE_USER },
  { _fnum_windowNewDialog, kernelWindowNewDialog, 2, PRIVILEGE_USER },
  { _fnum_windowDestroy, kernelWindowDestroy, 1, PRIVILEGE_USER },
  { _fnum_windowUpdateBuffer, kernelWindowUpdateBuffer, 5, PRIVILEGE_USER },
  { _fnum_windowSetTitle, kernelWindowSetTitle, 2, PRIVILEGE_USER },
  { _fnum_windowGetSize, kernelWindowGetSize, 3, PRIVILEGE_USER },
  { _fnum_windowSetSize, kernelWindowSetSize, 3, PRIVILEGE_USER },
  { _fnum_windowGetLocation, kernelWindowGetLocation, 3, PRIVILEGE_USER },
  { _fnum_windowSetLocation, kernelWindowSetLocation, 3, PRIVILEGE_USER },
  { _fnum_windowPack, kernelWindowPack, 1, PRIVILEGE_USER },
  { _fnum_windowCenter, kernelWindowCenter, 1, PRIVILEGE_USER },
  { _fnum_windowSetHasBorder, kernelWindowSetHasBorder, 2, PRIVILEGE_USER },
  { _fnum_windowSetHasTitleBar, kernelWindowSetHasTitleBar,
    2, PRIVILEGE_USER },
  { _fnum_windowSetMovable, kernelWindowSetMovable, 2, PRIVILEGE_USER },
  { _fnum_windowSetResizable, kernelWindowSetResizable, 2, PRIVILEGE_USER },
  { _fnum_windowSetPacked, kernelWindowSetPacked, 2, PRIVILEGE_USER },
  { _fnum_windowSetHasCloseButton, kernelWindowSetHasCloseButton,
    2, PRIVILEGE_USER },
  { _fnum_windowSetColors, kernelWindowSetColors, 2, PRIVILEGE_USER },
  { _fnum_windowSetVisible, kernelWindowSetVisible, 2, PRIVILEGE_USER },
  { _fnum_windowAddConsoleTextArea, kernelWindowAddConsoleTextArea,
    2, PRIVILEGE_USER },
  { _fnum_windowRedrawArea, kernelWindowRedrawArea, 4, PRIVILEGE_USER },
  { _fnum_windowProcessEvent, kernelWindowProcessEvent, 1, PRIVILEGE_USER },
  { _fnum_windowComponentEventGet, kernelWindowComponentEventGet,
    2, PRIVILEGE_USER },
  { _fnum_windowTileBackground, kernelWindowTileBackground,
    1, PRIVILEGE_USER },
  { _fnum_windowCenterBackground, kernelWindowCenterBackground,
    1, PRIVILEGE_USER },
  { _fnum_windowScreenShot, kernelWindowScreenShot, 1, PRIVILEGE_USER },
  { _fnum_windowSaveScreenShot, kernelWindowSaveScreenShot, 
    1, PRIVILEGE_USER },
  { _fnum_windowSetTextOutput, kernelWindowSetTextOutput, 1, PRIVILEGE_USER },
  { _fnum_windowComponentSetVisible, kernelWindowComponentSetVisible,
    2, PRIVILEGE_USER },
  { _fnum_windowComponentSetEnabled, kernelWindowComponentSetEnabled,
    2, PRIVILEGE_USER },
  { _fnum_windowComponentGetWidth, kernelWindowComponentGetWidth,
    1, PRIVILEGE_USER },
  { _fnum_windowComponentSetWidth, kernelWindowComponentSetWidth,
    2, PRIVILEGE_USER },
  { _fnum_windowComponentGetHeight, kernelWindowComponentGetHeight,
    1, PRIVILEGE_USER },
  { _fnum_windowComponentSetHeight, kernelWindowComponentSetHeight,
    2, PRIVILEGE_USER },
  { _fnum_windowComponentFocus, kernelWindowComponentFocus,
    1, PRIVILEGE_USER },
  { _fnum_windowComponentDraw, kernelWindowComponentDraw, 1, PRIVILEGE_USER },
  { _fnum_windowComponentGetData, kernelWindowComponentGetData,
    3, PRIVILEGE_USER },
  { _fnum_windowComponentSetData, kernelWindowComponentSetData,
    3, PRIVILEGE_USER },
  { _fnum_windowComponentGetSelected, kernelWindowComponentGetSelected,
    1, PRIVILEGE_USER },
  { _fnum_windowComponentSetSelected, kernelWindowComponentSetSelected,
    2, PRIVILEGE_USER },
  { _fnum_windowNewButton, kernelWindowNewButton, 4, PRIVILEGE_USER },
  { _fnum_windowNewCanvas, kernelWindowNewCanvas, 4, PRIVILEGE_USER },
  { _fnum_windowNewCheckbox, kernelWindowNewCheckbox, 3, PRIVILEGE_USER },
  { _fnum_windowNewContainer, kernelWindowNewContainer, 3, PRIVILEGE_USER },
  { _fnum_windowNewIcon, kernelWindowNewIcon, 5, PRIVILEGE_USER },
  { _fnum_windowNewImage, kernelWindowNewImage, 4, PRIVILEGE_USER },
  { _fnum_windowNewList, kernelWindowNewList, 7, PRIVILEGE_USER },
  { _fnum_windowNewListItem, kernelWindowNewListItem, 3, PRIVILEGE_USER },
  { _fnum_windowNewMenu, kernelWindowNewMenu, 3, PRIVILEGE_USER },
  { _fnum_windowNewMenuBar, kernelWindowNewMenuBar, 2, PRIVILEGE_USER },
  { _fnum_windowNewMenuItem, kernelWindowNewMenuItem, 3, PRIVILEGE_USER },
  { _fnum_windowNewPasswordField, kernelWindowNewPasswordField,
    3, PRIVILEGE_USER },
  { _fnum_windowNewProgressBar, kernelWindowNewProgressBar,
    2, PRIVILEGE_USER },
  { _fnum_windowNewRadioButton, kernelWindowNewRadioButton,
    6, PRIVILEGE_USER },
  { _fnum_windowNewScrollBar, kernelWindowNewScrollBar, 5, PRIVILEGE_USER },
  { _fnum_windowNewTextArea, kernelWindowNewTextArea, 5, PRIVILEGE_USER },
  { _fnum_windowNewTextField, kernelWindowNewTextField, 3, PRIVILEGE_USER },
  { _fnum_windowNewTextLabel, kernelWindowNewTextLabel, 3, PRIVILEGE_USER }
};

static kernelFunctionIndex userFunctionIndex[] = {

  // User functions (13000-13999 range)

  { _fnum_userAuthenticate, kernelUserAuthenticate, 2, PRIVILEGE_USER },
  { _fnum_userLogin, kernelUserLogin, 2, PRIVILEGE_SUPERVISOR },
  { _fnum_userLogout, kernelUserLogout, 1, PRIVILEGE_USER },
  { _fnum_userGetNames, kernelUserGetNames, 2, PRIVILEGE_USER },
  { _fnum_userAdd, kernelUserAdd, 2, PRIVILEGE_USER },
  { _fnum_userDelete, kernelUserDelete, 1, PRIVILEGE_SUPERVISOR },
  { _fnum_userSetPassword, kernelUserSetPassword, 3, PRIVILEGE_USER },
  { _fnum_userGetPrivilege, kernelUserGetPrivilege, 1, PRIVILEGE_USER },
  { _fnum_userGetPid, kernelUserGetPid, 0, PRIVILEGE_USER },
  { _fnum_userSetPid, kernelUserSetPid, 2, PRIVILEGE_SUPERVISOR }
};

static kernelFunctionIndex miscFunctionIndex[] = {

  // Miscellaneous functions (99000-99999 range)
  
  { _fnum_fontGetDefault, kernelFontGetDefault, 1, PRIVILEGE_USER },
  { _fnum_fontSetDefault, kernelFontSetDefault, 1, PRIVILEGE_USER },
  { _fnum_fontLoad, kernelFontLoad, 4, PRIVILEGE_USER },
  { _fnum_fontGetPrintedWidth, kernelFontGetPrintedWidth, 2, PRIVILEGE_USER },
  { _fnum_imageLoadBmp, kernelImageLoadBmp, 2, PRIVILEGE_USER },
  { _fnum_imageSaveBmp, kernelImageSaveBmp, 2, PRIVILEGE_USER },
  { _fnum_shutdown, kernelShutdown, 2, PRIVILEGE_USER },
  { _fnum_version, kernelVersion, 0, PRIVILEGE_USER },
  { _fnum_encryptMD5, kernelEncryptMD5, 2, PRIVILEGE_USER },
  { _fnum_lockGet, kernelLockGet, 1, PRIVILEGE_USER },
  { _fnum_lockRelease, kernelLockRelease, 1, PRIVILEGE_USER },
  { _fnum_lockVerify, kernelLockVerify, 1, PRIVILEGE_USER },
  { _fnum_variableListCreate, kernelVariableListCreate, 3, PRIVILEGE_USER },
  { _fnum_variableListGet, kernelVariableListGet, 4, PRIVILEGE_USER },
  { _fnum_variableListSet, kernelVariableListSet, 3, PRIVILEGE_USER },
  { _fnum_variableListUnset, kernelVariableListUnset, 2, PRIVILEGE_USER },
  { _fnum_configurationReader, kernelConfigurationReader, 1, PRIVILEGE_USER },
  { _fnum_configurationWriter, kernelConfigurationWriter, 2, PRIVILEGE_USER },
  { _fnum_keyboardGetMaps, kernelKeyboardGetMaps, 2, PRIVILEGE_USER },
  { _fnum_keyboardSetMap, kernelKeyboardSetMap, 1, PRIVILEGE_USER }
};

static kernelFunctionIndex *functionIndex[] = {
  miscFunctionIndex,
  textFunctionIndex,
  diskFunctionIndex,
  filesystemFunctionIndex,
  fileFunctionIndex,
  memoryFunctionIndex,
  multitaskerFunctionIndex,
  loaderFunctionIndex,
  rtcFunctionIndex,
  randomFunctionIndex,
  environmentFunctionIndex,
  graphicFunctionIndex,
  windowFunctionIndex,
  userFunctionIndex
};


static int processCall(unsigned *argList)
{
  int status = 0;
  int argCount = 0;
  unsigned functionNumber = 0;
  kernelFunctionIndex *functionEntry = NULL;
  int currentProc = 0;
  int currentPriv = 0;
  int (*functionPointer)();

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

  if ((functionNumber / 1000) == 99)
    // 'misc' functions are in spot 0
    functionEntry = &functionIndex[0][functionNumber % 1000];
  else
    functionEntry =
      &functionIndex[functionNumber / 1000][functionNumber % 1000];

  // Is there such a function?
  if ((functionEntry == NULL) ||
      (functionEntry->functionNumber != functionNumber))
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


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelApi(unsigned CS, unsigned argStart)
{
  // This is the initial entry point for the kernel's API.  This
  // function will be first the recipient of all calls to the global
  // call gate.  This function will pass a pointer to the rest of the
  // arguments to the processCall function that does all the real work.
  // This funcion does the far return.

  static int status = 0;
  static void *argument = 0;
 	
  kernelProcessorApiEnter();

  // We get a pointer to the calling function's parameters differently
  // depending on whether there was a privilege level switch.  Find
  // out by checking the privilege of the CS register pushed as
  // part of the return address.
  if (CS & PRIVILEGE_USER)
    // The caller is unprivileged, so its stack pointer is on our
    // stack just beyond IP, CS, and the flags
    argument = (void *) argStart;
  else
    // Privileged.  Same as above, but the first argument is on *our*
    // stack.
    argument = &argStart;

  status = processCall(argument);

  kernelProcessorApiExit(status);
  // Make the compiler happy -- never reached
  return (0);
}
