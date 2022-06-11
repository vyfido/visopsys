;;
;;  Visopsys
;;  Copyright (C) 1998-2003 J. Andrew McLaughlin
;; 
;;  This program is free software; you can redistribute it and/or modify it
;;  under the terms of the GNU General Public License as published by the Free
;;  Software Foundation; either version 2 of the License, or (at your option)
;;  any later version.
;; 
;;  This program is distributed in the hope that it will be useful, but
;;  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
;;  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
;;  for more details.
;;  
;;  You should have received a copy of the GNU General Public License along
;;  with this program; if not, write to the Free Software Foundation, Inc.,
;;  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
;;
;;  kernelMiscAsmFunctions.s
;;
	
	SEGMENT .text
	BITS 32

	%include "kernelAssemblerHeader.h"

	GLOBAL kernelSuddenStop
	GLOBAL kernelSuddenReboot
	GLOBAL kernelMemCopy
	GLOBAL kernelMemClear
	GLOBAL kernelInstallGDT
	GLOBAL kernelInstallIDT
	GLOBAL kernelTaskJump
	GLOBAL kernelTaskCall
	

kernelSuddenStop:	
	;; Stops the processor, no questions asked
	cli
	hlt
	

kernelSuddenReboot:

	;; Does a sudden, irreversible reboot.

	;; Of course, we don't need to save any registers or anything,
	;; but disable interrupts
	cli
	
	;; Try to reset using the PCI chipset, if any
	;; mov DX, 0CF9h
	;; xor AL, AL
	;; out DX, AL
	;; jecxz $+2
	;; jecxz $+2
	;; mov AL, 06h
	;; out DX, AL
	;; jecxz $+2
	;; jecxz $+2

	;; Maybe this will work... but...
	
	;; Alternatively, write the reset command to the keyboard controller
	mov AL, 0FEh
	out 64h, AL
	jecxz $+2
	jecxz $+2

	;; Done.  The computer should now be rebooting.

	;; Just in case.  Should never get here.
	hlt


kernelMemCopy:
	;; This routine will do the fastest possible memory transfer
	;; from one area to another

	;; The C prototype would look like this:
	;; void kernelMemCopy(const void *src, void *dest, unsigned count)
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Set up the data pointers
	mov ESI, dword [SS:(EBP + 36)]
	mov EDI, dword [SS:(EBP + 40)]
	
	;; The number of bytes to transfer
	mov ECX, dword [SS:(EBP + 44)]

	;; (divide it by 4)
	shr ECX, 2
	
	;; Clear the direction flag
	cld
	
	;; This should not be interrupted
	pushfd
	cli

	;; Do the actual copy
	rep movsd

	;; Now take care of any additional bytes (i.e. the number of bytes
	;; to copy was not a multiple of 4)
	mov ECX, dword [SS:(EBP + 44)]
	and ECX, 00000003h

	rep movsb

	;; Restore flags
	popfd
	;; Restore regs
	popa

	ret


kernelMemClear:
	;; This routine will do the fastest possible memory clear
	;; Prototype:	
	;; void kernelFastMemoryClear(void *area, unsigned numBytes); 

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Set up the data pointer
	mov EDI, dword [SS:(EBP + 36)]
	
	;; The number of bytes to clear
	mov ECX, dword [SS:(EBP + 40)]

	;; (divide it by 4)
	shr ECX, 2
	
	;; Value to transfer is zero
	xor EAX, EAX
	
	;; Clear the direction flag
	cld
	
	;; This should not be interrupted
	pushfd
	cli

	;; Do the actual clear
	rep stosd

	;; Now take care of any additional bytes (i.e. the number of bytes
	;; to clear was not a multiple of 4)
	mov ECX, dword [SS:(EBP + 40)]
	and ECX, 00000003h

	rep stosb

	;; Restore flags
	popfd
	;; Restore regs
	popa

	ret

	
kernelInstallGDT:
	;; This function takes a pointer to the global descriptor table,
	;; and its size, and installs it into the global descriptor table
	;; register

	pusha

	;; Save the (modified) stack pointer
	mov EBP, ESP

	;; Get the pointer to the table
	mov EAX, dword [SS:(EBP + 36)]
	mov dword [SCRATCH + 2], EAX
	
	;; Get the size argument from the arguments
	mov EAX, dword [SS:(EBP + 40)]
	mov word [SCRATCH], AX

	;; Disable interrupts while we do this
	pushfd
	cli
	
	lgdt [SCRATCH]

	;; Restore the flags
	popfd
	
	popa
	ret


kernelInstallIDT:
	;; This function takes a pointer to the interrupt descriptor table,
	;; and its size, and installs it into the interrupt descriptor table
	;; register

	pusha

	;; Save the (modified) stack pointer
	mov EBP, ESP

	;; Get the pointer to the table
	mov EAX, dword [SS:(EBP + 36)]
	mov dword [SCRATCH + 2], EAX
	
	;; Get the size argument from the arguments
	mov EAX, dword [SS:(EBP + 40)]
	mov word [SCRATCH], AX

	;; Disable interrupts while we do this
	pushfd
	cli
	
	lidt [SCRATCH]

	;; Restore the flags
	popfd
	
	popa
	ret


kernelTaskJump:
	;; This function invokes a "task switch" or "context switch",
	;; using the TSS (Task State Segment) selector passed as the
	;; only parameter

	;; Save regs
	pusha
	
	;; Save ESP
	mov EBP, ESP

	mov EAX, dword [SS:(EBP + 36)]	
	mov word [SCRATCH + 4], AX
	mov dword [SCRATCH], 00000000h
	jmp dword far [SCRATCH]

	popa
	ret

	

kernelTaskCall:
	;; This function invokes a "task switch" or "context switch",
	;; using the TSS (Task State Segment) selector passed as the
	;; only parameter

	;; Save regs
	pusha
	
	;; Save ESP
	mov EBP, ESP

	;; Push the flags so that any iret instruction which returns
	;; to this task	doesn't kill us
	pushfd

	mov EAX, dword [SS:(EBP + 36)]	
	mov word [SCRATCH + 4], AX
	mov dword [SCRATCH], 00000000h

	;; Make the task call
	call dword far [SCRATCH]

	;; Restore the flags
	popfd
	
	popa
	ret


	SEGMENT .data
	ALIGN 4
	
SCRATCH:		dw 0
			dd 0
