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
;;  bootsect-fat12.s
;;

;; This code is a boot sector for FAT12 filesystems

	ORG 7C00h
	SEGMENT .text


;; Memory constants
%define LOADERSEGMENT	0100h
%define LOADEROFFSET	0000h

%define NYBBLESPERENTRY		3
%define BYTESPERDIRENTRY	32
%define STARTCLUSTER		2

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

;; Generic stuff
%define MEMORYMARKER		(DISKPACKET + DISKPACKET_SZ)
%define MEMORYMARKER_SZ		WORD
%define NEXTCLUSTER		(MEMORYMARKER + MEMORYMARKER_SZ)
%define NEXTCLUSTER_SZ		WORD
%define LOGICALSECTOR		(NEXTCLUSTER + NEXTCLUSTER_SZ)
%define LOGICALSECTOR_SZ	DWORD
%define PARTSTART		(LOGICALSECTOR + LOGICALSECTOR_SZ)
%define PARTSTART_SZ		DWORD

;; We need space to keep the FAT sectors in memory.  Keep it right after the
;; boot sector code
%define FATBUFFER		(PARTSTART + PARTSTART_SZ)

		
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
	;; the partition starting sector and add it to the PARTSTART value
	mov dword [PARTSTART], 0
	cmp DL, 80h
	jb .noOffset
	mov EAX, dword [SI + 8]
	mov dword [PARTSTART], EAX
	.noOffset:

	;; Get the drive parameters
	;; ES:DI = 0000h:0000h to guard against BIOS bugs
	push ES
	xor DI, DI
	mov AX, 0800h
	mov DL, byte [DriveNumber]
	int 13h
	jc near IOError
	pop ES

	;; Save info
	shr DX, 8				; Number of heads, 0-based
	inc DX
	mov word [Heads], DX
	and CX, 003Fh				; Sectors per cylinder
	mov word [SecPerTrack], CX

	;; Load FAT sectors
	
	;; Get the logical sector for the start of the FAT
	xor EAX, EAX
	mov AX, word [ResSectors]
	add EAX, dword [PARTSTART]

	push word [FATSecs]
	push word FATBUFFER	; Offset where we'll put it
	push word ES
	push dword EAX
	call read
	add SP, 10

	;; Now we must do a loop to walk through the FAT table gathering the
	;; loader's cluster numbers.  We have the starting cluster.  We're
	;; going to load the kernel starting at memory location 00001000 or
	;; 0100:0000

	mov word [NEXTCLUSTER], STARTCLUSTER
	mov word [MEMORYMARKER], 0000h

	.FATLoop:
	
	;; Calculate the logical sector number for the current cluster
	xor EAX, EAX
	mov AX, word [NEXTCLUSTER]
	sub AX, 2			;  Subtract 2 (because they start at 2)
	xor EBX, EBX
	mov BL, byte [SecPerClust]	; How many sectors per cluster?
	mul EBX
	mov dword [LOGICALSECTOR], EAX
	
	;; This little sequence figures out how many sectors are reserved
	;;  on the disk for the FATs and root directory
	mov AX, BYTESPERDIRENTRY
	mul word [RootDirEnts]		; AX mult by word - ans. in DX:AX
	xor DX, DX			; Clear for division
	div word [BytesPerSect]		; DX:AX divided by word
	mov DX, AX			; DX contains the sectors reserved for
					; root dir
	xor EAX, EAX
	mov AX, word [ResSectors]	; The reserved sectors
	add AX, word [FATSecs]		; Sectors for 1st FAT
	add AX, word [FATSecs]		; Sectors for 2nd FAT
	add AX, DX			; Root dir sectors
	add EAX, dword [PARTSTART]
	
	add dword [LOGICALSECTOR], EAX

	xor AX, AX
	mov AL, byte [SecPerClust]
	push word AX			; Read 1 cluster
	push word [MEMORYMARKER]	; Offset where we'll move it
	push word 0100h			; Segment where we'll move it
	push dword [LOGICALSECTOR]
	call read
	add SP, 10

	;; Now get the next cluster in the chain
	mov AX, word [NEXTCLUSTER]
	mov BX, NYBBLESPERENTRY	 ; For FAT12, 3 nybbles per entry
	mul BX   	; We can ignore DX because it shouldn't
			; be bigger than a word
	mov BX, 0002h	; 2 nybbles per byte
	xor DX, DX
	div BX
	;; Now, AX holds the quotient and DX holds the remainder
	mov BX, AX
	add BX, FATBUFFER
	mov AX, word [BX]
	
	;; Now we have to shift or mask the value in AX depending
	;; on whether DX is 1 or 0
	cmp DX, 0001h
	jne .mask

	;; we'll shift
	shr AX, 4
	jmp .doneConvert

	.mask:
	;; we'll mask
	and AX, 0FFFh

	.doneConvert:
	cmp AX, 0FF8h
	jae .done
	mov word [NEXTCLUSTER], AX

	;; Adjust our data pointer by the amount of data we just read
	xor CX, CX
	mov CL, byte [SecPerClust]
	.incrementPointer:
	mov AX, word [BytesPerSect]
	add word [MEMORYMARKER], AX
	loop .incrementPointer

	jmp .FATLoop

	.done:
	popa
	
	;; Go!
	jmp LOADERSEGMENT:LOADEROFFSET


IOError:
	;; If we got a fatal IO error or something, we just have to stop.
	;; This isn't very helpful, but unfortunately this piece of code
	;; is too small to do very much else.

	;; We used to stop here, but according to the docs by compaq/intel,
	;; we should issue an int 18h instead to allow the BIOS to attempt
	;; loading some other operating system
	int 18h

	;; Stop, just in case
	.fatalErrorLoop:
	jmp .fatalErrorLoop


read:
	;; Takes the stored logical sector number, segment in ES, offset in
	;; BX, count in CX, and does the read.
	;; Proto: int read(dword logical, word seg, word offset, word count);

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
	mov AX, word [SS:(BP + 26)]		; >
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
	;; Calculate the CHS.  First the sector
	mov EAX, dword [SS:(BP + 18)]
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

	mov AX, word [SS:(BP + 26)]	; Number to read
	mov AH, 02h			; Subfunction 2
	mov CX, word [CYLINDER]		; >
	rol CX, 8			; > Cylinder
	shl CL, 6			; >
	or CL, byte [SECTOR]		; Sector
	mov DH, byte [HEAD]		; Head
	mov DL, byte [DriveNumber]	; Drive
	mov BX, word [SS:(BP + 24)]	; Offset
	push ES				; Save ES
	mov ES, word [SS:(BP + 22)]	; Use user-supplied segment
	int 13h
	pop ES				; Restore ES
	jc IOError

	.done:	
	popa
	ret

		
;; This puts the value AA55h in the last two bytes of the boot
;; sector.  The BIOS uses this to determine whether this sector was
;; meant to be booted from (and also helps prevent us from making the
;; boot sector code larger than 512 bytes)
	
times (510-($-$$))	db 0
ENDSECTOR:		dw 0AA55h
