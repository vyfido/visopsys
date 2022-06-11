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
;;  kernelPicDriver.s
;;

	SEGMENT .text
	BITS 32

	GLOBAL kernelPicDriverInitialize
	GLOBAL kernelPicDriverEnableInterrupts
	GLOBAL kernelPicDriverDisableInterrupts
	GLOBAL kernelPicDriverEndOfInterrupt

	%include "kernelAssemblerHeader.h"

	
kernelPicDriverInitialize:
	;; Initializes the kernel's PIC controller

	;; Save regs
	pusha

	;; Save flags
	pushfd

	;; Disable interrupts
	cli
	
	;; Initialize the interrupt controllers

	;; The master controller
	mov AL, 00010001b	; Initialization word 1
	out 20h, AL
	jecxz $+2		; Delay
	jecxz $+2
	mov AL, 00100000b	; Initialization word 2
	out 21h, AL
	jecxz $+2		; Delay
	jecxz $+2
	mov AL, 00000100b	; Initialization word 3
	out 21h, AL
	jecxz $+2		; Delay
	jecxz $+2
	mov AL, 00000001b	; Initialization word 4
	out 21h, AL
	jecxz $+2		; Delay
	jecxz $+2

	;; mov AL, 00100111b	; Normal operation, normal priorities
	;; out 20h, AL
	;; jecxz $+2		; Delay
	;; jecxz $+2
	mov AL, 00000000b	; Mask all ints on
	out 21h, AL

	jecxz $+2		; Delay
	jecxz $+2
			
	;; The slave controller
	mov AL, 00010001b	; Initialization word 1
	out 0A0h, AL
	jecxz $+2		; Delay
	jecxz $+2
	mov AL, 00101000b	; Initialization word 2
	out 0A1h, AL
	jecxz $+2		; Delay
	jecxz $+2
	mov AL, 00000010b	; Initialization word 3
	out 0A1h, AL
	jecxz $+2		; Delay
	jecxz $+2
	mov AL, 00000001b	; Initialization word 4
	out 0A1h, AL
	jecxz $+2		; Delay
	jecxz $+2

	;; mov AL, 00100111b	; Normal operation, normal priorities
	;; out 0A0h, AL
	;; jecxz $+2		; Delay
	;; jecxz $+2
	mov AL, 00000000b	; Mask all ints on
	out 0A1h, AL


	;; Restore flags (will enable interrupts if they were previously)
	popfd

	;; Restore regs
	popa
	
	;; Ok.  Return success.
	mov EAX, 0
	ret


kernelPicDriverEndOfInterrupt:
	;; Sends end of interrupt (EOI) commands to one or both of the
	;; PICs

	pusha

	;; Save ESP
	mov EBP, ESP

	;; Our only parameter should be the number of the interrupt.  If
	;; The number is greater than 27h, we will issue EOI to both the
	;; slave and master controllers.  Otherwise, just the master.
	
	mov EAX, [SS:(EBP + 36)]
	cmp EAX, 27h
	jbe .master

	;; Issue an end-of-interrupt (EOI) to the slave PIC
	mov AL, 00100000b
	out 0A0h, AL

	.master:
	;; Issue an end-of-interrupt (EOI) to the master PIC
	mov AL, 00100000b
	out 20h, AL

	popa
	ret
