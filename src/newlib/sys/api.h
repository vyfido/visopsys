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
#include <sys/file.h>
#include <sys/stream.h>

// Included in the Visopsys standard library to prevent API calls from
// within kernel code.
extern int visopsys_in_kernel;

// Text input/output functions.  All are in the 1000-1999 range.
#define _fnum_textGetForeground                   1003
#define _fnum_textSetForeground                   1004
#define _fnum_textGetBackground                   1005
#define _fnum_textSetBackground                   1006
#define _fnum_textPutc                            1007
#define _fnum_textPrint                           1008
#define _fnum_textPrintLine                       1009
#define _fnum_textNewline                         1010
#define _fnum_textBackSpace                       1011
#define _fnum_textTab                             1012
#define _fnum_textCursorUp                        1013
#define _fnum_textCursorDown                      1014
#define _fnum_ternelTextCursorLeft                1015
#define _fnum_textCursorRight                     1016
#define _fnum_textGetNumColumns                   1017
#define _fnum_textGetNumRows                      1018
#define _fnum_textGetColumn                       1019
#define _fnum_textSetColumn                       1020
#define _fnum_textGetRow                          1021
#define _fnum_textSetRow                          1022
#define _fnum_textClearScreen                     1023
#define _fnum_textInputCount                      1024
#define _fnum_textInputGetc                       1025
#define _fnum_textInputReadN                      1026
#define _fnum_textInputReadAll                    1027
#define _fnum_textInputAppend                     1028
#define _fnum_textInputAppendN                    1029
#define _fnum_textInputRemove                     1030
#define _fnum_textInputRemoveN                    1031
#define _fnum_textInputRemoveAll                  1032
#define _fnum_textInputSetEcho                    1033

// Filesystem functions.  All are in the 8000-8999 range.
#define _fnum_filesystemSync                      8001
#define _fnum_filesystemMount                     8002
#define _fnum_filesystemUnmount                   8003
#define _fnum_filesystemNumberMounted             8004
#define _fnum_filesystemFirstFilesystem           8005
#define _fnum_filesystemNextFilesystem            8006
#define _fnum_filesystemGetFree                   8007
#define _fnum_filesystemGetBlockSize              8008

// File functions.  All are in the 9000-9999 range.
#define _fnum_fileFixupPath                       9001
#define _fnum_fileFirst                           9002
#define _fnum_fileNext                            9003
#define _fnum_fileFind                            9004
#define _fnum_fileOpen                            9005
#define _fnum_fileClose                           9006
#define _fnum_fileRead                            9007
#define _fnum_fileWrite                           9008
#define _fnum_fileDelete                          9009
#define _fnum_fileDeleteSecure                    9010
#define _fnum_fileMakeDir                         9011
#define _fnum_fileRemoveDir                       9012
#define _fnum_fileCopy                            9013
#define _fnum_fileMove                            9014
#define _fnum_fileTimestamp                       9015
#define _fnum_fileStreamOpen                      9016
#define _fnum_fileStreamSeek                      9017
#define _fnum_fileStreamRead                      9018
#define _fnum_fileStreamWrite                     9019
#define _fnum_fileStreamFlush                     9020
#define _fnum_fileStreamClose                     9021

// Memory manager functions.  All are in the 10000-10999 range.
#define _fnum_memoryPrintUsage                    10001
#define _fnum_memoryRequestBlock                  10002
#define _fnum_memoryRequestPhysicalBlock          10003
#define _fnum_memoryReleaseBlock                  10004
#define _fnum_memoryReleaseAllByProcId            10005
#define _fnum_memoryChangeOwner                   10006

// Multitasker functions.  All are in the 11000-11999 range.
#define _fnum_multitaskerCreateProcess            11001
#define _fnum_multitaskerSpawn                    11002
#define _fnum_multitaskerGetCurrentProcessId      11003
#define _fnum_multitaskerGetProcessOwner          11004
#define _fnum_multitaskerGetProcessName           11005
#define _fnum_multitaskerGetProcessState          11006
#define _fnum_multitaskerSetProcessState          11007
#define _fnum_multitaskerGetProcessPriority       11008
#define _fnum_multitaskerSetProcessPriority       11009
#define _fnum_multitaskerGetProcessPrivilege      11010
#define _fnum_multitaskerGetCurrentDirectory      11011
#define _fnum_multitaskerSetCurrentDirectory      11012
#define _fnum_multitaskerGetProcessorTime         11013
#define _fnum_multitaskerYield                    11014
#define _fnum_multitaskerWait                     11015
#define _fnum_multitaskerBlock                    11016
#define _fnum_multitaskerKillProcess              11017
#define _fnum_multitaskerTerminate                11018
#define _fnum_multitaskerDumpProcessList          11019

// Loader functions.  All are in the 12000-12999 range.
#define _fnum_loaderLoadAndExec                   12001

// Real-time clock functions.  All are in the 13000-13999 range.
#define _fnum_rtcReadSeconds                      13001
#define _fnum_rtcReadMinutes                      13002
#define _fnum_rtcReadHours                        13003
#define _fnum_rtcReadDayOfWeek                    13004
#define _fnum_rtcReadDayOfMonth                   13005
#define _fnum_rtcReadMonth                        13006
#define _fnum_rtcReadYear                         13007
#define _fnum_rtcUptimeSeconds                    13008
#define _fnum_rtcDateTime                         13009

// Random number functions.  All are in the 14000-14999 range.
#define _fnum_randomUnformatted                   14001
#define _fnum_randomFormatted                     14002
#define _fnum_randomSeededUnformatted             14003
#define _fnum_randomSeededFormatted               14004

// Environment functions.  All are in the 15000-15999 range.
#define _fnum_environmentGet                      15001
#define _fnum_environmentSet                      15002
#define _fnum_environmentUnset                    15003
#define _fnum_environmentDump                     15004

// Window manager functions
#define _fnum_windowManagerStart                  16001
#define _fnum_windowManagerNewWindow              16002
#define _fnum_windowSetTitle                      16003
#define _fnum_windowSetSize                       16004
#define _fnum_windowSetLocation                   16005
#define _fnum_windowSetVisible                    16006

// Miscellaneous functions
#define _fnum_shutdown                            99001
#define _fnum_version                             99002


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
  // Do a syscall with 5 parameters
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


// These inline functions are used to call specific kernel functions.  
// There will be one of these for every API function.


//
// Text input/output functions
//

static inline int textGetForeground(void)
{
  // Proto: int kernelTextGetForeground(void);
  return (sysCall_0(_fnum_textGetForeground));
}

static inline int textSetForeground(int color)
{
  // Proto: int kernelTextSetForeground(int);
  return (sysCall_1(_fnum_textSetForeground, (void *) color));
}

static inline int textGetBackground(void)
{
  // Proto: int kernelTextGetBackground(void);
  return (sysCall_0(_fnum_textGetBackground));
}

static inline int textSetBackground(int color)
{
  // Proto: int kernelTextSetBackground(int);
  return (sysCall_1(_fnum_textSetBackground, (void *) color));
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

static inline int textInputCount(void)
{
  // Proto: int kernelTextInputCount(void);
  return (sysCall_0(_fnum_textInputCount));
}

static inline int textInputGetc(char *cp)
{
  // Proto: char kernelTextInputGetc(void);
  return (sysCall_1(_fnum_textInputGetc, cp));
}

static inline int textInputReadN(int num, char *buff)
{
  // Proto: int kernelTextInputReadN(int, char *);
  return (sysCall_2(_fnum_textInputReadN, (void *)num, buff));
}

static inline int textInputReadAll(char *buff)
{
  // Proto: int kernelTextInputReadAll(char *);
  return (sysCall_1(_fnum_textInputReadAll, buff));
}

static inline int textInputAppend(int ascii)
{
  // Proto: int kernelTextInputAppend(int);
  return (sysCall_1(_fnum_textInputAppend, (void *) ascii));
}

static inline int textInputAppendN(int num, char *str)
{
  // Proto: int kernelTextInputAppendN(int, char *);
  return (sysCall_2(_fnum_textInputAppendN, (void *) num, str));
}

static inline int textInputRemove(void)
{
  // Proto: int kernelTextInputRemove(void);
  return (sysCall_0(_fnum_textInputRemove));
}

static inline int textInputRemoveN(int num)
{
  // Proto: int kernelTextInputRemoveN(int);
  return (sysCall_1(_fnum_textInputRemoveN, (void *) num));
}

static inline int textInputRemoveAll(void)
{
  // Proto: int kernelTextInputRemoveAll(void);
  return (sysCall_0(_fnum_textInputRemoveAll));
}

static inline void textInputSetEcho(int onOff)
{
  // Proto: void kernelTextInputSetEcho(int);
  sysCall_1(_fnum_textInputSetEcho, (void *) onOff);
}


//
// Filesystem functions
//

static inline int filesystemMount(int disknum, const char *mp)
{
  // Proto: int kernelFilesystemMount(int, const char *);
  return (sysCall_2(_fnum_filesystemMount, (void *) disknum, (void *) mp));
}

static inline int filesystemSync(const char *fs)
{
  // Proto: int kernelFilesystemSync(const char *);
  return (sysCall_1(_fnum_filesystemSync, (void *)fs));
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

static inline void memoryPrintUsage(void)
{
  // Proto: void kernelMemoryPrintUsage(void);
  sysCall_0(_fnum_memoryPrintUsage);
}

static inline void *memoryRequestBlock(unsigned int size, unsigned int align,
				       const char *desc)
{
  // Proto: void *kernelMemoryRequestBlock(unsigned int, unsigned int, 
  //          char *);
  return ((void *) sysCall_3(_fnum_memoryRequestBlock, (void *) size, 
		    (void *) align, (void *) desc));
}

static inline void *memoryRequestPhysicalBlock(unsigned int size, 
				       unsigned int align, const char *desc)
{
  // Proto: void *kernelMemoryRequestPhysicalBlock(unsigned int, 
  //          unsigned int, char *);
  return ((void *) sysCall_3(_fnum_memoryRequestPhysicalBlock, (void *) size, 
		    (void *) align, (void *) desc));
}

static inline int memoryReleaseBlock(void *p)
{
  // Proto: int kernelMemoryReleaseBlock(void *);
  return (sysCall_1(_fnum_memoryReleaseBlock, p));
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

static inline int rtcReadDayOfWeek(void)
{
  // Proto: int kernelRtcReadDayOfWeek(void);
  return (sysCall_0(_fnum_rtcReadDayOfWeek));
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


// Window manager functions
static inline int windowManagerStart(void)
{
  // Proto: int kernelWindowManagerStart(void);
  return (sysCall_0(_fnum_windowManagerStart));
}

static inline int windowManagerNewWindow(int processId, char *title,
					 int xCoord, int yCoord, int width,
					 int height)
{
  // Proto: int kernelWindowManagerNewWindow(int, const char *, int, int,
  //                                         int, int);
  return (sysCall_6(_fnum_windowManagerNewWindow, (void *) processId,
		    (void *) title, (void *) xCoord, (void *) yCoord,
		    (void *) width, (void *) height));
}

static inline int windowSetTitle(int windowId, const char *title)
{
  // Proto: int kernelWindowSetTitle(int, const char *);
  return (sysCall_2(_fnum_windowSetTitle, (void *) windowId, (void *) title));
}

static inline int windowSetSize(int windowId, int width, int height)
{
  // Proto: int kernelWindowSetSize(int, int, int);
  return (sysCall_3(_fnum_windowSetSize, (void *) windowId, (void *) width,
		    (void *) height));
}

static inline int windowSetLocation(int windowId, int xCoord, int yCoord)
{
  // Proto: int kernelWindowSetLocation(int, int, int)
  return (sysCall_3(_fnum_windowSetLocation, (void *) windowId,
		    (void *) xCoord, (void *) yCoord));
}

static inline int windowSetVisible(int windowId, int visible)
{
  // Proto: int kernelWindowSetVisible(int, int);
  return (sysCall_2(_fnum_windowSetVisible, (void *) windowId,
		    (void *) visible));
}


//
// Miscellaneous functions
//

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
