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

;; FAT12 filesystem constants
%define FATSECTORS		9
%define BYTESPERSECTOR		512
%define SECTORSPERCLUSTER	1
%define NYBBLESPERENTRY		3
%define BYTESPERDIRENTRY	32
%define STARTCLUSTER		2

;; We need space to keep the FAT sectors in memory.  Keep
;; it right after the boot sector code
%define FATBUFFERSIZE	(FATSECTORS * SECTORSPERCLUSTER * BYTESPERSECTOR)
%define FATBUFFER	(7C00h + BYTESPERSECTOR)

;; A place for our stack.  We don't need much.  Put it after the
;; FAT buffer
%define STACKSIZE	512
%define STACKPOINTER	(FATBUFFER + FATBUFFERSIZE + STACKSIZE)

		
main:
	jmp bootCode

					; 00 - 02 jump instruction
OEMName		db 'Visopsys'		; 03 - 0A OEM Name
BytesPerSect	dw BYTESPERSECTOR	; 0B - 0C Bytes per sector
SecPerClust	db SECTORSPERCLUSTER	; 0D - 0D Sectors per cluster
ResSectors	dw 0001h		; 0E - 0F Reserved sectors
FATs		db 02h 			; 10 - 10 Copies of FAT
RootDirEnts	dw 00E0h		; 11 - 12 Max. rootdir entries
Sectors		dw 0B40h		; 13 - 14 Sectors in logical image
Media		db 0F0h	     		; 15 - 15 Media descriptor byte
FATSecs		dw FATSECTORS		; 16 - 17 Sectors in FAT
SecPerTrack	dw 0012h		; 18 - 19 Sectors per track
Heads		dw 0002h		; 1A - 1B Number of heads
Hidden		dd 00000000h		; 1C - 1D Number of hidden sectors
HugeSecs	dd 00000B40h    	; 1D - 20 Real number of sectors
DriveNumber	db 00h			; 21 - 21 BIOS dr. #
Reserved1	db 00h 			; 22 - 22 ?
BootSignature	db 29h          	; 23 - 23 Signature
VolumeID	dd 00000001h    	; 24 - 27 Volume ID
VolumeName	db 'Visopsys   '	; 28 - 42 Volume name
FSType		db 'FAT12   '   	; 43 - 4A Filesystem type

IOERR		db 'Disk I/O error'
IOERRLEN	equ $-IOERR


bootCode:

	;; Make the data segment registers be the same as the code segment
	mov AX, CS
	mov DS, AX
	mov ES, AX
	mov FS, AX
	mov GS, AX
	
	;; Make sure the stack register is the same as the code as well
	mov SS, AX

	;; Make sure the stack pointer is set to something reasonable
	mov SP, STACKPOINTER

	;; The BIOS will pass the boot device number to us in the DL
	;; register.  Save it for future use
	mov byte [BOOTDEVICE], DL
	
	xor AX, AX
	push AX

	;; Load FAT sectors
	
	.readAttempt:
	push ES
	mov AX, SS	        ; Segment where we'll move it
	mov ES, AX
	
	mov BX, FATBUFFER   	; Offset where we'll move it
	mov AH, 02h
	mov AL, FATSECTORS	; Subfunction 2, Read FATSECTORS FAT sectors
	mov CX, 0002h           ; Starting at track 0, sector 2
	mov DH, 00h             ; Head 0
	mov DL, byte [BOOTDEVICE]
	int 13h

	;; Restore ES
	pop ES

	jnc gotFAT

	;; We'll reset the disk and retry 4 more times
	xor AH, AH
	mov DL, byte [BOOTDEVICE]
	int 13h

	pop AX
	inc AX
	push AX
	cmp AL, 05h
	jnae .readAttempt

	jmp IOError


gotFAT:
	pop AX	; Leftover

	mov word [NEXTCLUSTER], STARTCLUSTER

	;; Now we must do a loop to walk through the FAT table
	;; gathering the kernel's cluster numbers.  We have the
	;; starting cluster.  We're going to load the kernel
	;; starting at memory location 00001000 or 0100:0000

	mov word [MEMORYMARKER], 0000h

	xor AX, AX
	push AX


FATLoop:
	.readAttempt:

	mov AX, word [NEXTCLUSTER]

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
	mov DL, byte [BOOTDEVICE]
	int 13h

	;; Restore ES
	pop ES

	jnc .gotCluster

	;; We'll reset the disk and retry 4 more times
	xor AH, AH
	mov DL, byte [BOOTDEVICE]
	int 13h

	pop AX
	inc AX
	push AX
	cmp AL, 05h
	jnae .readAttempt

	jmp IOError


	.gotCluster:
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

	add word [MEMORYMARKER], (BYTESPERSECTOR * SECTORSPERCLUSTER)
	jmp FATLoop


end:
	;; Before we turn over control to the kernel loader,
	;; we have to pass it some information about the disk.
	;; We use the stack to pass the info.
	push word [ResSectors]
	push word [FATSecs]
	xor AX, AX
	mov AL, byte [FATs]
	push AX
	push word [RootDirEnts]
	push word 0	; FAT12 is filesystem type 0
	xor AX, AX
	mov AL, byte [SecPerClust]
	push AX
	push word [BytesPerSect]
	xor AX, AX
	mov AL, byte [DriveNumber]
	push AX
	
	jmp LOADERSEGMENT:LOADEROFFSET


IOError:
	;; If we got a fatal IO error or something, we just have
	;; to stop.  This isn't very helpful, but unfortunately this
	;; piece of code is too small to do very much else.

	mov SI, IOERR
	mov CX, IOERRLEN
	mov DL, 04h
	call print

	;; We used to stop here, but according to the docs by
	;; compaq/intel, we should issue an int 18h instead to allow
	;; the BIOS to attempt loading some other operating system
	int 18h

	;; Stop, just in case
	.fatalErrorLoop:
	nop
	jmp .fatalErrorLoop


headTrackSector:
	;; this routine accepts the cluster number in AX.
	;; First it calculates the logical sector, then
	;; the head, track and sector number on disk.  It
	;; places them in the variables of the same name

	;; We destroy a bunch of registers, so save them
	pusha

	sub AX, 2	;  Subtract 2 (because they start at 2)
	
	mul byte [SecPerClust]

	;; This little sequence figures out how many
	;; sectors are reserved on the disk for the root
	;; directory

	push AX
	mov AX, BYTESPERDIRENTRY
	mul word [RootDirEnts]		; AX mult by word - ans. in DX:AX
	mov DX, 0			; Clear for division
	div word [BytesPerSect]		; DX:AX divided by word
	mov BX, AX	; BX contains the sectors reserved for root dir

	pop AX

	add AX, 1		; The boot sector
	add AX, BX		; Root dir sectors
	add AX, word [FATSecs]	; Sectors for 1st FAT
	add AX, word [FATSecs]	; Sectors for 2nd FAT

	;; Now we have the logical sector number in AX
	mov BX, AX   	; So we have the value for the next op

	;; First the sector
	mov DX, 0
	div word [SecPerTrack]
	add DL, 01h			; Sectors start at 1
	mov byte [SECTOR], DL		; The remainder
	mov AX, BX

	;; Now the head and track
	mov DX, 0
	div word [SecPerTrack]
	mov DX, 0
	div word [Heads]
	mov byte [HEAD], DL		; The remainder
	mov byte [TRACK], AL

	popa
	ret


print:
	;; The offset to the chars should already be in
	;; SI, the segment in DS, the length should be in CX,
	;; and the colour in DL.

	pusha

	;; Now we have a destination segment and offset, which should go
	;; into ES and DI

	mov DI, 0

	push ES
	mov AX, (SCREENSTART / 16)
	mov ES, AX

	mov AX, DX

	.characterLoop:
	movsb
	stosb
	dec CX
	jnz .characterLoop

	pop ES
	
	popa
	ret

	
;; Data.  There is no data segment, so this space will have to do
	
BOOTDEVICE	db 0
HEAD            db 0
TRACK           db 0
SECTOR		db 0
MEMORYMARKER	dw 0
NEXTCLUSTER	dw 0

COPYRIGHT	db 'Visopsys FAT12 boot v1.0 Copyright '
		db '(c) 1998-2000, J. Andrew McLaughlin'

;; This puts the value AA55h in the last two bytes of the boot
;; sector.  The BIOS uses this to determine whether this sector was
;; meant to be booted from (and also helps prevent us from making the
;; boot sector code larger than 512 bytes)
	
TIMES (510-($-$$)) DB 0
DW 0AA55h
