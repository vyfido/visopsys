;;
;;  Visopsys
;;  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
;;  bootsect-fat.s
;;

;; This code is a boot sector for generic FAT filesystems

	ORG 7C00h
	SEGMENT .text


;; Memory constants
%define LOADERSEGMENT		0100h
%define LOADEROFFSET		0000h

%define BYTE			1
%define WORD			2
%define DWORD			4

;; Memory for storing things...

;; For int13 disk ops
%define HEAD			(ENDSECTOR + 2)
%define HEAD_SZ			BYTE
%define SECTOR			(HEAD + HEAD_SZ)
%define SECTOR_SZ		BYTE
%define CYLINDER		(SECTOR + SECTOR_SZ)
%define CYLINDER_SZ		WORD

;; Disk cmd packet for ext. int13
%define DISKPACKET		(CYLINDER + CYLINDER_SZ)
%define DISKPACKET_SZ		(BYTE * 16)


main:
	jmp short bootCode			; 00 - 01 Jump instruction
	nop					; 02 - 02 No op

	%include "bootsect-fatBPB.s"	

bootCode:

	cli

	;; Make the data segment registers be the same as the code segment
	xor AX, AX
	mov DS, AX
	mov ES, AX

	;; Set the stack to be at the top of the code
	mov SS, AX
	mov SP, main

	sti

	pusha

	;; The BIOS will pass the boot device number to us in the DL
	;; register.  Make sure that our 'DriveNumber' value from the
	;; boot sector is the correct drive number.
	mov byte [DriveNumber], DL

	;; If we are not booting from a floppy, the MBR code should have put
	;; a pointer to the MBR record for this partition in SI.  Calculate
	;; the partition starting sector and add it to the LOADERSTARTSECTOR
	;; value
	cmp DL, 80h
	jb .noOffset
	mov EAX, dword [SI + 8]
	add dword [LOADERSTARTSECTOR], EAX
	.noOffset:

	;; Get the drive parameters
	;; ES:DI = 0000h:0000h to guard against BIOS bugs
	push ES
	xor DI, DI
	mov AX, 0800h
	mov DL, byte [DriveNumber]
	int 13h
	jc IOError
	pop ES

	;; Save info
	shr DX, 8				; Number of heads, 0-based
	inc DX
	mov word [Heads], DX
	and CX, 003Fh				; Sectors per cylinder
	mov word [SecPerTrack], CX

	push dword [LOADERNUMSECTORS]
	push word LOADEROFFSET			; Offset where we'll move it
	push word LOADERSEGMENT			; Segment where we'll move it
	push dword [LOADERSTARTSECTOR]
	call read
	add SP, 12

	popa

	jmp LOADERSEGMENT:LOADEROFFSET


IOError:
	;; If we got a fatal IO error or something, we just have to print
	;; an error and try to let the BIOS select another device to boot.
	;; This isn't very helpful, but unfortunately this piece of code
	;; is too small to do very much else.

	mov SI, IOERR
	call print

	;; Wait for a key press
	mov AX, 0000h
	int 16h

	;; Continue to the next bootable media
	int 18h

	;; Stop, just in case
	.fatalErrorLoop:
	jmp .fatalErrorLoop


read:
	;; Proto: int read(dword logical, word seg, word offset, dword count);

	pusha
	
	;; Save the stack pointer
	mov BP, SP

	;; Determine whether int13 extensions are available
	cmp byte [DriveNumber], 80h
	jb .noExtended
	
	mov AH, 41h
	mov BX, 55AAh
	mov DL, byte [DriveNumber]
	int 13h
	jc .noExtended

	;; We have a nice extended read function which will allow us to
	;; just use the logical sector number for the read

	mov word [DISKPACKET], 0010h		; Packet size
	mov EAX, dword [SS:(BP + 26)]		; >
	mov word [DISKPACKET + 2], AX		; > Sector count
	mov AX, word [SS:(BP + 24)]		; >
	mov word [DISKPACKET + 4], AX		; > Offset
	mov AX, word [SS:(BP + 22)]		; > 
	mov word [DISKPACKET + 6], AX		; > Segment
	mov EAX, dword [SS:(BP + 18)]		; > 
	mov dword [DISKPACKET + 8], EAX		; > Logical sector 
	mov AX, 4200h
	mov DL, byte [DriveNumber]
	mov SI, DISKPACKET
	int 13h
	jc IOError

	;; Done
	jmp .done
	
	.noExtended:
	;; No extended functionality.  Read the sectors one at a time.
	push dword 0

	.readSector:
	;; Calculate the CHS.  First the sector
	pop EAX
	push EAX
	add EAX, dword [SS:(BP + 18)]
	xor EBX, EBX
	xor EDX, EDX
	mov BX, word [SecPerTrack]
	div EBX
	mov byte [SECTOR], DL		; The remainder
	add byte [SECTOR], 1		; Sectors start at 1
	
	;; Now the head and track
	xor EDX, EDX			; Don't need the remainder anymore
	xor EBX, EBX
	mov BX, word [Heads]
	div EBX
	mov byte [HEAD], DL		; The remainder
	mov word [CYLINDER], AX

	mov AX, 0201h			; Number to read, subfunction 2
	mov CX, word [CYLINDER]		; >
	rol CX, 8			; > Cylinder
	shl CL, 6			; >
	or CL, byte [SECTOR]		; Sector
	mov DH, byte [HEAD]		; Head
	mov DL, byte [DriveNumber]	; Disk
	mov BX, word [SS:(BP + 24)]	; Offset
	push ES				; Save ES
	mov ES, word [SS:(BP + 22)]	; Use user-supplied segment
	int 13h
	pop ES				; Restore ES
	jc IOError

	mov AX, word [BytesPerSect]	; > Increment the memory pointer
	add word [SS:(BP + 24)], AX	; > by one sector size
	pop EAX				; Increment the counter
	inc EAX
	push EAX
	cmp EAX, dword [SS:(BP + 26)]	; Check whether we're finished 
	jb .readSector
	pop EAX
	
	.done:	
	popa
	ret

	
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


IOERR			db 'I/O Error reading Visopsys loader', 0Dh, 0Ah
			db 'Press any key to continue.', 0Dh, 0Ah, 0

times (502-($-$$))	db 0

;; The installation process will record the logical starting sector of
;; the loader, and number of sectors to read, here.
LOADERSTARTSECTOR	dd 0
LOADERNUMSECTORS	dd 0
	
;; This puts the value AA55h in the last two bytes of the boot
;; sector.  The BIOS uses this to determine whether this sector was
;; meant to be booted from (and also helps prevent us from making the
;; boot sector code larger than 512 bytes)
ENDSECTOR:		dw 0AA55h
