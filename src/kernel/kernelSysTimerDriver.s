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
;;  kernelSysTimerDriver.s
;;

	SEGMENT .text
	BITS 32

	GLOBAL kernelSysTimerDriverInitialize
	GLOBAL kernelSysTimerDriverTick
	GLOBAL kernelSysTimerDriverRead
	GLOBAL kernelSysTimerDriverReadTimer
	GLOBAL kernelSysTimerDriverSetTimer

	%include "kernelAssemblerHeader.h"


kernelSysTimerDriverInitialize:
	;; Zeroes the system timer counter and initializes the counters

	;; Save regs
	pusha
	
	;; Make sure that counter 0 is set to operate in mode 3 
	;; (some systems erroneously use mode 2) with an initial value of 0
	push dword 0		; Data value
	push dword 3		; Mode 3
	push dword 0		; Counter 0
	call kernelSysTimerDriverSetTimer
	add ESP, 12

	;; Check for an error code
	cmp EAX, 0
	jne .returnError
	
	;; Reset the counter we use to count the number
	;; of timer 0 (system timer) interrupts we've
	;; encountered
	mov dword [TIMERTICKS], 0

	;; Done

	.returnOk:	
	;; Restore regs and return success
	popa
	mov EAX, 0
	ret

	.returnError:
	;; Restore regs and return error
	popa
	mov EAX, -1
	ret 
	

kernelSysTimerDriverTick:
	;; This updates the count of the system timer

	;; Add one to the timer 0 tick counter
	add dword [TIMERTICKS], 1
	ret


kernelSysTimerDriverRead:
	;; Returns the value of the system timer tick counter.

	mov EAX, dword [TIMERTICKS]
	ret

	
kernelSysTimerDriverSetTimer:	
	;; This function is used to select, set the mode and count of one
	;; of the system timer counters
	;; 
	;; The C prototype of the function would look like this:
	;;   int kernelSysTimerDriverSetTimer(int counter, int mode, 
	;;      int count);

	;; Save regs
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Get the timer number from the stack
	mov EBX, dword [SS:(EBP + 36)]

	;; Make sure it's not greater than 2.  This driver only supports
	;; timers 0 through 2 (since that's all most systems will have)
	cmp EBX, 2
	ja .returnError

	
	;; Calculate the data command to use

	mov CL, byte [EBX + DATACOMMAND]

	;; Now CL contains the command.
	;; Shift the command left by 4 bits
	shl CL, 4
	
	;; Load the mode into EAX, 
	mov EAX, dword [SS:(EBP + 40)]

	;; Make sure the mode is legal
	cmp EAX, 5
	ja .returnError
	
	;; shift AL left by one, and OR it with the command in CL.  The
	;; result is the formatted command byte we'll send to the timer
	shl AL, 1
	or AL, CL

	;; We can send the command to the general command port
	out 43h, AL

	;; Delay
	jecxz $+2
	

	;; The timer is now expecting us to send two bytes which represent
	;; the initial count of the timer.  We will get this value from
	;; the parameters.  First, calculate the IO port to which we
	;; should send the data

	push EBX		; Save EBX because we need it again
	
	;; Port
	shl EBX, 1		; Multiply it by 2 bytes per item
	mov DX, word [EBX + PORTNUMBERS] ; The address of the port numbers

	pop EBX			; Restore EBX

	;; Get the data
	mov EAX, dword [SS:(EBP + 44)]

	;; Send low byte first, followed by the high byte to the data
	;; port
	out DX, AL
		
	;; Delay
	jecxz $+2

	shr AX, 8
	out DX, AL

	;; Delay
	jecxz $+2

	;; Done

	.returnOk:	
	;; Restore regs and return success
	popa
	mov EAX, 0
	ret

	.returnError:
	;; Restore regs and return error
	popa
	mov EAX, -1
	ret 
	
	
kernelSysTimerDriverReadTimer:	
	;; This function is used to select and read one of the system 
	;; timer counters
	;; 
	;; The C prototype of the function would look like this:
	;;   int kernelSysTimerDriverReadTimer(int counter);

	;; Save regs
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Get the timer number from the stack
	mov EBX, dword [SS:(EBP + 36)]

	;; Make sure it's not greater than 2.  This driver only supports
	;; timers 0 through 2 (since that's all most systems will have)
	cmp EBX, 2
	ja .returnError


	;; Before we can read the timer reliably, we must send a command
	;; to cause it to latch the current value.  Calculate which latch 
	;; command to use

	mov AL, byte [EBX + LATCHCOMMAND] ; Get the address for latch commands

	;; Now AL contains the command.  Shift the command left by 4 bits
	shl AL, 4

	;; We can send the command to the general command port
	out 43h, AL

	;; Delay
	jecxz $+2

	
	;; The counter will now be expecting us to read two bytes from
	;; the applicable port.  Get the IO port to use
	
	push EBX		; Save EBX
	
	;; Port
	shl EBX, 1		; Multiply it by 2 bytes per item
	mov DX, word [EBX + PORTNUMBERS] ; The address of the port numbers

	pop EBX			; Restore EBX

	;; Read the low byte first, followed by the high byte
	in AL, DX
	mov byte [TIMERVALUE], AL
	
	;; Delay
	jecxz $+2

	in AL, DX
	mov byte [TIMERVALUE + 1], AL

	;; Delay
	jecxz $+2

	;; Done

	.returnOk:	
	;; Restore regs and return the value
	popa
	xor EAX, EAX
	mov AX, word [TIMERVALUE]
	ret

	.returnError:
	;; Restore regs and return error
	popa
	mov EAX, -1
	ret 
	
	
	SEGMENT .data
	ALIGN 4
	
TIMERTICKS	dd 0	
TIMERVALUE	dw 0
PORTNUMBERS	dw 0040h, 0041h, 0042h
LATCHCOMMAND	db 00h, 04h, 08h
DATACOMMAND	db 03h, 07h, 0Bh
