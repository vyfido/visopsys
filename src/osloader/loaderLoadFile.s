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
;;  loaderLoadFile.s
;;

	GLOBAL loaderFindFile
	GLOBAL loaderLoadFile
	GLOBAL PARTENTRY
 
        EXTERN loaderPrint
        EXTERN loaderPrintNewline
	EXTERN loaderPrintNumber
	EXTERN loaderDiskError
	EXTERN loaderGetCursorAddress
	EXTERN loaderSetCursorAddress

	EXTERN BYTESPERSECT
	EXTERN ROOTDIRENTS
	EXTERN RESSECS
	EXTERN FATSECS
	EXTERN SECPERTRACK
	EXTERN HEADS
	EXTERN SECPERCLUST
	EXTERN FATS
	EXTERN DRIVENUMBER

	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"
	%include "../kernel/kernelAssemblerHeader.h"

	%define BYTESPERENTRY 32
	%define NYBBLESPERENTRY 3


loaderLoadFile:
	
	;; This routine is responsible for loading the requested file into
	;; the requested memory location.  It is specific to the FAT-12 
	;; filesystem.

	;; Save a word for our return code
	sub SP, 2

	;; Save registers
	pusha
	
	;; Save the stack pointer
	mov BP, SP

	;; The first parameter is a pointer to an 11-character string
	;; (FAT 8.3 format) containing the name of the file to load.

	;; The second parameter is a DWORD value representing the absolute
	;; memory location at which we should load the file.

	;; First we need to calculate a couple of values that will help
	;; us deal with this filesystem volume correctly
	call calculateVolInfo

	;; We need to locate the file.  Read the root directory from 
	;; the disk
	call loadDirectory

	;; Was that successful?  Do a signed comparison.  Less than 0
	;; means error.
	cmp AX, 0
	jge .search
	
	;; Failed to load the directory.  Put a -1 as our return code
	mov word [SS:(BP + 16)], -1
	jmp .done
	
	.search:
	;; Now we need to search for the requested file in the root
	;; directory.
	push word [SS:(BP + 20)]
	call searchFile
	add SP, 2
	
	;; Was that successful?  Do a signed comparison.  Less than 0
	;; means error.
	cmp AX, 0
	jge .loadFAT

	;; Failed to find the file.  Put a -2 as our return code
	mov word [SS:(BP + 16)], -2
	jmp .done

	.loadFAT:
	;; push the starting cluster of the file (it's in AX) for
	;; safekeeping
	push AX
	
	;; Now we load the FAT table into memory
	call loadFAT
	
	;; Was that successful?  Do a signed comparison.  Less than 0
	;; means error.
	cmp AX, 0
	jge .loadFile

	;; Failed to load the FAT.  Put -3 as our return code
	mov word [SS:(BP + 16)], -3
	jmp .done

	.loadFile:
	;; Now we can actually load the file.  The starting cluster
	;; that we saved earlier on the stack will be the first parameter
	;; to the loadFile function.  The second parameter is the
	;; load location of the file (which was THIS function's second
	;; parameter, also)
	pop AX
	push dword [SS:(BP + 22)]
	push AX
	call loadFile
	add SP, 6

	;; Was that successful?  Do a signed comparison.  Less than 0
	;; means error.
	cmp AX, 0
	jge .success

	;; Failed to load the file.  Put -4 as our return code
	mov word [SS:(BP + 16)], -4
	jmp .done

	.success:
	;; Success.  Put the file size as our return code.
	mov AX, word [FILESIZE]
	mov word [SS:(BP + 16)], AX
	
	.done:
	popa
	;; Pop the return code
	xor EAX, EAX
	pop AX
	ret


loaderFindFile:
	
	;; This routine is will simply search for the requested file, and
	;; return the starting cluster number if it is present.  Returns
	;; negative otherwise.

	;; Save a word for our return code
	sub SP, 2

	;; Save registers
	pusha
	
	;; Save the stack pointer
	mov BP, SP

	;; The parameter is a pointer to an 11-character string
	;; (FAT 8.3 format) containing the name of the file to find.

	;; First we need to calculate a couple of values that will help
	;; us deal with this filesystem volume correctly
	call calculateVolInfo

	;; We need to locate the file.  Read the root directory from 
	;; the disk
	call loadDirectory

	;; Was that successful?  Do a signed comparison.  Less than 0
	;; means error.
	cmp AX, 0
	jge .search
	
	;; Failed to load the directory.  Put a 0 as our return code
	mov word [SS:(BP + 16)], 0
	jmp .done
	
	.search:
	;; Now we need to search for the requested file in the root
	;; directory.
	push word [SS:(BP + 20)]
	call searchFile
	add SP, 2
	
	;; If the file was found successfully, put a 1 as our return
	;; code.  Otherwise, put 0
	cmp AX, 0
	jge .success
	
	mov word [SS:(BP + 16)], 0
	jmp .done

	.success:
	mov word [SS:(BP + 16)], 1

	.done:	
	popa
	pop AX			; return code
	ret


calculateVolInfo:
	;; This little routine will calculate some constant things that
	;; are dependent upon the type of the current volume.  It stores
	;; the results in the static data area for the use of the other
	;; routines

	pusha

	;; How many root directory sectors are there?
	mov AX, BYTESPERENTRY
	mul word [ROOTDIRENTS]
	xor DX, DX
	div word [BYTESPERSECT]
	mov word [DIRSECTORS], AX

	;; Calculate the number of bytes per cluster in this volume
	mov AX, word [BYTESPERSECT]
	mul word [SECPERCLUST]
	mov word [BYTESPERCLUST], AX

	;; Calculate the segment where we will keep FAT (and directory)
	;; data after loading them.  It comes at the beginning of the
	;; LDRDATABUFFER
	mov AX, (LDRDATABUFFER / 16)
	mov word [FATSEGMENT], AX

	;; Calculate the segment where we will keep cluster data
	;; after each one is loaded.  It comes after the FAT data
	;; in the LDRDATABUFFER
	xor EAX, EAX
	mov AX, word [FATSECS]
	mul word [BYTESPERSECT]
	add EAX, LDRDATABUFFER
	shr EAX, 4
	mov word [CLUSTERSEGMENT], AX

	popa
	ret


loadDirectory:

	;; This subroutine finds the root directory of the boot volume
	;; and loads it into memory at LDRDATABUFFER

	;; Save 1 word for our return code
	sub SP, 2
	
	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; Get the logical sector number of the root directory
	mov AX, word [RESSECS]		; The reserved sectors
	add AX, word [FATSECS]		; Sectors for 1st FAT
	add AX, word [FATSECS]		; Sectors for 2nd FAT
	
	;; Calculate the head, track and sector
	call headTrackSector
	
	push word 0		; To keep track of read attempts

	.readAttempt:
	;; We need to make ES point to the data buffer.  This is normally
	;; where we will keep the FAT data, but we will put the directory
	;; here temporarily
	push ES
	mov ES, word [FATSEGMENT]
	xor BX, BX			; Load at offset 0 of the data buffer
	mov CH, byte [CURRENTTRACK]
	mov CL, byte [CURRENTSECTOR]
	mov DX, word [DRIVENUMBER]
	mov DH, byte [CURRENTHEAD]
	mov AX, word [DIRSECTORS]	; Number of sectors to read
	mov AH, 02			; Subfunction 2
	int 13h

	;; Check the drive status
	jnc near .gotDirectory

	;; Restore ES
	pop ES

	;; We'll reset the disk and retry up to 4 more times

	;; Reset the disk controller
	xor AH, AH
	mov DX, word [DRIVENUMBER]
	int 13h
			
	;; Increment the counter
	pop AX
	inc AX
	push AX
	cmp AX, 05h
	jnae .readAttempt

	.fail:
	;; Remove the counter left over from the last disk read
	add SP, 2
	;; Call the 'disk error' routine
	call loaderDiskError
	;; Put a -1 as our return code
	mov word [SS:(BP + 16)], -1
	jmp .done

	.gotDirectory:
	;; Restore ES
	pop ES
	;; Remove the counter left over from the last disk read
	add SP, 2
	;; Put a 0 as our return code
	mov word [SS:(BP + 16)], 0
	
	.done:
	popa
	;; Pop the return code
	xor EAX, EAX
	pop AX
	ret


searchFile:
	
	;; This routine will search the pre-loaded root directory of the 
	;; boot volume at LDRDATABUFFER and return the starting cluster of 
	;; the requested file.

	;; Save a word for our return code (the starting cluster of the
	;; file)
	sub SP, 2

	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; The root directory should be loaded into memory at the location
	;; LDRDATABUFFER.  We will walk through looking for the name
	;; we were passed as a parameter.
	mov word [ENTRYSTART], 0

	;; Get the pointer to the requested name from the stack
	mov SI, word [SS:(BP + 20)]

	;; We need to make ES point to the data buffer
	push ES
	mov ES, word [FATSEGMENT]
		
	.entryLoop:

	;; Determine whether this is a valid, undeleted file.
	;; E5 means this is a deleted entry
	mov DI, word [ENTRYSTART]
	mov AL, byte [ES:DI]
	cmp AL, 0E5h
	je .notThisEntry	; Deleted
	;; 00 means that there are no more entries
	cmp AL, 0
	je .noFile

	xor CX, CX

	.nextLetter:
	mov DI, word [ENTRYSTART]
	add DI, CX
	mov AL, byte [ES:DI]
	mov BX, SI
	add BX, CX
	mov DL, byte [BX]

	cmp AL, DL
	jne .notThisEntry

	inc CX
	cmp CX, 11
	jb .nextLetter
	jmp .foundFile

	.notThisEntry:
	;; Move to the next directory entry
	add word [ENTRYSTART], BYTESPERENTRY

	;; Make sure we're not at the end of the directory
	mov AX, word [BYTESPERSECT]
	mul word [DIRSECTORS]
	cmp word [ENTRYSTART], AX
	jae .noFile

	jmp .entryLoop
	
	.noFile:
	;; Restore ES
	pop ES
	;; The file is not there.  Return -1 as our error code
	mov word [SS:(BP + 16)], -1
	;; Jump to the end.  We're finished
	jmp .done
	
	.foundFile:	
	;; Return the starting cluster number of the file
	mov BX, word [ENTRYSTART]
	add BX, 001Ah		;; Offset in directory entry
	mov AX, word [ES:BX]
	mov word [SS:(BP + 16)], AX

        ;; Record the size of the file
        mov BX, word [ENTRYSTART]
        add BX, 001Ch           ;; Offset in directory entry
        mov EAX, dword [ES:BX]
        xor EDX, EDX
        mov ECX, 1024           ;; We want the size in kilobytes (div by 1024)
        div ECX
        cmp EDX, 0              ;; Round up?
        je .noRound
        add EAX, 1              ;; Round up to the next highest K
        .noRound:
        mov word [FILESIZE], AX

	;; Restore ES
	pop ES
	
	.done:
	popa
	;; Pop our return value
	xor EAX, EAX
	pop AX
	ret


loadFAT:
	;; This routine will load the entire FAT table into memory at 
	;; location LDRDATABUFFER

	;; Save 1 word for our return code
	sub SP, 2
	
	pusha

	;; Save the stack pointer
	mov BP, SP

	mov AX, word [RESSECS]		; FAT starts after reserved sectors
	call headTrackSector
	
	;; Now load the FAT table.  Push a counter to keep track
	;; of read attempts
	push word 0

	.readFATAttempt:	
	;; We need to make ES point to the data buffer
	push ES
	mov ES, word [FATSEGMENT]
	xor BX, BX			; Put at beginning of buffer
	mov CH, byte [CURRENTTRACK]
	mov CL, byte [CURRENTSECTOR]
	mov DX, word [DRIVENUMBER]
	mov DH, byte [CURRENTHEAD]
	mov AX, word [FATSECS]		; Read entire FAT
	mov AH, 02h			; Subfunction 2
	int 13h

	jnc near .success

	;; Restore ES
	pop ES

	;; We'll reset the disk and retry up to 4 more times

	;; Reset the disk controller
	xor AH, AH
	mov DX, word [DRIVENUMBER]
	int 13h
			
	;; Increment the counter
	pop AX
	inc AX
	push AX
	cmp AX, 05h
	jnae .readFATAttempt

	.fail:
	;; Remove the counter we used to keep track of read attempts
	add SP, 2
	call loaderDiskError
	;; Return -1 as our error code
	mov word [SS:(BP + 16)], -1
	jmp .done

	.success:
	;; Restore ES
	pop ES
	;; Remove the counter we used to keep track of read attempts
	add SP, 2
	;; Return 0 (success)
	mov word [SS:(BP + 16)], 0
	
	.done:
	popa
	;; Pop our return code
	xor EAX, EAX
	pop AX
	ret
	

loadFile:
	;; This routine is responsible for loading the requested file into
	;; the requested memory location.  The FAT table must have previously
	;; been loaded at memory location LDRDATABUFFER

	;; Save a word for our return code
	sub SP, 2

	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; The first parameter is the starting cluster of the file we're
	;; supposed to load.

	;; The second parameter is a DWORD pointer to the absolute memory
	;; location where we should load the file.

	mov word [BYTESREAD], 0
	mov word [OLDPROGRESS], 0
	
	;; Make a little progress indicator so the user gets transfixed,
	;; and suddenly doesn't mind the time it takes to load the file.
	call makeProgress

	;; Put the starting cluster number in NEXTCLUSTER
	mov AX, word [SS:(BP + 20)]
	mov word [NEXTCLUSTER], AX

	;; Put the starting memory offset in MEMORYMARKER
	mov EAX, dword [SS:(BP + 22)]
	mov dword [MEMORYMARKER], EAX
	
	;; Save EX, because we're going to dick with it throughout.
	push ES

	.FATLoop:
	
	;; Push a counter to keep track of read attempts
	push word 0

	.readAttempt:
	;; Get headTrackSector to calculate the physical head, track, and
	;; sector of the logical cluster number
	mov AX, word [NEXTCLUSTER]
	call clusterToLogical
	call headTrackSector

	;; We need to make ES point to the portion of loader's data buffer
	;; that comes AFTER the FAT data.  This is where we will initially
	;; load each cluster's contents.
	mov ES, word [CLUSTERSEGMENT]
	xor BX, BX			; ES:BX is real-mode buffer for data
	mov CH, byte [CURRENTTRACK]
	mov CL, byte [CURRENTSECTOR]
	mov DX, word [DRIVENUMBER]
	mov DH, byte [CURRENTHEAD]
	mov AX, word [SECPERCLUST] 	; Read 1 cluster's worth of sectors
	mov AH, 02h           		; Subfunction 2
	int 13h

	jnc .gotCluster

	;; We'll reset the disk and retry up to 4 more times

	;; Reset the disk controller
	mov AH, 00h
	mov DX, word [DRIVENUMBER]
	int 13h
			
	pop AX
	inc AX
	push AX
	cmp AL, 05h
	jnae near .readAttempt

	
	.fail:
	;; Get rid of the read-attempt counter
	add SP, 2
	;; Make an error message
	call killProgress
	call loaderDiskError
	;; Return -1 as our error code
	mov word [SS:(BP + 16)], -1
	;; Jump to the end.  We're done
	jmp .done
	
	.gotCluster:
	;; Get rid of the read-attempt counter
	add SP, 2

	;; Update the number of bytes read
	mov AX, word [BYTESREAD]
	add AX, word [BYTESPERCLUST]
	mov word [BYTESREAD], AX

	;; Determine whether we should update the progress indicator
	mov AX, word [BYTESREAD]
	shr AX, 10
	mov BX, 100
	mul BX
	xor DX, DX
	div word [FILESIZE]
	mov BX, AX
	sub BX, word [OLDPROGRESS]
	cmp BX, (100 / PROGRESSLENGTH)
	jb .noSpin
	mov word [OLDPROGRESS], AX
	call updateProgress
	.noSpin:
	
	;; This part of the function operates in "big real mode" so that it
	;; can load the kernel at an arbitrary address in memory (not just
	;; in the first megabyte).  To achieve big real mode, we need to
	;; have a valid protected mode GDT (Global Descriptor Table) and
	;; we need to switch to protected mode to load a data segment register
	;; with the "global" data segment selector.  The GDT should have
	;; already been set up, so now we do the other part.

	;; Disable interrupts
	cli

	;; Switch to protected mode temporarily
	mov EAX, CR0
	or AL, 01h
	mov CR0, EAX

	BITS 32
	
	;; ES currently holds the segment of the cluster data.  Now load 
	;; it with the global data segment selector
	mov EAX, PRIV_DATASELECTOR
	mov ES, AX

	;; Return to real mode
	mov EAX, CR0
	and AL, 0FEh
	mov CR0, EAX

	BITS 16
	
	;; We need to make DS point to the portion of loader's data 
	;; buffer that comes AFTER the FAT data.  This is where we will 
	;; store each cluster's contents.
	push DS
	mov DS, word [CLUSTERSEGMENT]

	;; Copy the data we just read to the appropriate target address.
	;; This movsd instruction (combined with the "a32" prefix will use 
	;; DS:ESI as the source address (the cluster buffer address) 
	;; and ES:EDI as the destination (remember we modified ES, above).
	xor ECX, ECX
	mov CX, word [CS:BYTESPERCLUST]		; Cluster size
	shr ECX, 2				; Divide by 4
	xor ESI, ESI				; Source data is at 0
	mov EDI, dword [CS:MEMORYMARKER]	; Destination address
	cld					; Clear direction flag
	a32 rep movsd				; Do the copy
	
	;; Restore DS
	pop DS

	;; Reenable interrupts
	sti

	;; Increment the buffer pointer
	xor EAX, EAX
	mov AX, word [BYTESPERCLUST]
	add dword [MEMORYMARKER], EAX

	;; Now make ES point to the beginning of loader's data buffer,
	;; which contains the FAT data
	mov ES, word [FATSEGMENT]

	;; Get the next cluster in the chain
	mov AX, word [NEXTCLUSTER]
	mov BX, NYBBLESPERENTRY ; For FAT12, 3 nybbles per entry
	mul BX   	; We can ignore DX because it shouldn't
			; be bigger than a word
	mov BX, AX

	;; There are 2 nybbles per byte.  We will shift the register
	;; right by 1, and the remainder (1 or 0) will be in the
	;; carry flag.
	shr BX, 1	; Divide by 2

	;; Now we have to shift or mask the value in AX depending
	;; on whether CF is 1 or 0
	jnc .mask

	;; Get the value at ES:BX
	mov AX, word [ES:BX]
	;; Shift the value we got
	shr AX, 4
	jmp .doneConvert

	.mask:
	;; Get the value at ES:BX
	mov AX, word [ES:BX]
	;; Mask the value we got
	and AX, 0FFFh

	.doneConvert:	
	cmp AX, 0FF8h
	jae .success

	mov word [NEXTCLUSTER], AX
	jmp .FATLoop

	.success:
	;; Return 0 for success
	mov word [SS:(BP + 16)], 0

	.done:
	;; Restore ES
	pop ES

	popa
	;; Pop our return code
	xor EAX, EAX
	pop AX
	ret


clusterToLogical:
	;; This takes the cluster number in AX and returns the logical
	;; sector number in AX

	;; Save a word for our return code
	sub SP, 2

	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP
	
	;; Subtract 2 from the logical cluster number (because they
	;; start at 2)
	sub AX, 2	

	;; Multiply by sectors per cluster
	mov BX, word [SECPERCLUST]
	mul BX

	;; This little sequence figures out where the data clusters
	;; start on this volume

	add AX, word [RESSECS]		; The reserved sectors
	add AX, word [FATSECS]		; Sectors for 1st FAT
	add AX, word [FATSECS]		; Sectors for 2nd FAT
	add AX, word [DIRSECTORS]	; Root dir sectors

	mov word [SS:(BP + 16)], AX
	
	popa
	pop AX
	ret
	
	
headTrackSector:
	;; this routine accepts the cluster number in AX.
	;; First it calculates the logical sector, then
	;; the head, track and sector number on disk.  It
	;; places them in the variables of the same name

	;; We destroy a bunch of registers, so save them
	pusha

	;; Add the partition starting LBA offset, if applicable
	cmp word [DRIVENUMBER], 80h
	jne .noOffset
	add AX, word [PARTENTRY + 8]

	.noOffset:
	;; Now we have the logical sector number in AX

	;; First the sector
	xor DX, DX
	div word [SECPERTRACK]
	mov byte [CURRENTSECTOR], DL		; The remainder
	add byte [CURRENTSECTOR], 1		; Sectors start at 1
	
	;; Now the head and track
	xor DX, DX			; Don't need the remainder anymore
	div word [HEADS]
	mov byte [CURRENTHEAD], DL	; The remainder
	mov byte [CURRENTTRACK], AL
	
	popa
	ret


makeProgress:
	;; This routine sets up a little progress indicator.

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
	mov word [PROGRESSCHARS], 0
	
	popa

	
updateProgress:
	pusha

	;; Make sure we're not already at the end
	mov AX, word [PROGRESSCHARS]
	cmp AX, PROGRESSLENGTH
	jae .done
	inc AX
	mov word [PROGRESSCHARS], AX
	
	;; Print the character on the screen
	mov DL, 2
	mov CX, 1
	mov SI, PROGRESSCHAR
	call loaderPrint

	.done:	
	popa
	ret

	
killProgress:
	;; Get rid of the progress indicator

	pusha

	;; Re-enable the cursor
	;; xor CX, CX
	;; mov CL, 07h
	;; mov AH, 01h
	;; int 10h

	call loaderPrintNewline
	call loaderPrintNewline
	
	popa
	ret


	SEGMENT .data
	ALIGN 4

MEMORYMARKER	dd 0	;; Offset to load next data cluster
FATSEGMENT	dw 0	;; The segment for FAT and directory data
CLUSTERSEGMENT	dw 0	;; The segment for cluster data
DIRSECTORS	dw 0	;; The size of the root directory, in sectors
BYTESPERCLUST   dw 0	;; Bytes per cluster
ENTRYSTART	dw 0 	;; Directory entry start
FILESIZE	dw 0	;; Size of the file we're loading
BYTESREAD	dw 0    ;; Number of bytes read so far
OLDPROGRESS	dw 0	;; Percentage of file load completed
NEXTCLUSTER	dw 0	;; Next cluster to load
PROGRESSCHARS	dw 0	;; Number of progress indicator chars showing
PARTENTRY	times 16 db 0	;; Partition table entry of bootable partition
CURRENTHEAD	db 0	;; Of disk seek
CURRENTTRACK	db 0	;; Of disk seek
CURRENTSECTOR	db 0	;; Of disk seek
PROGRESSTOP	db 218
		times PROGRESSLENGTH db 196
		db 191, 0
PROGRESSMIDDLE	db 179
		times PROGRESSLENGTH db ' '
		db 179, 0
PROGRESSBOTTOM	db 192
		times PROGRESSLENGTH db 196
		db 217, 0
PROGRESSCHAR	db 177, 0
SPACE		db ' ', 0
