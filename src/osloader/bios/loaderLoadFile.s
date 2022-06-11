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
;;  loaderLoadFile.s
;;

	GLOBAL loaderCalcVolInfo
	GLOBAL loaderFindFile
	GLOBAL loaderLoadFile
	GLOBAL PARTENTRY
	GLOBAL FILEDATABUFFER

	EXTERN loaderPrint
	EXTERN loaderDiskError
	EXTERN loaderLoadSectors
	EXTERN loaderLoadSectorsHi
	EXTERN loaderMakeProgress
	EXTERN loaderUpdateProgress
	EXTERN loaderKillProgress

	EXTERN BYTESPERSECT
	EXTERN ROOTDIRCLUST
	EXTERN ROOTDIRENTS
	EXTERN RESSECS
	EXTERN FATSECS
	EXTERN SECPERCLUST
	EXTERN FATS
	EXTERN DRIVENUMBER
	EXTERN FSTYPE

	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"


loadFAT:
	;; This routine will load up to a segment of the FAT table into memory at
	;; location [FATSEGMENT:0000]

	;; Save 1 word for our return code
	push word 0

	pusha

	;; Save the stack pointer
	mov BP, SP

	xor EAX, EAX
	mov AX, word [RESSECS]			; FAT starts after reserved sectors

	cmp word [DRIVENUMBER], 80h
	jb .noOffset
	add EAX, dword [PARTENTRY + 8]
	.noOffset:

	push dword [FATSECSLOADED]		; Number of FAT sectors
	push word 0						; Offset (beginning of buffer)
	push word [FATSEGMENT]			; Segment of data buffer
	push dword EAX
	call loaderLoadSectors
	add SP, 12

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
	;; Finds the root directory of the boot volume and loads it into memory
	;; at location [FATSEGMENT:0000]

	;; Save 1 word for our return code
	push word 0

	;; Save regs
	pushad

	;; Save the stack pointer
	mov BP, SP

	;; Get the logical sector number of the root directory
	xor EAX, EAX
	mov AX, word [RESSECS]			; The reserved sectors
	mov EBX, dword [FATSECS]		; Sectors per FAT
	add EAX, EBX					; Sectors for 1st FAT
	add EAX, EBX					; Sectors for 2nd FAT

	cmp word [FSTYPE], FS_FAT32
	jne .notFat32

	;; Obviously I was intending to put something here for finding the
	;; root directory sector here in FAT32?

	.notFat32:
	;; Add the partition starting sector if applicable
	cmp word [DRIVENUMBER], 80h
	jb .noOffset
	add EAX, dword [PARTENTRY + 8]
	.noOffset:

	;; This is normally where we will keep the FAT data, but we will
	;; put the directory here temporarily
	xor EBX, EBX
	mov BX, word [DIRSECTORS]		; Number of sectors to read
	push dword EBX
	push word 0						; Load at offset 0 of the data buffer
	push word [FATSEGMENT]			; Segment of the data buffer
	push dword EAX
	call loaderLoadSectors
	add SP, 12

	;; Check status
	cmp AX, 0
	je .done

	;; Call the 'disk error' routine
	call loaderDiskError

	;; Put a -1 as our return code
	mov word [SS:(BP + 32)], -1

	.done:
	popad
	;; Pop the return code
	xor EAX, EAX
	pop AX
	ret


searchFile:
	;; Search the pre-loaded root directory of the boot volume at
	;; LDRDATABUFFER and return the starting cluster of the requested file.
	;;
	;; Proto:
	;;   int searchFile(char *filename);

	;; Save a dword for our return code (the starting cluster of the file)
	push dword 0

	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; The root directory should be loaded into memory at the location
	;; LDRDATABUFFER.  We will walk through looking for the name
	;; we were passed as a parameter.
	mov word [ENTRYSTART], 0

	;; Get the pointer to the requested name from the stack
	mov SI, word [SS:(BP + 22)]

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
	add word [ENTRYSTART], FAT_BYTESPERDIRENTRY

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
	mov dword [SS:(BP + 16)], -1
	;; Jump to the end.  We're finished
	jmp .done

	.foundFile:
	;; Return the starting cluster number of the file
	xor EAX, EAX
	mov BX, word [ENTRYSTART]
	add BX, 0014h					;; Offset in directory entry of high word
	mov AX, word [ES:BX]
	shl EAX, 16
	add BX, 6						;; Offset (1Ah) in dir entry of low word
	mov AX, word [ES:BX]
	mov dword [SS:(BP + 16)], EAX

	;; Record the size of the file
	mov BX, word [ENTRYSTART]
	add BX, 001Ch					;; Offset in directory entry
	mov EAX, dword [ES:BX]
	mov dword [FILESIZE], EAX

	;; Restore ES
	pop ES

	.done:
	popa
	;; Pop our return value
	xor EAX, EAX
	pop EAX
	ret


clusterToLogical:
	;; This takes the cluster number in EAX and returns the logical sector
	;; number in EAX

	;; Save a dword for our return code
	push dword 0

	;; Save regs
	pushad

	;; Save the stack pointer
	mov BP, SP

	sub EAX, 2						; Subtract 2 (because they start at 2)
	xor EBX, EBX
	mov BX, word [SECPERCLUST]		; How many sectors per cluster?
	mul EBX

	;; This little sequence figures out where the data clusters
	;; start on this volume

	xor EBX, EBX
	mov BX, word [RESSECS]			; The reserved sectors
	add EAX, EBX
	mov EBX, dword [FATSECS]
	shl EBX, 1						; Add sectors for both FATs
	add EAX, EBX

	cmp word [FSTYPE], FS_FAT32
	je .noAddDir
	xor EBX, EBX
	mov BX, word [DIRSECTORS]		; Root dir sectors
	add EAX, EBX
	.noAddDir:

	cmp word [DRIVENUMBER], 80h
	jb .noOffset
	add EAX, dword [PARTENTRY + 8]
	.noOffset:

	mov dword [SS:(BP + 32)], EAX

	popad
	pop EAX
	ret


loadFile:
	;; Loads the requested file into the requested memory location.  The FAT
	;; table must have previously been loaded at memory location LDRDATABUFFER
	;;
	;; Proto:
	;;   word loadFile(dword cluster, dword memory_address);

	;; Save a word for our return code
	push word 0

	;; Save regs
	pushad

	;; Save the stack pointer
	mov BP, SP

	;; The first parameter is the starting cluster of the file we're
	;; supposed to load.

	;; The second parameter is a DWORD pointer to the absolute memory
	;; location where we should load the file.

	mov dword [BYTESREAD], 0

	cmp byte [SHOWPROGRESS], 0
	je .noProgress1
	;; Make a progress indicator
	call loaderMakeProgress

	.noProgress1:
	;; Put the starting cluster number in NEXTCLUSTER
	mov EAX, dword [SS:(BP + 36)]
	mov dword [NEXTCLUSTER], EAX

	;; Put the starting memory offset in MEMORYMARKER
	mov EAX, dword [SS:(BP + 40)]
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
	push word 0					; We have our own progress indicator
	xor EBX, EBX
	mov BX, word [SECPERCLUST]	; Read 1 cluster's worth of sectors
	push dword EBX
	push dword [MEMORYMARKER]
	push dword EAX
	call loaderLoadSectorsHi
	add SP, 14

	cmp AX, 0
	je .gotCluster

	;; Make an error message
	cmp byte [SHOWPROGRESS], 0
	je .noProgress2
	call loaderKillProgress
	.noProgress2:
	call loaderDiskError

	;; Return -1 as our error code
	mov word [SS:(BP + 32)], -1
	jmp .done

	.gotCluster:
	;; Update the number of bytes read
	mov EAX, dword [BYTESREAD]
	add EAX, dword [BYTESPERCLUST]
	mov dword [BYTESREAD], EAX

	cmp byte [SHOWPROGRESS], 0
	je .noProgress3
	;; Update the progress indicator
	mov EAX, dword [BYTESREAD]
	mov EBX, 100
	mul EBX
	xor EDX, EDX
	div dword [FILESIZE]
	push word AX
	call loaderUpdateProgress
	add SP, 2

	.noProgress3:
	;; Increment the buffer pointer
	mov EAX, dword [BYTESPERCLUST]
	add dword [MEMORYMARKER], EAX

	;; Now make ES point to the beginning of loader's data buffer,
	;; which contains the FAT data
	mov ES, word [FATSEGMENT]

	;; Get the next cluster in the chain

	cmp word [FSTYPE], FS_FAT32
	je .fat32

	cmp word [FSTYPE], FS_FAT16
	je .fat16

	.fat12:
	mov EAX, dword [NEXTCLUSTER]
	mov BX, FAT12_NYBBLESPERCLUST
	mul BX   						; We can ignore DX because it shouldn't
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
	jmp .next

	.fat16:
	mov EBX, dword [NEXTCLUSTER]
	;; FAT16_NYBBLESPERCLUST is 4, so we can just shift
	shl EBX, 1
	mov AX, word [ES:BX]
	cmp AX, 0FFF8h
	jae .success
	jmp .next

	.fat32:
	mov EBX, dword [NEXTCLUSTER]
	;; FAT32_NYBBLESPERCLUST is 8, so we can just shift
	shl EBX, 2
	mov EAX, dword [ES:EBX]
	cmp EAX, 0FFFFFF8h
	jae .success

	.next:
	mov dword [NEXTCLUSTER], EAX
	jmp .FATLoop

	.success:
	;; Return 0 for success
	mov word [SS:(BP + 32)], 0

	cmp byte [SHOWPROGRESS], 0
	je .noProgress5
	call loaderKillProgress
	.noProgress5:

	.done:
	;; Restore ES
	pop ES
	popad
	;; Pop our return code
	xor EAX, EAX
	pop AX
	ret


loaderCalcVolInfo:
	;; Calculate some constant things that are dependent upon the type of the
	;; current volume.  It stores the results in the static data area for the
	;; use of the other functions

	pusha

	;; Calculate the number of bytes per cluster in this volume
	xor EAX, EAX
	mov AX, word [BYTESPERSECT]
	mul word [SECPERCLUST]
	mov dword [BYTESPERCLUST], EAX

	mov AX, word [FSTYPE]
	cmp AX, FS_FAT32
	je .fat32

	;; How many root directory sectors are there?
	mov AX, FAT_BYTESPERDIRENTRY
	mul word [ROOTDIRENTS]
	xor DX, DX
	div word [BYTESPERSECT]
	mov word [DIRSECTORS], AX
	jmp .doneRoot

	.fat32:
	;; Just do one cluster of the root dir
	mov AX, word [SECPERCLUST]
	mov word [DIRSECTORS], AX

	.doneRoot:
	;; Calculate the segment where we will keep FAT (and directory)
	;; data after loading them.  It comes at the beginning of the
	;; LDRDATABUFFER
	mov AX, (LDRDATABUFFER / 16)
	mov word [FATSEGMENT], AX

	;; Calculate the number of FAT sectors we will load.  Don't want to read
	;; more than 128 FAT sectors (one segment's worth) but these disk
	;; operations are usually limited to 127 in any case.
	mov EAX, dword [FATSECS]
	cmp EAX, 127
	jbe .noShrink
	mov EAX, 127
	.noShrink:
	mov dword [FATSECSLOADED], EAX

	;; Calculate a buffer for general file data.  It comes after the FAT data
	;; in the LDRDATABUFFER.
	mov EAX, dword [FATSECSLOADED]
	mul word [BYTESPERSECT]
	add EAX, LDRDATABUFFER
	mov dword [FILEDATABUFFER], EAX

	popa
	ret


loaderFindFile:
	;; This function will simply search for the requested file, and return the
	;; starting cluster number if it is present.  Returns negative otherwise.
	;;
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
	cmp EAX, 0
	jge .success

	mov word [SS:(BP + 16)], 0
	jmp .done

	.success:
	mov word [SS:(BP + 16)], 1

	.done:
	popa
	pop AX							; return code
	ret


loaderLoadFile:
	;; This function is responsible for loading the requested file into the
	;; requested memory location.
	;;
	;; Proto:
	;;   dword loaderLoadFile(char *filename, dword loadOffset,
	;;		word showProgress)

	;; Save a dword for our return code
	push dword 0

	;; Save registers
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; The parameter is a pointer to an 11-character string
	;; (FAT 8.3 format) containing the name of the file to load.

	;; The second parameter is a DWORD value representing the absolute
	;; memory location at which we should load the file.

	;; We need to locate the file.  Read the root directory from
	;; the disk
	call loadDirectory

	;; Was that successful?  Do a signed comparison.  Less than 0
	;; means error.
	cmp AX, 0
	jge .search

	;; Failed to load the directory.  Put a -1 as our return code
	mov dword [SS:(BP + 16)], -1
	jmp .done

	.search:
	;; Now we need to search for the requested file in the root
	;; directory.
	push word [SS:(BP + 22)]
	call searchFile
	add SP, 2

	;; Was that successful?  Do a signed comparison.  Less than 0
	;; means error.
	cmp EAX, 0
	jge .loadFAT

	;; Failed to find the file.  Put a -2 as our return code
	mov dword [SS:(BP + 16)], -2
	jmp .done

	.loadFAT:
	;; Save the starting cluster of the file (it's in EAX)
	mov dword [NEXTCLUSTER], EAX

	;; Now we load the FAT table into memory
	call loadFAT

	;; Was that successful?  Do a signed comparison.  Less than 0
	;; means error.
	cmp AX, 0
	jge .loadFile

	;; Failed to load the FAT.  Put -3 as our return code
	mov dword [SS:(BP + 16)], -3
	jmp .done

	.loadFile:
	;; Now we can actually load the file.  The starting cluster
	;; that we saved earlier on the stack will be the first parameter
	;; to the loadFile function.  The second parameter is the
	;; load location of the file (which was THIS function's second
	;; parameter, also)

	mov AX, word [SS:(BP + 28)]
	mov byte [SHOWPROGRESS], AL

	push dword [SS:(BP + 24)]
	push dword [NEXTCLUSTER]
	call loadFile
	add SP, 8

	;; Was that successful?  Do a signed comparison.  Less than 0
	;; means error.
	cmp AX, 0
	jge .success

	;; Failed to load the file.  Put -4 as our return code
	mov dword [SS:(BP + 16)], -4
	jmp .done

	.success:
	;; Success.  Put the file size as our return code.
	mov EAX, dword [FILESIZE]
	mov dword [SS:(BP + 16)], EAX

	.done:
	popa
	;; Pop the return code
	pop EAX
	ret


	SEGMENT .data
	ALIGN 4

MEMORYMARKER	dd 0		;; Offset to load next data cluster
FATSEGMENT		dw 0		;; The segment for FAT and directory data
FATSECSLOADED	dd 0		;; The number of FAT sectors we loaded
FILEDATABUFFER	dd 0		;; The buffer for general file data
DIRSECTORS		dw 0		;; The size of the root directory, in sectors
BYTESPERCLUST	dd 0		;; Bytes per cluster
ENTRYSTART		dw 0		;; Directory entry start
FILESIZE		dd 0		;; Size of the file we're loading
BYTESREAD		dd 0		;; Number of bytes read so far
NEXTCLUSTER		dd 0		;; Next cluster to load

PARTENTRY		times 16 db 0	;; Partition table entry of bootable partition

SHOWPROGRESS	db 0            ;; Whether or not to show a progress bar

