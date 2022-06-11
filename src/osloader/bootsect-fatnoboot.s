;;
;;  Visopsys
;;  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
;;  bootsect-fatnoboot.s
;;

;; This code is for a non-bootable FAT12/16 filesystem

	ORG 7C00h
	SEGMENT .text


main:
	jmp short bootCode		; 00 - 01 Jump instruction
	nop				; 02 - 02 No op
	
	%include "bootsect-fatBPB.s"	

bootCode:

	;; Make the data segment be zero
	pusha
	push DS
	push word 0
	pop DS

	mov SI, NOBOOT
	call print

	;; Wait for a key press
	mov AX, 0000h
	int 16h

	pop DS
	popa
	
	;; According to the docs by compaq/intel, we should issue an 
	;; int 18h instead to allow the BIOS to attempt loading some other
	;; operating system
	int 18h

	;; Just in case
	cli
	hlt

	
print:
	;; The offset to the chars should already be in SI

	pusha

	mov AH, 0Eh
	mov BH, 0
	
	.characterLoop:
	mov AL, byte [SI]
	cmp AL, 0
	je .end
	int 10h
	inc SI
	jmp .characterLoop
	
	.end:
	popa
	ret

	
;; Data.  There is no data segment, so this space will have to do

NOBOOT		db 'This is not a bootable Visopsys disk.', 0Dh, 0Ah
		db 'Press any key to continue.', 0Dh, 0Ah, 0
	
;; This puts the value AA55h in the last two bytes of the boot
;; sector.  The BIOS uses this to determine whether this sector was
;; meant to be booted from (and also helps prevent us from making the
;; boot sector code larger than 512 bytes)
	
times (510-($-$$))	db 0
ENDSECTOR:		dw 0AA55h
