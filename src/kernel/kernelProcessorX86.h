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
//  kernelProcessorX86.h
//

#if !defined(_KERNELPROCESSORX86_H)

// Definitions
#define PAGEPRESENT_BIT   0x00000001
#define WRITABLE_BIT      0x00000002
#define USER_BIT          0x00000004
#define WRITETHROUGH_BIT  0x00000008
#define CACHEDISABLE_BIT  0x00000010
#define GLOBAL_BIT        0x00000100


#define kernelProcessorStop()      \
  __asm__ __volatile__ ("cli \n\t" \
			"hlt")

#define kernelProcessorReboot()                 \
  __asm__ __volatile__ ("cli \n\t"              \
			"movl $0xFE, %eax \n\t" \
			"outb %al, $0x64 \n\t"  \
			"hlt")

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
			: : "a" (selector));

#define kernelProcessorSetGDT(ptr, size)      \
  __asm__ __volatile__ ("pushfl \n\t"         \
			"cli \n\t"            \
			"pushl %0 \n\t"       \
			"pushw %%ax \n\t"     \
			"lgdt (%%esp) \n\t"   \
			"addl $6, %%esp \n\t" \
			"popfl"               \
			: : "r" (ptr), "a" (size));

#define kernelProcessorSetIDT(ptr, size)      \
  __asm__ __volatile__ ("pushfl \n\t"         \
			"cli \n\t"            \
			"pushl %0 \n\t"       \
			"pushw %%ax \n\t"     \
			"lidt (%%esp) \n\t"   \
			"addl $6, %%esp \n\t" \
			"popfl"               \
			: : "r" (ptr), "a" (size));

#define kernelProcessorClearAddressCache(address) \
  __asm__ __volatile__ ("invlpg %0"               \
                        : : "m" (*((char *)(address))))

#define kernelProcessorGetCR3(variable)  \
  __asm__ __volatile__ ("movl %%cr3, %0" \
                        : "=r" (variable))

#define kernelProcessorSetCR3(variable)  \
  __asm__ __volatile__ ("movl %0, %%cr3" \
                        : : "r" (variable))

#define kernelProcessorGetESP(variable)  \
  __asm__ __volatile__ ("movl %%esp, %0" \
                        : "=r" (variable))

#define kernelProcessorIntReturn() __asm__ __volatile__ ("iret")

#define kernelProcessorLoadTaskReg(selector) \
  __asm__ __volatile__ ("pushfl \n\t"        \
                        "cli \n\t"           \
                        "ltr %%ax \n\t"      \
                        "popfl"              \
                        : : "a" (selector))

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

#define kernelProcessorIntStatus(variable) \
  __asm__ __volatile__ ("pushfl \n\t"      \
                        "popl %0 \n\t"     \
                        "shr $9, %0 \n\t"  \
                        "and $1, %0"       \
                        : "=r" (variable))

#define kernelProcessorEnableInts() __asm__ __volatile__ ("sti")

#define kernelProcessorDisableInts() __asm__ __volatile__ ("cli")

#define kernelProcessorSuspendInts(variable) \
{                                            \
  kernelProcessorIntStatus(variable);        \
  kernelProcessorDisableInts();              \
}

#define kernelProcessorRestoreInts(variable) \
{                                            \
  if (variable)                              \
    kernelProcessorEnableInts();             \
}

#define kernelProcessorIsrEnter()              \
  __asm__ __volatile__ ("cli \n\t"             \
			"movl %ebp, %esp \n\t" \
			"pushal")

#define kernelProcessorIsrExit()               \
  __asm__ __volatile__ ("movl %ebp, %esp \n\t" \
                        "subl $32, %esp \n\t"  \
                        "popal \n\t"           \
                        "popl %ebp \n\t"       \
			"iret")

#define kernelProcessorApiEnter()              \
  __asm__ __volatile__ ("movl %ebp, %esp \n\t" \
			"pushal")

#define kernelProcessorApiExit(code)             \
  __asm__ __volatile__ ("movl %%ebp, %%esp \n\t" \
			"subl $32, %%esp \n\t"   \
			"popal \n\t"             \
			"popl %%ebp \n\t"        \
			"movl %0, %%eax \n\t"    \
			"lret" : : "m" (code));

#define kernelProcessorDelay()                 \
  __asm__ __volatile__ ("pushal \n\t"          \
			"mov $0x3F6, %dx \n\t" \
			"inb %dx, %al \n\t"    \
			"inb %dx, %al \n\t"    \
			"inb %dx, %al \n\t"    \
			"inb %dx, %al \n\t"    \
			"popal")

#define _KERNELPROCESSORX86_H
#endif
