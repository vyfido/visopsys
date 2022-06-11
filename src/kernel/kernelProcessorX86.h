//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  kernelProcessorX86.h
//

#if !defined(_KERNELPROCESSORX86_H)

#define kernelProcessorId(arg, rega, regb, regc, regd)                       \
  __asm__ __volatile__ ("cpuid"                                              \
			: "=a" (rega), "=b" (regb), "=c" (regc), "=d" (regd) \
			: "a" (arg))


#define kernelProcessorStop()      \
  __asm__ __volatile__ ("cli \n\t" \
			"hlt")

#define kernelProcessorReboot()                  \
  __asm__ __volatile__ ("cli \n\t"               \
			"movl $0xFE, %%eax \n\t" \
			"outb %%al, $0x64 \n\t"  \
			"hlt" : : : "%eax")

#define kernelProcessorCopyDwords(src, dest, count) \
  __asm__ __volatile__ ("pushal \n\t"               \
			"pushfl \n\t"               \
			"cld \n\t"                  \
			"rep movsl \n\t"            \
			"popfl \n\t"                \
			"popal"                     \
			: : "S" (src), "D" (dest), "c" (count))

#define kernelProcessorCopyBytes(src, dest, count) \
  __asm__ __volatile__ ("pushal \n\t"              \
			"pushfl \n\t"              \
			"cld \n\t"                 \
			"rep movsb \n\t"           \
			"popfl \n\t"               \
			"popal"                    \
			: : "S" (src), "D" (dest), "c" (count))

#define kernelProcessorWriteDwords(value, dest, count) \
  __asm__ __volatile__ ("pushal \n\t"                  \
			"pushfl \n\t"                  \
			"cld \n\t"                     \
			"rep stosl \n\t"               \
			"popfl \n\t"                   \
			"popal"                        \
			: : "a" (value), "D" (dest), "c" (count))

#define kernelProcessorWriteBytes(value, dest, count) \
  __asm__ __volatile__ ("pushal \n\t"                 \
			"pushfl \n\t"                 \
			"cld \n\t"                    \
			"rep stosb \n\t"              \
			"popfl \n\t"                  \
			"popal"                       \
			: : "a" (value), "D" (dest), "c" (count))

#define kernelProcessorFarCall(selector)      \
  __asm__ __volatile__ ("pushfl \n\t"         \
			"pushl %%eax \n\t"    \
			"pushl $0 \n\t"       \
			"lcall *(%%esp) \n\t" \
			"addl $8, %%esp \n\t" \
			"popfl"               \
			: : "a" (selector))

#define kernelProcessorSetGDT(ptr, size)      \
  __asm__ __volatile__ ("pushfl \n\t"         \
			"cli \n\t"            \
			"pushl %0 \n\t"       \
			"pushw %%ax \n\t"     \
			"lgdt (%%esp) \n\t"   \
			"addl $6, %%esp \n\t" \
			"popfl"               \
			: : "r" (ptr), "a" (size))

#define kernelProcessorSetIDT(ptr, size)      \
  __asm__ __volatile__ ("pushfl \n\t"         \
			"cli \n\t"            \
			"pushl %0 \n\t"       \
			"pushw %%ax \n\t"     \
			"lidt (%%esp) \n\t"   \
			"addl $6, %%esp \n\t" \
			"popfl"               \
			: : "r" (ptr), "a" (size))

#define kernelProcessorClearAddressCache(addr) \
  __asm__ __volatile__ ("invlpg %0"            \
                        : : "m" (*((char *)(addr))))

#define kernelProcessorGetCR0(variable)  \
  __asm__ __volatile__ ("movl %%cr0, %0" \
                        : "=r" (variable))

#define kernelProcessorSetCR0(variable)  \
  __asm__ __volatile__ ("movl %0, %%cr0" \
                        : : "r" (variable))

#define kernelProcessorGetCR3(variable)  \
  __asm__ __volatile__ ("movl %%cr3, %0" \
                        : "=r" (variable))

#define kernelProcessorSetCR3(variable)  \
  __asm__ __volatile__ ("movl %0, %%cr3" \
                        : : "r" (variable))

#define kernelProcessorIntReturn() __asm__ __volatile__ ("iret")

#define kernelProcessorFarReturn() __asm__ __volatile__ ("lret")

#define kernelProcessorLoadTaskReg(selector) \
  __asm__ __volatile__ ("pushfl \n\t"        \
                        "cli \n\t"           \
                        "ltr %%ax \n\t"      \
                        "popfl"              \
                        : : "a" (selector))

#define kernelProcessorGetTaskReg(variable) \
  __asm__ __volatile__ ("str %0"            \
                        : "=r" (variable))

#define kernelProcessorGetFlags(variable) \
  __asm__ __volatile__ ("pushfl \n\t"     \
                        "popl %0"         \
                        : "=r" (variable))

#define kernelProcessorSetFlags(variable) \
  __asm__ __volatile__ ("pushl %0 \n\t"   \
                        "popfl"           \
                        : : "r" (variable))

#define kernelProcessorInPort8(port, data) \
  __asm__ __volatile__ ("inb %%dx, %%al"   \
                        : "=a" (data) : "d" (port))

#define kernelProcessorOutPort8(port, data) \
  __asm__ __volatile__ ("outb %%al, %%dx"   \
                        : : "a" (data), "d" (port))

#define kernelProcessorRepInPort8(port, buffer, reads) \
  __asm__ __volatile__ ("pushal \n\t"                  \
			"pushfl \n\t"                  \
                        "cli \n\t"                     \
                        "cld \n\t"                     \
                        "rep insb \n\t"                \
                        "popfl \n\t"                   \
			"popal"                        \
                        : : "d" (port), "D" (buffer), "c" (reads))

#define kernelProcessorRepOutPort8(port, buffer, writes) \
  __asm__ __volatile__ ("pushal \n\t"                    \
			"pushfl \n\t"                    \
                        "cli \n\t"                       \
                        "cld \n\t"                       \
                        "rep outsb \n\t"                 \
                        "popfl \n\t"                     \
			"popal"                          \
                        : : "d" (port), "S" (buffer), "c" (writes))

#define kernelProcessorInPort16(port, data) \
  __asm__ __volatile__ ("inw %%dx, %%ax"    \
                        : "=a" (data) : "d" (port))

#define kernelProcessorOutPort16(port, data) \
  __asm__ __volatile__ ("outw %%ax, %%dx"    \
                        : : "a" (data), "d" (port))

#define kernelProcessorRepInPort16(port, buffer, reads) \
  __asm__ __volatile__ ("pushal \n\t"                   \
			"pushfl \n\t"                   \
                        "cli \n\t"                      \
                        "cld \n\t"                      \
                        "rep insw \n\t"                 \
                        "popfl \n\t"                    \
			"popal"                         \
                        : : "d" (port), "D" (buffer), "c" (reads))

#define kernelProcessorRepOutPort16(port, buffer, writes) \
  __asm__ __volatile__ ("pushal \n\t"                     \
			"pushfl \n\t"                     \
                        "cli \n\t"                        \
                        "cld \n\t"                        \
                        "rep outsw \n\t"                  \
                        "popfl \n\t"                      \
			"popal"                           \
                        : : "d" (port), "S" (buffer), "c" (writes))

#define kernelProcessorInPort32(port, data) \
  __asm__ __volatile__ ("inl %%dx, %%eax"   \
                        : "=a" (data) : "d" (port))

#define kernelProcessorOutPort32(port, data) \
  __asm__ __volatile__ ("outl %%eax, %%dx"   \
                        : : "a" (data), "d" (port))

#define kernelProcessorRepInPort32(port, buffer, reads) \
  __asm__ __volatile__ ("pushal \n\t"                   \
			"pushfl \n\t"                   \
                        "cli \n\t"                      \
                        "cld \n\t"                      \
                        "rep insl \n\t"                 \
                        "popfl \n\t"                    \
			"popal"                         \
                        : : "d" (port), "D" (buffer), "c" (reads))

#define kernelProcessorRepOutPort32(port, buffer, writes) \
  __asm__ __volatile__ ("pushal \n\t"                     \
			"pushfl \n\t"                     \
                        "cli \n\t"                        \
                        "cld \n\t"                        \
                        "rep outsl \n\t"                  \
                        "popfl \n\t"                      \
			"popal"                           \
                        : : "d" (port), "S" (buffer), "c" (writes))

#define kernelProcessorIntStatus(variable) \
  __asm__ __volatile__ ("pushfl \n\t"      \
                        "popl %0 \n\t"     \
                        "shr $9, %0 \n\t"  \
                        "and $1, %0"       \
                        : "=r" (variable))

#define kernelProcessorEnableInts() __asm__ __volatile__ ("sti")

#define kernelProcessorDisableInts() __asm__ __volatile__ ("cli")

#define kernelProcessorSuspendInts(variable) do { \
  kernelProcessorIntStatus(variable);             \
  kernelProcessorDisableInts();                   \
} while (0)

#define kernelProcessorRestoreInts(variable) do { \
  if (variable)                                   \
    kernelProcessorEnableInts();                  \
} while (0)

#define kernelProcessorPush(value) \
  __asm__ __volatile__ ("pushl %0" : : "r" (value) : "%esp")

#define kernelProcessorPop(variable) \
  __asm__ __volatile__ ("popl %0" : "=r" (variable) : : "%esp")

#define kernelProcessorPushRegs() __asm__ __volatile__ ("pushal" : : : "%esp")

#define kernelProcessorPopRegs() __asm__ __volatile__ ("popal" : : : "%esp")

#define kernelProcessorPushFlags() __asm__ __volatile__ ("pushfl" : : : "%esp")

#define kernelProcessorPopFlags() __asm__ __volatile__ ("popfl" : : : "%esp")

#define kernelProcessorPopFrame()                \
  __asm__ __volatile__ ("movl %%ebp, %%esp \n\t" \
			"popl %%ebp" : : : "%esp" )

#define kernelProcessorGetStackPointer(addr) \
  __asm__ __volatile__ ("movl %%esp, %0" : "=r" (addr))

#define kernelProcessorSetStackPointer(addr) \
  __asm__ __volatile__ ("movl %0, %%esp" : : "r" (addr) : "%esp")

#define kernelProcessorIsrEnter(stAddr) do { \
  kernelProcessorDisableInts();              \
  kernelProcessorPushRegs();                 \
  kernelProcessorPushFlags();                \
  kernelProcessorGetStackPointer(stAddr);    \
} while (0)  

#define kernelProcessorIsrExit(stAddr) do { \
  kernelProcessorSetStackPointer(stAddr);   \
  kernelProcessorPopFlags();                \
  kernelProcessorPopRegs();                 \
  kernelProcessorPopFrame();                \
  kernelProcessorEnableInts();              \
  kernelProcessorIntReturn();               \
} while (0)

#define kernelProcessorExceptionEnter(stAddr, exAddr) do {    \
  __asm__ __volatile__ ("movl 4(%%ebp), %0" : "=r" (exAddr)); \
  kernelProcessorPushRegs();                                  \
  kernelProcessorGetStackPointer(stAddr);                     \
} while (0)  

#define kernelProcessorExceptionExit(stAddr) do { \
  kernelProcessorSetStackPointer(stAddr);         \
  kernelProcessorPopRegs();                       \
  kernelProcessorPopFrame();                      \
  kernelProcessorIntReturn();                     \
} while (0)

#define kernelProcessorApiEnter(stAddr) do { \
  kernelProcessorPushRegs();                 \
  kernelProcessorGetStackPointer(stAddr);    \
} while (0)  

#define kernelProcessorApiExit(stAddr, code) do {                   \
  kernelProcessorSetStackPointer(stAddr);                           \
  kernelProcessorPopRegs();                                         \
  __asm__ __volatile__ ("movl %0, %%eax" : : "r" (code) : "%eax" ); \
  kernelProcessorPopFrame();                                        \
  kernelProcessorFarReturn();                                       \
} while (0)

#define kernelProcessorDelay()                 \
  __asm__ __volatile__ ("pushal \n\t"          \
			"mov $0x3F6, %dx \n\t" \
			"inb %dx, %al \n\t"    \
			"inb %dx, %al \n\t"    \
			"inb %dx, %al \n\t"    \
			"inb %dx, %al \n\t"    \
			"popal")

#define kernelProcessorClearTaskSwitched() __asm__ __volatile__ ("clts")

#define kernelProcessorFpuInit() __asm__ __volatile__ ("fninit")

#define kernelProcessorFpuStateSave(addr) \
  __asm__ __volatile__ ("fnsave %0" : : "m" (*(addr)))

#define kernelProcessorFpuStateRestore(addr) \
  __asm__ __volatile__ ("frstor %0" : : "m" (*(addr)))

#define kernelProcessorGetFpuStatus(code) \
  __asm__ __volatile__ ("fnstsw %0" : "=a" (code))

static inline unsigned short kernelProcessorSwap16(unsigned short variable)
{
  volatile unsigned short tmp = (variable);
  __asm__ __volatile__ ("rolw $8, %0" : "=r" (tmp) : "r" (tmp));
  return (tmp);
}

static inline unsigned kernelProcessorSwap32(unsigned variable)
{
  volatile unsigned tmp = (variable);
  __asm__ __volatile__ ("bswap %0" : "=r" (tmp) : "r" (tmp));
  return (tmp);
}

#define kernelProcessorLock(lck, proc)         \
  __asm__ __volatile__ ("lock cmpxchgl %1, %2" \
			: : "a" (0), "r" (proc), "m" (lck) : "memory")

#define _KERNELPROCESSORX86_H
#endif
