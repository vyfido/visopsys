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
//  kernelMultitasker.h
//

#if !defined(_KERNELMULTITASKER_H)

#include "kernelDescriptor.h"
#include "kernelEnvironment.h"
#include "kernelText.h"
#include <time.h>
#include <sys/file.h>

// Definitions
#define MAX_PROCESSES ((GDT_SIZE -  RES_GLOBAL_DESCRIPTORS))
#define PRIORITY_LEVELS 8
#define PRIORITY_RATIO 3
#define PRIORITY_DEFAULT ((PRIORITY_LEVELS / 2) - 1)
#define TIME_SLICE_LENGTH 0x00002000
#define CPU_PERCENT_TIMESLICES 300
#define DEFAULT_STACK_SIZE 0x00040000
#define DEFAULT_SUPER_STACK_SIZE (DEFAULT_STACK_SIZE / 4)
#define MAX_PROCNAME_LENGTH 64
#define HOOK_TIMER_INT_NUMBER 0x20

// An enumeration listing possible process states
typedef enum
{
  running, ready, waiting, sleeping, stopped, zombie

} kernelProcessState;

// An enumeration listing possible process types
typedef enum
{
  process, thread

} kernelProcessType;


// A structure representing x86 TSSes (Task State Sements)
typedef volatile struct
{
  unsigned int oldTSS;
  unsigned int ESP0;
  unsigned int SS0;
  unsigned int ESP1;
  unsigned int SS1;
  unsigned int ESP2;
  unsigned int SS2;
  unsigned int CR3;
  unsigned int EIP;
  unsigned int EFLAGS;
  unsigned int EAX;
  unsigned int ECX;
  unsigned int EDX;
  unsigned int EBX;
  unsigned int ESP;
  unsigned int EBP;
  unsigned int ESI;
  unsigned int EDI;
  unsigned int ES;
  unsigned int CS;
  unsigned int SS;
  unsigned int DS;
  unsigned int FS;
  unsigned int GS;
  unsigned int LDTSelector;
  unsigned int IOMap;

} kernelTSS;


// A structure for processes
typedef volatile struct
{
  char processName[MAX_PROCNAME_LENGTH];
  int userId;
  int processId;
  kernelProcessType type;
  int priority;
  int privilege;
  int parentProcessId;
  int descendentThreads;
  unsigned int startTime;
  unsigned cpuTime;
  int cpuPercent;
  unsigned int waitTime;
  unsigned int waitUntil;
  int waitForProcess;
  int waitThreadTerm;
  kernelProcessState state;
  void *codeDataPointer;
  unsigned int codeDataSize;
  void *userStack;
  unsigned int userStackSize;
  void *superStack;
  unsigned int superStackSize;
  kernelSelector tssSelector;
  kernelTSS taskStateSegment;
  char currentDirectory[MAX_PATH_LENGTH];
  kernelEnvironment *environment;
  kernelTextStream *textInputStream;
  kernelTextStream *textOutputStream;

} kernelProcess;

// When in system calls, processes will be allowed to access information
// about themselves
extern kernelProcess *currentProcess;

// Functions exported by kernelMultitasker.c
int kernelMultitaskerInitialize(void);
int kernelMultitaskerShutdown(int);
void kernelMultitaskerDumpProcessList(void);
int kernelMultitaskerGetCurrentProcessId(void);
int kernelMultitaskerCreateProcess(void *, unsigned int, const char *,
				   int, void *);
int kernelMultitaskerSpawn(void *, const char *, int, void *);
int kernelMultitaskerSpawnKernelThread(void *, const char *, int, void *);
int kernelMultitaskerPassArgs(int, int, void *);
int kernelMultitaskerGetProcessOwner(int);
const char *kernelMultitaskerGetProcessName(int);
int kernelMultitaskerGetProcessState(int, kernelProcessState *);
int kernelMultitaskerSetProcessState(int, kernelProcessState);
int kernelMultitaskerGetProcessPriority(int);
int kernelMultitaskerSetProcessPriority(int, int);
int kernelMultitaskerGetProcessPrivilege(int);
int kernelMultitaskerSetProcessPrivilege(int, int);
int kernelMultitaskerGetCurrentDirectory(char *, int);
int kernelMultitaskerSetCurrentDirectory(char *);
kernelTextStream *kernelMultitaskerGetTextInput(void);
int kernelMultitaskerSetTextInput(int, kernelTextStream *);
kernelTextStream *kernelMultitaskerGetTextOutput(void);
int kernelMultitaskerSetTextOutput(int, kernelTextStream *);
int kernelMultitaskerGetProcessorTime(clock_t *);
void kernelMultitaskerYield(void);
void kernelMultitaskerWait(unsigned int);
int kernelMultitaskerBlock(int);
int kernelMultitaskerKillProcess(int);
int multitaskerKillAllProcesses(void);
int kernelMultitaskerTerminate(int);

#define _KERNELMULTITASKER_H
#endif
