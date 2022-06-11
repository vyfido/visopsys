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
;;  loaderDetectHardware.s
;;

	GLOBAL loaderDetectHardware
	GLOBAL HARDWAREINFO

	EXTERN loaderDetectVideo
	EXTERN loaderPrint
	EXTERN loaderPrintNewline
	EXTERN loaderPrintNumber
	EXTERN FATALERROR

	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"
	%include "../kernel/kernelAssemblerHeader.h"


loaderDetectHardware:
	;; This routine is called by the main program.  It is the master
	;; routine for detecting hardware, and is responsible for filling
	;; out the data structure which contains all of the information 
	;; about the hardware detected, which in turn gets passed to 
	;; the kernel.  
	
	;; The routine is also responsible for stopping the boot process 
	;; in the event that the system does not meet hardware requirements.
	;; The function returns a single value representing the number of
	;; fatal errors encountered during this process.

	;; Save regs
	pusha
	
	;; Detect the processor
	call detectProcessor
		
	;; Detect the memory
	call detectMemory

	;; Detect video.  Push a pointer to the start of the video
	;; information in the hardware structure
	push word VIDEOMEMORY
	call loaderDetectVideo
	add SP, 2
	
	;; Detect floppy drives, if any
	call detectFloppies

	;; Detect Fixed disk drives, if any
	call detectHardDisks

	;; Restore flags
	popa

	;; Return whether we detected any fatal errors
	xor AX, AX
	mov AL, byte [FATALERROR]

	ret

	
	
detectProcessor:
	;; We're going to attempt to get a basic idea of the CPU
	;; type we're running on.  (We don't need to be really, really
	;; particular about the details)

	;; Save regs
	pusha

	;; Hook the 'invalid opcode' interrupt
	call int6_hook

	;; Bochs hack
	;; mov byte [INVALIDOPCODE], 1
	;; jmp .goodCPU
	
	;; Try an opcode which is only good on 386+
	mov word [ISRRETURNADDR], .return1
	mov EDX, CR0

	;; OK, now try an opcode which is only good on 486+
	.return1:	
	mov word [ISRRETURNADDR], .return2
	xadd DX, DX

	;; Now try an opcode which is only good on a pentium+
	.return2:	
	mov word [ISRRETURNADDR], .return3
	mov EAX, dword [0000h]
	not EAX
	cmpxchg8b [0000h]

	;; We know we're OK, but let's check for Pentium Pro+
	.return3:	
	mov word [ISRRETURNADDR], .return4
	cmovne AX, BX

	;; Now we have to compare the number of 'invalid opcodes'
	;; generated
	.return4:	
	mov AL, byte [INVALIDOPCODE]

	cmp AL, 3
	jae .badCPU

	.goodCPU:		
	;; If we fall through, we have an acceptible CPU
	;; Use green colour
	mov DL, 02h
		
	mov SI, HAPPY
	call loaderPrint
	mov SI, PROCESSOR
	call loaderPrint

	;; Now again, we make the distinction between different processors
	;; based on how many invalid opcodes we got
	mov AL, byte [INVALIDOPCODE]

	;; Switch to foreground colour
	mov DL, FOREGROUNDCOLOUR
	
	cmp AL, 2
	je .cpu486
	cmp AL, 1
	je .cpuPentium

	;; Say we found a pentium pro CPU
	mov SI, CPUPPRO
	call loaderPrint
	mov dword [CPUTYPE], pentiumPro
	jmp .cpuId

	.cpuPentium:	
	;; Say we found a Pentium CPU
	mov SI, CPUPENTIUM
	call loaderPrint
	mov dword [CPUTYPE], pentium
	jmp .cpuId

	.cpu486:	
	;; Say we found a 486 CPU
	mov SI, CPU486
	call loaderPrint
	mov dword [CPUTYPE], i486
	;; 486 doesn't support CPUID, but maybe some MMX?
	jmp .detectMMX


	.badCPU:	
	;; Print out the fatal message that we're not running an
	;; adequate processor

	;; Use error colour
	mov DL, ERRORCOLOUR

	mov SI, SAD
	call loaderPrint
	mov SI, PROCESSOR
	call loaderPrint

	cmp byte [INVALIDOPCODE], 4
	jae .older

	mov SI, CPU386
	call loaderPrint
	jmp .endBadCPUMessages

	.older:	
	mov SI, CPU286
	call loaderPrint

	.endBadCPUMessages:	
	mov SI, CPUCHECKBAD
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, CPUCHECKBAD2
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, CPUCHECKBAD3
	call loaderPrint

	;; Register the fatal error
	add byte [FATALERROR], 01h

	;; We're finished
	jmp .unhook6

	
	.cpuId:
	;; If we have a pentium or better, we can find out some more
	;; information using the cpuid instruction
	mov EAX, 0
	cpuid
	
	;; Now, EBX:EDX:ECX should contain the vendor "string".  This might
	;; be, for example "AuthenticAMD" or "GenuineIntel"
	mov dword [CPUVEND], EBX
	mov dword [(CPUVEND + 4)], EDX
	mov dword [(CPUVEND + 8)], ECX

	;; Print the CPU vendor string
	mov CX, 12
	mov SI, CPUVEND
	mov DL, FOREGROUNDCOLOUR
	call loaderPrint
	mov SI, CLOSEBRACKETS
	call loaderPrint
	
	
	.detectMMX:
	;; Are MMX or 3DNow! extensions supported by the processor?
	
	mov dword [MMXEXT], 0
	mov byte [INVALIDOPCODE], 0

	;; Try an MMX opcode
	mov word [ISRRETURNADDR], .return
	movd MM0, EAX

	.return:
	;; If it was an invalid opcode, the processor does not support
	;; MMX extensions
	cmp byte [INVALIDOPCODE], 0
	jnz .unhook6

	mov dword [MMXEXT], 1
	
	mov SI, MMX			;; Say we found MMX
	mov DL, FOREGROUNDCOLOUR

	call loaderPrint

	.unhook6:
	call loaderPrintNewline
	;; Unhook the 'invalid opcode' interrupt
	call int6_restore

	;; Restore regs
	popa
	ret
	

detectMemory:	
	;; Determine the amount of extended memory


	;; Save regs
	pusha
	
	;; Print out a message about memory

	;; Use green colour
	mov DL, 02h

	mov SI, HAPPY
	call loaderPrint
	mov SI, MEMDETECT1
	call loaderPrint

	mov dword [EXTENDEDMEMORY], 0
	
	;; This BIOS function will give us the amount of extended memory
	;; (even greater than 64M), but it is not found in old BIOSes.  We'll
	;; have to assume that if the function is not available, the
	;; extended memory is less than 64M.

	mov EAX, 0000E801h	; Subfunction
	int 15h
	jc .noE801

	and EAX, 0000FFFFh	; 16-bit value, memory under 16M in 1K blocks
	and EBX, 0000FFFFh	; 16-bit, memory over 16M in 64K blocks
	shl EBX, 6		; Multiply by 64 to get 1K blocks
	add EAX, EBX
	mov dword [EXTENDEDMEMORY], EAX

	jmp .printMemory	
	
	.noE801:
	;; We will use this as a last-resort method for getting memory
	;; size.  We just grab the 16-bit value from CMOS
	
	mov AL, 17h	;; Select the address we need to get the data
	out 70h, AL	;; from

	in AL, 71h
	mov byte [EXTENDEDMEMORY], AL

	mov AL, 18h	;; Select the address we need to get the data
	out 70h, AL	;; from

	in AL, 71h
	mov byte [(EXTENDEDMEMORY + 1)], AL

	.printMemory:
	mov EAX, dword [EXTENDEDMEMORY]
	call loaderPrintNumber
	mov SI, KREPORTED
	mov DL, FOREGROUNDCOLOUR
	call loaderPrint
	call loaderPrintNewline

	;; Now, can the system supply us with a memory map?  If it can,
	;; this will allow us to supply a list of unusable memory to the
	;; kernel (which will improve reliability, we hope).  Try to call
	;; the appropriate BIOS function.

	;; This function might dink with ES
	push ES
	
	xor EBX, EBX		; Continuation counter
	mov DI, MEMORYMAP	; The buffer

	.smapLoop:
	mov EAX, 0000E820h	; Function number
	mov EDX, 534D4150h	; ('SMAP')
	mov ECX, 20		; Size of buffer
	int 15h

	;; Call successful?
	jc .doneSmap
	
	;; Function supported?
	cmp EAX, 534D4150h	; ('SMAP')
	jne .doneSmap

	;; All done?
	cmp EBX, 0
	je .doneSmap

	;; Call the BIOS for the next bit
	add DI, 20
	cmp DI, (MEMORYMAP + (MEMORYMAPSIZE * 20))
	jl .smapLoop
	
	.doneSmap:
	;; Restore ES
	pop ES
	
	;; Restore regs
	popa
	ret


detectFloppies:
	;; This routine will detect the number and types of floppy
	;; disk drives on board

	;; Save regs
	pusha

	
	;; Initialize 'number of floppies' value
	mov dword [FLOPPYDISKS], 0
	
	;; We need to test for up to two floppy disks.  We could do this
	;; in a loop, but that would be silly for two iterations


	;; Test for floppy 0.  
	
	;; This interrupt call will destroy ES, so save it
	push ES
	
	mov AX, 0800h
	xor DX, DX		; Disk 0
	int 13h
	
	;; Restore ES
	pop ES
	
	;; If there was an error, we will say there are no floppies
	jc .print

	;; Is the disk installed?  If not, we will say no floppies
	cmp CX, 0
	je .print

	;; Count it
	add dword [FLOPPYDISKS], 1

	;; Put the type/head/track/sector values into the data structures
	xor EAX, EAX
	mov AL, BL
	mov dword [FDD0TYPE], EAX
	inc DH			; Number is 0-based
	mov AL, DH
	mov dword [FDD0HEADS], EAX
	inc CH			; Number is 0-based
	mov AL, CH
	mov dword [FDD0TRACKS], EAX
	mov AL, CL
	mov dword [FDD0SECTS], EAX

	
	;; Test for floppy 1

	;; This interrupt call will destroy ES, so save it
	push ES
	
	mov AX, 0800h
	mov DX, 0001h		; Disk 1
	int 13h

	;; Restore ES
	pop ES
	
	;; If there was an error, we're finished
	jc .print

	;; Is the disk installed?  If not, we're finished
	cmp CX, 0
	je .print

	;; Count it
	add dword [FLOPPYDISKS], 1

	;; Put the type/head/track/sector values into the data structures
	xor EAX, EAX
	mov AL, BL
	mov dword [FDD1TYPE], EAX
	inc DH			; Number is 0-based
	mov AL, DH
	mov dword [FDD1HEADS], EAX
	inc CH			; Number is 0-based
	mov AL, CH
	mov dword [FDD1TRACKS], EAX
	mov AL, CL
	mov dword [FDD1SECTS], EAX


	.print:
	;; Print message about the disk scan

	;; Use green colour
	mov DL, 02h

	mov SI, HAPPY
	call loaderPrint
	mov SI, FDDCHECK
	call loaderPrint

	;; Switch to foreground colour
	mov DL, FOREGROUNDCOLOUR
	
	mov EAX, dword [FLOPPYDISKS]
	call loaderPrintNumber
	mov SI, DISKCHECK
	call loaderPrint
	call loaderPrintNewline

	cmp dword [FLOPPYDISKS], 1
	jb .done
	;; Print information about the disk.  EBX contains the pointer...
	mov EBX, FDD0TYPE
	call printFddInfo
	
	cmp dword [FLOPPYDISKS], 2
	jb .done
	;; Print information about the disk.  EBX contains the pointer...
	mov EBX, FDD1TYPE
	call printFddInfo
	

	.done:
	;; Restore regs
	popa

	ret
	
	
detectHardDisks:
	;; This routine will detect the number, types, and sizes of 
	;; hard disk drives on board

	;; Save regs
	pusha

	;; Initialize
	mov dword [HARDDISKS], 0
	
	;; Print messages about the disk scan

	;; Use green colour
	mov DL, 02h

	mov SI, HAPPY
	call loaderPrint
	mov SI, HDDCHECK
	call loaderPrint

	;; Call the BIOS int13h function with the number of the first
	;; disk drive.  Doesn't matter if it's actually present -- all
	;; we want to do is find out how many drives there are

	;; This interrupt call will destroy ES, so save it
	push ES
	
	mov AH, 08h
	mov DL, 80h
	int 13h

	;; Restore ES
	pop ES

	;; Now, if the carry bit is set, we assume no hard disks
	;; and we're finished
	jc near .done

	;; Otherwise, save the number
	xor EAX, EAX
	mov AL, DL
	mov dword [HARDDISKS], EAX

	.printDisks:
	mov EAX, dword [HARDDISKS]
	call loaderPrintNumber
	mov DL, FOREGROUNDCOLOUR
	mov SI, DISKCHECK
	call loaderPrint
	call loaderPrintNewline

	
	;; Attempt to determine information about the drives

	;; Start with drive 0
	mov ECX, 0
	mov EDI, HDD0HEADS

	.driveLoop:
	;; First try an advanced EBIOS function that will give us nice,
	;; modern, large values
	mov word [HDDINFO], 42h  ; Size of the info buffer we provide
	mov AH, 48h
	mov DL, CL
	add DL, 80h
	mov SI, HDDINFO
	int 13h
	
	;; Function call successful?
	jc near .noEBIOS

	;; Save the numbers of heads, cylinders, and sectors, and the
	;; sector size (usually 512)
	mov EAX, dword [(HDDINFO + 08h)]
	mov dword [(EDI + 00h)], EAX		; heads
	mov EAX, dword [(HDDINFO + 04h)]
	mov dword [(EDI + 04h)], EAX 	; cylinders
	mov EAX, dword [(HDDINFO + 0Ch)]
	mov dword [(EDI + 08h)], EAX		; sectors
	xor EAX, EAX
	mov AX, word [(HDDINFO + 18h)]
	mov dword [(EDI + 0Ch)], EAX		; bytes per sector
	
	;; Calculate the disk size.  EDI contains the pointer...
	call diskSize

	;; Is there any additional EDD info?
	cmp dword [(HDDINFO + 1Ah)], 0FFFFFFFFh
	je .noEDD0

	.noEDD0:
	;; Print information about the disk.  EBX contains the pointer...
	mov EBX, EDI
	call printHddInfo

	;; Reset/specify/recalibrate the disk and controller
	push ECX
	mov AX, 0D00h
	mov DL, 80h
	add DL, CL
	int 13h
	pop ECX

	;; Tell the controller to use the default PIO mode (and disable
	;; DMA in the process)
	push ECX
	mov AX, 4E04h
	mov DL, 80h
	add DL, AL
	int 13h
	pop ECX	

	;; Any more disks to inventory?
	inc ECX
	cmp ECX, dword [HARDDISKS]
	jae near .done

	;; Go to the next drive
	add EDI, HDDBLOCKSIZE
	jmp .driveLoop

	.noEBIOS:
	;; We use this part if there is no EBIOS call supported to
	;; determine hard disk info.  This is an old-fashioned call
	;; that should be available on all PC systems

	;; Start with drive 0.
	mov ECX, 0
	mov EDI, HDD0HEADS
	
	.noEBDiskLoop:
	push ECX		; Save this

	;; This interrupt call will destroy ES, so save it
	push ES
	
	mov AH, 08h		; Read disk drive parameters
	mov DL, CL
	add DL, 80h
	int 13h

	;; Restore ES
	pop ES
	
	;; If carry set, the call was unsuccessful (for whatever reason)
	;; and we will consider ourselves finished
	jnc .ok

	;; There was an error
	pop ECX
	jmp .done
	
	.ok:
	;; Save the numbers of heads, cylinders, and sectors, and the
	;; sector size (usually 512)
	xor EAX, EAX		; heads
	mov AL, DH
	inc AX			; Number is 0-based
	mov dword [(EDI + 00h)], EAX 
	xor EAX, EAX		; cylinders
	mov AL, CL		; Two bits of cylinder number in bits 6&7
	and AL, 11000000b	; Mask it
	shl AX, 2		; Move them to bits 8&9
	mov AL, CH		; Rest of the cylinder bits
	inc AX			; Number is 0-based
	mov dword [(EDI + 04h)], EAX 
	xor EAX, EAX		; sectors
	mov AL, CL		; Bits 0-5
	and AL, 00111111b	; Mask it
	mov dword [(EDI + 08h)], EAX
	mov dword [(EDI + 0Ch)], 512		; Assume 512 bps

	;; Restore ECX
	pop ECX
	
	;; Calculate the disk size.  EDI contains the pointer...
	call diskSize
	
	;; Print information about the disk.  EBX contains the pointer...
	mov EBX, EDI
	call printHddInfo

	;; Reset/specify/recalibrate the disk and controller
	push ECX
	mov AX, 0D00h
	mov DL, 80h
	add DL, CL
	int 13h
	pop ECX

	
	;; Any more disks to inventory?
	inc ECX
	cmp ECX, dword [HARDDISKS]
	jae .done

	;; Go to the next drive
	add EDI, HDDBLOCKSIZE
	jmp .noEBDiskLoop

	.done:
	;; Restore regs
	popa

	ret


diskSize:
	;; This calculates the size of a disk drive based on the numbers
	;; of heads, cylinders, sectors, and bytes per sector.  Takes a
	;; pointer to the disk data in EDI, and puts the number in the
	;; "size" slot of the data block

	;; Save regs
	pusha

	;; Calculate the disk size, in megabytes
	
	mov EAX, dword [(EDI + 0)]
	mov EBX, dword [(EDI + 4)]
	mul EBX
	mov ECX, EAX

	mov EAX, dword [(EDI + 8)]
	mov EBX, dword [(EDI + 12)]
	mul EBX
	
	mul ECX			; Combine
	
	;; Divide EDX:EAX by 1Mb
	mov EBX, 1000000
	div EBX
	mov dword [(EDI + 16)], EAX

	;; Restore regs
	popa

	ret
	
	
printFddInfo:	
	;; This takes a pointer to the disk data in EBX, and prints
	;; disk info to the console

	;; Save regs
	pusha

	;; Print a message about what we found
	mov DL, 02h
	mov SI, BLANK
	call loaderPrint
	
	mov EAX, dword [(EBX + 4)]
	call loaderPrintNumber

	mov DL, FOREGROUNDCOLOUR
	mov SI, HEADS
	call loaderPrint
				
	mov EAX, dword [(EBX + 8)]
	call loaderPrintNumber

	mov DL, FOREGROUNDCOLOUR
	mov SI, TRACKS
	call loaderPrint
				
	mov EAX, dword [(EBX + 12)]
	call loaderPrintNumber

	mov DL, FOREGROUNDCOLOUR
	mov SI, SECTS
	call loaderPrint
	call loaderPrintNewline
				
	;; Restore regs
	popa

	ret

	
printHddInfo:	
	;; This takes a pointer to the disk data in EBX, and prints
	;; disk info to the console

	;; Save regs
	pusha

	;; Print a message about what we found
	mov DL, 02h
	mov SI, BLANK
	call loaderPrint

	mov EAX, dword [(EBX + 0)]
	call loaderPrintNumber

	mov DL, FOREGROUNDCOLOUR
	mov SI, HEADS
	call loaderPrint
				
	mov EAX, dword [(EBX + 4)]
	call loaderPrintNumber

	mov DL, FOREGROUNDCOLOUR
	mov SI, CYLS
	call loaderPrint
				
	mov EAX, dword [(EBX + 8)]
	call loaderPrintNumber

	mov DL, FOREGROUNDCOLOUR
	mov SI, SECTS
	call loaderPrint

	mov EAX, dword [(EBX + 16)]
	call loaderPrintNumber
	
	mov DL, FOREGROUNDCOLOUR
	mov SI, MEGA
	call loaderPrint
	call loaderPrintNewline
				
	;; Restore regs
	popa

	ret

	
int6_hook:
	;; This sets up our hook for interrupt 6, in order to catch
	;; invalid opcodes for CPU determination

	;; Save regs
	pusha

	;; Get the address of the current interrupt 6 handler
	;; and save it

	;; Set ES so that it points to the beginning of memory
	push ES
	xor AX, AX
	mov ES, AX

	mov AX, word [ES:0018h]	;; The offset of the routine
	mov word [OLDINT6], AX
	mov AX, word [ES:001Ah]	;; The segment of the routine
	mov word [(OLDINT6 + 2)], AX

	cli

	;; Move the address of our new handler into the interrupt
	;; table
	mov word [ES:0018h], int6_handler	;; The offset
	mov word [ES:001Ah], CS		;; The segment

	sti

	;; Restore ES
	pop ES

	;; Initialize the value that keeps track of invalid opcodes
	mov byte [INVALIDOPCODE], 00h

	popa
	ret
	

int6_handler:
	;; This is our int 6 interrupt handler, to determine
	;; when an invalid opcode has been generated

	;; If we got here, then we know we have an invalid opcode,
	;; so we have to change the value in the INVALIDOPCODE
	;; memory location

	push AX
	push BX

	cli

	add byte [INVALIDOPCODE], 1

	;; Better change the instruction pointer to point to the
	;; next instruction to execute
	mov AX, word [ISRRETURNADDR]
	mov BX, SP
	mov word [SS:(BX + 4)], AX

	sti

	pop BX
	pop AX

	iret

	
int6_restore:
	;; This unhooks interrupt 6 (we'll let the kernel handle
	;; interrupts in its own way later)

	pusha

	;; Set ES so that it points to the beginning of memory
	push ES
	xor AX, AX
	mov ES, AX

	cli

	mov AX, word [OLDINT6]
	mov word [ES:0018h], AX	;; The offset of the routine
	mov AX, word [OLDINT6 + 2]
	mov word [ES:001Ah], AX	;; The segment of the routine

	sti

	;; Restore ES
	pop ES

	popa
	ret


;;
;; The data segment
;;

	SEGMENT .data
	ALIGN 4

OLDINT6		dd 0		;; Address of the interrupt 6 handler
ISRRETURNADDR	dw 0		;; The offset of the return address for int6
INVALIDOPCODE	db 0		;; To pass data from our interrupt handler
HDDINFO		times 42h  db 0	;; Space for info ret by EBIOS

;; This is the data structure that these routines will fill.  The
;; (flat-mode) address of this structure is eventually passed to
;; the kernel at invocation

	ALIGN 4

HARDWAREINFO:
	CPUTYPE		dd 0	;; See %defines at top
	CPUVEND		dd 0, 0, 0 ;; CPU vendor string, if supported
	MMXEXT		dd 0	;; Boolean; 1 or zero
	EXTENDEDMEMORY	dd 0	;; In Kbytes
	;; Info returned by int 15h function E820h
	MEMORYMAP	times (MEMORYMAPSIZE * 20) db 0
	;; This is all the information about the video capabilities
	VIDEOMEMORY	dd 0 	;; In Kbytes
	VIDEOLFB	dd 0 	;; Address
	VIDEOX		dd 0	;; maximum X resolution
	VIDEOY		dd 0	;; maximum Y resolution
	VIDEOBPP	dd 0	;; maximum bits per pixel
	;; This is an array of info about up to 2 floppy disks in the system
	FLOPPYDISKS	dd 0	;; Number present
	;; Floppy 0
	FDD0TYPE	dd 0	;; Floppy 0 type
	FDD0HEADS	dd 0	;; Number of heads, floppy 0
	FDD0TRACKS	dd 0	;; Number of tracks, floppy 0
	FDD0SECTS	dd 0	;; Number of sectors, floppy 0
	;; Floppy 1
	FDD1TYPE	dd 0	;; Floppy 1 type
	FDD1HEADS	dd 0	;; Number of heads, floppy 1
	FDD1TRACKS	dd 0	;; Number of tracks, floppy 1
	FDD1SECTS	dd 0	;; Number of sectors, floppy 1
	;; This is an array of info about up to 4 hard disks in the system
	HARDDISKS	dd 0	;; Number present
	;; Disk 0
	HDD0HEADS	dd 0	;; Number of heads, disk 0
	HDD0CYLS	dd 0	;; Number of cylinders, disk 0
	HDD0SECTS	dd 0	;; Number of sectors, disk 0
	HDD0SECSIZE	dd 0	;; Bytes per sector, disk 0
	HDD0SIZE	dd 0	;; Size in Mb, disk 0
	;; Disk 1
	HDD1HEADS	dd 0	;; Number of heads, disk 1
	HDD1CYLS	dd 0	;; Number of cylinders, disk 1
	HDD1SECTS	dd 0	;; Number of sectors, disk 1
	HDD1SECSIZE	dd 0	;; Bytes per sector, disk 1
	HDD1SIZE	dd 0	;; Size in Mb, disk 1
	;; Disk 2
	HDD2HEADS	dd 0	;; Number of heads, disk 2
	HDD2CYLS	dd 0	;; Number of cylinders, disk 2
	HDD2SECTS	dd 0	;; Number of sectors, disk 2
	HDD2SECSIZE	dd 0	;; Bytes per sector, disk 2
	HDD2SIZE	dd 0	;; Size in Mb, disk 2
	;; Disk 3
	HDD3HEADS	dd 0	;; Number of heads, disk 3
	HDD3CYLS	dd 0	;; Number of cylinders, disk 3
	HDD3SECTS	dd 0	;; Number of sectors, disk 3
	HDD3SECSIZE	dd 0	;; Bytes per sector, disk 3
	HDD3SIZE	dd 0	;; Size in Mb, disk 3

;; 
;; These are general messages related to hardware detection
;;

HAPPY		db 01h, ' ', 0
BLANK		db '               ', 10h, ' ', 0
PROCESSOR	db 'Processor    ', 10h, ' ', 0
CPUPPRO		db 'pentium pro or better ("', 0
CPUPENTIUM	db 'pentium ("', 0
CPU486		db 'i486', 0
CPU386		db 'i386', 0
CPU286		db 'i286 (or lower)', 0
CLOSEBRACKETS	db '") ', 0
MMX		db 'with MMX', 0
MEMDETECT1	db 'Extended RAM ', 10h, ' ', 0
KREPORTED	db 'K reported', 0
FDDCHECK	db 'Floppy disks ', 10h, ' ', 0
HDDCHECK	db 'Hard disks   ', 10h, ' ', 0
DISKCHECK	db ' disk(s)', 0
HEADS		db ' heads, ', 0
TRACKS		db ' tracks, ', 0
CYLS		db ' cyls, ', 0
SECTS		db ' sects   ', 0
MEGA		db ' Mbytes', 0

	
;;
;; These are error messages related to hardware detection
;;

SAD		db 'x ', 0
CPUCHECKBAD	db ': This processor is not adequate to run Visopsys.  Visopsys', 0
CPUCHECKBAD2	db 'requires an i486 or better processor in order to function', 0
CPUCHECKBAD3	db 'properly.  Please see your computer dealer.', 0
