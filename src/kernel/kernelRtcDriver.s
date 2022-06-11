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
;;  kernelRtcDriver.s
;;

	SEGMENT .text
	BITS 32

	GLOBAL kernelRtcDriverInitialize
	GLOBAL kernelRtcDriverReadSeconds
	GLOBAL kernelRtcDriverReadMinutes
	GLOBAL kernelRtcDriverReadHours
	GLOBAL kernelRtcDriverReadDayOfWeek
	GLOBAL kernelRtcDriverReadDayOfMonth
	GLOBAL kernelRtcDriverReadMonth
	GLOBAL kernelRtcDriverReadYear

	%include "kernelAssemblerHeader.h"

	
kernelRtcDriverInitialize:
	;; Initializes the kernel's Real-Time clock driver

	;; Empty for now
	xor EAX, EAX
	ret


kernelRtcDriverWaitReady:
	;; This returns when the RTC is ready to be read or written.
	;; Make sure to disable interrupts before calling this routine.

	push EAX

	;; Read the clock's "update in progress" bit from Register A.
	;; If it is set, do a loop until the clock has finished doing the 
	;; update.  This is so we know the data we're getting from the
	;; clock is reasonable.
	.loopLow:
	mov AL, 0Ah
	out 70h, AL
	in AL, 71h
	and AL, 10000000b
	cmp AL, 0
	jne .loopLow
	
	pop EAX
	ret


kernelRtcDriverRead:
	;; This takes a register number on the stack as its only
	;; parameter, does the necessary probe of the RTC, and returns
	;; the data in EAX
	
	push EBX
	push ECX
	push EBP

	xor EAX, EAX
	xor EBX, EBX
	xor ECX, ECX

	;; Save the current stack pointer in EBP
	mov EBP, ESP
		
	;; Disable interrupts
	pushfd
	cli

	;; Wait until the clock is stable
	call kernelRtcDriverWaitReady

	;; Now we have 244 us to read the data we want.  We'd better stop
	;; talking and do it.
	mov EAX, dword [SS:(EBP + 16)]
	or AL, 10000000b	; Disable NMI at the same time
	out 70h, AL

	;; Now read the data
	in AL, 71h

	;; Save the data temporarily
	push EAX

	;; Reenable NMI
	mov AL, 00000000b
	out 70h, AL

	;; Get the data back
	pop EAX

	;; Reenable interrupts
	popfd

	;; The data is in BCD format.  Sucks.  Convert it to binary.  We
	;; Deal with the "tens" column in AL, and the "ones" column in BL.
	mov BL, AL
	shr AL, 4
	and AL, 00001111b
	mov CL, 10
	mul CL
	;; Now AL contains the "tens" portion.  Add the "ones" portion 
	;; from BL.
	and BL, 00001111b
	add AL, BL

	pop EBP
	pop ECX
	pop EBX
	ret
	
	
kernelRtcDriverReadSeconds:
	;; This function takes no parameters, and returns the seconds
	;; value from the Real-Time clock.

	push dword 00000000h		; Seconds register
	call kernelRtcDriverRead
	add ESP, 4

	;; The result is already in EAX
	ret
	

kernelRtcDriverReadMinutes:
	;; This function takes no parameters, and returns the minutes
	;; value from the Real-Time clock.

	push dword 00000002h		; Minutes register
	call kernelRtcDriverRead
	add ESP, 4

	;; The result is already in EAX
	ret


kernelRtcDriverReadHours:
	;; This function takes no parameters, and returns the hours
	;; value from the Real-Time clock.

	push dword 00000004h		; Hours register
	call kernelRtcDriverRead
	add ESP, 4

	;; The result is already in EAX
	ret


kernelRtcDriverReadDayOfWeek:
	;; This function takes no parameters, and returns the day
	;; value from the Real-Time clock.

	push dword 00000006h		; Day of week register
	call kernelRtcDriverRead
	add ESP, 4

	;; The result is already in EAX
	ret


kernelRtcDriverReadDayOfMonth:
	;; This function takes no parameters, and returns the day
	;; value from the Real-Time clock.

	push dword 00000007h		; Day of month register
	call kernelRtcDriverRead
	add ESP, 4

	;; The result is already in EAX
	ret


kernelRtcDriverReadMonth:
	;; This function takes no parameters, and returns the month
	;; value from the Real-Time clock.

	push dword 00000008h		; Month register
	call kernelRtcDriverRead
	add ESP, 4

	;; The result is already in EAX
	ret


kernelRtcDriverReadYear:
	;; This function takes no parameters, and returns the year
	;; value from the Real-Time clock.

	push dword 00000009h		; Year register
	call kernelRtcDriverRead
	add ESP, 4

	;; The result is already in EAX
	ret
