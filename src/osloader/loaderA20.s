;;
;;  Visopsys
;;  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
;;  loaderA20.s
;;

	GLOBAL loaderEnableA20

	EXTERN loaderPrint
	EXTERN loaderPrintNewline
	EXTERN FATALERROR

	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"


biosMethod:
	;; This method attempts the easiest thing, which is to try to let the
	;; BIOS set A20 for us.

	;; Save space on the stack for the return code
	sub SP, 2
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; By default, return the value -1
	mov word [SS:(BP + 16)], -1

	;; Do the 'enable' call.
	mov AX, 2401h	
	int 15h
	jc .done
	
	;; Check
	mov AX, 2402h
	int 15h
	jc .done
	
	;; On?
	cmp AL, 01h
	jne .done
	
	;; The BIOS did it for us.
	mov word [SS:(BP + 16)], 0

	.done:
	popa
	pop AX	
	ret


port92Method:
	;; This method attempts the second easiest thing, which is to try
	;; writing a bit to port 92h
	
	;; Save space on the stack for the return code
	sub SP, 2
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; By default, return the value -1
	mov word [SS:(BP + 16)], -1

	;; Read port 92h
	xor AX, AX
	in AL, 92h
	
	;; Already set?
	bt AX, 1
	jnc .continue

	;; A20 is on
	mov word [SS:(BP + 16)], 0
	jmp .done

	.continue:
	;; Try to write it
	or AL, 2
	out 92h, AL

	;; Re-read and check
	in AL, 92h
	bt AX, 1
	jnc .done

	;; A20 is on
	mov word [SS:(BP + 16)], 0
	
	.done:
	popa
	pop AX	
	ret
	

keyboardCommandWait:
	;; Wait for the keyboard controller to be ready for a command
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc keyboardCommandWait
	ret


keyboardDataWait:	
	;; Wait for the controller to be ready with a byte of data
	xor AX, AX
	in AL, 64h
	bt AX, 0
	jnc keyboardDataWait
	ret

	
delay:
	;; Delay
	jcxz $+2
	jcxz $+2
	ret


keyboardRead60:
	;; Wait for the controller to be ready for a command
	call keyboardCommandWait

	;; Tell the controller we want to read the current status.
	;; Send the command D0h: read output port.
	mov AL, 0D0h
	out 64h, AL

	;; Delay
	call delay

	;; Wait for the controller to be ready with a byte of data
	call keyboardDataWait

	;; Read the current port status from port 60h
	xor AX, AX
	in AL, 60h
	ret
	

keyboardWrite60:
	;; Save AX on the stack for the moment
	push AX
	
	;; Wait for the controller to be ready for a command
	call keyboardCommandWait

	;; Tell the controller we want to write the status byte
	mov AL, 0D1h
	out 64h, AL	

	;; Delay
	call delay
	
	;; Wait for the controller to be ready for a command
	call keyboardCommandWait

	;; Write the new value to port 60h.  Remember we saved the old value
	;; on the stack
	pop AX
	out 60h, AL
	ret	
	

keyboardMethod:
	;; Save space on the stack for the return code
	sub SP, 2
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; By default, return the value -1
	mov word [SS:(BP + 16)], -1

	;; Make sure interrupts are disabled
	cli

	;; Read the current port 60h status
	call keyboardRead60

	;; Turn on the A20 enable bit and write it.
	or AL, 2
	call keyboardWrite60
	
	;; Read back the A20 status to ensure it was enabled.
	call keyboardRead60

	;; Check the result
	bt AX, 1
	jnc .done

	;; A20 is on
	mov word [SS:(BP + 16)], 0

	.done:
	sti
	popa
	pop AX	
	ret


altKeyboardMethod:
	;; This is alternate way to set A20 using the keyboard (which is
	;; supposedly not supported on many chipsets).
	
	;; Save space on the stack for the return code
	sub SP, 2
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; By default, return the value -1
	mov word [SS:(BP + 16)], -1

	;; Make sure interrupts are disabled
	cli

	;; Wait for the controller to be ready for a command
	call keyboardCommandWait

	;; Tell the controller we want to turn on A20
	mov AL, 0DFh
	out 64h, AL

	;; Delay
	call delay

	;; Attempt to read back the A20 status to ensure it was enabled.
	call keyboardRead60

	;; Check the result
	bt AX, 1
	jnc .done

	;; A20 is on
	mov word [SS:(BP + 16)], 0
		
	.done:
	sti
	popa
	pop AX	
	ret


loaderEnableA20:
	;; This routine will attempt to enable the A20 address line using
	;; various methods, if necessary.  Takes no arguments, and sets the
	;; 'fatal' error flag on total failure.

	pusha

	;; Try to let the BIOS do it for us
	call biosMethod
	cmp AX, 0
	jz .done

	;; Try the 'port 92h' method
	call port92Method
	cmp AX, 0
	jz .done

	;; Try the standard keyboard method
	call keyboardMethod
	cmp AX, 0
	jz .done

	;; Try an alternate keyboard method
	call altKeyboardMethod
	cmp AX, 0
	jz .done

	;; OK, we weren't able to set the A20 address line, so we'll not be
	;; able to access much memory.  We can give a fairly helpful error
	;; message, however, because in my experience, this tends to happen
	;; when laptops have external keyboards attached

	;; Print an error message, make a fatal error, and finish
	
	call loaderPrintNewline
	mov DL, ERRORCOLOR
	mov SI, A20BAD1
	call loaderPrint
	call loaderPrintNewline
	mov SI, A20BAD2
	call loaderPrint
	call loaderPrintNewline

	add byte [FATALERROR], 1

	.done:
	popa
	ret


;;
;; The data segment
;;

	SEGMENT .data
	ALIGN 4
	
A20BAD1		db 'Could not enable the A20 address line, which would cause serious memory', 0
A20BAD2		db 'problems for the kernel.  This is often associated with keyboard errors.', 0
