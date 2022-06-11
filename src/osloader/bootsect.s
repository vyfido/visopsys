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
;;  bootsect.s
;;

;; This routine is strictly for booting from a floppy disk,
;; and thus assumes that the filesystem is FAT12

	GLOBAL main

	ORG 7C00h
	SEGMENT .text

	%include "../kernel/kernelAssemblerHeader.h"


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
	
	;; The data below are only samples that are appropriate for a
	;; FAT12 floppy disk.  These should not be used, for example, on
	;; a hard disk boot sector
	
OEMName		db 'Visopsys'		; 03 - 0A OEM Name
BytesPerSect	dw 512			; 0B - 0C Bytes per sector
SecPerClust	db 1			; 0D - 0D Sectors per cluster
ResSectors	dw 1			; 0E - 0F Reserved sectors
FATs		db 2 			; 10 - 10 Copies of FAT
RootDirEnts	dw 00E0h		; 11 - 12 Max. rootdir entries
Sectors		dw 0B40h		; 13 - 14 Sectors in logical image
Media		db 0F0h	     		; 15 - 15 Media descriptor byte
FATSecs		dw 9			; 16 - 17 Sectors in FAT
SecPerTrack	dw 18			; 18 - 19 Sectors per track
Heads		dw 2			; 1A - 1B Number of heads
Hidden		dd 0			; 1C - 1F Number of hidden sectors
HugeSecs	dd 00000B40h    	; 20 - 23 Real number of sectors
DriveNumber	db 0			; 24 - 24 BIOS drive #
Reserved1	db 0 			; 25 - 25 ?
BootSignature	db 29h          	; 26 - 26 Signature
VolumeID	dd 00000001h    	; 27 - 2A Volume ID
VolumeName	db 'Visopsys   '	; 2B - 35 Volume name
FSType		db 'FAT12   '   	; 36 - 3D Filesystem type

bootCode:

	cli
	
	;; Make the data segment registers be the same as the code segment
	mov AX, CS
	mov DS, AX
	mov ES, AX

	;; Set the stack to be at the top of the code
	xor AX, AX
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

	;; mov SI, HELLO
	;; call print
	
	;; Load FAT sectors
	
	;; Get the CHS for the start of the FAT
	mov AX, word [ResSectors]
	call headTrackSector
	
	;; A counter to keep track of read attempts
	push word 0

	.readAttempt:
	push ES
	mov BX, FATBUFFER   	; Offset where we'll move it
	mov CH, byte [TRACK]
	mov CL, byte [SECTOR]
	mov DH, byte [HEAD]
	mov DL, byte [DriveNumber]
	mov AX, word [FATSecs]	; Read FATSecs FAT sectors
	mov AH, 02h		; Subfunction 2
	int 13h

	;; Restore ES
	pop ES

	jnc gotFAT

	;; We'll reset the disk and retry 4 more times
	xor AH, AH
	mov DL, byte [DriveNumber]
	int 13h

	pop AX
	inc AX
	push AX
	cmp AL, 05h
	jnae .readAttempt

	jmp IOError


gotFAT:
	pop AX	; Leftover

	;; Now we must do a loop to walk through the FAT table
	;; gathering the kernel's cluster numbers.  We have the
	;; starting cluster.  We're going to load the kernel
	;; starting at memory location 00001000 or 0100:0000

	mov word [NEXTCLUSTER], STARTCLUSTER
	mov word [MEMORYMARKER], 0000h


FATLoop:
	push word 0

	.readAttempt:

	mov AX, word [NEXTCLUSTER]

	;; Get the logical sector number
	call clusterToLogical

	mov AX, word [LOGICALSECT]
	
	;; Get the CHS
	call headTrackSector

	push ES
	mov AX, 0100h			; Segment where we'll move it
	mov ES, AX
	mov BX, word [MEMORYMARKER]	; Offset where we'll move it
	mov AL, byte [SecPerClust]	; Read 1 cluster
	mov AH, 02h			; Subfunction 2
	mov CH, byte [TRACK]
	mov CL, byte [SECTOR]
	mov DH, byte [HEAD]
	mov DL, byte [DriveNumber]
	int 13h

	;; Restore ES
	pop ES

	jnc .gotCluster

	;; We'll reset the disk and retry 4 more times
	xor AH, AH
	mov DL, byte [DriveNumber]
	int 13h

	pop AX
	inc AX
	push AX
	cmp AL, 05h
	jnae .readAttempt

	jmp IOError


	.gotCluster:
	
	pop AX	; Leftover
	
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
	jae end
	mov word [NEXTCLUSTER], AX

	;; Adjust our data pointer by the amount of data we just read
	xor CX, CX
	mov CL, byte [SecPerClust]
	.incrementPointer:
	mov AX, word [BytesPerSect]
	add word [MEMORYMARKER], AX
	loop .incrementPointer
	jmp FATLoop


end:
	;; Before we turn over control to the kernel loader, we need to
	;; tell it the filesystem type of the boot device
	push word 0			; FAT12 is filesystem type 0

	;; Pass the pointer to the partition table entry in SI
	mov SI, word [PARTENTRY]
	
	;; Go!
	jmp LOADERSEGMENT:LOADEROFFSET


IOError:
	;; If we got a fatal IO error or something, we just have
	;; to stop.  This isn't very helpful, but unfortunately this
	;; piece of code is too small to do very much else.

	mov SI, IOERR
	call print

	;; We used to stop here, but according to the docs by
	;; compaq/intel, we should issue an int 18h instead to allow
	;; the BIOS to attempt loading some other operating system
	int 18h

	;; Stop, just in case
	.fatalErrorLoop:
	jmp .fatalErrorLoop

	
clusterToLogical:
	;; This takes the cluster number in AX and returns the logical
	;; sector number in AX

	pusha
	
	sub AX, 2		;  Subtract 2 (because they start at 2)
		
	mul byte [SecPerClust]	; How many sectors per cluster?

	;; This little sequence figures out how many sectors are reserved
	;;  on the disk for the FATs and root directory

	push AX
	mov AX, BYTESPERDIRENTRY
	mul word [RootDirEnts]		; AX mult by word - ans. in DX:AX
	xor DX, DX			; Clear for division
	div word [BytesPerSect]		; DX:AX divided by word
	mov DX, AX			; DX contains the sectors reserved for
					; root dir
	pop AX

	add AX, word [ResSectors]	; The reserved sectors
	add AX, word [FATSecs]		; Sectors for 1st FAT
	add AX, word [FATSecs]		; Sectors for 2nd FAT
	add AX, DX			; Root dir sectors

	mov word [LOGICALSECT], AX
	
	popa
	ret
	
	
headTrackSector:
	;; This routine accepts the cluster number in AX.  It calculates
	;; the head, track and sector number on disk.  It places them in
	;;  the variables of the same name

	;; We destroy a bunch of registers, so save them
	pusha

	;; Add the partition starting LBA offset, if applicable
	cmp byte [DriveNumber], 80h
	jne .noOffset
	mov SI, word [PARTENTRY]
	add AX, word [SI + 8]

	.noOffset:
	;; We have the logical sector number in AX

	;; First the sector
	xor DX, DX
	div word [SecPerTrack]
	mov byte [SECTOR], DL		; The remainder
	add byte [SECTOR], 1		; Sectors start at 1
	
	;; Now the head and track
	xor DX, DX			; Don't need the remainder anymore
	div word [Heads]
	mov byte [HEAD], DL		; The remainder
	mov byte [TRACK], AL

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

	
;; Data.  There is no data segment, so this space will have to do
	
IOERR		db 'Disk I/O error', 0Dh, 0Ah, 0
HELLO		db 'Visopsys', 0Dh, 0Ah, 0

HEAD            db 0
TRACK           db 0
SECTOR		db 0
MEMORYMARKER	dw 0
NEXTCLUSTER	dw 0
LOGICALSECT	dw 0
PARTENTRY	dw 0
	
;; This puts the value AA55h in the last two bytes of the boot
;; sector.  The BIOS uses this to determine whether this sector was
;; meant to be booted from (and also helps prevent us from making the
;; boot sector code larger than 512 bytes)
	
TIMES (510-($-$$))	db 0
endsector:		dw 0AA55h
