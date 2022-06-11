// 
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  api.h
//

// This file describes all of the functions that are directly exported by
// the Visopsys kernel to the outside world.  All functions and their
// numbers are listed here, as well as macros needed to perform call-gate
// calls into the kernel.  Also, each exported kernel function is represented
// here in the form of a little inline function.

#if !defined(_API_H)

// This file should mostly never be included when we're compiling a kernel
// file (kernelApi.c is an exception)
#if defined(KERNEL)
#error "You cannot call the kernel API from within a kernel function"
#endif

#ifndef _X_
#define _X_
#endif

#include <time.h>
#include <sys/cdefs.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/file.h>
#include <sys/guid.h>
#include <sys/image.h>
#include <sys/keyboard.h>
#include <sys/loader.h>
#include <sys/lock.h>
#include <sys/memory.h>
#include <sys/network.h>
#include <sys/process.h>
#include <sys/progress.h>
#include <sys/stream.h>
#include <sys/text.h>
#include <sys/utsname.h>
#include <sys/variable.h>
#include <sys/window.h>

// Included in the Visopsys standard library to prevent API calls from
// within kernel code.
extern int visopsys_in_kernel;

// This is the big list of kernel function codes.

// Text input/output functions.  All are in the 1000-1999 range.
#define _fnum_textGetConsoleInput                    1000
#define _fnum_textSetConsoleInput                    1001
#define _fnum_textGetConsoleOutput                   1002
#define _fnum_textSetConsoleOutput                   1003
#define _fnum_textGetCurrentInput                    1004
#define _fnum_textSetCurrentInput                    1005
#define _fnum_textGetCurrentOutput                   1006
#define _fnum_textSetCurrentOutput                   1007
#define _fnum_textGetForeground                      1008
#define _fnum_textSetForeground                      1009
#define _fnum_textGetBackground                      1010
#define _fnum_textSetBackground                      1011
#define _fnum_textPutc                               1012
#define _fnum_textPrint                              1013
#define _fnum_textPrintAttrs                         1014
#define _fnum_textPrintLine                          1015
#define _fnum_textNewline                            1016
#define _fnum_textBackSpace                          1017
#define _fnum_textTab                                1018
#define _fnum_textCursorUp                           1019
#define _fnum_textCursorDown                         1020
#define _fnum_ternelTextCursorLeft                   1021
#define _fnum_textCursorRight                        1022
#define _fnum_textEnableScroll                       1023
#define _fnum_textScroll                             1024
#define _fnum_textGetNumColumns                      1025
#define _fnum_textGetNumRows                         1026
#define _fnum_textGetColumn                          1027
#define _fnum_textSetColumn                          1028
#define _fnum_textGetRow                             1029
#define _fnum_textSetRow                             1030
#define _fnum_textSetCursor                          1031
#define _fnum_textScreenClear                        1032
#define _fnum_textScreenSave                         1033
#define _fnum_textScreenRestore                      1034
#define _fnum_textInputStreamCount                   1035
#define _fnum_textInputCount                         1036
#define _fnum_textInputStreamGetc                    1037
#define _fnum_textInputGetc                          1038
#define _fnum_textInputStreamReadN                   1039
#define _fnum_textInputReadN                         1040
#define _fnum_textInputStreamReadAll                 1041
#define _fnum_textInputReadAll                       1042
#define _fnum_textInputStreamAppend                  1043
#define _fnum_textInputAppend                        1044
#define _fnum_textInputStreamAppendN                 1045
#define _fnum_textInputAppendN                       1046
#define _fnum_textInputStreamRemove                  1047
#define _fnum_textInputRemove                        1048
#define _fnum_textInputStreamRemoveN                 1049
#define _fnum_textInputRemoveN                       1050
#define _fnum_textInputStreamRemoveAll               1051
#define _fnum_textInputRemoveAll                     1052
#define _fnum_textInputStreamSetEcho                 1053
#define _fnum_textInputSetEcho                       1054

// Disk functions.  All are in the 2000-2999 range.
#define _fnum_diskReadPartitions                     2000
#define _fnum_diskReadPartitionsAll                  2001
#define _fnum_diskSync                               2002
#define _fnum_diskSyncAll                            2003
#define _fnum_diskGetBoot                            2004
#define _fnum_diskGetCount                           2005
#define _fnum_diskGetPhysicalCount                   2006
#define _fnum_diskGet                                2007
#define _fnum_diskGetAll                             2008
#define _fnum_diskGetAllPhysical                     2009
#define _fnum_diskGetFilesystemType                  2010
#define _fnum_diskGetMsdosPartType                   2011
#define _fnum_diskGetMsdosPartTypes                  2012
#define _fnum_diskGetGptPartType                     2013
#define _fnum_diskGetGptPartTypes                    2014
#define _fnum_diskSetFlags                           2015
#define _fnum_diskSetLockState                       2016
#define _fnum_diskSetDoorState                       2017
#define _fnum_diskGetMediaState                      2018
#define _fnum_diskReadSectors                        2019
#define _fnum_diskWriteSectors                       2020
#define _fnum_diskEraseSectors                       2021
#define _fnum_diskGetStats                           2022
#define _fnum_diskRamDiskCreate                      2023
#define _fnum_diskRamDiskDestroy                     2024

// Filesystem functions.  All are in the 3000-3999 range.
#define _fnum_filesystemFormat                       3000
#define _fnum_filesystemClobber                      3001
#define _fnum_filesystemCheck                        3002
#define _fnum_filesystemDefragment                   3003
#define _fnum_filesystemResizeConstraints            3004
#define _fnum_filesystemResize                       3005
#define _fnum_filesystemMount                        3006
#define _fnum_filesystemUnmount                      3007
#define _fnum_filesystemGetFreeBytes                 3008
#define _fnum_filesystemGetBlockSize                 3009

// File functions.  All are in the 4000-4999 range.
#define _fnum_fileFixupPath                          4000
#define _fnum_fileGetDisk                            4001
#define _fnum_fileCount                              4002
#define _fnum_fileFirst                              4003
#define _fnum_fileNext                               4004
#define _fnum_fileFind                               4005
#define _fnum_fileOpen                               4006
#define _fnum_fileClose                              4007
#define _fnum_fileRead                               4008
#define _fnum_fileWrite                              4009
#define _fnum_fileDelete                             4010
#define _fnum_fileDeleteRecursive                    4011
#define _fnum_fileDeleteSecure                       4012
#define _fnum_fileMakeDir                            4013
#define _fnum_fileRemoveDir                          4014
#define _fnum_fileCopy                               4015
#define _fnum_fileCopyRecursive                      4016
#define _fnum_fileMove                               4017
#define _fnum_fileTimestamp                          4018
#define _fnum_fileSetSize                            4019
#define _fnum_fileGetTemp                            4020
#define _fnum_fileGetFullPath                        4021
#define _fnum_fileStreamOpen                         4022
#define _fnum_fileStreamSeek                         4023
#define _fnum_fileStreamRead                         4024
#define _fnum_fileStreamReadLine                     4025
#define _fnum_fileStreamWrite                        4026
#define _fnum_fileStreamWriteStr                     4027
#define _fnum_fileStreamWriteLine                    4028
#define _fnum_fileStreamFlush                        4029
#define _fnum_fileStreamClose                        4030
#define _fnum_fileStreamGetTemp                      4031

// Memory manager functions.  All are in the 5000-5999 range.
#define _fnum_memoryGet                              5000
#define _fnum_memoryRelease                          5001
#define _fnum_memoryReleaseAllByProcId               5002
#define _fnum_memoryGetStats                         5003
#define _fnum_memoryGetBlocks                        5004
#define _fnum_memoryBlockInfo                        5005

// Multitasker functions.  All are in the 6000-6999 range.
#define _fnum_multitaskerCreateProcess               6000
#define _fnum_multitaskerSpawn                       6001
#define _fnum_multitaskerGetCurrentProcessId         6002
#define _fnum_multitaskerGetProcess                  6003
#define _fnum_multitaskerGetProcessByName            6004
#define _fnum_multitaskerGetProcesses                6005
#define _fnum_multitaskerSetProcessState             6006
#define _fnum_multitaskerProcessIsAlive              6007
#define _fnum_multitaskerSetProcessPriority          6008
#define _fnum_multitaskerGetProcessPrivilege         6009
#define _fnum_multitaskerGetCurrentDirectory         6010
#define _fnum_multitaskerSetCurrentDirectory         6011
#define _fnum_multitaskerGetTextInput                6012
#define _fnum_multitaskerSetTextInput                6013
#define _fnum_multitaskerGetTextOutput               6014
#define _fnum_multitaskerSetTextOutput               6015
#define _fnum_multitaskerDuplicateIO                 6016
#define _fnum_multitaskerGetProcessorTime            6017
#define _fnum_multitaskerYield                       6018
#define _fnum_multitaskerWait                        6019
#define _fnum_multitaskerBlock                       6020
#define _fnum_multitaskerDetach                      6021
#define _fnum_multitaskerKillProcess                 6022
#define _fnum_multitaskerKillByName                  6023
#define _fnum_multitaskerTerminate                   6024
#define _fnum_multitaskerSignalSet                   6025
#define _fnum_multitaskerSignal                      6026
#define _fnum_multitaskerSignalRead                  6027
#define _fnum_multitaskerGetIOPerm                   6028
#define _fnum_multitaskerSetIOPerm                   6029
#define _fnum_multitaskerStackTrace                  6030

// Loader functions.  All are in the 7000-7999 range.
#define _fnum_loaderLoad                             7000
#define _fnum_loaderClassify                         7001
#define _fnum_loaderClassifyFile                     7002
#define _fnum_loaderGetSymbols                       7003
#define _fnum_loaderCheckCommand                     7004
#define _fnum_loaderLoadProgram                      7005
#define _fnum_loaderLoadLibrary                      7006
#define _fnum_loaderGetLibrary                       7007
#define _fnum_loaderLinkLibrary                      7008
#define _fnum_loaderGetSymbol                        7009
#define _fnum_loaderExecProgram                      7010
#define _fnum_loaderLoadAndExec                      7011

// Real-time clock functions.  All are in the 8000-8999 range.
#define _fnum_rtcReadSeconds                         8000
#define _fnum_rtcReadMinutes                         8001
#define _fnum_rtcReadHours                           8002
#define _fnum_rtcDayOfWeek                           8003
#define _fnum_rtcReadDayOfMonth                      8004
#define _fnum_rtcReadMonth                           8005
#define _fnum_rtcReadYear                            8006
#define _fnum_rtcUptimeSeconds                       8007
#define _fnum_rtcDateTime                            8008

// Random number functions.  All are in the 9000-9999 range.
#define _fnum_randomUnformatted                      9000
#define _fnum_randomFormatted                        9001
#define _fnum_randomSeededUnformatted                9002
#define _fnum_randomSeededFormatted                  9003
#define _fnum_randomBytes                            9004

// Environment functions.  All are in the 10000-10999 range.
#define _fnum_environmentGet                         10000
#define _fnum_environmentSet                         10001
#define _fnum_environmentUnset                       10002
#define _fnum_environmentDump                        10003

// Raw graphics drawing functions.  All are in the 11000-11999 range
#define _fnum_graphicsAreEnabled                     11000
#define _fnum_graphicGetModes                        11001
#define _fnum_graphicGetMode                         11002
#define _fnum_graphicSetMode                         11003
#define _fnum_graphicGetScreenWidth                  11004
#define _fnum_graphicGetScreenHeight                 11005
#define _fnum_graphicCalculateAreaBytes              11006
#define _fnum_graphicClearScreen                     11007
#define _fnum_graphicDrawPixel                       11008
#define _fnum_graphicDrawLine                        11009
#define _fnum_graphicDrawRect                        11010
#define _fnum_graphicDrawOval                        11011
#define _fnum_graphicGetImage                        11012
#define _fnum_graphicDrawImage                       11013
#define _fnum_graphicDrawText                        11014
#define _fnum_graphicCopyArea                        11015
#define _fnum_graphicClearArea                       11016
#define _fnum_graphicRenderBuffer                    11017

// Windowing system functions.  All are in the 12000-12999 range
#define _fnum_windowLogin                            12000
#define _fnum_windowLogout                           12001
#define _fnum_windowNew                              12002
#define _fnum_windowNewDialog                        12003
#define _fnum_windowDestroy                          12004
#define _fnum_windowUpdateBuffer                     12005
#define _fnum_windowSetTitle                         12006
#define _fnum_windowGetSize                          12007
#define _fnum_windowSetSize                          12008
#define _fnum_windowGetLocation                      12009
#define _fnum_windowSetLocation                      12010
#define _fnum_windowCenter                           12011
#define _fnum_windowSnapIcons                        12012
#define _fnum_windowSetHasBorder                     12013
#define _fnum_windowSetHasTitleBar                   12014
#define _fnum_windowSetMovable                       12015
#define _fnum_windowSetResizable                     12016
#define _fnum_windowRemoveMinimizeButton             12017
#define _fnum_windowRemoveCloseButton                12018
#define _fnum_windowSetVisible                       12019
#define _fnum_windowSetMinimized                     12020 
#define _fnum_windowAddConsoleTextArea               12021
#define _fnum_windowRedrawArea                       12022
#define _fnum_windowDrawAll                          12023
#define _fnum_windowGetColor                         12024
#define _fnum_windowSetColor                         12025
#define _fnum_windowResetColors                      12026
#define _fnum_windowProcessEvent                     12027
#define _fnum_windowComponentEventGet                12028
#define _fnum_windowSetBackgroundColor               12029
#define _fnum_windowTileBackground                   12030
#define _fnum_windowCenterBackground                 12031
#define _fnum_windowScreenShot                       12032
#define _fnum_windowSaveScreenShot                   12033
#define _fnum_windowSetTextOutput                    12034
#define _fnum_windowLayout                           12035
#define _fnum_windowDebugLayout                      12036
#define _fnum_windowContextAdd                       12037
#define _fnum_windowContextSet                       12038
#define _fnum_windowSwitchPointer                    12039
#define _fnum_windowComponentDestroy                 12040
#define _fnum_windowComponentSetVisible              12041
#define _fnum_windowComponentSetEnabled              12042
#define _fnum_windowComponentGetWidth                12043
#define _fnum_windowComponentSetWidth                12044
#define _fnum_windowComponentGetHeight               12045
#define _fnum_windowComponentSetHeight               12046
#define _fnum_windowComponentFocus                   12047
#define _fnum_windowComponentUnfocus                 12048
#define _fnum_windowComponentDraw                    12049
#define _fnum_windowComponentGetData                 12050
#define _fnum_windowComponentSetData                 12051
#define _fnum_windowComponentGetSelected             12052
#define _fnum_windowComponentSetSelected             12053
#define _fnum_windowNewButton                        12054
#define _fnum_windowNewCanvas                        12055
#define _fnum_windowNewCheckbox                      12056
#define _fnum_windowNewContainer                     12057
#define _fnum_windowNewDivider                       12058
#define _fnum_windowNewIcon                          12059
#define _fnum_windowNewImage                         12060
#define _fnum_windowNewList                          12061
#define _fnum_windowNewListItem                      12062
#define _fnum_windowNewMenu                          12063
#define _fnum_windowNewMenuBar                       12064
#define _fnum_windowNewMenuItem                      12065
#define _fnum_windowNewPasswordField                 12066
#define _fnum_windowNewProgressBar                   12067
#define _fnum_windowNewRadioButton                   12068
#define _fnum_windowNewScrollBar                     12069
#define _fnum_windowNewSlider                        12070
#define _fnum_windowNewTextArea                      12071
#define _fnum_windowNewTextField                     12072
#define _fnum_windowNewTextLabel                     12073

// User functions.  All are in the 13000-13999 range
#define _fnum_userAuthenticate                       13000
#define _fnum_userLogin                              13001
#define _fnum_userLogout                             13002
#define _fnum_userGetNames                           13003
#define _fnum_userAdd                                13004
#define _fnum_userDelete                             13005
#define _fnum_userSetPassword                        13006
#define _fnum_userGetPrivilege                       13007
#define _fnum_userGetPid                             13008
#define _fnum_userSetPid                             13009
#define _fnum_userFileAdd                            13010
#define _fnum_userFileDelete                         13011
#define _fnum_userFileSetPassword                    13012

// Network functions.  All are in the 14000-14999 range
#define _fnum_networkDeviceGetCount                  14000
#define _fnum_networkDeviceGet                       14001
#define _fnum_networkInitialized                     14002
#define _fnum_networkInitialize                      14003
#define _fnum_networkShutdown                        14004
#define _fnum_networkOpen                            14005
#define _fnum_networkClose                           14006
#define _fnum_networkCount                           14007
#define _fnum_networkRead                            14008
#define _fnum_networkWrite                           14009
#define _fnum_networkPing                            14010
#define _fnum_networkGetHostName                     14011
#define _fnum_networkSetHostName                     14012
#define _fnum_networkGetDomainName                   14013
#define _fnum_networkSetDomainName                   14014

// Miscellaneous functions.  All are in the 99000-99999 range
#define _fnum_fontGetDefault                         99000
#define _fnum_fontSetDefault                         99001
#define _fnum_fontLoad                               99002
#define _fnum_fontGetPrintedWidth                    99003
#define _fnum_fontGetWidth                           99004
#define _fnum_fontGetHeight                          99005
#define _fnum_imageNew                               99006
#define _fnum_imageFree                              99007
#define _fnum_imageLoad                              99008
#define _fnum_imageSave                              99009
#define _fnum_imageResize                            99010
#define _fnum_imageCopy                              99011
#define _fnum_shutdown                               99012
#define _fnum_getVersion                             99013
#define _fnum_systemInfo                             99014
#define _fnum_encryptMD5                             99015
#define _fnum_lockGet                                99016
#define _fnum_lockRelease                            99017
#define _fnum_lockVerify                             99018
#define _fnum_variableListCreate                     99019
#define _fnum_variableListDestroy                    99020
#define _fnum_variableListGet                        99021
#define _fnum_variableListSet                        99022
#define _fnum_variableListUnset                      99023
#define _fnum_configRead                             99024
#define _fnum_configWrite                            99025
#define _fnum_configGet                              99026
#define _fnum_configSet                              99027
#define _fnum_configUnset                            99028
#define _fnum_guidGenerate                           99029
#define _fnum_crc32                                  99030
#define _fnum_keyboardGetMap                         99031
#define _fnum_keyboardSetMap                         99032
#define _fnum_deviceTreeGetCount                     99033
#define _fnum_deviceTreeGetRoot                      99034
#define _fnum_deviceTreeGetChild                     99035
#define _fnum_deviceTreeGetNext                      99036
#define _fnum_mouseLoadPointer                       99037
#define _fnum_pageGetPhysical                        99038
#define _fnum_setLicensed                            99039


//
// Text input/output functions
//
objectKey textGetConsoleInput(void);
int textSetConsoleInput(objectKey);
objectKey textGetConsoleOutput(void);
int textSetConsoleOutput(objectKey);
objectKey textGetCurrentInput(void);
int textSetCurrentInput(objectKey);
objectKey textGetCurrentOutput(void);
int textSetCurrentOutput(objectKey);
int textGetForeground(color *);
int textSetForeground(color *);
int textGetBackground(color *);
int textSetBackground(color *);
int textPutc(int);
int textPrint(const char *);
int textPrintAttrs(textAttrs *, const char *);
int textPrintLine(const char *);
void textNewline(void);
int textBackSpace(void);
int textTab(void);
int textCursorUp(void);
int textCursorDown(void);
int textCursorLeft(void);
int textCursorRight(void);
int textEnableScroll(int);
void textScroll(int);
int textGetNumColumns(void);
int textGetNumRows(void);
int textGetColumn(void);
void textSetColumn(int);
int textGetRow(void);
void textSetRow(int);
void textSetCursor(int);
int textScreenClear(void);
int textScreenSave(textScreen *);
int textScreenRestore(textScreen *);
int textInputStreamCount(objectKey);
int textInputCount(void);
int textInputStreamGetc(objectKey, char *);
int textInputGetc(char *);
int textInputStreamReadN(objectKey, int, char *);
int textInputReadN(int, char *);
int textInputStreamReadAll(objectKey, char *);
int textInputReadAll(char *);
int textInputStreamAppend(objectKey, int);
int textInputAppend(int);
int textInputStreamAppendN(objectKey, int, char *);
int textInputAppendN(int, char *);
int textInputStreamRemove(objectKey);
int textInputRemove(void);
int textInputStreamRemoveN(objectKey, int);
int textInputRemoveN(int);
int textInputStreamRemoveAll(objectKey);
int textInputRemoveAll(void);
void textInputStreamSetEcho(objectKey, int);
void textInputSetEcho(int);

// 
// Disk functions
//
int diskReadPartitions(const char *);
int diskReadPartitionsAll(void);
int diskSync(const char *);
int diskSyncAll(void);
int diskGetBoot(char *);
int diskGetCount(void);
int diskGetPhysicalCount(void);
int diskGet(const char *, disk *);
int diskGetAll(disk *, unsigned);
int diskGetAllPhysical(disk *, unsigned);
int diskGetFilesystemType(const char *, char *, unsigned);
int diskGetMsdosPartType(int, msdosPartType *);
msdosPartType *diskGetMsdosPartTypes(void);
int diskGetGptPartType(guid *, gptPartType *);
gptPartType *diskGetGptPartTypes(void);
int diskSetFlags(const char *, unsigned, int);
int diskSetLockState(const char *, int);
int diskSetDoorState(const char *, int);
int diskGetMediaState(const char *);
int diskReadSectors(const char *, uquad_t, uquad_t, void *);
int diskWriteSectors(const char *, uquad_t, uquad_t, const void *);
int diskEraseSectors(const char *, uquad_t, uquad_t, int);
int diskGetStats(const char *, diskStats *);
int diskRamDiskCreate(unsigned, char *);
int diskRamDiskDestroy(const char *);

//
// Filesystem functions
//
int filesystemFormat(const char *, const char *, const char *, int,
		     progress *);
int filesystemClobber(const char *);
int filesystemCheck(const char *, int, int, progress *);
int filesystemDefragment(const char *, progress *);
int filesystemResizeConstraints(const char *, uquad_t *, uquad_t *,
				progress *);
int filesystemResize(const char *, uquad_t, progress *);
int filesystemMount(const char *, const char *);
int filesystemUnmount(const char *);
uquad_t filesystemGetFreeBytes(const char *);
unsigned filesystemGetBlockSize(const char *);

//
// File functions
//
int fileFixupPath(const char *, char *);
int fileGetDisk(const char *, disk *);
int fileCount(const char *);
int fileFirst(const char *, file *);
int fileNext(const char *, file *);
int fileFind(const char *, file *);
int fileOpen(const char *, int, file *);
int fileClose(file *);
int fileRead(file *, unsigned, unsigned, void *);
int fileWrite(file *, unsigned, unsigned, void *);
int fileDelete(const char *);
int fileDeleteRecursive(const char *);
int fileDeleteSecure(const char *, int);
int fileMakeDir(const char *);
int fileRemoveDir(const char *);
int fileCopy(const char *, const char *);
int fileCopyRecursive(const char *, const char *);
int fileMove(const char *, const char *);
int fileTimestamp(const char *);
int fileSetSize(file *, unsigned);
int fileGetTemp(file *);
int fileGetFullPath(file *, char *, int);
int fileStreamOpen(const char *, int, fileStream *);
int fileStreamSeek(fileStream *, unsigned);
int fileStreamRead(fileStream *, unsigned, char *);
int fileStreamReadLine(fileStream *, unsigned, char *);
int fileStreamWrite(fileStream *, unsigned, const char *);
int fileStreamWriteStr(fileStream *, const char *);
int fileStreamWriteLine(fileStream *, const char *);
int fileStreamFlush(fileStream *);
int fileStreamClose(fileStream *);
int fileStreamGetTemp(fileStream *);

//
// Memory functions
//
void *memoryGet(unsigned, const char *);
int memoryRelease(void *);
int memoryReleaseAllByProcId(int);
int memoryGetStats(memoryStats *, int);
int memoryGetBlocks(memoryBlock *, unsigned, int);
int memoryBlockInfo(void *, memoryBlock *);

//
// Multitasker functions
//
int multitaskerCreateProcess(const char *, int, processImage *);
int multitaskerSpawn(void *, const char *, int, void *[]);
int multitaskerGetCurrentProcessId(void);
int multitaskerGetProcess(int, process *);
int multitaskerGetProcessByName(const char *, process *);
int multitaskerGetProcesses(void *, unsigned);
int multitaskerSetProcessState(int, int);
int multitaskerProcessIsAlive(int);
int multitaskerSetProcessPriority(int, int);
int multitaskerGetProcessPrivilege(int);
int multitaskerGetCurrentDirectory(char *, int);
int multitaskerSetCurrentDirectory(const char *);
objectKey multitaskerGetTextInput(void);
int multitaskerSetTextInput(int, objectKey);
objectKey multitaskerGetTextOutput(void);
int multitaskerSetTextOutput(int, objectKey);
int multitaskerDuplicateIO(int, int, int);
int multitaskerGetProcessorTime(clock_t *);
void multitaskerYield(void);
void multitaskerWait(unsigned);
int multitaskerBlock(int);
int multitaskerDetach(void);
int multitaskerKillProcess(int, int);
int multitaskerKillByName(const char *, int);
int multitaskerTerminate(int);
int multitaskerSignalSet(int, int, int);
int multitaskerSignal(int, int);
int multitaskerSignalRead(int);
int multitaskerGetIOPerm(int, int);
int multitaskerSetIOPerm(int, int, int);
int multitaskerStackTrace(int);

//
// Loader functions
//
void *loaderLoad(const char *, file *);
objectKey loaderClassify(const char *, void *, unsigned, loaderFileClass *);
objectKey loaderClassifyFile(const char *, loaderFileClass *);
loaderSymbolTable *loaderGetSymbols(const char *);
int loaderCheckCommand(const char *);
int loaderLoadProgram(const char *, int);
int loaderLoadLibrary(const char *);
void *loaderGetLibrary(const char *);
void *loaderLinkLibrary(const char *);
void *loaderGetSymbol(const char *);
int loaderExecProgram(int, int);
int loaderLoadAndExec(const char *, int, int);

//
// Real-time clock functions
//
int rtcReadSeconds(void);
int rtcReadMinutes(void);
int rtcReadHours(void);
int rtcDayOfWeek(unsigned, unsigned, unsigned);
int rtcReadDayOfMonth(void);
int rtcReadMonth(void);
int rtcReadYear(void);
unsigned rtcUptimeSeconds(void);
int rtcDateTime(struct tm *);

//
// Random number functions
//
unsigned randomUnformatted(void);
unsigned randomFormatted(unsigned, unsigned);
unsigned randomSeededUnformatted(unsigned);
unsigned randomSeededFormatted(unsigned, unsigned, unsigned);
void randomBytes(unsigned char *, unsigned);

//
// Environment functions
//
int environmentGet(const char *, char *, unsigned);
int environmentSet(const char *, const char *);
int environmentUnset(const char *);
void environmentDump(void);

//
// Raw graphics functions
//
int graphicsAreEnabled(void);
int graphicGetModes(videoMode *, unsigned);
int graphicGetMode(videoMode *);
int graphicSetMode(videoMode *);
int graphicGetScreenWidth(void);
int graphicGetScreenHeight(void);
int graphicCalculateAreaBytes(int, int);
int graphicClearScreen(color *);
int graphicDrawPixel(objectKey, color *, drawMode, int, int);
int graphicDrawLine(objectKey, color *, drawMode, int, int, int, int);
int graphicDrawRect(objectKey, color *, drawMode, int, int, int, int, int,
		    int);
int graphicDrawOval(objectKey, color *, drawMode, int, int, int, int, int,
		    int);
int graphicGetImage(objectKey, image *, int, int, int, int);
int graphicDrawImage(objectKey, image *, drawMode, int, int, int, int, int,
		     int);
int graphicDrawText(objectKey, color *, color *, objectKey, const char *,
		    drawMode, int, int);
int graphicCopyArea(objectKey, int, int, int, int, int, int);
int graphicClearArea(objectKey, color *, int, int, int, int);
int graphicRenderBuffer(objectKey, int, int, int, int, int, int);

//
// Windowing system functions
//
int windowLogin(const char *);
int windowLogout(void);
objectKey windowNew(int, const char *);
objectKey windowNewDialog(objectKey, const char *);
int windowDestroy(objectKey);
int windowUpdateBuffer(void *, int, int, int, int);
int windowSetTitle(objectKey, const char *);
int windowGetSize(objectKey, int *, int *);
int windowSetSize(objectKey, int, int);
int windowGetLocation(objectKey, int *, int *);
int windowSetLocation(objectKey, int, int);
int windowCenter(objectKey);
int windowSnapIcons(objectKey);
int windowSetHasBorder(objectKey, int);
int windowSetHasTitleBar(objectKey, int);
int windowSetMovable(objectKey, int);
int windowSetResizable(objectKey, int);
int windowRemoveMinimizeButton(objectKey);
int windowRemoveCloseButton(objectKey);
int windowSetVisible(objectKey, int);
void windowSetMinimized(objectKey, int);
int windowAddConsoleTextArea(objectKey);
void windowRedrawArea(int, int, int, int);
void windowDrawAll(void);
int windowGetColor(const char *, color *);
int windowSetColor(const char *, color *);
void windowResetColors(void);
void windowProcessEvent(objectKey);
int windowComponentEventGet(objectKey, windowEvent *);
int windowSetBackgroundColor(objectKey, color *);
int windowTileBackground(const char *);
int windowCenterBackground(const char *);
int windowScreenShot(image *);
int windowSaveScreenShot(const char *);
int windowSetTextOutput(objectKey);
int windowLayout(objectKey);
void windowDebugLayout(objectKey);
int windowContextAdd(objectKey, windowMenuContents *);
int windowContextSet(objectKey, objectKey);
int windowSwitchPointer(objectKey, const char *);
void windowComponentDestroy(objectKey);
int windowComponentSetVisible(objectKey, int);
int windowComponentSetEnabled(objectKey, int);
int windowComponentGetWidth(objectKey);
int windowComponentSetWidth(objectKey, int);
int windowComponentGetHeight(objectKey);
int windowComponentSetHeight(objectKey, int);
int windowComponentFocus(objectKey);
int windowComponentUnfocus(objectKey);
int windowComponentDraw(objectKey);
int windowComponentGetData(objectKey, void *, int);
int windowComponentSetData(objectKey, void *, int);
int windowComponentGetSelected(objectKey, int *);
int windowComponentSetSelected(objectKey, int );
objectKey windowNewButton(objectKey, const char *, image *,
			  componentParameters *);
objectKey windowNewCanvas(objectKey, int, int, componentParameters *);
objectKey windowNewCheckbox(objectKey, const char *, componentParameters *);
objectKey windowNewContainer(objectKey, const char *, componentParameters *);
objectKey windowNewDivider(objectKey, dividerType, componentParameters *);
objectKey windowNewIcon(objectKey, image *, const char *,
			componentParameters *);
objectKey windowNewImage(objectKey, image *, drawMode, componentParameters *);
objectKey windowNewList(objectKey, windowListType, int, int, int,
			listItemParameters *, int, componentParameters *);
objectKey windowNewListItem(objectKey, windowListType, listItemParameters *,
			    componentParameters *);
objectKey windowNewMenu(objectKey, const char *, windowMenuContents *,
			componentParameters *);
objectKey windowNewMenuBar(objectKey, componentParameters *);
objectKey windowNewMenuItem(objectKey, const char *, componentParameters *);
objectKey windowNewPasswordField(objectKey, int, componentParameters *);
objectKey windowNewProgressBar(objectKey, componentParameters *);
objectKey windowNewRadioButton(objectKey, int, int, char *[], int,
			       componentParameters *);
objectKey windowNewScrollBar(objectKey, scrollBarType, int, int,
			     componentParameters *);
objectKey windowNewSlider(objectKey, scrollBarType, int, int,
			  componentParameters *);
objectKey windowNewTextArea(objectKey, int, int, int, componentParameters *);
objectKey windowNewTextField(objectKey, int, componentParameters *);
objectKey windowNewTextLabel(objectKey, const char *, componentParameters *);

//
// User functions
//
int userAuthenticate(const char *, const char *);
int userLogin(const char *, const char *);
int userLogout(const char *);
int userGetNames(char *, unsigned);
int userAdd(const char *, const char *);
int userDelete(const char *);
int userSetPassword(const char *, const char *, const char *);
int userGetPrivilege(const char *);
int userGetPid(void);
int userSetPid(const char *, int);
int userFileAdd(const char *, const char *, const char *);
int userFileDelete(const char *, const char *);
int userFileSetPassword(const char *, const char *, const char *,
			const char *);

//
// Network functions
//
int networkDeviceGetCount(void);
int networkDeviceGet(const char *, networkDevice *);
int networkInitialized(void);
int networkInitialize(void);
int networkShutdown(void);
objectKey networkOpen(int, networkAddress *, networkFilter *);
int networkClose(objectKey);
int networkCount(objectKey);
int networkRead(objectKey, unsigned char *, unsigned);
int networkWrite(objectKey, unsigned char *, unsigned);
int networkPing(objectKey, int, unsigned char *, unsigned);
int networkGetHostName(char *, int);
int networkSetHostName(const char *, int);
int networkGetDomainName(char *, int);
int networkSetDomainName(const char *, int);

//
// Miscellaneous functions
//
int fontGetDefault(objectKey *);
int fontSetDefault(const char *);
int fontLoad(const char *, const char *, objectKey *, int);
int fontGetPrintedWidth(objectKey, const char *);
int fontGetWidth(objectKey);
int fontGetHeight(objectKey);
int imageNew(image *, unsigned, unsigned);
int imageFree(image *);
int imageLoad(const char *, unsigned, unsigned, image *);
int imageSave(const char *, int, image *);
int imageResize(image *, unsigned, unsigned);
int imageCopy(image *, image *);
int shutdown(int, int);
void getVersion(char *, int);
int systemInfo(struct utsname *);
int encryptMD5(const char *, char *);
int lockGet(lock *);
int lockRelease(lock *);
int lockVerify(lock *);
int variableListCreate(variableList *);
int variableListDestroy(variableList *);
int variableListGet(variableList *, const char *, char *, unsigned);
int variableListSet(variableList *, const char *, const char *);
int variableListUnset(variableList *, const char *);
int configRead(const char *, variableList *);
int configWrite(const char *, variableList *);
int configGet(const char *, const char *, char *, unsigned);
int configSet(const char *, const char *, const char *);
int configUnset(const char *, const char *);
int guidGenerate(guid *);
unsigned crc32(void *, unsigned, unsigned *);
int keyboardGetMap(keyMap *);
int keyboardSetMap(const char *);
int deviceTreeGetCount(void);
int deviceTreeGetRoot(device *);
int deviceTreeGetChild(device *, device *);
int deviceTreeGetNext(device *);
int mouseLoadPointer(const char *, const char *);
void *pageGetPhysical(int, void *);
void setLicensed(int);

#define _API_H
#endif
