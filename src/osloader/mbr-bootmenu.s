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
;;  mbr-bootmenu.s
;;

;; This code is the boot menu MBR code, which loads the boot menu program
;; from the rest of the track and runs it.

	ORG 7C00h
	SEGMENT .text

;; Memory constants
%define LOADERSEGMENT	0100h
%define LOADEROFFSET	0000h

%define BYTE			1
%define WORD			2
%define DWORD			4

;; Heap memory for storing things...

;; The place where we relocate our code
%define NEWCODELOCATION		0600h
%define CODE_SIZE		512

;; Device info
%define DISK			(NEWCODELOCATION + CODE_SIZE)
%define DISK_SZ			BYTE
%define NUMHEADS		(DISK + DISK_SZ)
%define NUMHEADS_SZ		WORD
%define NUMCYLS			(NUMHEADS + NUMHEADS_SZ)
%define NUMCYLS_SZ		WORD
%define NUMSECTS		(NUMCYLS + NUMCYLS_SZ)
%define NUMSECTS_SZ		BYTE

;; For int13 disk ops
%define HEAD			(NUMSECTS + NUMSECTS_SZ)
%define HEAD_SZ			BYTE
%define SECTOR			(HEAD + HEAD_SZ)
%define SECTOR_SZ		BYTE
%define CYLINDER		(SECTOR + SECTOR_SZ)
%define CYLINDER_SZ		WORD

;; Disk cmd packet for extended int13 disk ops
%define DISKPACKET		(CYLINDER + CYLINDER_SZ)
%define DISKPACKET_SZ		(BYTE * 16)

%define	NOTFOUND		(NEWCODELOCATION + (DATA1 - main))
%define	IOERR			(NEWCODELOCATION + (DATA2 - main))
%define	PART_TABLE		(NEWCODELOCATION + (DATA3 - main))

		
main:
	;; A jump is expected at the start of a boot sector, sometimes.
	jmp short .bootCode			; 00 - 01 Jump instruction
	nop					; 02 - 02 No op

	.bootCode:
	cli

	xor AX, AX
	mov DS, AX
	mov ES, AX

	;; Make the data segment registers be the same as the code segment
	;; Set the stack to be at the top of the code
	mov SS, AX
	mov SP, main
	
	sti

	pusha
	
	;; Relocate our code, so we can copy the chosen boot sector over
	;; top of ourselves
	mov SI, main
	mov DI, NEWCODELOCATION
	mov CX, CODE_SIZE
	cld
	rep movsb

	;; Jump to it
	jmp (NEWCODELOCATION + (jmpTarget - main))

jmpTarget:
		
	;; The BIOS will pass the boot device number to us in the DL
	;; register.
	mov byte [DISK], DL

	;; Get the drive parameters
	;; ES:DI = 0000h:0000h to guard against BIOS bugs
	xor DI, DI
	push ES
	mov AX, 0
	mov ES, AX
	mov AX, 0800h
	mov DL, byte [DISK]
	int 13h
	pop ES
	jc IOError

	;; Save info
	shr DX, 8				; Number of heads, 0-based
	inc DX
	mov word [NUMHEADS], DX
	and CX, 003Fh				; Sectors per cylinder
	mov byte [NUMSECTS], CL

	;; Load the boot menu at logical sector 1
	;; xor EAX, EAX
	;; mov AL, byte [NUMSECTS]
	;; sub AL, 1
	;; push dword EAX			; Read (NUMSECTS - 1) sectors
	push dword 4
	push word LOADEROFFSET			; Offset where we'll move it
	push word LOADERSEGMENT			; Segment where we'll move it
	push dword 1				; Start at logical sector 1
	call read
	add SP, 12

	popa

	;; Move the pointer to the start of the partiton table into SI
	mov SI, PART_TABLE
	
	;; Move the boot disk device number into DL
	mov DL, byte [DISK]

	;; Go
	jmp LOADERSEGMENT:LOADEROFFSET


IOError:
	;; If we got a fatal IO error or something, we just have to print
	;; an error and try to let the BIOS select another device to boot.
	;; This isn't very helpful, but unfortunately this piece of code
	;; is too small to do very much else.

	mov SI, IOERR
	call print
	
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
	cmp byte [DISK], 80h
	jb .noExtended
	
	mov AH, 41h
	mov BX, 55AAh
	mov DL, byte [DISK]
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
	mov DL, byte [DISK]
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
	mov BL, byte [NUMSECTS]
	div EBX
	mov byte [SECTOR], DL		; The remainder
	add byte [SECTOR], 1		; Sectors start at 1
	
	;; Now the head and track
	xor EDX, EDX			; Don't need the remainder anymore
	xor EBX, EBX
	mov BX, word [NUMHEADS]
	div EBX
	mov byte [HEAD], DL		; The remainder
	mov word [CYLINDER], AX

	mov EAX, dword [SS:(BP + 26)]	; Number to read
	mov AH, 02h			; Subfunction 2
	mov CX, word [CYLINDER]		; >
	rol CX, 8			; > Cylinder
	shl CL, 6			; >
	or CL, byte [SECTOR]		; Sector
	mov DH, byte [HEAD]		; Head
	mov DL, byte [DISK]		; Disk
	mov BX, word [SS:(BP + 24)]	; Offset
	push ES				; Save ES
	mov ES, word [SS:(BP + 22)]	; Use user-supplied segment
	int 13h
	pop ES				; Restore ES
	jc IOError

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

;; Static data follows.  We don't want to refer to it by meaningful symbolic
;; names here; after relocation these are not so meaningful

;; Messages
DATA1		db 'No bootable partition found', 0Dh, 0Ah, 0
DATA2		db 'I/O Error reading boot sector', 0Dh, 0Ah, 0

;; Move to the end of the sector
times (446-($-$$))	db 0

;; Here's where the partition table goes
DATA3	times 16	db 0
	times 16	db 0
	times 16	db 0
	times 16	db 0
		
;; This puts the value AA55h in the last two bytes of the boot
;; sector.  The BIOS uses this to determine whether this sector was
;; meant to be booted from (and also helps prevent us from making the
;; boot sector code larger than 512 bytes)
ENDSECTOR:		dw 0AA55h
