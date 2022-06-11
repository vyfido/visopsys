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
;;  loaderLoad.s
;;

	GLOBAL loaderLoadSectors
	GLOBAL loaderLoadSectorsHi

	EXTERN loaderMemCopy
	EXTERN loaderPrint
	EXTERN loaderPrintNewline
	EXTERN loaderDiskError
	EXTERN loaderMakeProgress
	EXTERN loaderUpdateProgress
	EXTERN loaderKillProgress

	EXTERN BYTESPERSECT
	EXTERN HEADS
	EXTERN SECPERTRACK
	EXTERN DRIVENUMBER
	EXTERN FILEDATABUFFER

	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"


headTrackSector:
	;; Takes the logical sector number in EAX.  From this it calculates the
	;; head, track and sector number on disk.

	;; We destroy a bunch of registers, so save them
	pushad

	;; First the sector
	xor EDX, EDX
	xor EBX, EBX
	mov BX, word [SECPERTRACK]
	div EBX
	mov byte [SECTOR], DL			; The remainder
	add byte [SECTOR], 1			; Sectors start at 1

	;; Now the head and track
	xor EDX, EDX					; Don't need the remainder anymore
	xor EBX, EBX
	mov BX, word [HEADS]
	div EBX
	mov byte [HEAD], DL				; The remainder
	mov word [CYLINDER], AX

	popad
	ret


loaderLoadSectors:
	;; Loads the requested sectors into the requested memory location.
	;;
	;; Proto:
	;;   word loaderLoadSectors(dword logical, word seg, word offset,
	;;                          word count);

	;; Save a word on the stack for our return value
	push word 0

	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP

	push word 0						; To keep track of read attempts

	.readAttempt:
	;; Determine whether int13 extensions are available
	cmp word [DRIVENUMBER], 80h
	jb .noExtended

	mov AX, 4100h
	mov BX, 55AAh
	mov DX, word [DRIVENUMBER]
	int 13h
	jc .noExtended

	;; We have a nice extended read function which will allow us to
	;; just use the logical sector number for the read

	mov word [DISKPACKET], 0010h	; Packet size
 	mov AX, word [SS:(BP + 28)]
	mov word [DISKPACKET + 2], AX	; Sector count
	mov AX, word [SS:(BP + 26)]
	mov word [DISKPACKET + 4], AX	; Offset
	mov AX, word [SS:(BP + 24)]
	mov word [DISKPACKET + 6], AX	; Segment
	mov EAX, dword [SS:(BP + 20)]
	mov dword [DISKPACKET + 8], EAX	; Logical sector
	mov dword [DISKPACKET + 12], 0	;
	mov AX, 4200h
	mov DX, word [DRIVENUMBER]
	mov SI, DISKPACKET
	int 13h
	jc .IOError
	jmp .done

	.noExtended:
	;; Calculate the CHS
	mov EAX, dword [SS:(BP + 20)]
	call headTrackSector

	mov AX, word [SS:(BP + 28)]		; Number to read
	mov AH, 02h						; Subfunction 2
	mov CX, word [CYLINDER]			; >
	rol CX, 8						; > Cylinder
	shl CL, 6						; >
	or CL, byte [SECTOR]			; Sector
	mov DX, word [DRIVENUMBER]		; Drive
	mov DH, byte [HEAD]				; Head
	mov BX, word [SS:(BP + 26)]		; Offset
	push ES							; Save ES
	push word [SS:(BP + 24)]		; Use user-supplied segment
	pop ES
	int 13h
	pop ES							; Restore ES
	jc .IOError
	jmp .done

	.IOError:
	;; We'll reset the disk and retry up to 4 more times

	;; Reset the disk controller
	xor AX, AX
	mov DX, word [DRIVENUMBER]
	int 13h

	;; Increment the counter
	pop AX
	inc AX
	push AX
	cmp AX, 05h
	jnae .readAttempt

	mov word [SS:(BP + 16)], -1

	.done:
	pop AX							; Counter
	popa
	xor EAX, EAX
	pop AX							; Status
	ret


loaderLoadSectorsHi:
	;; Loads the requested sectors into the requested high memory location.
	;;
	;; Proto:
	;;   word loadFile(dword logical, dword memory_address, dword count,
	;;		word showProgress);

	;; Save a word for our return code
	push word 0

	;; Save regs
	pushad

	;; Save the stack pointer
	mov BP, SP

	;; The first parameter is the starting sector we're supposed to load.

	;; The second parameter is a DWORD pointer to the absolute memory
	;; location where we should load the file

	;; Put the starting sector number in NEXTSECTOR
	mov EAX, dword [SS:(BP + 36)]
	mov dword [NEXTSECTOR], EAX

	;; Put the starting memory offset in MEMORYMARKER
	mov EAX, dword [SS:(BP + 40)]
	mov dword [MEMORYMARKER], EAX

	;; Put the number of sectors in NUMSECTORS
	mov EAX, dword [SS:(BP + 44)]
	mov dword [NUMSECTORS], EAX

	;; Should we show a progress indicator?
	mov AX, word [SS:(BP + 48)]
	mov byte [SHOWPROGRESS], AL

	mov dword [SECTORSREAD], 0

	cmp AL, 0
	je .noProgress1
	;; Make a progress indicator
	call loaderMakeProgress
	.noProgress1:

	.sectorLoop:
	mov ECX, dword [NUMSECTORS]
	cmp ECX, 127		; Max sectors for one operation
	jbe .sectorsOk
	mov ECX, 127
	.sectorsOk:
	mov word [DOSECTORS], CX
	push word CX
	;; Use the portion of loader's data buffer that comes AFTER the
	;; FAT data.  This is where we will initially load each cluster's
	;; contents.
	mov EAX, dword [FILEDATABUFFER]
	shr EAX, 4
	push word 0			; >
	push word AX		; > Real-mode buffer for data
	mov EAX, dword [NEXTSECTOR]
	push dword EAX
	call loaderLoadSectors
	add SP, 10

	cmp AX, 0
	je .gotSectors

	;; Make an error message
	cmp byte [SHOWPROGRESS], 0
	je .noProgress2
	call loaderKillProgress
	.noProgress2:
	call loaderDiskError

	;; Return -1 as our error code
	mov word [SS:(BP + 32)], -1
	jmp .done

	.gotSectors:
	mov EAX, dword [DOSECTORS]
	add dword [SECTORSREAD], EAX
	cmp byte [SHOWPROGRESS], 0
	je .noProgress3
	;; Update the progress indicator
	mov EAX, dword [SECTORSREAD]
	mov EBX, 100
	mul EBX
	xor EDX, EDX
	div dword [NUMSECTORS]
	push word AX
	call loaderUpdateProgress
	add SP, 2

	.noProgress3:
	xor EAX, EAX
	mov AX, word [DOSECTORS]
	mov BX, word [BYTESPERSECT]
	mul BX
	push dword EAX					; Bytes to copy
	push dword [MEMORYMARKER]		; 32-bit destination address
	push dword [FILEDATABUFFER]		; 32-bit source address
	call loaderMemCopy
	add SP, 12

	cmp AX, 0
	je .copiedData

	;; Couldn't copy the data into high memory
	cmp byte [SHOWPROGRESS], 0
	je .noProgress4
	call loaderKillProgress
	.noProgress4:
	call loaderPrintNewline
	mov DL, BADCOLOR
	mov SI, INT15ERR
	call loaderPrint
	call loaderPrintNewline

	;; Return -2 as our error code
	mov word [SS:(BP + 32)], -2
	jmp .done

	.copiedData:
	;; Increment the sector number
	mov EAX, dword [DOSECTORS]
	add dword [NEXTSECTOR], EAX
	;; Increment the buffer pointer
	xor EBX, EBX
	mov BX, word [BYTESPERSECT]
	mul EBX
	add dword [MEMORYMARKER], EAX
	;; Decrement the sector count
	mov EAX, dword [DOSECTORS]
	sub dword [NUMSECTORS], EAX
	jnz .sectorLoop

	;; Return 0 for success
	mov word [SS:(BP + 32)], 0

	.done:
	popad
	;; Pop our return code
	xor EAX, EAX
	pop AX
	ret


	SEGMENT .data
	ALIGN 4

NEXTSECTOR		dd 0		;; Next sector to load
MEMORYMARKER	dd 0		;; Offset to load next data cluster
NUMSECTORS		dd 0		;; Number of sectors to load
SECTORSREAD		dd 0		;; Number of sectors to loaded so far
DOSECTORS		dd 0		;; Number of sectors to load in one operation

;; For int13 disk ops
CYLINDER		dw 0
HEAD			db 0
SECTOR			db 0

;; Disk cmd packet for ext. int13
DISKPACKET		dd 0, 0, 0, 0

SHOWPROGRESS	db 0            ;; Whether or not to show a progress bar

INT15ERR		db 'The computer', 27h, 's BIOS was unable to move data into '
				db 'high memory.', 0

