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
//  kernelMultitasker.h
//

#if !defined(_KERNELMULTITASKER_H)

#include "kernelDescriptor.h"
#include "kernelEnvironment.h"
#include "kernelText.h"
#include <time.h>
#include <sys/process.h>

// Definitions
#define MAX_PROCESSES ((GDT_SIZE - RES_GLOBAL_DESCRIPTORS))
#define PRIORITY_LEVELS 8
#define DEFAULT_STACK_SIZE (32 * 1024)
#define DEFAULT_SUPER_STACK_SIZE (32 * 1024)
#define HOOK_TIMER_INT_NUMBER 0x20
#define TIME_SLICE_LENGTH 0x00002000
#define CPU_PERCENT_TIMESLICES 300
#define PRIORITY_RATIO 3
#define PRIORITY_DEFAULT ((PRIORITY_LEVELS / 2) - 1)

// A structure representing x86 TSSes (Task State Sements)
typedef volatile struct
{
  unsigned oldTSS;
  unsigned ESP0;
  unsigned SS0;
  unsigned ESP1;
  unsigned SS1;
  unsigned ESP2;
  unsigned SS2;
  unsigned CR3;
  unsigned EIP;
  unsigned EFLAGS;
  unsigned EAX;
  unsigned ECX;
  unsigned EDX;
  unsigned EBX;
  unsigned ESP;
  unsigned EBP;
  unsigned ESI;
  unsigned EDI;
  unsigned ES;
  unsigned CS;
  unsigned SS;
  unsigned DS;
  unsigned FS;
  unsigned GS;
  unsigned LDTSelector;
  unsigned IOMap;

} kernelTSS;

// A structure for processes
typedef volatile struct
{
  char processName[MAX_PROCNAME_LENGTH];
  int userId;
  int processId;
  processType type;
  int priority;
  int privilege;
  int parentProcessId;
  int descendentThreads;
  unsigned startTime;
  unsigned cpuTime;
  int cpuPercent;
  unsigned yieldSlice;
  unsigned waitTime;
  unsigned waitUntil;
  int waitForProcess;
  int blockingExitCode;
  processState state;
  void *codeDataPointer;
  unsigned codeDataSize;
  void *userStack;
  unsigned userStackSize;
  void *superStack;
  unsigned superStackSize;
  kernelSelector tssSelector;
  kernelTSS taskStateSegment;
  char currentDirectory[MAX_PATH_LENGTH];
  kernelEnvironment *environment;
  kernelTextInputStream *textInputStream;
  kernelTextOutputStream *textOutputStream;

} kernelProcess;

// When in system calls, processes will be allowed to access information
// about themselves
extern kernelProcess *kernelCurrentProcess;

// Functions exported by kernelMultitasker.c
int kernelMultitaskerInitialize(void);
int kernelMultitaskerShutdown(int);
void kernelExceptionHandler(void);
void kernelMultitaskerDumpProcessList(void);
int kernelMultitaskerGetCurrentProcessId(void);
int kernelMultitaskerGetProcess(int, process *);
int kernelMultitaskerGetProcessByName(const char *, process *);
int kernelMultitaskerGetProcesses(void *, unsigned);
int kernelMultitaskerCreateProcess(void *, unsigned, const char *, int, int,
				   void *);
int kernelMultitaskerSpawn(void *, const char *, int, void *);
int kernelMultitaskerSpawnKernelThread(void *, const char *, int, void *);
//int kernelMultitaskerFork(void);
int kernelMultitaskerPassArgs(int, int, void *);
int kernelMultitaskerGetProcessState(int, processState *);
int kernelMultitaskerSetProcessState(int, processState);
int kernelMultitaskerProcessIsAlive(int);
int kernelMultitaskerGetProcessPriority(int);
int kernelMultitaskerSetProcessPriority(int, int);
int kernelMultitaskerGetProcessPrivilege(int);
int kernelMultitaskerGetCurrentDirectory(char *, int);
int kernelMultitaskerSetCurrentDirectory(char *);
kernelTextInputStream *kernelMultitaskerGetTextInput(void);
int kernelMultitaskerSetTextInput(int, kernelTextInputStream *);
kernelTextOutputStream *kernelMultitaskerGetTextOutput(void);
int kernelMultitaskerSetTextOutput(int, kernelTextOutputStream *);
int kernelMultitaskerDuplicateIO(int, int, int);
int kernelMultitaskerGetProcessorTime(clock_t *);
void kernelMultitaskerYield(void);
void kernelMultitaskerWait(unsigned);
int kernelMultitaskerBlock(int);
int kernelMultitaskerDetach(void);
int kernelMultitaskerKillProcess(int, int);
int kernelMultitaskerKillByName(const char *, int);
int kernelMultitaskerKillAll(void);
int kernelMultitaskerTerminate(int);

#define _KERNELMULTITASKER_H
#endif
