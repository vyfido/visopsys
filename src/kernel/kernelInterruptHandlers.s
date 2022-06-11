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
;;  kernelInterruptHandlers.s
;;
	
	SEGMENT .text
	BITS 32

	EXTERN kernelTextConsolePrint
	EXTERN kernelFloppyDriverReceiveInterrupt
	EXTERN kernelIdeDriverReceiveInterrupt
	
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
	GLOBAL kernelProcessingInterrupt
	
	%include "kernelAssemblerHeader.h"


;; Following are the interrupt service routines for several
;; basic exceptions and hardware interfaces.

	
kernelInterruptHandler20:	
	;; This is the system timer interrupt handler

	pusha
	pushfd		
	cli

	;; be consistent between the kernel and the interrupted task.
	;; Save the existing values on the stack
	;; push DS
	;; push ES
	;; push FS
	;; push GS

	;; mov EAX, USER_DATASELECTOR
	;; mov DS, AX
	;; mov ES, AX
	;; mov FS, AX
	;; mov GS, AX

	mov dword [kernelProcessingInterrupt], 20h
	
	;; Issue an end-of-interrupt (EOI) to the PIC
	mov AL, 00100000b
	out 20h, AL

	;; Change the data registers, since they will not necessarily
	;; Call the kernel's generalized driver function
	call kernelSysTimerTick
	
	mov dword [kernelProcessingInterrupt], 0
	
	;; Restore the data registers
	;; pop GS
	;; pop FS
	;; pop ES
	;; pop DS
	
	popfd
	popa
	iret


kernelInterruptHandler21:	
	;; This is the keyboard interrupt handler

	pusha
	pushfd
	cli

	;; Issue an end-of-interrupt (EOI) to the PIC
	mov AL, 00100000b
	out 20h, AL

	mov dword [kernelProcessingInterrupt], 21h
	
	;; Call the kernel's keyboard driver to handle this data
	call kernelKeyboardReadData

	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret

	
kernelInterruptHandler25:	
	;; This is the parallel port 2 interrupt handler

	pusha
	pushfd	
	cli

	mov dword [kernelProcessingInterrupt], 25h
	
	;; Issue an end-of-interrupt (EOI) to the PIC
	mov AL, 00100000b
	out 20h, AL

	push dword 00000004h		; Red color
	push dword PARALLEL2LEN		; Number of characters
	push dword PARALLEL2		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret


kernelInterruptHandler26:	
	;; This is the floppy drive interrupt

	pusha
	pushfd
	cli

	mov dword [kernelProcessingInterrupt], 26h
	
	;; Issue an end-of-interrupt (EOI) to the PIC
	mov AL, 00100000b
	out 20h, AL

	;; Call the kernel's floppy disk driver
	call kernelFloppyDriverReceiveInterrupt

	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret


kernelInterruptHandler27:	
	;; This is a 'reserved' (unimplemented?) exception handler
	
	;; This interrupt can sometimes occur frivolously from "noise"
	;; on the interrupt request lines.  Before we do anything at all,
	;; we MUST ensure that the interrupt really occurred.
	
	pusha
	pushfd
	cli

	mov dword [kernelProcessingInterrupt], 27h
	
	;; Issue an end-of-interrupt (EOI) to the PIC
	mov AL, 00100000b
	out 20h, AL

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
	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret


kernelInterruptHandler28:	
	;; This is the real time clock interrupt handler

	pusha
	pushfd
	cli

	mov dword [kernelProcessingInterrupt], 28h
	
	;; Print the exception message

	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	push dword 00000004h		; Red color
	push dword REALTIMECLKLEN	; Number of characters
	push dword REALTIMECLK		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret


kernelInterruptHandler29:	
	;; This is the VGA retrace interrupt handler

	pusha
	pushfd
	cli

	mov dword [kernelProcessingInterrupt], 29h
	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Print the interrupt message
	push dword 00000004h		; Red color
	push dword VGARETRACELEN	; Number of characters
	push dword VGARETRACE		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret


kernelInterruptHandler2A:	
	;; This is the 'available 1' interrupt handler

	pusha
	pushfd
	cli

	mov dword [kernelProcessingInterrupt], 2Ah
	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Print the interrupt message
	push dword 00000004h		; Red color
	push dword INT72LEN		; Number of characters
	push dword INT72		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret


kernelInterruptHandler2B:	
	;; This is the 'available 2' interrupt handler

	;; This interrupt can sometimes occur frivolously from "noise"
	;; on the interrupt request lines.  Before we do anything at all,
	;; we MUST ensure that the interrupt really occurred.
	
	pusha
	pushfd
	cli
	
	mov dword [kernelProcessingInterrupt], 2Bh
	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

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

	;; DON'T print the interrupt message
	;; This looks like it might be connected somehow to the
	;; real-time clock on my K6-2 machine.  Issues one of these
	;; interrupts every second (by my count).

	.done:
	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret


kernelInterruptHandler2C:	
	;; This is the mouse interrupt handler

	pusha
	pushfd
	cli
	
	mov dword [kernelProcessingInterrupt], 2Ch
	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Call the kernel's mouse driver
	call kernelMouseReadData

	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret


kernelInterruptHandler2D:	
	;; This is the numeric co-processor error interrupt handler

	pusha
	pushfd
	cli
	
	mov dword [kernelProcessingInterrupt], 2Dh

	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Print the interrupt message

	push dword 00000004h		; Red color
	push dword NUMCOPROCERRLEN	; Number of characters
	push dword NUMCOPROCERR		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12
	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret


kernelInterruptHandler2E:	
	;; This is the hard disk interrupt handler

	pusha
	pushfd
	cli
	
	mov dword [kernelProcessingInterrupt], 2Eh
	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Call the kernel's hard disk driver
	call kernelIdeDriverReceiveInterrupt

	mov dword [kernelProcessingInterrupt], 0
	
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
	pushfd
	cli
	
	mov dword [kernelProcessingInterrupt], 2Fh
	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

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

	;; Call the kernel's hard disk driver
	call kernelIdeDriverReceiveInterrupt

	.done:
	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret

	
kernelInterruptHandlerUnimp:	
	;; This is the "unimplemented interrupt" handler

	pusha
	pushfd
	cli

	mov dword [kernelProcessingInterrupt], 99h ; Bogus
	
	mov AL, 00100000b
	;; Issue an end-of-interrupt (EOI) to the slave PIC
	out 0A0h, AL
	;; Issue an end-of-interrupt (EOI) to the master PIC
	out 20h, AL

	;; Print the fault message
	push dword 00000004h		; Red color
	push dword INTUNIMPLEN		; Number of characters
	push dword INTUNIMP		; Pointer to characters

	call kernelTextConsolePrint
	add ESP, 12

	mov dword [kernelProcessingInterrupt], 0
	
	popfd
	popa
	iret

	
	SEGMENT .data
	ALIGN 4

kernelProcessingInterrupt	dd 0	;; We set this when we are processing
					;; an interrupt so the scheduler and
					;; exception handler can do the right
					;; things
	
;; Messages for some of the interrupt service routines

;; Interrupt 0Dh
PARALLEL2	db 'Parallel port 2 interrupt', 0Ah
PARALLEL2LEN	equ $-PARALLEL2

;; Interrupt 70h
REALTIMECLK   	db 'Real-time clock alarm interrupt', 0Ah
REALTIMECLKLEN  equ $-REALTIMECLK

;; Interrupt 71h
VGARETRACE	db 'VGA retrace interrupt', 0Ah
VGARETRACELEN	equ $-VGARETRACE

;; Interrupt 72h
INT72	   	db 'Interrupt 72', 0Ah
INT72LEN        equ $-INT72

;; Interrupt 75h
NUMCOPROCERR    db 'Numeric co-processor error', 0Ah
NUMCOPROCERRLEN equ $-NUMCOPROCERR

;; Interrupt unimplemented
INTUNIMP   	db 'Unimplemented interrupt handler', 0Ah
INTUNIMPLEN	equ $-INTUNIMP
