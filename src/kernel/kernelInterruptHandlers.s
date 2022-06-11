;;
;;  Visopsys
;;  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
;;  kernelInterruptHandlers.s
;;
	
	SEGMENT .text
	BITS 32

	EXTERN kernelTextConsolePrint
	EXTERN kernelFloppyDriverReceiveInterrupt
	EXTERN kernelHardDiskDriverReceiveInterrupt
	
	GLOBAL kernelExceptionHandler0
	GLOBAL kernelExceptionHandler1
	GLOBAL kernelExceptionHandler2
	GLOBAL kernelExceptionHandler3
	GLOBAL kernelExceptionHandler4
	GLOBAL kernelExceptionHandler5
	GLOBAL kernelExceptionHandler6
	GLOBAL kernelExceptionHandler7
	GLOBAL kernelExceptionHandler8
	GLOBAL kernelExceptionHandler9
	GLOBAL kernelExceptionHandlerA
	GLOBAL kernelExceptionHandlerB
	GLOBAL kernelExceptionHandlerC
	GLOBAL kernelExceptionHandlerD
	GLOBAL kernelExceptionHandlerE
	GLOBAL kernelExceptionHandlerF
	GLOBAL kernelExceptionHandler10
	GLOBAL kernelExceptionHandler11
	GLOBAL kernelExceptionHandler12
	GLOBAL kernelInterruptHandler20
	GLOBAL kernelInterruptHandler21
	GLOBAL kernelInterruptHandler25
	GLOBAL kernelInterruptHandler26
	GLOBAL kernelInterruptHandler27
	GLOBAL kernelInterruptHandler28
	GLOBAL kernelInterruptHandler29
	GLOBAL kernelInterruptHandler2A
	GLOBAL kernelInterruptHandler2B
	GLOBAL kernelInterruptHandler2C
	GLOBAL kernelInterruptHandler2D
	GLOBAL kernelInterruptHandler2E
	GLOBAL kernelInterruptHandler2F
	GLOBAL kernelInterruptHandlerUnimp

	%include "kernelAssemblerHeader.h"


;; Following are the interrupt service routines for several
;; basic exceptions and hardware interfaces.

	
kernelExceptionHandler0:	
	;; This is the divide-by-zero exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 0
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS

	popfd
	popa
	iret


kernelExceptionHandler1:	
	;; This is the single-step trap handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 1
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandler2:	
	;; This is the non-maskable interrupt (NMI) handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 2
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandler3:	
	;; This is the breakpoint trap handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 3
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandler4:	
	;; This is the overflow trap handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 4
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandler5:	
	;; This is the bound range exceeded exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 5
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandler6:	
	;; This is the invalid opcode exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 6
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandler7:	
	;; This is the coprocessor not available exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 7
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandler8:	
	;; This is the double-fault exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd		
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 8
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandler9:	
	;; This is the coprocessor segment overrun exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 9
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret

	
kernelExceptionHandlerA:	
	;; This is the invalid TSS exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 0Ah
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandlerB:	
	;; This is the segment not present exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 0Bh
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandlerC:	
	;; This is the stack exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 0Ch
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandlerD:	
	;; This is the general protection fault handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd	
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 0Dh
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandlerE:	
	;; This is the page fault handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 0Eh
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
		
	popfd
	popa
	iret


kernelExceptionHandlerF:	
	;; This is a 'reserved' (unimplemented?) exception handler
	iret


kernelExceptionHandler10:	
	;; This is the coprocessor error exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 010h
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandler11:	
	;; This is the alignment check exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 11h
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelExceptionHandler12:	
	;; This is the machine check exception handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's exception handler
	push dword 12h
	call kernelExceptionHandler
	add ESP, 4

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler20:	
	;; This is the system timer interrupt handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd		
	cli

	;; Issue an end-of-interrupt (EOI) to the PIC
	mov AL, 00100000b
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX

	;; Call the kernel's generalized driver function
	call kernelSysTimerTick
	
	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler21:	
	;; This is the keyboard interrupt handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Issue an end-of-interrupt (EOI) to the PIC
	mov AL, 00100000b
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's keyboard driver to handle this data
	call kernelKeyboardReadData	

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret

	
kernelInterruptHandler25:	
	;; This is the parallel port 2 interrupt handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd	
	cli

	;; Issue an end-of-interrupt (EOI) to the PIC
	mov AL, 00100000b
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	push dword 00000004h		; Red colour
	push dword PARALLEL2LEN		; Number of characters
	push dword PARALLEL2		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler26:	
	;; This is the floppy drive interrupt

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Issue an end-of-interrupt (EOI) to the PIC
	mov AL, 00100000b
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's floppy disk driver
	call kernelFloppyDriverReceiveInterrupt

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
		
	popfd
	popa
	iret


kernelInterruptHandler27:	
	;; This is a 'reserved' (unimplemented?) exception handler
	
	;; This interrupt can sometimes occur frivolously from "noise"
	;; on the interrupt request lines.  Before we do anything at all,
	;; we MUST ensure that the interrupt really occurred.
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	;; Issue an end-of-interrupt (EOI) to the PIC
	mov AL, 00100000b
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Poll bit 7 in the PIC
	mov AL, 0Bh
	out 20h, AL
	jecxz $+2	;; Delay
	jecxz $+2
	in AL, 20h
	jecxz $+2	;; Delay
	jecxz $+2
	and AL, 10000000b
	cmp AL, 0
	je .done	;; It's a bogus interrupt

	.done:
	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler28:	
	;; This is the real time clock interrupt handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Print the exception message

	push dword 00000004h		; Red colour
	push dword REALTIMECLKLEN	; Number of characters
	push dword REALTIMECLK		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler29:	
	;; This is the VGA retrace interrupt handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Print the interrupt message
	push dword 00000004h		; Red colour
	push dword VGARETRACELEN	; Number of characters
	push dword VGARETRACE		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler2A:	
	;; This is the 'available 1' interrupt handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli

	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Print the interrupt message
	push dword 00000004h		; Red colour
	push dword INT72LEN		; Number of characters
	push dword INT72		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler2B:	
	;; This is the 'available 2' interrupt handler

	;; This interrupt can sometimes occur frivolously from "noise"
	;; on the interrupt request lines.  Before we do anything at all,
	;; we MUST ensure that the interrupt really occurred.
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli
	
	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Poll bit 3 in the PIC
	mov AL, 0Bh
	out 0A0h, AL
	jecxz $+2	;; Delay
	jecxz $+2
	in AL, 0A0h
	jecxz $+2	;; Delay
	jecxz $+2
	and AL, 00001000b
	cmp AL, 0
	je .done			; It's a bogus interrupt

	.realInterrupt:	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; DON'T print the interrupt message
	;; This looks like it might be connected somehow to the
	;; real-time clock on my K6-2 machine.  Issues one of these
	;; interrupts every second (by my count).

	;; push dword 00000004h	; Red colour
	;; push dword INT73LEN		; Number of characters
	;; push dword INT73		; Pointer to characters

	;; call kernelTextConsolePrint
	;; add ESP, 12

	.done:
	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler2C:	
	;; This is the mouse interrupt handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli
	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Print the interrupt message

	push dword 00000004h		; Red colour
	push dword MOUSELEN		; Number of characters
	push dword MOUSE		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler2D:	
	;; This is the numeric co-processor error interrupt handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli
	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Print the interrupt message

	push dword 00000004h		; Red colour
	push dword NUMCOPROCERRLEN	; Number of characters
	push dword NUMCOPROCERR		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler2E:	
	;; This is the hard drive interrupt handler

	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli
	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Call the kernel's hard disk driver
	call kernelHardDiskDriverReceiveInterrupt

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler2F:	
	;; This is the 'available 3' interrupt handler.  We will be using
	;; it for the secondary hard disk controller interrupt.
	
	;; This interrupt can sometimes occur frivolously from "noise"
	;; on the interrupt request lines.  Before we do anything at all,
	;; we MUST ensure that the interrupt really occurred.
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP
	
	pushfd
	cli
	
	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Poll bit 7 in the PIC
	mov AL, 0Bh
	out 0A0h, AL
	jecxz $+2	;; Delay
	jecxz $+2
	in AL, 0A0h
	jecxz $+2	;; Delay
	jecxz $+2
	and AL, 10000000b
	cmp AL, 0
	je .done		; It's a bogus interrupt

	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Call the kernel's hard disk driver
	call kernelHardDiskDriverReceiveInterrupt

	.done:
	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret

	
kernelInterruptHandlerUnimp:	
	;; This is the "unimplemented interrupt" handler

	pusha
	pushfd
	cli

	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	push DS
	push ES
	push FS
	push GS

	mov EAX, ALLDATASELECTOR
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Print the fault message
	push dword 00000004h		; Red colour
	push dword INTUNIMPLEN		; Number of characters
	push dword INTUNIMP		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	;; Stop the machine
	;; hlt

	;; Restore the data registers
	pop GS
	pop FS
	pop ES
	pop DS
	
	popfd
	popa
	iret

	
	SEGMENT .data
	ALIGN 4

;; Messages for the interrupt service routines

;; Interrupt 0Dh
PARALLEL2	db 'Parallel port 2 interrupt'
PARALLEL2LEN	equ $-PARALLEL2

;; Interrupt 70h
REALTIMECLK   	db 'Real-time clock alarm interrupt'
REALTIMECLKLEN  equ $-REALTIMECLK

;; Interrupt 71h
VGARETRACE	db 'VGA retrace interrupt'
VGARETRACELEN	equ $-VGARETRACE

;; Interrupt 72h
INT72	   	db 'Interrupt 72'
INT72LEN        equ $-INT72

;; Interrupt 73h
INT73 	  	db 'Interrupt 73'
INT73LEN        equ $-INT73

;; Interrupt 74h
MOUSE	   	db 'Mouse interrupt'
MOUSELEN        equ $-MOUSE

;; Interrupt 75h
NUMCOPROCERR    db 'Numeric co-processor error'
NUMCOPROCERRLEN equ $-NUMCOPROCERR

;; Interrupt unimplemented
INTUNIMP   	db 'Unimplemented interrupt handler'
INTUNIMPLEN	equ $-INTUNIMP