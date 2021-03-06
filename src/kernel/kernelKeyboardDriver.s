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
;;  kernelKeyboardDriver.s
;;
	
	SEGMENT .text
	BITS 32

	GLOBAL kernelKeyboardDriverInitialize
	GLOBAL kernelKeyboardDriverSetStream
	GLOBAL kernelKeyboardDriverReadData
	GLOBAL kernelKeyboardDriverReadMouseData

	%include "kernelAssemblerHeader.h"


consoleLogin:
	;; This function gets called when the console user presses F1
	;; to launch a new console login process

	pusha
	sti	; Enable interrupts
	call kernelConsoleLogin
	popa
	ret
	

reboot:
	;; This function gets called when the console user initiates a
	;; reboot by pressing CTRL-ALT-DEL

	;; Enable interrupts again
	sti

	push dword 1		; force the reboot
	push dword 1		; reboot enum
	call kernelShutdown
	add ESP, 8

	;; Wait
	jmp $

	
kernelKeyboardDriverInitialize:
	;; This routine accepts parameters from the caller, then issues
	;; the appropriate commands to the keyboard controller to set
	;; keyboard settings.
	;; The C prototype is:
	;; int kernelKeyboardDriverInitialize(void)

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Wait for port 64h to be ready for a command.  We know it's
	;; ready when port 64h bit 1 is 0
	.diagWaitLoop1:
	xor EAX, EAX
	in AL, 64h
	bt AX, 1
	jc .diagWaitLoop1

	;; Tell the keyboard to enable
	mov AL, 0AEh
	out 64h, AL
	
	mov byte [INITIALIZED], 1
	popa
	mov EAX, 0		; return success
	ret


kernelKeyboardDriverSetStream:	
	;; Set the incoming characters to be placed in the requested stream
	;; The C prototype is:
	;; int kernelKeyboardDriverSetStream(stream *, <append function>)
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Save the address of the kernelStream we were passed to use for
	;; keyboard data
	mov EAX, dword [SS:(EBP + 36)]
	mov dword [CONSOLESTREAM], EAX
	mov EAX, dword [SS:(EBP + 40)]
	mov dword [APPENDFUNCTION], EAX
	
	popa
	mov EAX, 0		; return success
	ret


kernelKeyboardDriverReadData:
	;; This routine reads the keyboard data and returns it to the
	;; keyboard console text input stream

	pusha

	;; Don't do anything unless we're initialized
	cmp byte [INITIALIZED], 1
	jne near .done
	
	;; Read the data from port 60h
	xor EAX, EAX
	in AL, 60h

	;; We only care about a couple of cases if it's a key release
	cmp AL, 128
	jb .keyPress

	;; If an extended scan code is coming next...
	cmp AL, 224
	je .nextExtended

	;; If the last one was an extended, but this is a key release,
	;; then we have to make sure we clear the extended flag even though
	;; we're ignoring it
	mov byte [EXTENDED], 0
	
	cmp AL, (128 + 42)	; Left shift
	je .shiftRelease
	cmp AL, (128 + 54)	; Right shift
	je .shiftRelease
	cmp AL, (128 + 29)	; Left Control
	je .controlRelease
	cmp AL, (128 + 56)	; Left Alt
	je .altRelease
	jmp .done

	.nextExtended:
	;; We have an extended scan code coming up, so set the flag so it
	;; can be collected next time
	mov byte [EXTENDED], 1
	jmp .done
	
	.shiftRelease:	
	;; We reset the value of the SHIFT_DOWN byte
	mov byte [SHIFT_DOWN], 0
	jmp .done

	.controlRelease:
	;; Reset the value of the CONTROL_DOWN byte
	mov byte [CONTROL_DOWN], 0
	jmp .done

	.altRelease:
	;; Reset the value of the ALT_DOWN byte
	mov byte [ALT_DOWN], 0
	jmp .done

	.keyPress:	
	;; Check whether the last key pressed was one with an extended
	;; scan code.  If so, we need to do a little something.  We put
	;; a NULL into the buffer, followed by the scan code of the
	;; key, so that individual applications can deal with it as they
	;; like.
	cmp byte [EXTENDED], 1
	jne .notExtended

	;; The last one was an extended flag.  Clear the flag
	mov byte [EXTENDED], 0

	.notExtended:	
	;; Check for a few 'special action' keys
	
	cmp AL, 42		; Check for left shift
	je .shiftPress
	cmp AL, 54		; Check for right shift
	je .shiftPress
	cmp AL, 29		; Check for left control
	je .controlPress
	cmp AL, 56		; Check for left alt
	je .altPress
	cmp AL, 59		; Check for F1
	je .F1Press
	cmp AL, 60		; Check for F2
	je .F2Press

	jmp .regularKey
		
	.shiftPress:	
	mov byte [SHIFT_DOWN], 1
	jmp .done

	.controlPress:
	mov byte [CONTROL_DOWN], 1
	jmp .done
	
	.altPress:
	mov byte [ALT_DOWN], 1
	jmp .done

	.F1Press:
	call consoleLogin
	jmp .done

	.F2Press:
	call kernelMultitaskerDumpProcessList
	jmp .done

	.regularKey:	
	;; Now the regular keys

	;; Check whether the control or shift keys are pressed.
	;; Shift overrides control.
	mov BL, byte [SHIFT_DOWN]
	cmp BL, 1
	je .shiftDown

	mov BL, byte [CONTROL_DOWN]
	cmp BL, 1
	je .controlDown

	mov EBX, EAX
	add EBX, (KEYMAP - 1)
	jmp .gotASCII

	.shiftDown:	
	mov EBX, EAX
	add EBX, (SHIFTKEYMAP - 1)
	jmp .gotASCII

	.controlDown:
	;; CTRL-ALT-DEL means reboot
	cmp byte [ALT_DOWN], 1
	jne .noSpecial
	cmp EAX, 83		; DEL key
	jne .noSpecial

	;; We will reboot
	call reboot
	jmp .done
		
	.noSpecial:
	mov EBX, EAX
	add EBX, (CONTROLKEYMAP - 1)

	;; Fall through to .gotASCII
	
	.gotASCII:
	;; Make sure we have a text stream to append to.  If not, do nothing.
	cmp dword [CONSOLESTREAM], 0
	je .done
	
	push dword [EBX]	; The character to print
	push dword [CONSOLESTREAM]
	call dword [APPENDFUNCTION]
	add ESP, 8
	
	.done:
	popa
	ret
	
	
	SEGMENT .data
	ALIGN 4
	
	
;; Keyboard handler data

INITIALIZED	db 0
SHIFT_DOWN	db 0
CONTROL_DOWN	db 0
ALT_DOWN	db 0
EXTENDED	db 0
	
CONSOLESTREAM	dd 0
APPENDFUNCTION  dd 0

KEYMAP		db 27, '1234567890-=', 8, 9, 'q'		;; 00-0F
		db 'wertyuiop[]', 10, 0, 'asd'			;; 10=1F 
		db 'fghjkl;', 39, 96, 0, '\', 'zxcvb'		;; 20-2F
		db 'nm,./', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0	;; 30-3F
		db 0, 0, 0, 0, 0, 0, 13, 17			;; 40-47
		db 11, '-', 18, '5', 19, '+1', 20		;; 48-4F
		db 12, '0', 127, 0, 0				;; 50-54

SHIFTKEYMAP	db 27, '!@#$%^&*()_+', 8, 9, 'Q'		;; 00-0F
		db 'WERTYUIOP{}', 10, 0, 'ASD'			;; 10=1F 
		db 'FGHJKL:', 34, '~', 0, '|', 'ZXCVB'		;; 20-2F
		db 'NM<>?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0	;; 30-3F
		db 0, 0, 0, 0, 0, 0, '789-456+12'		;; 40-4F
		db '30.', 0, 0					;; 50-54

CONTROLKEYMAP	db 27, '1234567890-=', 8, 9, 17			;; 00-0F
		db 23, 5, 18, 20, 25, 21, 9, 15
		db 16, '[]', 10, 0, 1, 19, 4			;; 10=1F 
		db 6, 7, 8, 10, 11, 12, ';', 34
		db '`', 0, 0, 26, 24, 3, 22, 2			;; 20-2F
		db 14, 13, ',./', 0, '*', 0
		db ' ', 0, 0, 0, 0, 0, 0, 0			;; 30-3F
		db 0, 0, 0, 0, 0, 0, 13, 17			;; 40-47
		db 11, '-', 18, '5', 19, '+1', 20		;; 48-4F
		db 12, '0', 127, 0, 0				;; 50-54
