;;
;;  Visopsys
;;  Copyright (C) 1998-2004 J. Andrew McLaughlin
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

	GLOBAL main

	ORG 7C00h
	SEGMENT .text


main:
	jmp short bootCode		; 00 - 01 Jump instruction
	nop				; 02 - 02 No op
	
OEMName		times 8 db ' '		; 03 - 0A OEM Name
BytesPerSect	dw 0			; 0B - 0C Bytes per sector
SecPerClust	db 0			; 0D - 0D Sectors per cluster
ResSectors	dw 0			; 0E - 0F Reserved sectors
FATs		db 0 			; 10 - 10 Copies of FAT
RootDirEnts	dw 0			; 11 - 12 Max. rootdir entries
Sectors		dw 0			; 13 - 14 Sectors in logical image
Media		db 0	     		; 15 - 15 Media descriptor byte
FATSecs		dw 0			; 16 - 17 Sectors in FAT
SecPerTrack	dw 0			; 18 - 19 Sectors per track
Heads		dw 0			; 1A - 1B Number of heads
Hidden		dd 0			; 1C - 1F Number of hidden sectors
HugeSecs	dd 0		    	; 20 - 23 Real number of sectors
DriveNumber	db 0			; 24 - 24 BIOS drive #
Reserved1	db 0 			; 25 - 25 ?
BootSignature	db 0	          	; 26 - 26 Signature
VolumeID	dd 0		    	; 27 - 2A Volume ID
VolumeName	times 11 db ' '		; 2B - 35 Volume name
FSType		times 8 db ' '   	; 36 - 3D Filesystem type

	
bootCode:

	;; Make the data segment be zero
	pusha
	push DS
	push word 0
	pop DS

	mov SI, NOBOOT
	call print

	;; Hook the keyboard interrupt 9.

	;; Get the address of the current interrupt 9 handler and save it
	mov AX, word [0024h]			;; The offset
	mov word [OLDINT9], AX
	mov AX, word [0026h]			;; The segment
	mov word [OLDINT9 + 2], AX

	;; Move the address of our new handler into the interrupt
	;; table
	mov word [0024h], int9_handler		;; The offset
	mov word [0026h], CS			;; The segment

	;; Wait for a key press
	.wait:
	cmp byte [GOTKEY], 1
	jne .wait
	
	;; Restore the old interrupt 9 handler
	mov AX, word [OLDINT9]
	mov word [0024h], AX			;; The offset
	mov AX, word [OLDINT9 + 2]
	mov word [0026h], AX			;; The segment
	
	pop DS
	popa
	
	;; According to the docs by compaq/intel, we should issue an 
	;; int 18h instead to allow the BIOS to attempt loading some other
	;; operating system
	int 18h

	;; Just in case
	cli
	hlt

	
int9_handler:
	;; This routine handles the interrupt 9 key pressed event

	cli
	push DS
	push word 0
	pop DS
	mov byte [GOTKEY], 1
	mov AX, word [OLDINT9 + 2]
	mov BX, word [OLDINT9]
	pop DS

	;; Call the original handler
	push AX
	push BX
	retf

		
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
OLDINT9		dd 0	;; Address of the interrupt 9 handler
GOTKEY		db 0
	
;; This puts the value AA55h in the last two bytes of the boot
;; sector.  The BIOS uses this to determine whether this sector was
;; meant to be booted from (and also helps prevent us from making the
;; boot sector code larger than 512 bytes)
	
TIMES (510-($-$$))	db 0
endsector:		dw 0AA55h
