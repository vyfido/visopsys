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
;;  loaderLoadKernel.s
;;

	GLOBAL loaderLoadKernel

	EXTERN loaderLoadFile
	EXTERN loaderPrint
	EXTERN loaderPrintNewline
	EXTERN loaderPrintNumber

	EXTERN KERNELSIZE
	
	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"
	%include "../kernel/kernelAssemblerHeader.h"


loaderLoadKernel:
	;; This function is in charge of loading the kernel file and
	;; setting it up for execution.  This is designed to load the
	;; kernel as an ELF binary.  First, it sets up to call the
	;; loaderLoadFile routine with the correct parameters.  If there
	;; is an error, it can print an informative error message about
	;; the problem that was encountered (based on the error code from
	;; the loaderLoadFile function).  Next, it performs functions
	;; like that of any other 'loader': it examines the ELF header
	;; of the file,does any needed memory spacing as specified therein
	;; (such as moving segments around and creating data segments)

	;; Save a dword on the stack for our return value
	sub SP, 2
	
	;; Save regs
	pusha

	;; Save the stack register
	mov BP, SP

	;; Load the kernel file
	push dword KERNELCODEDATALOCATION
	push word KERNELNAME
	call loaderLoadFile
	add SP, 6

	;; Make sure the load was successful
	cmp AX, 0
	jge near .okLoad

	;; We failed to load the kernel.  The following call will determine
	;; the type of error encountered while loading the kernel, and print
	;; a helpful error message about the reason.	
	push AX
	call evaluateLoadError
	add SP, 2
	
	;; Quit
	mov word [SS:(BP + 16)], -1
	jmp .done
	
	.okLoad:
	;; We were successful.  The kernel's size is in AX.  Ignore it.

	;; Save GS, since we will mess with it
	push GS
	
	;; Disable interrupts
	cli

	;; Switch to protected mode temporarily
	mov EAX, CR0
	or AL, 01h
	mov CR0, EAX

	BITS 32
	
	;; Load GS with the global data segment selector
	mov EAX, ALLDATASELECTOR
	mov GS, AX

	;; Return to real mode
	mov EAX, CR0
	and AL, 0FEh
	mov CR0, EAX

	BITS 16

	;; Reenable interrupts
	sti
	
	;; Now we need to examine the elf header.
	call getElfHeaderInfo

	;; Make sure the evaluation was successful
	cmp AX, 0
	jge near .okEval

	;; The kernel image is not what we expected.  Return the error
	;; code from the call
	mov word [SS:(BP + 16)], AX
	jmp .done_GS

	.okEval:
	;; OK, call the routine to create the proper layout for the kernel
	;; based on the ELF information we gathered
	call layoutKernel
		
	;; Make sure the layout was successful
	cmp AX, 0
	jge near .success

	;; We failed.  Return the error code.
	;; code from the call
	mov word [SS:(BP + 16)], AX
	jmp .done_GS

	.success:
	;; Set the size of the kernel image, which is the combined memory
	;; size of the code and data segments.  Return 0
	mov EAX, dword [CODE_SIZEINFILE]
	add EAX, dword [DATA_SIZEINMEM]
	;; Make it the next multiple of 4K
	add EAX, 00001000h
	and EAX, 0FFFFF000h
	mov dword [KERNELSIZE], EAX
	mov word [SS:(BP + 16)], 0

	.done_GS:
	;; Restore GS
	pop GS

	.done:
	;; Restore regs
	popa
	;; Pop our return code
	pop AX
	ret


layoutKernel:
	;; This function takes information about the kernel ELF file
	;; sections and modifies the kernel image appropriately in memory.

	;; Save a word for our return code
	sub SP, 2

	;; Save regs
	pusha

	;; We will do layout for two segments; the code and data segments
	;; (the getElfHeaderInfo() function should have caught any deviation
	;; from that state of affairs).

	;; For the code segment, we simply place it at the entry point.  The
	;; entry point, in physical memory, should be KERNELCODEDATALOCATION.
	;; Thus, all we do is move all code backwards by CODE_OFFSET bytes.
	;; This will have the side effect of deleting the ELF header and
	;; program header from memory.

	mov ECX, dword [CODE_SIZEINFILE]
	mov ESI, KERNELCODEDATALOCATION
	add ESI, dword [CODE_OFFSET]
	mov EDI, KERNELCODEDATALOCATION

	.codeLoop:
	mov AL, byte [GS:ESI]
	mov byte [GS:EDI], AL
	inc ESI
	inc EDI
	;; Can't seem to get 'loop' instruction to play nicely with
	;; 32-bit ECX register in 16-bit mode
	dec ECX	
	cmp ECX, 0
	jne .codeLoop

	;; We do the same operation for the data segment, except we have to
	;; first make sure that the difference between the code and data's
	;; virtual address is the same as the difference between the offsets
	;; in the file.
	mov EAX, dword [DATA_VIRTADDR]
	sub EAX, dword [CODE_VIRTADDR]
	cmp EAX, dword [DATA_OFFSET]
	je .okDataOffset

	;; The kernel image doesn't look the way we expected.  This program
	;; isn't versatile enough to handle that yet.
	mov DL, ERRORCOLOUR
	mov SI, THEFILE
	call loaderPrint
	mov SI, SEGLAYOUT
	call loaderPrint
	call loaderPrintNewline
	mov SI, REINSTALL
	call loaderPrint
	call loaderPrintNewline
	mov word [SS:(BP + 16)], -1
	jmp .done

	.okDataOffset:
	;; We need to zero out the memory that makes up the difference
	;; between the data's file size and its size in memory.
	mov ECX, dword [DATA_SIZEINMEM]
	sub ECX, dword [DATA_SIZEINFILE]
	mov EDI, KERNELCODEDATALOCATION
	add EDI, dword [DATA_OFFSET]
	add EDI, dword [DATA_SIZEINFILE]
	
	.zeroLoop:
	mov byte [GS:EDI], 0
	inc EDI
	;; Can't seem to get 'loop' instruction to play nicely with
	;; 32-bit ECX register in 16-bit mode
	dec ECX	
	cmp ECX, 0
	jne .zeroLoop

	.success:
	;; Make 0 be our return code
	mov word [SS:(BP + 16)], 0
	
	.done:
	popa
	;; Pop our return code
	xor EAX, EAX
	pop AX
	ret
	

getElfHeaderInfo:
	;; This function checks the ELF header of the kernel file and
	;; saves the relevant information about the file.  Assumes that
	;; GS describes a global segment for all of physical memory.

	;; Save a word for our return code
	sub SP, 2

	;; Save regs
	pusha

	;; Save the stack register
	mov BP, SP

	;; Make sure it's an ELF file (check the magic number)
	mov ESI, KERNELCODEDATALOCATION
	mov EAX, dword [GS:ESI]
	cmp EAX, 464C457Fh	; ELF magic number (7Fh, 'ELF')
	je .isElf

	;; The kernel was not an ELF binary
	mov DL, ERRORCOLOUR
	mov SI, THEFILE
	call loaderPrint
	mov SI, NOTELF
	call loaderPrint
	call loaderPrintNewline
	mov SI, REINSTALL
	call loaderPrint
	call loaderPrintNewline
	mov word [SS:(BP + 16)], -1
	jmp .done
	
	.isElf:
	;; It's an ELF binary.  We will skip doing exhaustive checks, as we
	;; would do in the case of loading some user binary.  We will,
	;; however, make sure that it's an executable ELF binary
	mov ESI, (KERNELCODEDATALOCATION + 16)
	mov AX, word [GS:ESI]
	cmp AX, 2		; ELF executable file type
	je .isExec

	;; The kernel was not executable
	mov DL, ERRORCOLOUR
	mov SI, THEFILE
	call loaderPrint
	mov SI, NOTEXEC
	call loaderPrint
	call loaderPrintNewline
	mov SI, REINSTALL
	call loaderPrint
	call loaderPrintNewline
	mov word [SS:(BP + 16)], -2
	jmp .done
	
	.isExec:
	;; Cool.  Now we start parsing the header, collecting any info
	;; that we care about.
	
	;; First the kernel entry point.
	mov ESI, (KERNELCODEDATALOCATION + 24)
	mov EAX, dword [GS:ESI]
	mov dword [ENTRYPOINT], EAX

	;; Now the offset of the program header
	mov ESI, (KERNELCODEDATALOCATION + 28)
	mov EAX, dword [GS:ESI]
	mov dword [PROGHEADER], EAX

	;; Now the number of program header entries.  It must be 2 (one
	;; for code and one for data)
	mov ESI, (KERNELCODEDATALOCATION + 44)
	mov AX, word [GS:ESI]
	cmp AX, 2
	je .progHdr

	;; The kernel image doesn't look the way we expected.  This program
	;; isn't versatile enough to handle that yet.
	mov DL, ERRORCOLOUR
	mov SI, THEFILE
	call loaderPrint
	mov SI, NUMSEGS
	call loaderPrint
	call loaderPrintNewline
	mov SI, REINSTALL
	call loaderPrint
	call loaderPrintNewline
	mov word [SS:(BP + 16)], -3
	jmp .done

	.progHdr:
	;; That is all the information we get directly from the ELF header.
	;; Now, we need to get information from the program header.
	mov ESI, KERNELCODEDATALOCATION
	add ESI, dword [PROGHEADER]

	;; Skip the segment type.

	add ESI, 4
	mov EAX, dword 	[GS:ESI]
	mov dword [CODE_OFFSET], EAX

	add ESI, 4
	mov EAX, dword 	[GS:ESI]
	mov dword [CODE_VIRTADDR], EAX

	;; Skip the physical address.
	
	add ESI, 8
	mov EAX, dword 	[GS:ESI]
	mov dword [CODE_SIZEINFILE], EAX

	;; Make sure the size in memory is the same in the file
	add ESI, 4
	mov EAX, dword 	[GS:ESI]
	cmp EAX, dword [CODE_SIZEINFILE]
	je .codeFlags

	;; The kernel image doesn't look the way we expected.  This program
	;; isn't versatile enough to handle that yet.
	mov DL, ERRORCOLOUR
	mov SI, THEFILE
	call loaderPrint
	mov SI, SEGLAYOUT
	call loaderPrint
	call loaderPrintNewline
	mov SI, REINSTALL
	call loaderPrint
	call loaderPrintNewline
	mov word [SS:(BP + 16)], -3
	jmp .done
	
	.codeFlags:
	;; Skip the flags

	;; Just check the alignment.  Must be 4096 (page size)
	add ESI, 8
	mov EAX, dword 	[GS:ESI]
	cmp EAX, 4096
	je .dataSeg

	;; The kernel image doesn't look the way we expected.  This program
	;; isn't versatile enough to handle that yet.
	mov DL, ERRORCOLOUR
	mov SI, THEFILE
	call loaderPrint
	mov SI, SEGALIGN
	call loaderPrint
	call loaderPrintNewline
	mov SI, REINSTALL
	call loaderPrint
	call loaderPrintNewline
	mov word [SS:(BP + 16)], -4
	jmp .done

	.dataSeg:
	;; Now the data segment
	
	;; Skip the segment type.

	add ESI, 8
	mov EAX, dword 	[GS:ESI]
	mov dword [DATA_OFFSET], EAX

	add ESI, 4
	mov EAX, dword 	[GS:ESI]
	mov dword [DATA_VIRTADDR], EAX

	;; Skip the physical address.
	
	add ESI, 8
	mov EAX, dword 	[GS:ESI]
	mov dword [DATA_SIZEINFILE], EAX
	
	add ESI, 4
	mov EAX, dword 	[GS:ESI]
	mov dword [DATA_SIZEINMEM], EAX

	;; Skip the flags
	
	add ESI, 8
	mov EAX, dword 	[GS:ESI]
	cmp EAX, 4096
	je .success

	;; The kernel image doesn't look the way we expected.  This program
	;; isn't versatile enough to handle that yet.
	mov DL, ERRORCOLOUR
	mov SI, THEFILE
	call loaderPrint
	mov SI, SEGALIGN
	call loaderPrint
	call loaderPrintNewline
	mov SI, REINSTALL
	call loaderPrint
	call loaderPrintNewline
	mov word [SS:(BP + 16)], -5
	jmp .done

	.success:
	;; Make 0 be our return code
	mov word [SS:(BP + 16)], 0
	
	.done:
	popa
	;; Pop our return code.
	xor EAX, EAX
	pop AX
	ret


evaluateLoadError:
	;; This function takes an error code as its only parameter, and
	;; prints the appropriate error message

	pusha

	;; Save regs
	pusha

	;; Save the stack register
	mov BP, SP

	;; Use the error colour
	mov DL, ERRORCOLOUR
	mov SI, LOADFAIL
	call loaderPrint

	;; Get the error code
	mov AX, word [SS:(BP + 18)]
	
	;; Was there an error loading the directory?
	cmp AX, -1
	jne .errorFIND

	;; There was an error loading the directory.
	mov SI, NODIRECTORY
	call loaderPrint
	call loaderPrintNewline
	mov SI, CORRUPTFS1
	call loaderPrint
	call loaderPrintNewline
	mov SI, CORRUPTFS2
	call loaderPrint
	call loaderPrintNewline
	jmp .done

	.errorFIND:
	;; Was there an error finding the kernel file itself?
	cmp AX, -2
	jne .errorFAT

	;; The kernel file could not be found.
	mov SI, THEFILE
	call loaderPrint
	mov SI, NOFILE1
	call loaderPrint
	call loaderPrintNewline
	mov SI, NOFILE2
	call loaderPrint
	call loaderPrintNewline
	jmp .done

	.errorFAT:
	;; Was there an error loading the FAT table?
	cmp AX, -3
	jne .errorFILE

	;; The FAT table could not be read
	mov SI, NOFAT
	call loaderPrint
	call loaderPrintNewline
	mov SI, CORRUPTFS1
	call loaderPrint
	call loaderPrintNewline
	mov SI, CORRUPTFS2
	call loaderPrint
	call loaderPrintNewline
	jmp .done

	.errorFILE:
	;; There must have been an error loading the kernel file itself.
	mov SI, THEFILE
	call loaderPrint
	mov SI, BADFILE
	call loaderPrint
	call loaderPrintNewline
	mov SI, CORRUPTFS1
	call loaderPrint
	call loaderPrintNewline
	mov SI, CORRUPTFS2
	call loaderPrint
	call loaderPrintNewline
	
	.done:
	popa
	ret
	
	
;;
;; The data segment
;;

	SEGMENT .data
	ALIGN 4
	
ENTRYPOINT	dd 0
PROGHEADER	dd 0
CODE_OFFSET	dd 0
CODE_VIRTADDR	dd 0
CODE_SIZEINFILE	dd 0
DATA_OFFSET	dd 0
DATA_VIRTADDR	dd 0
DATA_SIZEINFILE	dd 0
DATA_SIZEINMEM	dd 0

KERNELNAME	db 'VISOPSYS   ', 0
	
;;
;; The error messages
;;

LOADFAIL	db 'Loading the Visopsys kernel failed.  ', 0
NODIRECTORY	db 'The root directory could not be read.', 0
NOFAT		db 'The FAT table could not be read.', 0
THEFILE		db 'The kernel file ', 27h, 'visopsys', 0
NOFILE1		db 27h, ' could not be found.', 0
NOFILE2		db 'Please make sure that this file exists in the root directory.', 0
BADFILE		db 27h, ' could not be read.', 0
CORRUPTFS1	db 'The filesystem on the boot device may be corrupt:  you should use a disk', 0
CORRUPTFS2	db 'utility to check the integrity of the filesystem and the Visopsys files.', 0
NOTELF		db 27h, ' is not an ELF binary.', 0
NOTEXEC		db 27h, ' is not executable.', 0
NUMSEGS		db 27h, ' does not contain exactly 2 ELF segments.', 0
SEGALIGN	db 27h, ' has incorrectly aligned ELF segments.', 0
SEGLAYOUT	db 27h, ' has an incorrect ELF segment layout.', 0
REINSTALL	db 'You will probably need to reinstall Visopsys on this boot media.', 0
