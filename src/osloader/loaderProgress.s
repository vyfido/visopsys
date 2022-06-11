;;
;;  Visopsys
;;  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
;;  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
;;
;;  loaderProgress.s
;;

	GLOBAL loaderMakeProgress
	GLOBAL loaderUpdateProgress
	GLOBAL loaderKillProgress

	EXTERN loaderPrint
	EXTERN loaderPrintNewline
	EXTERN loaderGetCursorAddress
	EXTERN loaderSetCursorAddress

	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"


loaderMakeProgress:
	;; Sets up a little progress indicator.

	pusha

	;; Disable the cursor
	mov CX, 2000h
	mov AH, 01h
	int 10h

	mov DL, FOREGROUNDCOLOR
	mov SI, PROGRESSTOP
	call loaderPrint
	call loaderPrintNewline
	call loaderGetCursorAddress
	add AX, 1
	push AX
	mov SI, PROGRESSMIDDLE
	call loaderPrint
	call loaderPrintNewline
	mov SI, PROGRESSBOTTOM
	call loaderPrint
	pop AX
	call loaderSetCursorAddress

	;; To keep track of how many characters we've printed in the
	;; progress indicator
	mov word [OLDPROGRESS], 0
	mov word [PROGRESSCHARS], 0

	.done:
	popa


loaderUpdateProgress:
	;; Update the progress indicator
	;;
	;; Proto:
	;;   void loaderUpdateProgress(word percent);

	pusha

	;; Save the stack pointer
	mov BP, SP

	mov AX, word [SS:(BP + 18)]

	mov BX, AX
	sub BX, word [OLDPROGRESS]
	cmp BX, (100 / PROGRESSLENGTH)
	jb .done

	mov word [OLDPROGRESS], AX

	;; Make sure we're not already at the end
	mov AX, word [PROGRESSCHARS]
	cmp AX, PROGRESSLENGTH
	jae .done
	inc AX
	mov word [PROGRESSCHARS], AX

	;; Print the character on the screen
	mov DL, GOODCOLOR
	mov CX, 1
	mov SI, PROGRESSCHAR
	call loaderPrint

	.done:
	popa
	ret


loaderKillProgress:
	;; Get rid of the progress indicator

	pusha

	call loaderPrintNewline
	call loaderPrintNewline

	.done:
	popa
	ret


	SEGMENT .data
	ALIGN 4

OLDPROGRESS		dw 0        ;; Percentage completed
PROGRESSCHARS	dw 0		;; Number of progress indicator chars showing
PROGRESSTOP		db 218
				times PROGRESSLENGTH db 196
				db 191, 0
PROGRESSMIDDLE	db 179
				times PROGRESSLENGTH db ' '
				db 179, 0
PROGRESSBOTTOM	db 192
				times PROGRESSLENGTH db 196
				db 217, 0
PROGRESSCHAR	db 177, 0

