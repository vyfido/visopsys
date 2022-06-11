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
;;  bootsect-fat12.s
;;

;; This code is a boot sector for FAT12 filesystems

	GLOBAL main

	ORG 7C00h
	SEGMENT .text


;; Memory constants
%define LOADERSEGMENT	0100h
%define LOADEROFFSET	0000h

%define NYBBLESPERENTRY		3
%define BYTESPERDIRENTRY	32
%define STARTCLUSTER		2

;; We need space to keep the FAT sectors in memory.  Keep it right after the
;; boot sector code
%define FATBUFFER	(endsector + 2)

		
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

	cli

	;; Make the data segment registers be the same as the code segment
	xor AX, AX
	mov DS, AX
	mov ES, AX

	;; Set the stack to be at the top of the code
	mov SS, AX
	mov SP, 7C00h

	sti

	;; The BIOS will pass the boot device number to us in the DL
	;; register.  Make sure that our 'DriveNumber' value from the
	;; boot sector is the correct drive number.
	mov byte [DriveNumber], DL

	;; If we are not booting from a floppy, then the MBR code should
	;; have put a pointer to the MBR record for this partition in SI
	mov word [PARTENTRY], SI

	;; Load FAT sectors
	
	;; Get the logical sector for the start of the FAT
	xor EAX, EAX
	mov AX, word [ResSectors]
	cmp DL, 80h
	jb .noOffset
	add EAX, dword [SI + 8]
	.noOffset:
	mov dword [LOGICALSECTOR], EAX
	
	push ES
	mov BX, FATBUFFER   	; Offset where we'll move it
	mov CX, word [FATSecs]	; Read FATSecs FAT sectors
	call read
	pop ES

	;; Now we must do a loop to walk through the FAT table gathering the
	;; loader's cluster numbers.  We have the starting cluster.  We're
	;; going to load the kernel starting at memory location 00001000 or
	;; 0100:0000

	mov word [NEXTCLUSTER], STARTCLUSTER
	mov word [MEMORYMARKER], 0000h

	.FATLoop:
	
	;; Calculate the logical sector number
	xor EAX, EAX
	mov AX, word [NEXTCLUSTER]
	call clusterToLogical

	push ES
	push word 0100h			; Segment where we'll move it
	pop ES
	mov BX, word [MEMORYMARKER]	; Offset where we'll move it
	xor CX, CX
	mov CL, byte [SecPerClust]	; Read 1 cluster
	call read
	pop ES

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
	;; Before we turn over control to the kernel loader, we need to
	;; tell it the filesystem type of the boot device
	push word 0			; FAT12 is filesystem type 0

	;; Pass the pointer to the partition table entry in SI
	mov SI, word [PARTENTRY]
	
	;; Go!
	jmp LOADERSEGMENT:LOADEROFFSET


clusterToLogical:
	;; This takes the cluster number in AX and returns the logical
	;; sector number in AX

	pusha
	
	sub EAX, 2		;  Subtract 2 (because they start at 2)
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

	cmp byte [DriveNumber], 80h
	jb .noOffset
	mov SI, word [PARTENTRY]
	add EAX, dword [SI + 8]
	.noOffset:
	
	add dword [LOGICALSECTOR], EAX
	
	popa
	ret
	
	
headTrackSector:
	;; This routine accepts the logical sector number in EAX.  It
	;; calculates the head, track and sector number on disk.

	;; We destroy a bunch of registers, so save them
	pusha

	;; First the sector
	xor EDX, EDX
	xor EBX, EBX
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

	popa
	ret


read:
	;; Takes the stored logical sector number, segment in ES, offset in
	;; BX, count in CX, and does the read.

	pusha
	
	;; Determine whether int13 extensions are available
	cmp byte [DriveNumber], 80h
	jb .noExtended
	push BX
	mov AH, 41h
	mov BX, 55AAh
	mov DL, byte [DriveNumber]
	int 13h
	pop BX

	jc .noExtended

	;; We have a nice extended read function which will allow us to
	;; just use the logical sector number for the read

	mov word [DISKPACKET + 2], CX
	mov word [DISKPACKET + 4], BX
	mov word [DISKPACKET + 6], ES
	mov EAX, dword [LOGICALSECTOR]
	mov dword [DISKPACKET + 8], EAX
	mov AH, 42h
	mov DL, byte [DriveNumber]
	mov SI, DISKPACKET
	int 13h
	jc .IOError

	;; Done
	jmp .done
	
	.noExtended:
	;; Calculate the CHS
	mov EAX, dword [LOGICALSECTOR]
	call headTrackSector

	push CX			; Number to read
	mov CX, word [CYLINDER]
	rol CX, 8
	or CL, byte [SECTOR]
	mov DH, byte [HEAD]
	mov DL, byte [DriveNumber]
	pop AX			; Number to read
	mov AH, 02h		; Subfunction 2
	int 13h
	jc .IOError

	.done:	
	popa
	ret

	.IOError:
	;; If we got a fatal IO error or something, we just have to stop.
	;; This isn't very helpful, but unfortunately this piece of code
	;; is too small to do very much else.

	mov SI, IOERR
	.charLoop:
	mov AH, 0Eh
	xor BH, BH
	mov AL, byte [SI]
	cmp AL, 0
	je .endPrint
	int 10h
	inc SI
	jmp .charLoop
	.endPrint:

	;; We used to stop here, but according to the docs by compaq/intel,
	;; we should issue an int 18h instead to allow the BIOS to attempt
	;; loading some other operating system
	int 18h

	;; Stop, just in case
	.fatalErrorLoop:
	jmp .fatalErrorLoop

		
;; Data.  There is no data segment, so this space will have to do
	
;; For int13 disk ops
HEAD            db 0
CYLINDER        dw 0
SECTOR		db 0
	
;; For extended int13 disk ops
DISKPACKET:	db 10h, 0		; Disk cmd packet for ext. int13 
		dw 0, 0, 0, 0, 0, 0, 0
	
MEMORYMARKER	dw 0
NEXTCLUSTER	dw 0
LOGICALSECTOR	dd 0
PARTENTRY	dw 0
	
IOERR		db 'I/O error', 0

;; This puts the value AA55h in the last two bytes of the boot
;; sector.  The BIOS uses this to determine whether this sector was
;; meant to be booted from (and also helps prevent us from making the
;; boot sector code larger than 512 bytes)
	
TIMES (510-($-$$))	db 0
endsector:		dw 0AA55h
