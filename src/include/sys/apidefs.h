//
//	Visopsys
//	Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//	apidefs.h
//

// This file defines things generic to the Visopsys kernel API, and also the
// API numbers of functions exported to userspace.

#ifndef _APIDEFS_H
#define _APIDEFS_H

// An "object key".  Really a pointer to an object in kernel memory, but
// of course not usable by applications other than as a reference
typedef volatile void * objectKey;

// This is the big list of kernel function codes.

// Text input/output functions.  All are in the 0x1000-0x1FFF range.
#define _fnum_textGetConsoleInput				0x1000
#define _fnum_textSetConsoleInput				0x1001
#define _fnum_textGetConsoleOutput				0x1002
#define _fnum_textSetConsoleOutput				0x1003
#define _fnum_textGetCurrentInput				0x1004
#define _fnum_textSetCurrentInput				0x1005
#define _fnum_textGetCurrentOutput				0x1006
#define _fnum_textSetCurrentOutput				0x1007
#define _fnum_textGetForeground					0x1008
#define _fnum_textSetForeground					0x1009
#define _fnum_textGetBackground					0x100A
#define _fnum_textSetBackground					0x100B
#define _fnum_textPutc							0x100C
#define _fnum_textPrint							0x100D
#define _fnum_textPrintAttrs					0x100E
#define _fnum_textPrintLine						0x100F
#define _fnum_textNewline						0x1010
#define _fnum_textBackSpace						0x1011
#define _fnum_textTab							0x1012
#define _fnum_textCursorUp						0x1013
#define _fnum_textCursorDown					0x1014
#define _fnum_ternelTextCursorLeft				0x1015
#define _fnum_textCursorRight					0x1016
#define _fnum_textEnableScroll					0x1017
#define _fnum_textScroll						0x1018
#define _fnum_textGetNumColumns					0x1019
#define _fnum_textGetNumRows					0x101A
#define _fnum_textGetColumn						0x101B
#define _fnum_textSetColumn						0x101C
#define _fnum_textGetRow						0x101D
#define _fnum_textSetRow						0x101E
#define _fnum_textSetCursor						0x101F
#define _fnum_textScreenClear					0x1020
#define _fnum_textScreenSave					0x1021
#define _fnum_textScreenRestore					0x1022
#define _fnum_textInputStreamCount				0x1023
#define _fnum_textInputCount					0x1024
#define _fnum_textInputStreamGetc				0x1025
#define _fnum_textInputGetc						0x1026
#define _fnum_textInputStreamReadN				0x1027
#define _fnum_textInputReadN					0x1028
#define _fnum_textInputStreamReadAll			0x1029
#define _fnum_textInputReadAll					0x102A
#define _fnum_textInputStreamAppend				0x102B
#define _fnum_textInputAppend					0x102C
#define _fnum_textInputStreamAppendN			0x102D
#define _fnum_textInputAppendN					0x102E
#define _fnum_textInputStreamRemoveAll			0x102F
#define _fnum_textInputRemoveAll				0x1030
#define _fnum_textInputStreamSetEcho			0x1031
#define _fnum_textInputSetEcho					0x1032

// Disk functions.  All are in the 0x2000-0x2FFF range.
#define _fnum_diskReadPartitions				0x2000
#define _fnum_diskReadPartitionsAll				0x2001
#define _fnum_diskSync							0x2002
#define _fnum_diskSyncAll						0x2003
#define _fnum_diskGetBoot						0x2004
#define _fnum_diskGetCount						0x2005
#define _fnum_diskGetPhysicalCount				0x2006
#define _fnum_diskGet							0x2007
#define _fnum_diskGetAll						0x2008
#define _fnum_diskGetAllPhysical				0x2009
#define _fnum_diskGetFilesystemType				0x200A
#define _fnum_diskGetMsdosPartType				0x200B
#define _fnum_diskGetMsdosPartTypes				0x200C
#define _fnum_diskGetGptPartType				0x200D
#define _fnum_diskGetGptPartTypes				0x200E
#define _fnum_diskSetFlags						0x200F
#define _fnum_diskSetLockState					0x2010
#define _fnum_diskSetDoorState					0x2011
#define _fnum_diskMediaPresent					0x2012
#define _fnum_diskReadSectors					0x2013
#define _fnum_diskWriteSectors					0x2014
#define _fnum_diskEraseSectors					0x2015
#define _fnum_diskGetStats						0x2016
#define _fnum_diskRamDiskCreate					0x2017
#define _fnum_diskRamDiskDestroy				0x2018

// Filesystem functions.  All are in the 0x3000-0x3FFF range.
#define _fnum_filesystemScan					0x3000
#define _fnum_filesystemFormat					0x3001
#define _fnum_filesystemClobber					0x3002
#define _fnum_filesystemCheck					0x3003
#define _fnum_filesystemDefragment				0x3004
#define _fnum_filesystemResizeConstraints		0x3005
#define _fnum_filesystemResize					0x3006
#define _fnum_filesystemMount					0x3007
#define _fnum_filesystemUnmount					0x3008
#define _fnum_filesystemGetFreeBytes			0x3009
#define _fnum_filesystemGetBlockSize			0x300A

// File functions.  All are in the 0x4000-0x4FFF range.
#define _fnum_fileFixupPath						0x4000
#define _fnum_fileGetDisk						0x4001
#define _fnum_fileCount							0x4002
#define _fnum_fileFirst							0x4003
#define _fnum_fileNext							0x4004
#define _fnum_fileFind							0x4005
#define _fnum_fileOpen							0x4006
#define _fnum_fileClose							0x4007
#define _fnum_fileRead							0x4008
#define _fnum_fileWrite							0x4009
#define _fnum_fileDelete						0x400A
#define _fnum_fileDeleteRecursive				0x400B
#define _fnum_fileDeleteSecure					0x400C
#define _fnum_fileMakeDir						0x400D
#define _fnum_fileRemoveDir						0x400E
#define _fnum_fileCopy							0x400F
#define _fnum_fileCopyRecursive					0x4010
#define _fnum_fileMove							0x4011
#define _fnum_fileTimestamp						0x4012
#define _fnum_fileSetSize						0x4013
#define _fnum_fileGetTempName					0x4014
#define _fnum_fileGetTemp						0x4015
#define _fnum_fileGetFullPath					0x4016
#define _fnum_fileStreamOpen					0x4017
#define _fnum_fileStreamSeek					0x4018
#define _fnum_fileStreamRead					0x4019
#define _fnum_fileStreamReadLine				0x401A
#define _fnum_fileStreamWrite					0x401B
#define _fnum_fileStreamWriteStr				0x401C
#define _fnum_fileStreamWriteLine				0x401D
#define _fnum_fileStreamFlush					0x401E
#define _fnum_fileStreamClose					0x401F
#define _fnum_fileStreamGetTemp					0x4020

// Memory manager functions. All are in the 0x5000-0x5FFF range.
#define _fnum_memoryGet							0x5000
#define _fnum_memoryRelease						0x5001
#define _fnum_memoryReleaseAllByProcId			0x5002
#define _fnum_memoryGetStats					0x5003
#define _fnum_memoryGetBlocks					0x5004

// Multitasker functions.  All are in the 0x6000-0x6FFF range.
#define _fnum_multitaskerCreateProcess			0x6000
#define _fnum_multitaskerSpawn					0x6001
#define _fnum_multitaskerGetCurrentProcessId	0x6002
#define _fnum_multitaskerGetProcess				0x6003
#define _fnum_multitaskerGetProcessByName		0x6004
#define _fnum_multitaskerGetProcesses			0x6005
#define _fnum_multitaskerSetProcessState		0x6006
#define _fnum_multitaskerProcessIsAlive			0x6007
#define _fnum_multitaskerSetProcessPriority		0x6008
#define _fnum_multitaskerGetProcessPrivilege	0x6009
#define _fnum_multitaskerGetCurrentDirectory	0x600A
#define _fnum_multitaskerSetCurrentDirectory	0x600B
#define _fnum_multitaskerGetTextInput			0x600C
#define _fnum_multitaskerSetTextInput			0x600D
#define _fnum_multitaskerGetTextOutput			0x600E
#define _fnum_multitaskerSetTextOutput			0x600F
#define _fnum_multitaskerDuplicateIo			0x6010
#define _fnum_multitaskerGetProcessorTime		0x6011
#define _fnum_multitaskerYield					0x6012
#define _fnum_multitaskerWait					0x6013
#define _fnum_multitaskerBlock					0x6014
#define _fnum_multitaskerDetach					0x6015
#define _fnum_multitaskerKillProcess			0x6016
#define _fnum_multitaskerKillByName				0x6017
#define _fnum_multitaskerTerminate				0x6018
#define _fnum_multitaskerSignalSet				0x6019
#define _fnum_multitaskerSignal					0x601A
#define _fnum_multitaskerSignalRead				0x601B
#define _fnum_multitaskerGetIoPerm				0x601C
#define _fnum_multitaskerSetIoPerm				0x601D
#define _fnum_multitaskerStackTrace				0x601E

// Loader functions.  All are in the 0x7000-0x7FFF range.
#define _fnum_loaderLoad						0x7000
#define _fnum_loaderClassify					0x7001
#define _fnum_loaderClassifyFile				0x7002
#define _fnum_loaderGetSymbols					0x7003
#define _fnum_loaderCheckCommand				0x7004
#define _fnum_loaderLoadProgram					0x7005
#define _fnum_loaderLoadLibrary					0x7006
#define _fnum_loaderGetLibrary					0x7007
#define _fnum_loaderLinkLibrary					0x7008
#define _fnum_loaderGetSymbol					0x7009
#define _fnum_loaderExecProgram					0x700A
#define _fnum_loaderLoadAndExec					0x700B

// Real-time clock functions.  All are in the 0x8000-0x8FFF range.
#define _fnum_rtcReadSeconds					0x8000
#define _fnum_rtcReadMinutes					0x8001
#define _fnum_rtcReadHours						0x8002
#define _fnum_rtcDayOfWeek						0x8003
#define _fnum_rtcReadDayOfMonth					0x8004
#define _fnum_rtcReadMonth						0x8005
#define _fnum_rtcReadYear						0x8006
#define _fnum_rtcUptimeSeconds					0x8007
#define _fnum_rtcDateTime						0x8008

// Random number functions.	All are in the 9000-9999 range.
#define _fnum_randomUnformatted					0x9000
#define _fnum_randomFormatted					0x9001
#define _fnum_randomSeededUnformatted			0x9002
#define _fnum_randomSeededFormatted				0x9003
#define _fnum_randomBytes						0x9004

// Former variable list functions were in the 0xA000-0xAFFF range.

// Environment functions.  All are in the 0xB000-0xBFFF range.
#define _fnum_environmentGet					0xB000
#define _fnum_environmentSet					0xB001
#define _fnum_environmentUnset					0xB002
#define _fnum_environmentDump					0xB003

// Raw graphics drawing functions.  All are in the 0xC000-0xCFFF range.
#define _fnum_graphicsAreEnabled				0xC000
#define _fnum_graphicGetModes					0xC001
#define _fnum_graphicGetMode					0xC002
#define _fnum_graphicSetMode					0xC003
#define _fnum_graphicGetScreenWidth				0xC004
#define _fnum_graphicGetScreenHeight			0xC005
#define _fnum_graphicCalculateAreaBytes			0xC006
#define _fnum_graphicClearScreen				0xC007
#define _fnum_graphicDrawPixel					0xC008
#define _fnum_graphicDrawLine					0xC009
#define _fnum_graphicDrawRect					0xC00A
#define _fnum_graphicDrawOval					0xC00B
#define _fnum_graphicGetImage					0xC00C
#define _fnum_graphicDrawImage					0xC00D
#define _fnum_graphicDrawText					0xC00E
#define _fnum_graphicCopyArea					0xC00F
#define _fnum_graphicClearArea					0xC010
#define _fnum_graphicRenderBuffer				0xC011

// Image functions  All are in the 0xD000-0xDFFF range.
#define _fnum_imageNew							0xD000
#define _fnum_imageFree							0xD001
#define _fnum_imageLoad							0xD002
#define _fnum_imageSave							0xD003
#define _fnum_imageResize						0xD004
#define _fnum_imageCopy							0xD005
#define _fnum_imageFill							0xD006
#define _fnum_imagePaste						0xD007

// Font functions  All are in the 0xE000-0xEFFF range.
#define _fnum_fontGetSystem						0xE000
#define _fnum_fontGet							0xE001
#define _fnum_fontGetPrintedWidth				0xE002
#define _fnum_fontGetWidth						0xE003
#define _fnum_fontGetHeight						0xE004

// Windowing system functions.  All are in the 0xF000-0xFFFF range.
#define _fnum_windowLogin						0xF000
#define _fnum_windowLogout						0xF001
#define _fnum_windowNew							0xF002
#define _fnum_windowNewDialog					0xF003
#define _fnum_windowDestroy						0xF004
#define _fnum_windowGetList						0xF005
#define _fnum_windowGetInfo						0xF006
#define _fnum_windowSetCharSet					0xF007
#define _fnum_windowSetTitle					0xF008
#define _fnum_windowSetSize						0xF009
#define _fnum_windowSetLocation					0xF00A
#define _fnum_windowCenter						0xF00B
#define _fnum_windowSnapIcons					0xF00C
#define _fnum_windowSetHasBorder				0xF00D
#define _fnum_windowSetHasTitleBar				0xF00E
#define _fnum_windowSetMovable					0xF00F
#define _fnum_windowSetResizable				0xF010
#define _fnum_windowSetFocusable				0xF011
#define _fnum_windowSetRoot						0xF012
#define _fnum_windowRemoveMinimizeButton		0xF013
#define _fnum_windowRemoveCloseButton			0xF014
#define _fnum_windowSetVisible					0xF015
#define _fnum_windowSetMinimized				0xF016
#define _fnum_windowAddConsoleTextArea			0xF017
#define _fnum_windowGetColor					0xF018
#define _fnum_windowSetColor					0xF019
#define _fnum_windowResetColors					0xF01A
#define _fnum_windowComponentEventGet			0xF01B
#define _fnum_windowSetBackgroundColor			0xF01C
#define _fnum_windowSetBackgroundImage			0xF01D
#define _fnum_windowShellTileBackground			0xF01E
#define _fnum_windowShellCenterBackground		0xF01F
#define _fnum_windowShellNewTaskbarIcon			0xF020
#define _fnum_windowShellNewTaskbarTextLabel	0xF021
#define _fnum_windowShellDestroyTaskbarComp		0xF022
#define _fnum_windowShellIconify				0xF023
#define _fnum_windowScreenShot					0xF024
#define _fnum_windowSaveScreenShot				0xF025
#define _fnum_windowSetTextOutput				0xF026
#define _fnum_windowLayout						0xF027
#define _fnum_windowDebugLayout					0xF028
#define _fnum_windowContextSet					0xF029
#define _fnum_windowSwitchPointer				0xF02A
#define _fnum_windowToggleMenuBar				0xF02B
#define _fnum_windowRefresh						0xF02C
#define _fnum_windowComponentDestroy			0xF02D
#define _fnum_windowComponentSetCharSet			0xF02E
#define _fnum_windowComponentSetVisible			0xF02F
#define _fnum_windowComponentSetEnabled			0xF030
#define _fnum_windowComponentGetWidth			0xF031
#define _fnum_windowComponentSetWidth			0xF032
#define _fnum_windowComponentGetHeight			0xF033
#define _fnum_windowComponentSetHeight			0xF034
#define _fnum_windowComponentFocus				0xF035
#define _fnum_windowComponentUnfocus			0xF036
#define _fnum_windowComponentLayout				0xF037
#define _fnum_windowComponentDraw				0xF038
#define _fnum_windowComponentGetData			0xF039
#define _fnum_windowComponentSetData			0xF03A
#define _fnum_windowComponentAppendData			0xF03B
#define _fnum_windowComponentGetSelected		0xF03C
#define _fnum_windowComponentSetSelected		0xF03D
#define _fnum_windowNewButton					0xF03E
#define _fnum_windowNewCanvas					0xF03F
#define _fnum_windowNewCheckbox					0xF040
#define _fnum_windowNewContainer				0xF041
#define _fnum_windowNewDivider					0xF042
#define _fnum_windowNewIcon						0xF043
#define _fnum_windowNewImage					0xF044
#define _fnum_windowNewList						0xF045
#define _fnum_windowNewListItem					0xF046
#define _fnum_windowNewMenu						0xF047
#define _fnum_windowNewMenuBar					0xF048
#define _fnum_windowNewMenuBarIcon				0xF049
#define _fnum_windowNewMenuItem					0xF04A
#define _fnum_windowNewPasswordField			0xF04B
#define _fnum_windowNewProgressBar				0xF04C
#define _fnum_windowNewRadioButton				0xF04D
#define _fnum_windowNewScrollBar				0xF04E
#define _fnum_windowNewSlider					0xF04F
#define _fnum_windowNewTextArea					0xF050
#define _fnum_windowNewTextField				0xF051
#define _fnum_windowNewTextLabel				0xF052
#define _fnum_windowNewTree						0xF053
#define _fnum_windowMenuUpdate					0xF054
#define _fnum_windowMenuDestroy					0xF055

// User functions.  All are in the 0x10000-0x10FFF range.
#define _fnum_userAuthenticate					0x10000
#define _fnum_userLogin							0x10001
#define _fnum_userLogout						0x10002
#define _fnum_userExists						0x10003
#define _fnum_userGetNames						0x10004
#define _fnum_userAdd							0x10005
#define _fnum_userDelete						0x10006
#define _fnum_userSetPassword					0x10007
#define _fnum_userGetCurrent					0x10008
#define _fnum_userGetPrivilege					0x10009
#define _fnum_userGetSessions					0x1000A
#define _fnum_userFileAdd						0x1000B
#define _fnum_userFileDelete					0x1000C
#define _fnum_userFileSetPassword				0x1000D

// Network functions.  All are in the 0x11000-0x11FFF range.
#define _fnum_networkEnabled					0x11000
#define _fnum_networkEnable						0x11001
#define _fnum_networkDisable					0x11002
#define _fnum_networkOpen						0x11003
#define _fnum_networkClose						0x11004
#define _fnum_networkConnectionGetCount			0x11005
#define _fnum_networkConnectionGetAll			0x11006
#define _fnum_networkCount						0x11007
#define _fnum_networkRead						0x11008
#define _fnum_networkWrite						0x11009
#define _fnum_networkPing						0x1100A
#define _fnum_networkGetHostName				0x1100B
#define _fnum_networkSetHostName				0x1100C
#define _fnum_networkGetDomainName				0x1100D
#define _fnum_networkSetDomainName				0x1100E
#define _fnum_networkDeviceEnable				0x1100F
#define _fnum_networkDeviceDisable				0x11010
#define _fnum_networkDeviceGetCount				0x11011
#define _fnum_networkDeviceGet					0x11012
#define _fnum_networkDeviceHook					0x11013
#define _fnum_networkDeviceUnhook				0x11014
#define _fnum_networkDeviceSniff				0x11015

// Miscellaneous functions.  All are in the 0xFF000-0xFFFFF range.
#define _fnum_systemShutdown					0xFF000
#define _fnum_getVersion						0xFF001
#define _fnum_systemInfo						0xFF002
#define _fnum_cryptHashMd5						0xFF003
#define _fnum_lockGet							0xFF004
#define _fnum_lockRelease						0xFF005
#define _fnum_lockVerify						0xFF006
#define _fnum_configRead						0xFF007
#define _fnum_configWrite						0xFF008
#define _fnum_configGet							0xFF009
#define _fnum_configSet							0xFF00A
#define _fnum_configUnset						0xFF00B
#define _fnum_guidGenerate						0xFF00C
#define _fnum_crc32								0xFF00D
#define _fnum_keyboardGetMap					0xFF00E
#define _fnum_keyboardSetMap					0xFF00F
#define _fnum_keyboardVirtualInput				0xFF010
#define _fnum_deviceTreeGetRoot					0xFF011
#define _fnum_deviceTreeGetChild				0xFF012
#define _fnum_deviceTreeGetNext					0xFF013
#define _fnum_mouseLoadPointer					0xFF014
#define _fnum_pageGetPhysical					0xFF015
#define _fnum_charsetToUnicode					0xFF016
#define _fnum_charsetFromUnicode				0xFF017
#define _fnum_cpuGetMs							0xFF018
#define _fnum_cpuSpinMs							0xFF019
#define _fnum_touchAvailable					0xFF01A

#endif

