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
;;  kernelKeyboardDriver.s
;;
	
	SEGMENT .text
	BITS 32

	GLOBAL kernelKeyboardDriverInitialize
	GLOBAL kernelKeyboardDriverReadData

	%include "kernelAssemblerHeader.h"


consoleLogin:
	;; This function gets called when the console user presss F2
	;; to launch a new console login process

	pusha
	sti	; Enable interrupts
	call kernelConsoleLogin
	popa
	ret
	

showProcesses:
	;; This function gets called when the console user presses F1
	;; to see a list of running processes
	pusha
	call kernelMultitaskerDumpProcessList
	popa
	ret

	
killProcess:
	;; This function gets called when the console user presses
	;; CTRL-C to stop the foreground process
	pusha
	call kernelMultitaskerTerminate
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
	add ESP, 4

	;; Wait
	jmp $
	

kernelKeyboardDriverInitialize:
	;; This routine accepts parameters from the caller, then issues
	;; the appropriate commands to the keyboard controller to set
	;; keyboard settings.
	;; The C prototype is:
	;; int kernelKeyboardDriverInitialize(stream *, <append function>)

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
	
	;; Delay
	jecxz $+2
	jecxz $+2

	;; We flash the keyboard lights on during initialization, then flash 
	;; them off to show that everything's good -- and also just to show 
	;; that we can, and because it looks cool

	;; Make the little keyboard lights go on

	;; Wait for port 60h to be ready for a command.  We know it's
	;; ready when port 64h bit 1 is 0
	.onWaitLoop1:
	in AL, 64h
	bt AX, 1
	jc .onWaitLoop1

	;; Tell the keyboard we want to change them
	mov AL, 0EDh
	out 60h, AL
	
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Wait for port 60h to be ready for a command.  We know it's
	;; ready when port 64h bit 1 is 0
	.onWaitLoop2:
	in AL, 64h
	bt AX, 1
	jc .onWaitLoop2

	;; Tell the keyboard to turn them on!
	mov AL, 00000111b
	out 60h, AL

	;; Delay
	jecxz $+2
	jecxz $+2


	;; OK, now we can do something worthwhile.  Save the address of
	;; the kernelStream we were passed to use for keyboard data
	mov EAX, dword [SS:(EBP + 36)]
	mov dword [CONSOLESTREAM], EAX
	mov EAX, dword [SS:(EBP + 40)]
	mov dword [APPENDFUNCTION], EAX
	
	
	;; Delay so the keyboard lights-on we did above is visible
	mov ECX, 0000FFFFh
	.delayLoop:
	dec ECX
	jnz .delayLoop

	;; Now make the little keyboard lights go off again

	;; Wait for port 60h to be ready for a command.  We know it's
	;; ready when port 64h bit 1 is 0
	.offWaitLoop1:
	in AL, 64h
	bt AX, 1
	jc .offWaitLoop1

	;; Tell the keyboard we want to change them
	mov AL, 0EDh
	out 60h, AL
	
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Wait for port 60h to be ready for a command.  We know it's
	;; ready when port 64h bit 1 is 0
	
	.offWaitLoop2:
	in AL, 64h
	bt AX, 1
	jc .offWaitLoop2

	;; Tell the keyboard to turn them off again!
	mov AL, 00000000b
	out 60h, AL
	
	;; Delay
	jecxz $+2
	jecxz $+2

	.done:
	popa
	mov EAX, 0		; return success
	ret


kernelKeyboardDriverReadData:
	;; This routine reads the keyboard data and returns it to the
	;; keyboard console text input stream

	pusha
	
	;; Make sure the routine has been initialized to get the address
	;; of the keyboard input stream buffer from the kernel
	cmp byte [INITIALIZED], 1
	je .initialized

	mov byte [INITIALIZED], 1
	
	.initialized:	
	;; Read the data from port 60h
	in AL, 60h

	;; Clear out the top of the register
	and EAX, 000000FFh

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

	;; The following scan codes should be treated the same as
	;; if they were not extended
	cmp AL, 28		; Keypad enter
	je .notExtended
	cmp AL, 29		; Right control
	je .notExtended
	cmp AL, 53		; Keypad '/'
	je .notExtended
	cmp AL, 56		; Right alt
	je .notExtended
	cmp AL, 83		; Del (not keypad)
	je .notExtended
	
	;; Jump to the code that puts FFh and scan codes into the buffer
	jmp .unprintable

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
	call showProcesses
	jmp .done

	.F2Press:
	call consoleLogin
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
	;; There are a couple of special cases we want to look for here.
	
	;; CTRL-C means kill the current process
	cmp EAX, 46
	jne .rebootCheck

	;; We stop the current process
	call killProcess
	jmp .noSpecial

	.rebootCheck:
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
	;; If it's unprintable, put an FF into the buffer followed by the
	;; scan code
	cmp byte [EBX], 0
	jne .printable

	.unprintable:		
	push EAX
	
	;; Put an FF into the keyboard buffer
	push dword 000000FFh
	push dword [CONSOLESTREAM]
	call dword [APPENDFUNCTION]
	add ESP, 8

	pop EAX
	
	;; Put the scan code into the buffer
	push dword EAX
	push dword [CONSOLESTREAM]
	call dword [APPENDFUNCTION]
	add ESP, 8

	jmp .done

	.printable:	
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
		db 0, 0, 0, 0, 0, 0, '789-456+12'		;; 40-4F
		db '30.', 0, 0					;; 50-54

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
		db 0, 0, 0, 0, 0, 0, '789-456+12'		;; 40-4F
		db '30.', 0, 0					;; 50-54
