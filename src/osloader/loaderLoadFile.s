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
;;  loaderLoadFile.s
;;

	GLOBAL loaderFindFile
	GLOBAL loaderLoadFile
	GLOBAL PARTENTRY
 
	EXTERN loaderBigRealMode
        EXTERN loaderPrint
        EXTERN loaderPrintNewline
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

	%define BYTESPERENTRY 32
	%define NYBBLESPERENTRY 3


headTrackSector:
	;; This routine takes the logical sector number in EAX.  From this it
	;; calculates the head, track and sector number on disk.

	;; We destroy a bunch of registers, so save them
	pusha

	;; First the sector
	xor EDX, EDX
	xor EBX, EBX
	mov BX, word [SECPERTRACK]
	div EBX
	mov byte [SECTOR], DL		; The remainder
	add byte [SECTOR], 1		; Sectors start at 1
	
	;; Now the head and track
	xor EDX, EDX			; Don't need the remainder anymore
	xor EBX, EBX
	mov BX, word [HEADS]
	div EBX
	mov byte [HEAD], DL		; The remainder
	mov word [CYLINDER], AX
	
	popa
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


readSector:
	;; Proto: int readSector(dword logical, word seg, word offset,
	;;                       word count);

	;; Save a word on the stack for our return value
	push word 0

	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP

	push word 0		; To keep track of read attempts

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

	mov word [DISKPACKET], 0010h		; Packet size
	mov AX, word [SS:(BP + 28)]		; >
	mov word [DISKPACKET + 2], AX		; > Sector count
	mov AX, word [SS:(BP + 26)]		; >
	mov word [DISKPACKET + 4], AX		; > Offset
	mov AX, word [SS:(BP + 24)]		; > 
	mov word [DISKPACKET + 6], AX		; > Segment
	mov EAX, dword [SS:(BP + 20)]		; > 
	mov dword [DISKPACKET + 8], EAX		; > Logical sector 
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

	mov AX, word [SS:(BP + 28)]	; Number to read
	mov AH, 02h			; Subfunction 2
	mov CX, word [CYLINDER]		; >
	rol CX, 8			; > Cylinder
	shl CL, 6			; >
	or CL, byte [SECTOR]		; Sector
	mov DX, word [DRIVENUMBER]	; Drive
	mov DH, byte [HEAD]		; Head
	mov BX, word [SS:(BP + 26)]	; Offset
	push ES				; Save ES
	mov ES, word [SS:(BP + 24)]	; Use user-supplied segment
	int 13h
	pop ES				; Restore ES
	jc .IOError
	jmp .done
	
	.IOError:
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

	mov word [SS:(BP + 16)], -1

	.done:
	pop AX			; Counter
	popa
	xor EAX, EAX
	pop AX			; Status
	ret

		
loadFAT:
	;; This routine will load the entire FAT table into memory at 
	;; location LDRDATABUFFER

	;; Save 1 word for our return code
	push word 0
	
	pusha

	;; Save the stack pointer
	mov BP, SP

	xor EAX, EAX
	mov AX, word [RESSECS]		; FAT starts after reserved sectors
	
	cmp word [DRIVENUMBER], 80h
	jb .noOffset
	add EAX, [PARTENTRY + 8]
	.noOffset:
	
	;; Now load the FAT table.
	push word [FATSECS]		; Read entire FAT
	push word 0			; Offset (beginning of buffer)
	push word [FATSEGMENT]		; Segment of data buffer
	push dword EAX
	call readSector
	add SP, 10

	;; Check status
	cmp AX, 0
	je .done

	;; Call the 'disk error' routine
	call loaderDiskError
	;; Put a -1 as our return code
	mov word [SS:(BP + 16)], -1

	.done:
	popa
	;; Pop the return code
	xor EAX, EAX
	pop AX
	ret
	

loadDirectory:
	;; This subroutine finds the root directory of the boot volume
	;; and loads it into memory at LDRDATABUFFER

	;; Save 1 word for our return code
	push word 0
	
	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; Get the logical sector number of the root directory
	xor EAX, EAX
	mov AX, word [RESSECS]		; The reserved sectors
	xor EBX, EBX
	mov BX, word [FATSECS]		; Sectors per FAT
	add EAX, EBX			; Sectors for 1st FAT
	add EAX, EBX			; Sectors for 2nd FAT
	
	cmp word [DRIVENUMBER], 80h
	jb .noOffset
	add EAX, [PARTENTRY + 8]
	.noOffset:
	
	;; This is normally where we will keep the FAT data, but we will
	;; put the directory here temporarily
	push word [DIRSECTORS]		; Number of sectors to read
	push word 0			; Load at offset 0 of the data buffer
	push word [FATSEGMENT]		; Segment of the data buffer
	push dword EAX
	call readSector
	add SP, 10
		
	;; Check status
	cmp AX, 0
	je .done

	;; Call the 'disk error' routine
	call loaderDiskError
	
	;; Put a -1 as our return code
	mov word [SS:(BP + 16)], -1
	
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
	;; Proto:
	;;   int searchFile(char *filename);

	;; Save a word for our return code (the starting cluster of the
	;; file)
	push word 0

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


makeProgress:
	;; This routine sets up a little progress indicator.

	pusha

	cmp word [SHOWPROGRESS], 0
	je .done
	
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

	.done:
	popa

	
updateProgress:
	pusha

	cmp word [SHOWPROGRESS], 0
	je .done
	
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

	cmp word [SHOWPROGRESS], 0
	je .done
	
	;; Re-enable the cursor
	;; xor CX, CX
	;; mov CL, 07h
	;; mov AH, 01h
	;; int 10h

	call loaderPrintNewline
	call loaderPrintNewline
	
	.done:	
	popa
	ret


clusterToLogical:
	;; This takes the cluster number in EAX and returns the logical
	;; sector number in EAX

	;; Save a word for our return code
	sub SP, 4

	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP
	
	sub EAX, 2			;  Subtract 2 (because they start at 2)
	xor EBX, EBX
	mov BX, word [SECPERCLUST]	; How many sectors per cluster?
	mul EBX

	;; This little sequence figures out where the data clusters
	;; start on this volume

	xor EBX, EBX
	mov BX, word [RESSECS]		; The reserved sectors
	add EAX, EBX
	mov BX, word [FATSECS]		; Sectors for 1st FAT
	add EAX, EBX
	mov BX, word [FATSECS]		; Sectors for 2nd FAT
	add EAX, EBX
	mov BX, word [DIRSECTORS]	; Root dir sectors
	add EAX, EBX

	cmp word [DRIVENUMBER], 80h
	jb .noOffset
	add EAX, [PARTENTRY + 8]
	.noOffset:
	
	mov dword [SS:(BP + 16)], EAX

	popa
	pop EAX
	ret
	
	
loadFile:
	;; This routine is responsible for loading the requested file into
	;; the requested memory location.  The FAT table must have previously
	;; been loaded at memory location LDRDATABUFFER
	;; Proto:
	;;   int loadFile((short) cluster, (dword) memory-address); 

	;; Save a word for our return code
	push word 0

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
	xor EAX, EAX
	mov AX, word [SS:(BP + 20)]
	mov dword [NEXTCLUSTER], EAX

	;; Put the starting memory offset in MEMORYMARKER
	mov EAX, dword [SS:(BP + 22)]
	mov dword [MEMORYMARKER], EAX
	
	;; Save ES, because we're going to dick with it throughout.
	push ES

	.FATLoop:
	;; Get the logical sector for this cluster number
	mov EAX, dword [NEXTCLUSTER]
	call clusterToLogical

	;; Use the portion of loader's data buffer that comes AFTER the
	;; FAT data.  This is where we will initially load each cluster's
	;; contents.
	push word [SECPERCLUST]		; Read 1 cluster's worth of sectors
	push word 0			; >
	push word [CLUSTERSEGMENT]	; > Real-mode buffer for data
	push dword EAX
	call readSector
	add SP, 10
	
	cmp AX, 0
	je .gotCluster

	;; Make an error message
	call killProgress
	call loaderDiskError
	;; Return -1 as our error code
	mov word [SS:(BP + 16)], -1
	jmp .done
	
	.gotCluster:
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
	jb .noProgress
	mov word [OLDPROGRESS], AX
	call updateProgress
	.noProgress:
	
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
	mov EAX, dword [NEXTCLUSTER]
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

	mov dword [NEXTCLUSTER], EAX
	jmp .FATLoop

	.success:
	;; Return 0 for success
	mov word [SS:(BP + 16)], 0
	call killProgress

	.done:
	;; Restore ES
	pop ES
	popa
	;; Pop our return code
	xor EAX, EAX
	pop AX
	ret


loaderFindFile:
	;; This routine is will simply search for the requested file, and
	;; return the starting cluster number if it is present.  Returns
	;; negative otherwise.
	;; Proto:
	;;   int loaderFindFile(char *filename);

	;; Save a word for our return code
	push word 0

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


loaderLoadFile:
	;; This routine is responsible for loading the requested file into
	;; the requested memory location.  It is specific to the FAT-12 
	;; filesystem.
	;; Proto:
	;;   int loaderLoadFile(char *filename, (dword) loadOffset, int
	;;			showProgress)

	;; Save a word for our return code
	push word 0

	;; Save registers
	pusha
	
	;; Save the stack pointer
	mov BP, SP

	;; The parameter is a pointer to an 11-character string
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

	mov AX, word [SS:(BP + 26)]
	mov word [SHOWPROGRESS], AX

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
NEXTCLUSTER	dd 0	;; Next cluster to load

;; For int13 disk ops
CYLINDER	dw 0
HEAD		db 0
SECTOR		db 0

;; Disk cmd packet for ext. int13 
DISKPACKET:	dd 0, 0, 0, 0
	
PARTENTRY	times 16 db 0	;; Partition table entry of bootable partition

;; Stuff for the progress indicator
PROGRESSCHARS	dw 0	;; Number of progress indicator chars showing
OLDPROGRESS	dw 0	;; Percentage of file load completed
SHOWPROGRESS	dw 0	;; Whether or not to show a progress bar
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
