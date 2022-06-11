//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  processor.h
//

#ifndef _PROCESSOR_H
#define _PROCESSOR_H

// This file contains macros for processor-specific operations.  At the moment
// it's only for X86 processors.

#define PROCESSOR_LITTLE_ENDIAN			1

// Model-specific registers that we use
#define X86_MSR_APICBASE				0x1B
#define X86_MSR_PAT						0x277

// Bitfields for the APICBASE MSR
#define X86_MSR_APICBASE_BASEADDR		0xFFFFF000
#define X86_MSR_APICBASE_APICENABLE		0x00000800
#define X86_MSR_APICBASE_BSP			0x00000100

// Encodings for PAT MSR fields
#define X86_MSR_PATENC_UC				0x00	// Strong uncacheable
#define X86_MSR_PATENC_WC				0x01	// Write combining
#define X86_MSR_PATENC_WT				0x04	// Write through
#define X86_MSR_PATENC_WP				0x05	// Write protected
#define X86_MSR_PATENC_WB				0x06	// Write back
#define X86_MSR_PATENC_UC_				0x07	// Uncacheable

// Our settings for the PAT
#define X86_PAT7						X86_MSR_PATENC_UC
#define X86_PAT6						X86_MSR_PATENC_UC_
#define X86_PAT5						X86_MSR_PATENC_WC	// non-default
#define X86_PAT4						X86_MSR_PATENC_WB
#define X86_PAT3						X86_MSR_PATENC_UC
#define X86_PAT2						X86_MSR_PATENC_UC_
#define X86_PAT1						X86_MSR_PATENC_WT
#define X86_PAT0						X86_MSR_PATENC_WB

// Constant indexes for the above
#define X86_PAT_UC						7
#define X86_PAT_UC_						6
#define X86_PAT_WC						5
#define X86_PAT_WB						4
#define X86_PAT_WT						1

// Page entry bitfield selector for using the PAT
#define X86_PATSELECTOR(flags, index) do { \
	if ((index) & 0x4) (flags) |= X86_PAGEFLAG_PAT; \
	else (flags) &= ~X86_PAGEFLAG_PAT; \
	if ((index) & 0x2) (flags) |= X86_PAGEFLAG_CACHEDISABLE; \
	else (flags) &= ~X86_PAGEFLAG_CACHEDISABLE; \
	if ((index) & 0x1) (flags) |= X86_PAGEFLAG_WRITETHROUGH; \
	else (flags) &= ~X86_PAGEFLAG_WRITETHROUGH; \
} while (0)

// For setting the PAT MSRs
#define X86_PATMSR_HI	\
	((X86_PAT7 << 24) | (X86_PAT6 << 16) | (X86_PAT5 << 8) | X86_PAT4)
#define X86_PATMSR_LO	\
	((X86_PAT3 << 24) | (X86_PAT2 << 16) | (X86_PAT1 << 8) | X86_PAT0)

// Paging constants
#define X86_PAGE_TABLES_PER_DIR			1024
#define X86_PAGES_PER_TABLE				1024

// Page entry bitfield values
#define X86_PAGEFLAG_PRESENT			0x0001
#define X86_PAGEFLAG_WRITABLE			0x0002
#define X86_PAGEFLAG_USER				0x0004
#define X86_PAGEFLAG_WRITETHROUGH		0x0008
#define X86_PAGEFLAG_CACHEDISABLE		0x0010
#define X86_PAGEFLAG_ACCESSED			0x0020
#define X86_PAGEFLAG_DIRTY				0x0040
#define X86_PAGEFLAG_PAT				0x0080
#define X86_PAGEFLAG_GLOBAL				0x0100

// X86-specific CPU features
typedef struct {
	int cpuid1;
	unsigned cpuid1Ecx;
	unsigned cpuid1Edx;
	int cpuid8_1;
	unsigned cpuid8_1Ecx;
	unsigned cpuid8_1Edx;

} x86CpuFeatures;

//
// Processor registers
//

#define processorId(arg, rega, regb, regc, regd) \
	__asm__ __volatile__ ("cpuid" : "=a" (rega), "=b" (regb), "=c" (regc), \
		"=d" (regd) : "a" (arg))

#define processorReadMsr(msr, rega, regd) \
	__asm__ __volatile__ ("rdmsr" : "=a" (rega), "=d" (regd) : "c" (msr));

#define processorWriteMsr(msr, rega, regd) \
	__asm__ __volatile__ ("wrmsr" : : "a" (rega), "d" (regd), "c" (msr));

#define processorGetCR0(variable) \
	__asm__ __volatile__ ("movl %%cr0, %0" : "=r" (variable))

#define processorSetCR0(variable) \
	__asm__ __volatile__ ("movl %0, %%cr0" : : "r" (variable))

#define processorGetCR3(variable) \
	__asm__ __volatile__ ("movl %%cr3, %0" : "=r" (variable))

#define processorSetCR3(variable) \
	__asm__ __volatile__ ("movl %0, %%cr3" : : "r" (variable))

#define processorGetCR4(variable) \
	__asm__ __volatile__ ("movl %%cr4, %0" : "=r" (variable))

#define processorSetCR4(variable) \
	__asm__ __volatile__ ("movl %0, %%cr4" : : "r" (variable))

#define processorTimestamp(hi, lo) do { \
	processorId(0, hi, hi, hi, hi); /* serialize */ \
	__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi)); \
} while (0)

//
// Stack operations
//

#define processorPush(value) \
	__asm__ __volatile__ ("pushl %0" : : "r" (value) : "%esp")

#define processorPop(variable) \
	__asm__ __volatile__ ("popl %0" : "=r" (variable) : : "%esp")

#define processorPushRegs() __asm__ __volatile__ ("pushal" : : : "%esp")

#define processorPopRegs() __asm__ __volatile__ ("popal" : : : "%esp")

#define processorPushFlags() __asm__ __volatile__ ("pushfl" : : : "%esp")

#define processorPopFlags() __asm__ __volatile__ ("popfl" : : : "%esp")

#define processorGetStackPointer(addr) \
	__asm__ __volatile__ ("movl %%esp, %0" : "=r" (addr))

#define processorSetStackPointer(addr) \
	__asm__ __volatile__ ("movl %0, %%esp" : : "r" (addr) : "%esp")

//
// Interrupts
//

#define processorIntStatus(variable) do { \
	processorPushFlags(); \
	processorPop(variable); \
	variable = ((variable >> 9) & 1); \
} while (0)

#define processorEnableInts() __asm__ __volatile__ ("sti")

#define processorDisableInts() __asm__ __volatile__ ("cli")

#define processorSuspendInts(variable) do { \
	processorIntStatus(variable); \
	processorDisableInts(); \
} while (0)

#define processorRestoreInts(variable) do { \
	if (variable) \
		processorEnableInts(); \
} while (0)

//
// Memory copying
//

#define processorSetDirection() __asm__ __volatile__ ("std")

#define processorClearDirection() __asm__ __volatile__ ("cld")

#define processorCopyBytes(src, dest, count) do { \
	processorPushRegs(); \
	processorPushFlags(); \
	processorClearDirection(); \
	__asm__ __volatile__ ( \
		"rep movsb" \
		: : "S" (src), "D" (dest), "c" (count)); \
	processorPopFlags(); \
	processorPopRegs(); \
} while (0)

#define processorCopyBytesBackwards(src, dest, count) do { \
	processorPushRegs(); \
	processorPushFlags(); \
	processorSetDirection(); \
	__asm__ __volatile__ ( \
		"rep movsb" \
		: : "S" (src), "D" (dest), "c" (count)); \
	processorPopFlags(); \
	processorPopRegs(); \
} while (0)

#define processorCopyDwords(src, dest, count) do { \
	processorPushRegs(); \
	processorPushFlags(); \
	processorClearDirection(); \
	__asm__ __volatile__ ( \
		"rep movsl" \
		: : "S" (src), "D" (dest), "c" (count)); \
	processorPopFlags(); \
	processorPopRegs(); \
} while (0)

#define processorCopyDwordsBackwards(src, dest, count) do { \
	processorPushRegs(); \
	processorPushFlags(); \
	processorSetDirection(); \
	__asm__ __volatile__ ( \
		"rep movsl" \
		: : "S" (src), "D" (dest), "c" (count)); \
	processorPopFlags(); \
	processorPopRegs(); \
} while (0)

#define processorWriteBytes(value, dest, count) do { \
	processorPushRegs(); \
	processorPushFlags(); \
	processorClearDirection(); \
	__asm__ __volatile__ ( \
		"rep stosb" \
		: : "a" (value), "D" (dest), "c" (count)); \
	processorPopFlags(); \
	processorPopRegs(); \
} while (0)

#define processorWriteWords(value, dest, count) do { \
	processorPushRegs(); \
	processorPushFlags(); \
	processorClearDirection(); \
	__asm__ __volatile__ ( \
		"rep stosw" \
		: : "a" (value), "D" (dest), "c" (count)); \
	processorPopFlags(); \
	processorPopRegs(); \
} while (0)

#define processorWriteDwords(value, dest, count) do { \
	processorPushRegs(); \
	processorPushFlags(); \
	processorClearDirection(); \
	__asm__ __volatile__ ( \
		"rep stosl" \
		: : "a" (value), "D" (dest), "c" (count)); \
	processorPopFlags(); \
	processorPopRegs(); \
} while (0)

//
// Port I/O
//

#define processorInPort8(port, data) \
	__asm__ __volatile__ ("inb %%dx, %%al" : "=a" (data) : "d" (port))

#define processorOutPort8(port, data) \
	__asm__ __volatile__ ("outb %%al, %%dx" : : "a" (data), "d" (port))

#define processorInPort16(port, data) \
	__asm__ __volatile__ ("inw %%dx, %%ax" : "=a" (data) : "d" (port))

#define processorOutPort16(port, data) \
	__asm__ __volatile__ ("outw %%ax, %%dx" : : "a" (data), "d" (port))

#define processorInPort32(port, data) \
	__asm__ __volatile__ ("inl %%dx, %%eax" : "=a" (data) : "d" (port))

#define processorOutPort32(port, data) \
	__asm__ __volatile__ ("outl %%eax, %%dx" : : "a" (data), "d" (port))

//
// Task-related (multiasking, interrupt/exception handling, API)
//

#define processorSetGDT(ptr, size) do { \
	processorPushFlags(); \
	processorDisableInts(); \
	processorPush(ptr); \
	__asm__ __volatile__ ( \
		"pushw %%ax \n\t" \
		"lgdt (%%esp) \n\t" \
		"addl $6, %%esp" \
		: : "a" (size) : "%esp"); \
	processorPopFlags(); \
} while (0)

#define processorSetIDT(ptr, size) do { \
	processorPushFlags(); \
	processorDisableInts(); \
	processorPush(ptr); \
	__asm__ __volatile__ ( \
		"pushw %%ax \n\t" \
		"lidt (%%esp) \n\t" \
		"addl $6, %%esp" \
		: : "a" (size) : "%esp"); \
	processorPopFlags(); \
} while (0)

#define processorLoadTaskReg(selector) do { \
	processorPushFlags(); \
	processorDisableInts(); \
	__asm__ __volatile__ ("ltr %%ax" : : "a" (selector)); \
	processorPopFlags(); \
} while (0)

#define processorClearTaskSwitched() __asm__ __volatile__ ("clts")

#define processorGetInstructionPointer(addr) \
	__asm__ __volatile__ ( \
		"call 1f \n\t" \
		"1: pop %0" : "=r" (addr))

#define processorFarJump(selector) do { \
	processorPushFlags(); \
	processorPush(selector); \
	processorPush(0); \
	__asm__ __volatile__ ( \
		"ljmp *(%%esp) \n\t" \
		"addl $8, %%esp" \
		: : : "%esp"); \
	processorPopFlags(); \
} while (0)

#define processorIsrCall(addr) do { \
	processorPush(PRIV_CODE); \
	processorPush(addr); \
	__asm__ __volatile__ ("movl %%esp, %%eax" : : : "%eax"); \
	processorPushFlags(); \
	__asm__ __volatile__ ( \
		"lcall *(%%eax) \n\t" \
		"add $8, %%esp" \
		: : : "%eax"); \
} while (0)

#define processorIntReturn() __asm__ __volatile__ ("iret")

#define processorFarReturn() __asm__ __volatile__ ("lret")

#define processorGetFramePointer(addr) \
	__asm__ __volatile__ ("movl %%ebp, %0" : "=r" (addr))

#define processorPopFrame() \
	__asm__ __volatile__ ( \
		"movl %%ebp, %%esp \n\t" \
		"popl %%ebp" \
		: : : "%esp" )

#define processorExceptionEnter(exAddr, ints) do { \
	processorPushRegs(); \
	processorSuspendInts(ints); \
	__asm__ __volatile__ ("movl 4(%%ebp), %0" : "=r" (exAddr)); \
} while (0)

#define processorExceptionExit(ints) do { \
	processorRestoreInts(ints); \
	processorPopRegs(); \
	processorPopFrame(); \
	processorIntReturn(); \
} while (0)

#define processorIsrEnter(stAddr) do { \
	processorDisableInts(); \
	processorPushRegs(); \
	processorGetStackPointer(stAddr); \
} while (0)

#define processorIsrExit(stAddr) do { \
	processorSetStackPointer(stAddr); \
	processorPopRegs(); \
	processorPopFrame(); \
	processorEnableInts(); \
	processorIntReturn(); \
} while (0)

#define processorApiExit(stAddr, codeLo, codeHi) do { \
	__asm__ __volatile__ ( \
		"movl %0, %%eax \n\t" \
		"movl %1, %%edx" \
		: : "r" (codeLo), "r" (codeHi) : "%eax", "%edx" ); \
	processorPopFrame(); \
	processorFarReturn(); \
} while (0)

//
// Floating point ops
//

#define processorGetFpuStatus(code) \
	__asm__ __volatile__ ("fstsw %0" : "=a" (code))

#define processorFpuStateSave(addr) \
	__asm__ __volatile__ ( \
		"fnsave %0 \n\t" \
		"fwait" \
		: : "m" (addr) : "memory")

#define processorFpuStateRestore(addr) \
	__asm__ __volatile__ ("frstor %0" : : "m" (addr))

#define processorFpuInit() __asm__ __volatile__ ("fninit")

#define processorGetFpuControl(code) \
	__asm__ __volatile__ ("fstcw %0" : "=m" (code))

#define processorSetFpuControl(code) \
	__asm__ __volatile__ ("fldcw %0" : : "m" (code))

#define processorFpuClearEx() \
	__asm__ __volatile__ ("fnclex");

//
// Misc
//

#define processorLock(lck, proc) \
	__asm__ __volatile__ ("lock cmpxchgl %1, %2" \
		: : "a" (0), "r" (proc), "m" (lck) : "memory")

static inline unsigned short processorSwap16(unsigned short variable)
{
	volatile unsigned short tmp = (variable);
	__asm__ __volatile__ ("rolw $8, %0" : "=r" (tmp) : "r" (tmp));
	return (tmp);
}

static inline unsigned processorSwap32(unsigned variable)
{
	volatile unsigned tmp = (variable);
	__asm__ __volatile__ ("bswap %0" : "=r" (tmp) : "r" (tmp));
	return (tmp);
}

#define processorCacheInvalidate() __asm__ __volatile__ ("wbinvd")

#define processorAddressCacheInvalidate() do { \
	unsigned cr3; \
	processorGetCR3(cr3); \
	processorSetCR3(cr3); \
} while (0)

#define processorAddressCacheInvalidatePage(addr) \
	__asm__ __volatile__ ("invlpg %0" : : "m" (*((char *)(addr))))

#define processorDelay() do { \
	unsigned char d; \
	processorPushRegs(); \
	processorInPort8(0x3F6, d); \
	processorInPort8(0x3F6, d); \
	processorInPort8(0x3F6, d); \
	processorInPort8(0x3F6, d); \
	processorPopRegs(); \
} while (0)

#define processorHalt() __asm__ __volatile__ ("hlt")

#define processorIdle() do { \
	processorEnableInts(); \
	processorHalt(); \
} while (0)

#define processorStop() do { \
	processorDisableInts(); \
	processorHalt(); \
} while (0)

#define processorReboot() do { \
	processorDisableInts(); \
	__asm__ __volatile__ ( \
		"movl $0xFE, %%eax \n\t" \
		"outb %%al, $0x64" \
		: : : "%eax"); \
	processorHalt(); \
} while (0)

#endif

