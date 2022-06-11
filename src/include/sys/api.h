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

#include <time.h>
#include <sys/disk.h>
#include <sys/file.h>
#include <sys/image.h>
#include <sys/stream.h>
#include <sys/window.h>

// Included in the Visopsys standard library to prevent API calls from
// within kernel code.
extern int visopsys_in_kernel;

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
#define _fnum_textPrintLine                          1014
#define _fnum_textNewline                            1015
#define _fnum_textBackSpace                          1016
#define _fnum_textTab                                1017
#define _fnum_textCursorUp                           1018
#define _fnum_textCursorDown                         1019
#define _fnum_ternelTextCursorLeft                   1020
#define _fnum_textCursorRight                        1021
#define _fnum_textGetNumColumns                      1022
#define _fnum_textGetNumRows                         1023
#define _fnum_textGetColumn                          1024
#define _fnum_textSetColumn                          1025
#define _fnum_textGetRow                             1026
#define _fnum_textSetRow                             1027
#define _fnum_textClearScreen                        1028
#define _fnum_textInputStreamCount                   1029
#define _fnum_textInputCount                         1030
#define _fnum_textInputStreamGetc                    1031
#define _fnum_textInputGetc                          1032
#define _fnum_textInputStreamReadN                   1033
#define _fnum_textInputReadN                         1034
#define _fnum_textInputStreamReadAll                 1035
#define _fnum_textInputReadAll                       1036
#define _fnum_textInputStreamAppend                  1037
#define _fnum_textInputAppend                        1038
#define _fnum_textInputStreamAppendN                 1039
#define _fnum_textInputAppendN                       1040
#define _fnum_textInputStreamRemove                  1041
#define _fnum_textInputRemove                        1042
#define _fnum_textInputStreamRemoveN                 1043
#define _fnum_textInputRemoveN                       1044
#define _fnum_textInputStreamRemoveAll               1045
#define _fnum_textInputRemoveAll                     1046
#define _fnum_textInputStreamSetEcho                 1047
#define _fnum_textInputSetEcho                       1048

// Disk functions.  All are in the 2000-2999 range.
#define _fnum_diskReadPartitions                     2000
#define _fnum_diskSync                               2001
#define _fnum_diskGetBoot                            2002
#define _fnum_diskGetCount                           2003
#define _fnum_diskGetPhysicalCount                   2004
#define _fnum_diskGetInfo                            2005
#define _fnum_diskGetPhysicalInfo                    2006
#define _fnum_diskGetPartType                        2007
#define _fnum_diskGetPartTypes                       2008
#define _fnum_diskSetDoorState                       2009
#define _fnum_diskReadSectors                        2010
#define _fnum_diskWriteSectors                       2011
#define _fnum_diskReadAbsoluteSectors                2012
#define _fnum_diskWriteAbsoluteSectors               2013

// Filesystem functions.  All are in the 3000-3999 range.
#define _fnum_filesystemFormat                       3000
#define _fnum_filesystemCheck                        3001
#define _fnum_filesystemDefragment                   3002
#define _fnum_filesystemMount                        3003
#define _fnum_filesystemUnmount                      3004
#define _fnum_filesystemNumberMounted                3005
#define _fnum_filesystemFirstFilesystem              3006
#define _fnum_filesystemNextFilesystem               3007
#define _fnum_filesystemGetFree                      3008
#define _fnum_filesystemGetBlockSize                 3009

// File functions.  All are in the 4000-4999 range.
#define _fnum_fileFixupPath                          4000
#define _fnum_fileFirst                              4001
#define _fnum_fileNext                               4002
#define _fnum_fileFind                               4003
#define _fnum_fileOpen                               4004
#define _fnum_fileClose                              4005
#define _fnum_fileRead                               4006
#define _fnum_fileWrite                              4007
#define _fnum_fileDelete                             4008
#define _fnum_fileDeleteSecure                       4009
#define _fnum_fileMakeDir                            4010
#define _fnum_fileRemoveDir                          4011
#define _fnum_fileCopy                               4012
#define _fnum_fileCopyRecursive                      4013
#define _fnum_fileMove                               4014
#define _fnum_fileTimestamp                          4015
#define _fnum_fileStreamOpen                         4016
#define _fnum_fileStreamSeek                         4017
#define _fnum_fileStreamRead                         4018
#define _fnum_fileStreamWrite                        4019
#define _fnum_fileStreamFlush                        4020
#define _fnum_fileStreamClose                        4021

// Memory manager functions.  All are in the 5000-5999 range.
#define _fnum_memoryPrintUsage                       5000
#define _fnum_memoryGet                              5001
#define _fnum_memoryGetPhysical                      5002
#define _fnum_memoryRelease                          5003
#define _fnum_memoryReleaseAllByProcId               5004
#define _fnum_memoryChangeOwner                      5005

// Multitasker functions.  All are in the 6000-6999 range.
#define _fnum_multitaskerCreateProcess               6000
#define _fnum_multitaskerSpawn                       6001
#define _fnum_multitaskerGetCurrentProcessId         6002
#define _fnum_multitaskerGetProcessOwner             6003
#define _fnum_multitaskerGetProcessName              6004
#define _fnum_multitaskerGetProcessState             6005
#define _fnum_multitaskerSetProcessState             6006
#define _fnum_multitaskerGetProcessPriority          6007
#define _fnum_multitaskerSetProcessPriority          6008
#define _fnum_multitaskerGetProcessPrivilege         6009
#define _fnum_multitaskerGetCurrentDirectory         6010
#define _fnum_multitaskerSetCurrentDirectory         6011
#define _fnum_multitaskerGetTextInput                6012
#define _fnum_multitaskerSetTextInput                6013
#define _fnum_multitaskerGetTextOutput               6014
#define _fnum_multitaskerSetTextOutput               6015
#define _fnum_multitaskerGetProcessorTime            6016
#define _fnum_multitaskerYield                       6017
#define _fnum_multitaskerWait                        6018
#define _fnum_multitaskerBlock                       6019
#define _fnum_multitaskerDetach                      6020
#define _fnum_multitaskerKillProcess                 6021
#define _fnum_multitaskerTerminate                   6022
#define _fnum_multitaskerDumpProcessList             6023

// Loader functions.  All are in the 7000-7999 range.
#define _fnum_loaderLoad                             7000
#define _fnum_loaderLoadProgram                      7001
#define _fnum_loaderExecProgram                      7002
#define _fnum_loaderLoadAndExec                      7003

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

// Environment functions.  All are in the 10000-10999 range.
#define _fnum_environmentGet                         10000
#define _fnum_environmentSet                         10001
#define _fnum_environmentUnset                       10002
#define _fnum_environmentDump                        10003

// Raw graphics drawing functions.  All are in the 11000-11999 range
#define _fnum_graphicsAreEnabled                     11000
#define _fnum_graphicGetScreenWidth                  11001
#define _fnum_graphicGetScreenHeight                 11002
#define _fnum_graphicCalculateAreaBytes              11003
#define _fnum_graphicClearScreen                     11004
#define _fnum_graphicDrawPixel                       11005
#define _fnum_graphicDrawLine                        11006
#define _fnum_graphicDrawRect                        11007
#define _fnum_graphicDrawOval                        11008
#define _fnum_graphicDrawImage                       11009
#define _fnum_graphicGetImage                        11010
#define _fnum_graphicDrawText                        11011
#define _fnum_graphicCopyArea                        11012
#define _fnum_graphicClearArea                       11013
#define _fnum_graphicRenderBuffer                    11014

// Window manager functions.  All are in the 12000-12999 range
#define _fnum_windowManagerStart                     12000
#define _fnum_windowManagerLogin                     12001
#define _fnum_windowManagerLogout                    12002
#define _fnum_windowManagerNewWindow                 12003
#define _fnum_windowManagerNewDialog                 12004
#define _fnum_windowManagerDestroyWindow             12005
#define _fnum_windowManagerUpdateBuffer              12006
#define _fnum_windowSetTitle                         12007
#define _fnum_windowGetSize                          12008
#define _fnum_windowSetSize                          12009
#define _fnum_windowAutoSize                         12010
#define _fnum_windowGetLocation                      12011
#define _fnum_windowSetLocation                      12012
#define _fnum_windowCenter                           12013
#define _fnum_windowSetHasBorder                     12014
#define _fnum_windowSetHasTitleBar                   12015
#define _fnum_windowSetMovable                       12016
#define _fnum_windowSetHasCloseButton                12017
#define _fnum_windowLayout                           12018
#define _fnum_windowSetVisible                       12019
#define _fnum_windowAddComponent                     12020
#define _fnum_windowAddClientComponent               12021
#define _fnum_windowAddConsoleTextArea               12022
#define _fnum_windowComponentGetWidth                12023
#define _fnum_windowComponentGetHeight               12024
#define _fnum_windowManagerRedrawArea                12025
#define _fnum_windowManagerProcessEvent              12026
#define _fnum_windowComponentEventGet                12027
#define _fnum_windowManagerTileBackground            12028
#define _fnum_windowManagerCenterBackground          12029
#define _fnum_windowManagerScreenShot                12030
#define _fnum_windowManagerSaveScreenShot            12031
#define _fnum_windowManagerSetTextOutput             12032
#define _fnum_windowNewButton               12033
#define _fnum_windowNewIcon                 12034
#define _fnum_windowNewImage                12035
#define _fnum_windowNewTextArea             12036
#define _fnum_windowNewTextField            12037
#define _fnum_windowNewTextLabel            12038
#define _fnum_windowNewTitleBar             12039

// Miscellaneous functions.  All are in the 99000-99999 range
#define _fnum_fontGetDefault                         99000
#define _fnum_fontSetDefault                         99001
#define _fnum_fontLoad                               99002
#define _fnum_fontGetPrintedWidth                    99003
#define _fnum_imageLoadBmp                           99004
#define _fnum_imageSaveBmp                           99005
#define _fnum_userLogin                              99006
#define _fnum_userLogout                             99007
#define _fnum_userGetPrivilege                       99008
#define _fnum_userGetPid                             99009
#define _fnum_shutdown                               99010
#define _fnum_version                                99011

// Utility macros for stack manipulation
#define stackPush(value) \
  __asm__ __volatile__ ("pushl %0 \n\t" : : "r" (value))
#define stackAdj(bytes) \
  __asm__ __volatile__ ("addl %0, %%esp \n\t" \
                        : : "r" (bytes) : "%esp")

// The generic calls for accessing the kernel API
#define sysCall(retCode)                                            \
  if (!visopsys_in_kernel)                                          \
    {                                                               \
      __asm__ __volatile__ ("lcall $0x003B,$0x00000000 \n\t"        \
                            "movl %%eax, %0 \n\t"                   \
                            : "=r" (retCode) : : "%eax", "memory"); \
    }                                                               \
  else                                                              \
    {                                                               \
      retCode = -1;                                                 \
    }

// These use the macros defined above to call the kernel with the
// appropriate number of arguments

static inline int sysCall_0(int fnum)
{
  // Do a syscall with NO parameters
  int status = 0;
  stackPush(fnum);
  stackPush(1);
  sysCall(status);
  stackAdj(8);
  return (status);
}


static inline int sysCall_1(int fnum, void *arg1)
{
  // Do a syscall with 1 parameter
  int status = 0;
  stackPush(arg1);
  stackPush(fnum);
  stackPush(2);
  sysCall(status);
  stackAdj(12);
  return (status);
}


static inline int sysCall_2(int fnum, void *arg1, void *arg2)
{
  // Do a syscall with 2 parameters
  int status = 0;
  stackPush(arg2);
  stackPush(arg1);
  stackPush(fnum);
  stackPush(3);
  sysCall(status);
  stackAdj(16);
  return (status);
}


static inline int sysCall_3(int fnum, void *arg1, void *arg2, void *arg3)
{
  // Do a syscall with 3 parameters
  int status = 0;
  stackPush(arg3);
  stackPush(arg2);
  stackPush(arg1);
  stackPush(fnum);
  stackPush(4);
  sysCall(status);
  stackAdj(20);
  return (status);
}


static inline int sysCall_4(int fnum, void *arg1, void *arg2, void *arg3,
			    void *arg4)
{
  // Do a syscall with 4 parameters
  int status = 0;
  stackPush(arg4);
  stackPush(arg3);
  stackPush(arg2);
  stackPush(arg1);
  stackPush(fnum);
  stackPush(5);
  sysCall(status);
  stackAdj(24);
  return (status);
}


static inline int sysCall_5(int fnum, void *arg1, void *arg2, void *arg3,
			    void *arg4, void *arg5)
{
  // Do a syscall with 5 parameters
  int status = 0;
  stackPush(arg5);
  stackPush(arg4);
  stackPush(arg3);
  stackPush(arg2);
  stackPush(arg1);
  stackPush(fnum);
  stackPush(6);
  sysCall(status);
  stackAdj(28);
  return (status);
}


static inline int sysCall_6(int fnum, void *arg1, void *arg2, void *arg3,
			    void *arg4, void *arg5, void *arg6)
{
  // Do a syscall with 6 parameters
  int status = 0;
  stackPush(arg6);
  stackPush(arg5);
  stackPush(arg4);
  stackPush(arg3);
  stackPush(arg2);
  stackPush(arg1);
  stackPush(fnum);
  stackPush(7);
  sysCall(status);
  stackAdj(32);
  return (status);
}


static inline int sysCall_7(int fnum, void *arg1, void *arg2, void *arg3,
			    void *arg4, void *arg5, void *arg6, void *arg7)
{
  // Do a syscall with 7 parameters
  int status = 0;
  stackPush(arg7);
  stackPush(arg6);
  stackPush(arg5);
  stackPush(arg4);
  stackPush(arg3);
  stackPush(arg2);
  stackPush(arg1);
  stackPush(fnum);
  stackPush(8);
  sysCall(status);
  stackAdj(36);
  return (status);
}


static inline int sysCall_8(int fnum, void *arg1, void *arg2, void *arg3,
			    void *arg4, void *arg5, void *arg6, void *arg7,
			    void *arg8)
{
  // Do a syscall with 8 parameters
  int status = 0;
  stackPush(arg8);
  stackPush(arg7);
  stackPush(arg6);
  stackPush(arg5);
  stackPush(arg4);
  stackPush(arg3);
  stackPush(arg2);
  stackPush(arg1);
  stackPush(fnum);
  stackPush(9);
  sysCall(status);
  stackAdj(40);
  return (status);
}


static inline int sysCall_9(int fnum, void *arg1, void *arg2, void *arg3,
			    void *arg4, void *arg5, void *arg6, void *arg7,
			    void *arg8, void *arg9)
{
  // Do a syscall with 9 parameters
  int status = 0;
  stackPush(arg9);
  stackPush(arg8);
  stackPush(arg7);
  stackPush(arg6);
  stackPush(arg5);
  stackPush(arg4);
  stackPush(arg3);
  stackPush(arg2);
  stackPush(arg1);
  stackPush(fnum);
  stackPush(10);
  sysCall(status);
  stackAdj(44);
  return (status);
}


// These inline functions are used to call specific kernel functions.  
// There will be one of these for every API function.


//
// Text input/output functions
//

static inline objectKey textGetConsoleInput(void)
{
  // Proto: kernelTextInputStream *kernelTextGetConsoleInput(void);
  return ((objectKey) sysCall_0(_fnum_textGetConsoleInput));
}

static inline int textSetConsoleInput(objectKey newStream)
{
  // Proto: int kernelTextSetConsoleInput(kernelTextInputStream *);
  return (sysCall_1(_fnum_textSetConsoleInput, newStream));
}

static inline objectKey textGetConsoleOutput(void)
{
  // Proto: kernelTextOutputStream *kernelTextGetConsoleOutput(void);
  return ((objectKey) sysCall_0(_fnum_textGetConsoleOutput));
}

static inline int textSetConsoleOutput(objectKey newStream)
{
  // Proto: int kernelTextSetConsoleOutput(kernelTextOutputStream *);
  return (sysCall_1(_fnum_textSetConsoleOutput, newStream));
}

static inline objectKey textGetCurrentInput(void)
{
  // Proto: kernelTextInputStream *kernelTextGetCurrentInput(void);
  return ((objectKey) sysCall_0(_fnum_textGetCurrentInput));
}

static inline int textSetCurrentInput(objectKey newStream)
{
  // Proto: int kernelTextSetCurrentInput(kernelTextInputStream *);
  return (sysCall_1(_fnum_textSetCurrentInput, newStream));
}

static inline objectKey textGetCurrentOutput(void)
{
  // Proto: kernelTextOutputStream *kernelTextGetCurrentOutput(void);
  return ((objectKey) sysCall_0(_fnum_textGetCurrentOutput));
}

static inline int textSetCurrentOutput(objectKey newStream)
{
  // Proto: int kernelTextSetCurrentOutput(kernelTextOutputStream *);
  return (sysCall_1(_fnum_textSetCurrentOutput, newStream));
}

static inline int textGetForeground(void)
{
  // Proto: int kernelTextGetForeground(void);
  return (sysCall_0(_fnum_textGetForeground));
}

static inline int textSetForeground(int foreground)
{
  // Proto: int kernelTextSetForeground(int);
  return (sysCall_1(_fnum_textSetForeground, (void *) foreground));
}

static inline int textGetBackground(void)
{
  // Proto: int kernelTextGetBackground(void);
  return (sysCall_0(_fnum_textGetBackground));
}

static inline int textSetBackground(int background)
{
  // Proto: int kernelTextSetBackground(int);
  return (sysCall_1(_fnum_textSetBackground, (void *) background));
}

static inline int textPutc(int ascii)
{
  // Proto: int kernelTextPutc(int);
  return (sysCall_1(_fnum_textPutc, (void*)ascii));
}

static inline int textPrint(const char *str)
{
  // Proto: int kernelTextPrint(const char *);
  return (sysCall_1(_fnum_textPrint, (void *) str));
}

static inline int textPrintLine(const char *str)
{
  // Proto: int kernelTextPrintLine(const char *);
  return (sysCall_1(_fnum_textPrintLine, (void *) str));
}

static inline void textNewline(void)
{
  // Proto: void kernelTextNewline(void);
  sysCall_0(_fnum_textNewline);
}

static inline int textBackSpace(void)
{
  // Proto: void kernelTextBackSpace(void);
  return (sysCall_0(_fnum_textBackSpace));
}

static inline int textTab(void)
{
  // Proto: void kernelTextTab(void);
  return (sysCall_0(_fnum_textTab));
}

static inline int textCursorUp(void)
{
  // Proto: void kernelTextCursorUp(void);
  return (sysCall_0(_fnum_textCursorUp));
}

static inline int textCursorDown(void)
{
  // Proto: void kernelTextCursorDown(void);
  return (sysCall_0(_fnum_textCursorDown));
}

static inline int textCursorLeft(void)
{
  // Proto: void kernelTextCursorLeft(void);
  return (sysCall_0(_fnum_ternelTextCursorLeft));
}

static inline int textCursorRight(void)
{
  // Proto: void kernelTextCursorRight(void);
  return (sysCall_0(_fnum_textCursorRight));
}

static inline int textGetNumColumns(void)
{
  // Proto: int kernelTextGetNumColumns(void);
  return (sysCall_0(_fnum_textGetNumColumns));
}

static inline int textGetNumRows(void)
{
  // Proto: int kernelTextGetNumRows(void);
  return (sysCall_0(_fnum_textGetNumRows));
}

static inline int textGetColumn(void)
{
  // Proto: int kernelTextGetColumn(void);
  return (sysCall_0(_fnum_textGetColumn));
}

static inline void textSetColumn(int c)
{
  // Proto: void kernelTextSetColumn(int);
  sysCall_1(_fnum_textSetColumn, (void *) c);
}

static inline int textGetRow(void)
{
  // Proto: int kernelTextGetRow(void);
  return (sysCall_0(_fnum_textGetRow));
}

static inline void textSetRow(int r)
{
  // Proto: void kernelTextSetRow(int);
  sysCall_1(_fnum_textSetRow, (void *) r);
}

static inline int textClearScreen(void)
{
  // Proto: void kernelTextClearScreen(void);
  return (sysCall_0(_fnum_textClearScreen));
}

static inline int textInputStreamCount(objectKey strm)
{
  // Proto: int kernelTextInputStreamCount(kernelTextInputStream *);
  return (sysCall_1(_fnum_textInputStreamCount, strm));
}

static inline int textInputCount(void)
{
  // Proto: int kernelTextInputCount(void);
  return (sysCall_0(_fnum_textInputCount));
}

static inline int textInputStreamGetc(objectKey strm, char *cp)
{
  // Proto: int kernelTextInputStreamGetc(kernelTextInputStream *, char *);
  return (sysCall_2(_fnum_textInputStreamGetc, strm, cp));
}

static inline int textInputGetc(char *cp)
{
  // Proto: char kernelTextInputGetc(void);
  return (sysCall_1(_fnum_textInputGetc, cp));
}

static inline int textInputStreamReadN(objectKey strm, int num, char *buff)
{
  // Proto: int kernelTextInputStreamReadN(kernelTextInputStream *, int,
  //                                       char *);
  return (sysCall_3(_fnum_textInputStreamReadN, strm, (void *) num, buff));
}

static inline int textInputReadN(int num, char *buff)
{
  // Proto: int kernelTextInputReadN(int, char *);
  return (sysCall_2(_fnum_textInputReadN, (void *)num, buff));
}

static inline int textInputStreamReadAll(objectKey strm, char *buff)
{
  // Proto: int kernelTextInputStreamReadAll(kernelTextInputStream *, char *);
  return (sysCall_2(_fnum_textInputStreamReadAll, strm, buff));
}

static inline int textInputReadAll(char *buff)
{
  // Proto: int kernelTextInputReadAll(char *);
  return (sysCall_1(_fnum_textInputReadAll, buff));
}

static inline int textInputStreamAppend(objectKey strm, int ascii)
{
  // Proto: int kernelTextInputStreamAppend(kernelTextInputStream *, int);
  return (sysCall_2(_fnum_textInputStreamAppend, strm, (void *) ascii));
}

static inline int textInputAppend(int ascii)
{
  // Proto: int kernelTextInputAppend(int);
  return (sysCall_1(_fnum_textInputAppend, (void *) ascii));
}

static inline int textInputStreamAppendN(objectKey strm, int num, char *str)
{
  // Proto: int kernelTextInputStreamAppendN(kernelTextInputStream *, int,
  //                                         char *);
  return (sysCall_3(_fnum_textInputStreamAppendN, strm, (void *) num, str));
}

static inline int textInputAppendN(int num, char *str)
{
  // Proto: int kernelTextInputAppendN(int, char *);
  return (sysCall_2(_fnum_textInputAppendN, (void *) num, str));
}

static inline int textInputStreamRemove(objectKey strm)
{
  // Proto: int kernelTextInputStreamRemove(kernelTextInputStream *);
  return (sysCall_1(_fnum_textInputStreamRemove, strm));
}

static inline int textInputRemove(void)
{
  // Proto: int kernelTextInputRemove(void);
  return (sysCall_0(_fnum_textInputRemove));
}

static inline int textInputStreamRemoveN(objectKey strm, int num)
{
  // Proto: int kernelTextInputStreamRemoveN(kernelTextInputStream *, int);
  return (sysCall_2(_fnum_textInputStreamRemoveN, strm, (void *) num));
}

static inline int textInputRemoveN(int num)
{
  // Proto: int kernelTextInputRemoveN(int);
  return (sysCall_1(_fnum_textInputRemoveN, (void *) num));
}

static inline int textInputStreamRemoveAll(objectKey strm)
{
  // Proto: int kernelTextInputStreamRemoveAll(kernelTextInputStream *);
  return (sysCall_1(_fnum_textInputStreamRemoveAll, strm));
}

static inline int textInputRemoveAll(void)
{
  // Proto: int kernelTextInputRemoveAll(void);
  return (sysCall_0(_fnum_textInputRemoveAll));
}

static inline void textInputStreamSetEcho(objectKey strm, int onOff)
{
  // Proto: void kernelTextInputStreamSetEcho(kernelTextInputStream *, int);
  sysCall_2(_fnum_textInputStreamSetEcho, strm, (void *) onOff);
}

static inline void textInputSetEcho(int onOff)
{
  // Proto: void kernelTextInputSetEcho(int);
  sysCall_1(_fnum_textInputSetEcho, (void *) onOff);
}


// 
// Disk functions
//

static inline int diskReadPartitions(void)
{
  // Proto: int kernelDiskReadPartitions(void);
  return (sysCall_0(_fnum_diskReadPartitions));
}

static inline int diskSync(void)
{
  // Proto: int kernelDiskSync(void);
  return (sysCall_0(_fnum_diskSync));
}

static inline int diskGetBoot(char *name)
{
  // Proto: int kernelDiskGetBoot(char *)
  return (sysCall_1(_fnum_diskGetBoot, name));
}

static inline int diskGetCount(void)
{
  // Proto: int kernelDiskGetCount(void);
  return (sysCall_0(_fnum_diskGetCount));
}

static inline int diskGetPhysicalCount(void)
{
  // Proto: int kernelDiskGetPhysicalCount(void);
  return (sysCall_0(_fnum_diskGetPhysicalCount));
}

static inline int diskGetInfo(disk *d)
{
  // Proto: int kernelDiskGetInfo(disk *);
  return (sysCall_1(_fnum_diskGetInfo, d));
}

static inline int diskGetPhysicalInfo(disk *d)
{
  // Proto: int kernelDiskGetPhysicalInfo(disk *);
  return (sysCall_1(_fnum_diskGetPhysicalInfo, d));
}

static inline int diskGetPartType(int code, partitionType *p)
{
  // Proto: int kernelDiskGetPartType(int, partitionType *);
  return (sysCall_2(_fnum_diskGetPartType, (void *) code, p));
}

static inline partitionType *diskGetPartTypes(void)
{
  // Proto: partitionType *kernelDiskGetPartTypes(void);
  return ((partitionType *) sysCall_0(_fnum_diskGetPartTypes));
}

static inline int diskSetDoorState(const char *name, int state)
{
  // Proto: int kernelDiskSetDoorState(const char *, int);
  return (sysCall_2(_fnum_diskSetDoorState, (void *) name, (void *) state));
}

static inline int diskReadSectors(const char *name, unsigned sect,
				  unsigned count, void *buf)
{
  // Proto: int kernelDiskReadSectors(const char *, unsigned,
  //                                  unsigned, void *)
  return (sysCall_4(_fnum_diskReadSectors, (void *) name, (void *) sect,
		    (void *) count, buf));
}

static inline int diskWriteSectors(const char *name, unsigned sect,
				   unsigned count, void *buf)
{
  // Proto: int kernelDiskWriteSectors(const char *, unsigned, unsigned,
  //                                   void *)
  return (sysCall_4(_fnum_diskWriteSectors, (void *) name, (void *) sect,
		    (void *) count, buf));
}

static inline int diskReadAbsoluteSectors(const char *name, unsigned sect,
					  unsigned count, void *buf)
{
  // Proto: int kernelDiskReadAbsoluteSectors(const char *, unsigned,
  //                                          unsigned, void *)
  return (sysCall_4(_fnum_diskReadAbsoluteSectors, (void *) name,
		    (void *) sect, (void *) count, buf));
}

static inline int diskWriteAbsoluteSectors(const char *name, unsigned sect,
					   unsigned count, void *buf)
{
  // Proto: int kernelDiskWriteAbsoluteSectors(const char *, unsigned,
  //                                           unsigned, void *)
  return (sysCall_4(_fnum_diskWriteAbsoluteSectors, (void *) name,
		    (void *) sect, (void *) count, buf));
}


//
// Filesystem functions
//

static inline int filesystemFormat(const char *disk, const char *type,
				   const char *label, int longFormat)
{
  //Proto: int kernelFilesystemFormat(const char *, const char *,
  //                                  const char *, int);
  return (sysCall_4(_fnum_filesystemFormat, (void *) disk, (void *) type,
		    (void *) label, (void *) longFormat));
}

static inline int filesystemCheck(const char *name, int force, int repair)
{
  // Proto: int kernelFilesystemCheck(const char *, int, int)
  return (sysCall_3(_fnum_filesystemCheck, (void *) name, (void *) force,
		    (void *) repair));
}

static inline int filesystemDefragment(const char *name)
{
  // Proto: int kernelFilesystemDefragment(const char *)
  return (sysCall_1(_fnum_filesystemCheck, (void *) name));
}

static inline int filesystemMount(const char *name, const char *mp)
{
  // Proto: int kernelFilesystemMount(const char *, const char *)
  return (sysCall_2(_fnum_filesystemMount, (void *) name, (void *) mp));
}

static inline int filesystemUnmount(const char *mp)
{
  // Proto: int kernelFilesystemUnmount(const char *);
  return (sysCall_1(_fnum_filesystemUnmount, (void *)mp));
}

static inline int filesystemNumberMounted(void)
{
  // Proto: int kernelFilesystemNumberMounted(void);
  return (sysCall_0(_fnum_filesystemNumberMounted));
}

static inline void filesystemFirstFilesystem(char *buff)
{
  // Proto: void kernelFilesystemFirstFilesystem(char *);
  sysCall_1(_fnum_filesystemFirstFilesystem, buff);
}

static inline void filesystemNextFilesystem(char *buff)
{
  // Proto: void kernelFilesystemNextFilesystem(char *);
  sysCall_1(_fnum_filesystemNextFilesystem, buff);
}

static inline int filesystemGetFree(const char *fs)
{
  // Proto: unsigned int kernelFilesystemGetFree(const char *);
  return (sysCall_1(_fnum_filesystemGetFree, (void *) fs));
}

static inline unsigned int filesystemGetBlockSize(const char *fs)
{
  // Proto: unsigned int kernelFilesystemGetBlockSize(const char *);
  return (sysCall_1(_fnum_filesystemGetBlockSize, (void *) fs));
}


//
// File functions
//

static inline int fileFixupPath(const char *orig, char *new)
{
  // Proto: int kernelFileFixupPath(const char *, char *);
  return (sysCall_2(_fnum_fileFixupPath, (void *) orig, new));
}

static inline int fileFirst(const char *path, file *f)
{
  // Proto: int kernelFileFirst(const char *, file *);
  return (sysCall_2(_fnum_fileFirst, (void *) path, (void *) f));
}

static inline int fileNext(const char *path, file *f)
{
  // Proto: int kernelFileNext(const char *, file *);
  return (sysCall_2(_fnum_fileNext, (void *) path, (void *) f));
}

static inline int fileFind(const char *name, file *f)
{
  // Proto: int kernelFileFind(const char *, kernelFile *);
  return (sysCall_2(_fnum_fileFind, (void *) name, (void *) f));
}

static inline int fileOpen(const char *name, int mode, file *f)
{
  // Proto: int kernelFileOpen(const char *, int, file *);
  return (sysCall_3(_fnum_fileOpen, (void *) name, (void *) mode, 
		    (void *) f));
}

static inline int fileClose(file *f)
{
  // Proto: int kernelFileClose(const char *, file *);
  return (sysCall_1(_fnum_fileClose, (void *) f));
}

static inline int fileRead(file *f, unsigned int blocknum,
			   unsigned int blocks, unsigned char *buff)
{
  // Proto: int kernelFileRead(file *, unsigned int, unsigned int, 
  //          unsigned char *);
  return (sysCall_4(_fnum_fileRead, (void *) f, (void *) blocknum, 
		    (void *) blocks, buff));
}

static inline int fileWrite(file *f, unsigned int blocknum, 
			    unsigned int blocks, unsigned char *buff)
{
  // Proto: int kernelFileWrite(file *, unsigned int, unsigned int, 
  //          unsigned char *);
  return (sysCall_4(_fnum_fileRead, (void *) f, (void *) blocknum, 
		    (void *) blocks, buff));
}

static inline int fileDelete(const char *name)
{
  // Proto: int kernelFileDelete(const char *);
  return (sysCall_1(_fnum_fileDelete, (void *) name));
}

static inline int fileDeleteSecure(const char *name)
{
  // Proto: int kernelFileDeleteSecure(const char *);
  return (sysCall_1(_fnum_fileDeleteSecure, (void *) name));
}

static inline int fileMakeDir(const char *name)
{
  // Proto: int kernelFileMakeDir(const char *);
  return (sysCall_1(_fnum_fileMakeDir, (void *)name));
}

static inline int fileRemoveDir(const char *name)
{
  // Proto: int kernelFileRemoveDir(const char *);
  return (sysCall_1(_fnum_fileRemoveDir, (void *)name));
}

static inline int fileCopy(const char *src, const char *dest)
{
  // Proto: int kernelFileCopy(const char *, const char *);
  return (sysCall_2(_fnum_fileCopy, (void *) src, (void *) dest));
}

static inline int fileCopyRecursive(const char *src, const char *dest)
{
  // Proto: int kernelFileCopyRecursive(const char *, const char *);
  return (sysCall_2(_fnum_fileCopyRecursive, (void *) src, (void *) dest));
}

static inline int fileMove(const char *src, const char *dest)
{
  // Proto: int kernelFileMove(const char *, const char *);
  return (sysCall_2(_fnum_fileMove, (void *) src, (void *) dest));
}

static inline int fileTimestamp(const char *name)
{
  // Proto: int kernelFileTimestamp(const char *);
  return (sysCall_1(_fnum_fileTimestamp, (void *) name));
}

static inline int fileStreamOpen(const char *name, int mode, fileStream *f)
{
  // Proto: int kernelFileStreamOpen(const char *, int, fileStream *);
  return (sysCall_3(_fnum_fileStreamOpen, (char *) name, (void *) mode,
		    (void *) f));
}

static inline int fileStreamSeek(fileStream *f, int offset)
{
  // Proto: int kernelFileStreamSeek(fileStream *, int);
  return (sysCall_2(_fnum_fileStreamSeek, (void *) f, (void *) offset));
}

static inline int fileStreamRead(fileStream *f, int bytes, char *buff)
{
  // Proto: int kernelFileStreamRead(fileStream *, int, char *);
  return (sysCall_3(_fnum_fileStreamRead, (void *) f, (void *) bytes, buff));
}

static inline int fileStreamWrite(fileStream *f, int bytes, char *buff)
{
  // Proto: int kernelFileStreamWrite(fileStream *, int, char *);
  return (sysCall_3(_fnum_fileStreamWrite, (void *) f, (void *) bytes, buff));
}

static inline int fileStreamFlush(fileStream *f)
{
  // Proto: int kernelFileStreamFlush(fileStream *);
  return (sysCall_1(_fnum_fileStreamFlush, (void *) f));
}

static inline int fileStreamClose(fileStream *f)
{
  // Proto: int kernelFileStreamClose(fileStream *);
  return (sysCall_1(_fnum_fileStreamClose, (void *) f));
}


//
// Memory functions
//

static inline void memoryPrintUsage(int kernel)
{
  // Proto: void kernelMemoryPrintUsage(int);
  sysCall_1(_fnum_memoryPrintUsage, (void *) kernel);
}

static inline void *memoryGet(unsigned size, const char *desc)
{
  // Proto: void *kernelMemoryGet(unsigned, char *);
  return ((void *) sysCall_2(_fnum_memoryGet, (void *) size, (void *) desc));
}

static inline void *memoryGetPhysical(unsigned size, unsigned align,
				      const char *desc)
{
  // Proto: void *kernelMemoryGetPhysical(unsigned, unsigned, char *);
  return ((void *) sysCall_3(_fnum_memoryGetPhysical, (void *) size, 
			     (void *) align, (void *) desc));
}

static inline int memoryRelease(void *p)
{
  // Proto: int kernelMemoryRelease(void *);
  return (sysCall_1(_fnum_memoryRelease, p));
}

static inline int memoryReleaseAllByProcId(int pid)
{
  // Proto: int kernelMemoryReleaseAllByProcId(int);
  return (sysCall_1(_fnum_memoryReleaseAllByProcId, (void *) pid));
}

static inline int memoryChangeOwner(int opid, int npid, void *addr, 
				    void **naddr)
{
  // Proto: int kernelMemoryChangeOwner(int, int, void *, void **);
  return (sysCall_4(_fnum_memoryChangeOwner, (void *) opid, (void *) npid, 
		    addr, (void *) naddr));
}


//
// Multitasker functions
//

static inline int multitaskerCreateProcess(void *addr, unsigned int size, 
					   const char *name, int numargs,
					   void *args)
{
  // Proto: int kernelMultitaskerCreateProcess(void *, unsigned int, 
  //              const char *, int, void *);
  return (sysCall_5(_fnum_multitaskerCreateProcess, addr, (void *) size, 
		   (void *) name, (void *) numargs, args));
}

static inline int multitaskerSpawn(void *addr, const char *name, 
				   int numargs, void *args)
{
  // Proto: int kernelMultitaskerSpawn(void *, const char *, int, void *);
  return (sysCall_4(_fnum_multitaskerSpawn, addr, (void *) name, 
		    (void *) numargs, args));
}

static inline int multitaskerGetCurrentProcessId(void)
{
  // Proto: int kernelMultitaskerGetCurrentProcessId(void);
  return (sysCall_0(_fnum_multitaskerGetCurrentProcessId));
}

static inline int multitaskerGetProcessOwner(int pid)
{
  // Proto: int kernelMultitaskerGetProcessOwner(int);
  return (sysCall_1(_fnum_multitaskerGetProcessOwner, (void *) pid));
}

static inline const char *multitaskerGetProcessName(int pid)
{
  // Proto: const char *kernelMultitaskerGetProcessName(int);
  return ((const char *) sysCall_1(_fnum_multitaskerGetProcessName,
				   (void *) pid));
}

static inline int multitaskerGetProcessState(int pid, int *statep)
{
  // Proto: int kernelMultitaskerGetProcessState(int, kernelProcessState *);
  return (sysCall_2(_fnum_multitaskerGetProcessState, (void *) pid, statep));
}

static inline int multitaskerSetProcessState(int pid, int state)
{
  // Proto: int kernelMultitaskerSetProcessState(int, kernelProcessState);
  return (sysCall_2(_fnum_multitaskerSetProcessState, (void *) pid, 
		   (void *) state));
}

static inline int multitaskerGetProcessPriority(int pid)
{
  // Proto: int kernelMultitaskerGetProcessPriority(int);
  return (sysCall_1(_fnum_multitaskerGetProcessPriority, (void *) pid));
}

static inline int multitaskerSetProcessPriority(int pid, int priority)
{
  // Proto: int kernelMultitaskerSetProcessPriority(int, int);
  return (sysCall_2(_fnum_multitaskerSetProcessPriority, (void *) pid, 
		   (void *)priority));
}

static inline int multitaskerGetProcessPrivilege(int pid)
{
  // Proto: kernelMultitaskerGetProcessPrivilege(int);
  return (sysCall_1(_fnum_multitaskerGetProcessPrivilege, (void *) pid));
}

static inline int multitaskerGetCurrentDirectory(char *buff, int buffsz)
{
  // Proto: int kernelMultitaskerGetCurrentDirectory(char *, int);
  return (sysCall_2(_fnum_multitaskerGetCurrentDirectory, buff, 
		   (void *) buffsz));
}

static inline int multitaskerSetCurrentDirectory(char *buff)
{
  // Proto: int kernelMultitaskerSetCurrentDirectory(char *);
  return (sysCall_1(_fnum_multitaskerSetCurrentDirectory, buff));
}

static inline objectKey multitaskerGetTextInput(void)
{
  // Proto: kernelTextInputStream *kernelMultitaskerGetTextInput(void);
  return((objectKey) sysCall_0(_fnum_multitaskerGetTextInput));
}

static inline int multitaskerSetTextInput(int processId, objectKey key)
{
  // Proto: int kernelMultitaskerSetTextInput(int, kernelTextInputStream *);
  return (sysCall_2(_fnum_multitaskerSetTextInput, (void *) processId, key));
}

static inline objectKey multitaskerGetTextOutput(void)
{
  // Proto: kernelTextOutputStream *kernelMultitaskerGetTextOutput(void);
  return((objectKey) sysCall_0(_fnum_multitaskerGetTextOutput));
}

static inline int multitaskerSetTextOutput(int processId, objectKey key)
{
  // Proto: int kernelMultitaskerSetTextOutput(int, kernelTextOutputStream *);
  return (sysCall_2(_fnum_multitaskerSetTextOutput, (void *) processId, key));
}

static inline int multitaskerGetProcessorTime(clock_t *clk)
{
  // Proto: int kernelMultitaskerGetProcessorTime(clock_t *);
  return (sysCall_1(_fnum_multitaskerGetProcessorTime, clk));
}

static inline void multitaskerYield(void)
{
  // Proto: void kernelMultitaskerYield(void);
  sysCall_0(_fnum_multitaskerYield);
}

static inline void multitaskerWait(unsigned int ticks)
{
  // Proto: void kernelMultitaskerWait(unsigned int);
  sysCall_1(_fnum_multitaskerWait, (void *) ticks);
}

static inline int multitaskerBlock(int pid)
{
  // Proto: int kernelMultitaskerBlock(int);
  return (sysCall_1(_fnum_multitaskerBlock, (void *) pid));
}

static inline int multitaskerDetach(void)
{
  // Proto: int kernelMultitaskerDetach(void);
  return (sysCall_0(_fnum_multitaskerDetach));
}

static inline int multitaskerKillProcess(int pid, int force)
{
  // Proto: int kernelMultitaskerKillProcess(int);
  return (sysCall_2(_fnum_multitaskerKillProcess, (void *) pid,
		    (void *) force));
}

static inline int multitaskerTerminate(int code)
{
  // Proto: int kernelMultitaskerTerminate(int);
  return (sysCall_1(_fnum_multitaskerTerminate, (void *) code));
}

static inline void multitaskerDumpProcessList(void)
{
  // Proto: void kernelMultitaskerDumpProcessList(void);
  sysCall_0(_fnum_multitaskerDumpProcessList);
}


//
// Loader functions
//

static inline void *loaderLoad(const char *filename, file *theFile)
{
  // Proto: void *kernelLoaderLoad(const char *, file *);
  return ((void *) sysCall_2(_fnum_loaderLoad, (void *) filename,
			     (void *) theFile));
}

static inline int loaderLoadProgram(const char *userProgram, int privilege,
				     int argc, char *argv[])
{
  // Proto: int kernelLoaderLoadProgram(const char *, int, int, char *[]);
  return (sysCall_4(_fnum_loaderLoadProgram, (void *) userProgram,
		    (void *) privilege, (void *) argc, argv));
}

static inline int loaderExecProgram(int processId, int block)
{
  // Proto: int kernelLoaderExecProgram(int, int);
  return (sysCall_2(_fnum_loaderExecProgram, (void *) processId,
		    (void *) block));
}

static inline int loaderLoadAndExec(const char *name, int privilege,
				    int argc, char *argv[], int block)
{
  // Proto: kernelLoaderLoadAndExec(const char *, int, char *[], int);
  return (sysCall_5(_fnum_loaderLoadAndExec, (void *) name,
		    (void *) privilege, (void *) argc, argv, (void *) block));
}


//
// Real-time clock functions
//

static inline int rtcReadSeconds(void)
{
  // Proto: int kernelRtcReadSeconds(void);
  return (sysCall_0(_fnum_rtcReadSeconds));
}

static inline int rtcReadMinutes(void)
{
  // Proto: int kernelRtcReadMinutes(void);
  return (sysCall_0(_fnum_rtcReadMinutes));
}

static inline int rtcReadHours(void)
{
  // Proto: int kernelRtcReadHours(void);
  return (sysCall_0(_fnum_rtcReadHours));
}

static inline int rtcDayOfWeek(unsigned day, unsigned month, unsigned year)
{
  // Proto: int kernelRtcDayOfWeek(unsigned, unsigned, unsigned);
  return (sysCall_3(_fnum_rtcDayOfWeek, (void *) day, (void *) month,
		    (void *) year));
}

static inline int rtcReadDayOfMonth(void)
{
  // Proto: int kernelRtcReadDayOfMonth(void);
  return (sysCall_0(_fnum_rtcReadDayOfMonth));
}

static inline int rtcReadMonth(void)
{
  // Proto: int kernelRtcReadMonth(void);
  return (sysCall_0(_fnum_rtcReadMonth));
}

static inline int rtcReadYear(void)
{
  // Proto: int kernelRtcReadYear(void);
  return (sysCall_0(_fnum_rtcReadYear));
}

static inline unsigned int rtcUptimeSeconds(void)
{
  // Proto: unsigned int kernelRtcUptimeSeconds(void);
  return (sysCall_0(_fnum_rtcUptimeSeconds));
}


static inline int rtcDateTime(struct tm *time)
{
  // Proto: int kernelRtcDateTime(struct tm *);
  return (sysCall_1(_fnum_rtcDateTime, (void *) time));
}


//
// Random number functions
//

static inline unsigned int randomUnformatted(void)
{
  // Proto: unsigned int kernelRandomUnformatted(void);
  return (sysCall_0(_fnum_randomUnformatted));
}

static inline unsigned int randomFormatted(unsigned int start,
					   unsigned int end)
{
  // Proto: unsigned int kernelRandomFormatted(unsigned int, unsigned int);
  return (sysCall_2(_fnum_randomFormatted, (void *) start, (void *) end));
}

static inline unsigned int randomSeededUnformatted(unsigned int seed)
{
  // Proto: unsigned int kernelRandomSeededUnformatted(unsigned int);
  return (sysCall_1(_fnum_randomSeededUnformatted, (void *) seed));
}

static inline unsigned int randomSeededFormatted(unsigned int seed,
					 unsigned int start, unsigned int end)
{
  // Proto: unsigned int kernelRandomSeededFormatted(unsigned int,
  //                                   unsigned int, unsigned int);
  return (sysCall_3(_fnum_randomSeededFormatted, (void *) seed,
		    (void *) start, (void *) end));
}


//
// Environment functions
//

static inline int environmentGet(const char *var, char *buf,
				 unsigned int bufsz)
{
  // Proto: int kernelEnvironmentGet(const char *, char *, unsigned int);
  return (sysCall_3(_fnum_environmentGet, (void *) var, buf, (void *) bufsz));
}

static inline int environmentSet(const char *var, const char *val)
{
  // Proto: int kernelEnvironmentSet(const char *, const char *);
  return (sysCall_2(_fnum_environmentSet, (void *) var, (void *) val));
}

static inline int environmentUnset(const char *var)
{
  // Proto: int kernelEnvironmentUnset(const char *);
  return (sysCall_1(_fnum_environmentUnset, (void *) var));
}

static inline void environmentDump(void)
{
  // Proto: void kernelEnvironmentDump(void);
  sysCall_0(_fnum_environmentDump);
}


//
// Raw graphics functions
//

static inline int graphicsAreEnabled(void)
{
  // Proto: int kernelGraphicsAreEnabled(void);
  return (sysCall_0(_fnum_graphicsAreEnabled));
}

static inline unsigned graphicGetScreenWidth(void)
{
  // Proto: unsigned kernelGraphicGetScreenWidth(void);
  return (sysCall_0(_fnum_graphicGetScreenWidth));
}

static inline unsigned graphicGetScreenHeight(void)
{
  // Proto: unsigned kernelGraphicGetScreenHeight(void);
  return (sysCall_0(_fnum_graphicGetScreenHeight));
}

static inline unsigned graphicCalculateAreaBytes(unsigned width,
						 unsigned height)
{
  // Proto: unsigned kernelGraphicCalculateAreaBytes(unsigned, unsigned);
  return (sysCall_2(_fnum_graphicCalculateAreaBytes, (void *) width,
		    (void *) height));
}

static inline int graphicClearScreen(color *background)
{
  // Proto: int kernelGraphicClearScreen(color *);
  return (sysCall_1(_fnum_graphicClearScreen, background));
}

static inline int graphicDrawPixel(objectKey buffer, color *foreground,
				   drawMode mode, int xCoord, int yCoord)
{
  // Proto: int kernelGraphicDrawPixel(kernelGraphicBuffer *, color *,
  //                                   drawMode, int, int);
  return (sysCall_5(_fnum_graphicDrawPixel, buffer, foreground, (void *) mode,
		    (void *) xCoord, (void *) yCoord));
}

static inline int graphicDrawLine(objectKey buffer, color *foreground,
				  drawMode mode, int startX, int startY,
				  int endX, int endY)
{
  // Proto: int kernelGraphicDrawLine(kernelGraphicBuffer *, color *,
  //                                  drawMode, int, int, int, int);
  return (sysCall_7(_fnum_graphicDrawLine, buffer, foreground, (void *) mode,
		    (void *) startX, (void *) startY, (void *) endX,
		    (void *) endY));
}

static inline int graphicDrawRect(objectKey buffer, color *foreground,
	  drawMode mode, int xCoord, int yCoord, unsigned width,
	  unsigned height, unsigned thickness, int fill)
{
  // Proto: int kernelGraphicDrawRect(kernelGraphicBuffer *, color *,
  //                                  drawMode, int, int, unsigned,
  //                                  unsigned, unsigned, int);
  return (sysCall_9(_fnum_graphicDrawRect, buffer, foreground, (void *) mode,
		    (void *) xCoord, (void *) yCoord, (void *) width,
		    (void *) height, (void *) thickness, (void *) fill));
}

static inline int graphicDrawOval(objectKey buffer, color *foreground,
	  drawMode mode, int xCoord, int yCoord, unsigned width,
	  unsigned height, unsigned thickness, int fill)
{
  // Proto: int kernelGraphicDrawOval(kernelGraphicBuffer *, color *,
  //                              drawMode, int, int, unsigned, unsigned,
  //                              unsigned, int);
  return (sysCall_9(_fnum_graphicDrawOval, buffer, foreground, (void *) mode,
		    (void *) xCoord, (void *) yCoord, (void *) width,
		    (void *) height, (void *) thickness, (void *) fill));
}

static inline int graphicDrawImage(objectKey buffer, image *drawImage,
				   int xCoord, int yCoord, unsigned xOffset,
				   unsigned yOffset, unsigned width,
				   unsigned height)
{
  // Proto: int kernelGraphicDrawImage(kernelGraphicBuffer *, image *,
  //                                   int, int, unsigned, unsigned, unsigned,
  //                                   unsigned);
  return (sysCall_8(_fnum_graphicDrawImage, buffer, drawImage, (void *) xCoord,
		    (void *) yCoord, (void *) xOffset, (void *) yOffset,
		    (void *) width, (void *) height));
}

static inline int graphicGetImage(objectKey buffer, image *getImage,
				  int xCoord, int yCoord, unsigned width,
				  unsigned height)
{
  // Proto: int kernelGraphicGetImage(kernelGraphicBuffer *, image *,
  //                                  int, int, unsigned, unsigned);
  return (sysCall_6(_fnum_graphicGetImage, buffer, getImage, (void *) xCoord,
		    (void *) yCoord, (void *) width, (void *) height));
}

static inline int graphicDrawText(objectKey buffer, color *foreground,
				  objectKey font, const char *text,
				  drawMode mode, int xCoord, int yCoord)
{
  // Proto: int kernelGraphicDrawText(kernelGraphicBuffer *, color *,
  //                           kernelAsciiFont *, const char *, drawMode,
  //                           int, int);
  return (sysCall_7(_fnum_graphicDrawText, buffer, foreground, font,
		    (void *) text, (void *) mode, (void *) xCoord,
		    (void *) yCoord));
}

static inline int graphicCopyArea(objectKey buffer, int xCoord1, int yCoord1,
				  unsigned width, unsigned height,
				  int xCoord2, int yCoord2)
{
  // Proto: int kernelGraphicCopyArea(kernelGraphicBuffer *, int, int,
  //                                  unsigned, unsigned, int, int);
  return (sysCall_7(_fnum_graphicCopyArea, buffer, (void *) xCoord1,
		    (void *) yCoord1, (void *) width, (void *) height,
		    (void *) xCoord2, (void *) yCoord2));
}

static inline int graphicClearArea(objectKey buffer, color *background,
				   int xCoord, int yCoord,
				   unsigned width, unsigned height)
{
  // Proto: int kernelGraphicClearArea(kernelGraphicBuffer *, color *,
  //                                   int, int, unsigned, unsigned);
  return (sysCall_6(_fnum_graphicClearArea, buffer, background,
		    (void *) xCoord, (void *) yCoord, (void *) width,
		    (void *) height));
}

static inline int graphicRenderBuffer(objectKey buffer, int drawX,
				      int drawY, int clipX, int clipY,
				      unsigned clipWidth, unsigned clipHeight)
{
  // Proto: int kernelGraphicRenderBuffer(kernelGraphicBuffer *, int, int, int,
  //                                      int, unsigned, unsigned); 
  return (sysCall_7(_fnum_graphicRenderBuffer, buffer, (void *) drawX,
		    (void *) drawY, (void *) clipX, (void *) clipY,
		    (void *) clipWidth, (void *) clipHeight));
}


//
// Window manager functions
//

static inline int windowManagerStart(void)
{
  // Proto: int kernelWindowManagerStart(void);
  return (sysCall_0(_fnum_windowManagerStart));
}

static inline int windowManagerLogin(const char *userName, const char *passwd)
{
  // Proto: kernelWindowManagerLogin(const char *, const char *);
  return (sysCall_2(_fnum_windowManagerLogin, (void *) userName,
		    (void *) passwd));
}

static inline int windowManagerLogout(void)
{
  // Proto: kernelWindowManagerLogout(void);
  return (sysCall_0(_fnum_windowManagerLogout));
}

static inline objectKey windowManagerNewWindow(int processId, char *title,
					       int xCoord, int yCoord,
					       int width, int height)
{
  // Proto: kernelWindow *kernelWindowManagerNewWindow(int, const char *, int,
  //                                                  int, unsigned, unsigned);
  return ((objectKey) sysCall_6(_fnum_windowManagerNewWindow,
				 (void *) processId, (void *) title,
				 (void *) xCoord, (void *) yCoord,
				 (void *) width, (void *) height));
}

static inline objectKey windowManagerNewDialog(objectKey parent, char *title,
					       int xCoord, int yCoord,
					       int width, int height)
{
  // Proto: kernelWindow *kernelWindowManagerNewDialog(kernelWindow *,
  //                              const char *, int, int, unsigned, unsigned);
  return ((objectKey) sysCall_6(_fnum_windowManagerNewDialog, parent,
				(void *) title, (void *) xCoord,
				(void *) yCoord, (void *) width,
				(void *) height));
}

static inline int windowManagerDestroyWindow(objectKey window)
{
  // Proto: int kernelWindowManagerDestroyWindow(kernelWindow *);
  return (sysCall_1(_fnum_windowManagerDestroyWindow, window));
}

static inline int windowManagerUpdateBuffer(void *buffer, int xCoord,
					    int yCoord, unsigned width,
					    unsigned height)
{
  // Proto: kernelWindowManagerUpdateBuffer(kernelGraphicBuffer *, int, int,
  //				    unsigned, unsigned);
  return (sysCall_5(_fnum_windowManagerUpdateBuffer, buffer, (void *) xCoord,
					    (void *) xCoord, (void *) width,
					    (void *) height));
}

static inline int windowSetTitle(objectKey window, const char *title)
{
  // Proto: int kernelWindowSetTitle(kernelWindow *, const char *);
  return (sysCall_2(_fnum_windowSetTitle, window, (void *) title));
}


static inline int windowGetSize(objectKey window, unsigned *width,
				unsigned *height)
{
  // Proto: int kernelWindowGetSize(kernelWindow *, unsigned *, unsigned *);
  return (sysCall_3(_fnum_windowGetSize, window, width, height));
}

static inline int windowSetSize(objectKey window, unsigned width,
				unsigned height)
{
  // Proto: int kernelWindowSetSize(kernelWindow *, unsigned, unsigned);
  return (sysCall_3(_fnum_windowSetSize, window, (void *) width,
		    (void *) height));
}

static inline int windowAutoSize(objectKey window)
{
  // Proto: int kernelWindowAutoSize(kernelWindow *);
  return (sysCall_1(_fnum_windowAutoSize, window));
}

static inline int windowGetLocation(objectKey window, int *xCoord, int *yCoord)
{
  // Proto: int kernelWindowGetLocation(kernelWindow *, int *, int *);
  return (sysCall_3(_fnum_windowGetLocation, window, xCoord, yCoord));
}

static inline int windowSetLocation(objectKey window, int xCoord, int yCoord)
{
  // Proto: int kernelWindowSetLocation(kernelWindow *, int, int);
  return (sysCall_3(_fnum_windowSetLocation, window, (void *) xCoord,
		    (void *) yCoord));
}

static inline int windowCenter(objectKey window)
{
  // Proto: int kernelWindowCenter(kernelWindow *);
  return (sysCall_1(_fnum_windowCenter, window));
}

static inline int windowSetHasBorder(objectKey window, int trueFalse)
{
  // Proto: int kernelWindowSetHasBorder(kernelWindow *, int);
  return (sysCall_2(_fnum_windowSetHasBorder, window, (void *) trueFalse));
}

static inline int windowSetHasTitleBar(objectKey window, int trueFalse)
{
  // Proto: int kernelWindowSetHasTitleBar(kernelWindow *, int);
  return (sysCall_2(_fnum_windowSetHasTitleBar, window, (void *) trueFalse));
}

static inline int windowSetMovable(objectKey window, int trueFalse)
{
  // Proto: int kernelWindowSetMovable(kernelWindow *, int);
  return (sysCall_2(_fnum_windowSetMovable, window, (void *) trueFalse));
}

static inline int windowSetHasCloseButton(objectKey window, int trueFalse)
{
  // Proto: int kernelWindowSetHasCloseButton(kernelWindow *, int);
  return (sysCall_2(_fnum_windowSetHasCloseButton, window,
		    (void *) trueFalse));
}

static inline int windowLayout(objectKey window)
{
  // Proto: int kernelWindowLayout(kernelWindow *);
  return (sysCall_1(_fnum_windowLayout, window));
}

static inline int windowSetVisible(objectKey window, int visible)
{
  // Proto: int kernelWindowSetVisible(kernelWindow *, int);
  return (sysCall_2(_fnum_windowSetVisible, window, (void *) visible));
}

static inline int windowAddComponent(objectKey window, objectKey component,
				     componentParameters *params)
{
  // Proto: int kernelWindowAddComponent(kernelWindow *,
  //                                     kernelWindowComponent *,
  //                                     componentParameters *);
  return (sysCall_3(_fnum_windowAddComponent, window, component, params));
}

static inline int windowAddClientComponent(objectKey window,
					   objectKey component,
					   componentParameters *params)
{
  // Proto: int kernelWindowAddClientComponent(kernelWindow *,
  //                                           kernelWindowComponent *,
  //                                           componentParameters *);
  return (sysCall_3(_fnum_windowAddClientComponent, window, component,
		    params));
}

static inline int windowAddConsoleTextArea(objectKey window,
					   componentParameters *params)
{
  // Proto: int kernelWindowAddConsoleTextArea(kernelWindow *,
  //                                           componentParameters *);
  return (sysCall_2(_fnum_windowAddConsoleTextArea, window, params));
}

static inline unsigned windowComponentGetWidth(objectKey component)
{
  // Proto: unsigned kernelWindowComponentGetWidth(kernelWindowComponent *);
  return (sysCall_1(_fnum_windowComponentGetWidth, component));
}

static inline unsigned windowComponentGetHeight(objectKey component)
{
  // Proto: unsigned kernelWindowComponentGetHeight(kernelWindowComponent *);
  return (sysCall_1(_fnum_windowComponentGetHeight, component));
}

static inline void windowManagerRedrawArea(int xCoord, int yCoord,
					   unsigned width, unsigned height)
{
  // Proto: void kernelWindowManagerRedrawArea(int, int, unsigned, unsigned);
  sysCall_4(_fnum_windowManagerRedrawArea, (void *) xCoord, (void *) yCoord,
	    (void *) width, (void *) height);
}

static inline void windowManagerProcessEvent(objectKey event)
{
  // Proto: void kernelWindowManagerProcessEvent(windowEvent *);
  sysCall_1(_fnum_windowManagerProcessEvent, event);
}

static inline int windowComponentEventGet(objectKey key, windowEvent *event)
{
  // Proto: int kernelWindowComponentEventGet(objectKey, windowEvent *);
  return(sysCall_2(_fnum_windowComponentEventGet, key, event));
}

static inline int windowManagerTileBackground(const char *file)
{
  // Proto: int kernelWindowManagerTileBackground(const char *);
  return (sysCall_1(_fnum_windowManagerTileBackground, (void *) file));
}

static inline int windowManagerCenterBackground(const char *file)
{
  // Proto: int kernelWindowManagerCenterBackground(const char *file);
  return (sysCall_1(_fnum_windowManagerCenterBackground, (void *) file));
}

static inline int windowManagerScreenShot(image *saveImage)
{
  // Proto: int kernelWindowManagerScreenShot(image *);
  return (sysCall_1(_fnum_windowManagerScreenShot, saveImage));
}

static inline int windowManagerSaveScreenShot(const char *filename)
{
  // Proto: int kernelWindowManagerSaveScreenShot(void);
  return (sysCall_1(_fnum_windowManagerSaveScreenShot, (void *) filename));
}

static inline int windowManagerSetTextOutput(objectKey key)
{
  // Proto: int kernelWindowManagerSetTextOutput(kernelWindowComponent *);
  return (sysCall_1(_fnum_windowManagerSetTextOutput, key));
}

static inline objectKey windowNewButton(objectKey window, unsigned width,
					unsigned height, objectKey label,
					image *buttonImage)
{
  // Proto: kernelWindowComponent *kernelWindowNewButton(kernelWindow *,
  //                   unsigned, unsigned, kernelWindowComponent *, image *);
  return ((objectKey) sysCall_5(_fnum_windowNewButton, window,
				(void *) width, (void *) height,
				(void *) label, buttonImage));
}

static inline objectKey windowNewIcon(objectKey window, image *iconImage,
				      const char *label, const char *command)
{
  // Proto: kernelWindowComponent *kernelWindowNewIcon(kernelWindow *, image *,
  //                                       const char *, const char *);
  return ((objectKey) sysCall_4(_fnum_windowNewIcon, window, iconImage,
				(void *) label, (void *) command));
}

static inline objectKey windowNewImage(objectKey window, image *baseImage)
{
  // Proto: kernelWindowComponent *kernelWindowNewImage(kernelWindow *,
  //                                                    image *);
  return ((objectKey) sysCall_2(_fnum_windowNewImage, window, baseImage));
}

static inline objectKey windowNewTextArea(objectKey window, int columns,
					  int rows, objectKey font)
{
  // Proto: kernelWindowComponent *kernelWindowNewTextArea(kernelWindow *,
  //                                          int, int, kernelAsciiFont *);
  return ((objectKey) sysCall_4(_fnum_windowNewTextArea, window,
				(void *) columns, (void *) rows, font));
}

static inline objectKey windowNewTextField(objectKey window, int columns,
					   objectKey font)
{
  // Proto: kernelWindowComponent *kernelWindowNewTextField(kernelWindow *,
  //                                                 int, kernelAsciiFont *);
  return ((objectKey) sysCall_3(_fnum_windowNewTextField, window,
				(void *) columns, font));
}

static inline objectKey windowNewTextLabel(objectKey window, objectKey font,
					   const char *text)
{
  // Proto: kernelWindowComponent *kernelWindowNewTextLabel(kernelWindow *,
  //                                          kernelAsciiFont *, const char *);
  return ((objectKey) sysCall_3(_fnum_windowNewTextLabel, window, font,
				(void *) text));
}

static inline objectKey windowNewTitleBar(objectKey window, unsigned width,
					  unsigned height)
{
  // Proto: kernelWindowComponent *kernelWindowNewTitleBar(kernelWindow *,
  //                                                     unsigned, unsigned);
  return ((objectKey) sysCall_3(_fnum_windowNewTitleBar, window,
				(void *) width,	(void *) height));
}


//
// Miscellaneous functions
//

static inline int fontGetDefault(objectKey *pointer)
{
  // Proto: int kernelFontGetDefault(kernelAsciiFont **);
  return (sysCall_1(_fnum_fontGetDefault, pointer));
}

static inline int fontSetDefault(const char *name)
{
  // Proto: int kernelFontSetDefault(const char *);
  return (sysCall_1(_fnum_fontSetDefault, (void *) name));
}

static inline int fontLoad(const char* filename, const char *fontname,
			   objectKey *pointer)
{
  // Proto: int kernelFontLoad(const char*, const char*, kernelAsciiFont **);
  return (sysCall_3(_fnum_fontLoad, (void *) filename, (void *) fontname,
		    pointer));
}

static inline unsigned fontGetPrintedWidth(objectKey font, const char *string)
{
  // Proto: unsigned kernelFontGetPrintedWidth(kernelAsciiFont *,
  //                                           const char *);
  return (sysCall_2(_fnum_fontGetPrintedWidth, font, (void *) string));
}

static inline int imageLoadBmp(const char *filename, image *loadImage)
{
  // Proto: int imageLoadBmp(const char *, image *);
  return (sysCall_2(_fnum_imageLoadBmp, (void *) filename, loadImage));
}

static inline int imageSaveBmp(const char *filename, image *saveImage)
{
  // Proto: int imageSaveBmp(const char *, image *);
  return (sysCall_2(_fnum_imageSaveBmp, (void *) filename, saveImage));
}

static inline int userLogin(const char *name, int pid)
{
  // Proto: int kernelUserLogin(const char *, int);
  return (sysCall_2(_fnum_userLogin, (void *) name, (void *) pid));
}

static inline int userLogout(const char *name)
{
  // Proto: int kernelUserLogout(const char *);
  return (sysCall_1(_fnum_userLogout, (void *) name));
}

static inline int userGetPrivilege(const char *name)
{
  // Proto: int kernelUserGetPrivilege(const char *);
  return (sysCall_1(_fnum_userGetPrivilege, (void *) name));
}

static inline int userGetPid(void)
{
  // Proto: int kernelUserGetPid(void);
  return (sysCall_0(_fnum_userGetPid));
}

static inline int shutdown(int type, int nice)
{
  // Proto: int kernelShutdown(kernelShutdownType, int);
  return (sysCall_2(_fnum_shutdown, (void *) type, (void *) nice));
}

static inline const char *version(void)
{
  // Proto: const char *kernelVersion(void);
  return ((const char *) sysCall_0(_fnum_version));
}


#define _API_H
#endif
