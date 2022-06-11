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
;;  loaderDetectHardware.s
;;

	GLOBAL loaderDetectHardware
	GLOBAL HARDWAREINFO

	EXTERN loaderFindFile
	EXTERN loaderDetectVideo
	EXTERN loaderPrint
	EXTERN loaderPrintNewline
	EXTERN loaderPrintNumber
	EXTERN FATALERROR
	EXTERN PRINTINFO
	EXTERN DRIVENUMBER
	EXTERN PARTENTRY

	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"


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

	;; Before we check video, make sure that the user hasn't specified
	;; text-only mode
        push word NOGRAPHICS
        call loaderFindFile
        add SP, 2

        ;; Does the file exist?
        cmp AX, 1
        je .skipVideo	; The user doesn't want graphics

	;; Detect video.  Push a pointer to the start of the video
	;; information in the hardware structure
	push word VIDEOMEMORY
	call loaderDetectVideo
	add SP, 2
	
	.skipVideo:

	;; Save the boot device number
	xor EAX, EAX
	mov AX, word [DRIVENUMBER]
	mov dword [BOOTDEVICE], EAX
	
	;; Record the Visopsys name for the boot disk

	;; Check for CD-ROM emulation stuffs
	mov AX, 4B01h
	mov DX, word [DRIVENUMBER]
	mov SI, EMUL_SAVE
	int 13h
	jc .notCDROM
	mov AL, byte [EMUL_SAVE + 1]
	and AL, 0Fh
	cmp AL, 0
	je .notCDROM

	;; Make note that there's emulation
	mov byte [EMULATION], 1

	mov dword [BOOTDISK], 00306463h ; ("cd0")
	jmp .doneBootName
	
	.notCDROM:
	cmp word [DRIVENUMBER], 80h
	jb .notHDD

	mov word [BOOTDISK], 6468h ; ("hd")
	mov AX, word [DRIVENUMBER]
	sub AL, 80h
	add AL, 30h		; ("0")
	mov byte [BOOTDISK + 2], AL
	jmp .doneBootName
		
	.notHDD:
	mov word [BOOTDISK], 6466h ; ("fd")
	mov AX, word [DRIVENUMBER]
	add AL, 30h		; ("0")
	mov byte [BOOTDISK + 2], AL

	.doneBootName:
	
	;; Detect floppy drives, if any
	call detectFloppies

	;; Detect Fixed disk drives, if any
	call detectHardDisks

	;; If we were doing CD-ROM emulation, turn it back on
	cmp byte [EMULATION], 1
	jne .noEmul

	;; Put the real information into the disk hardware information
	mov EAX, [FDD1TYPE]
	mov dword [FDD0TYPE], EAX
		
	.noEmul:	
	
	;; Serial ports
	call detectSerial

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
	
	;; Try an opcode which is only good on 486+
	mov word [ISRRETURNADDR], .return1
	xadd DX, DX

	;; Now try an opcode which is only good on a pentium+
	.return1:	
	mov word [ISRRETURNADDR], .return2
	mov EAX, dword [0000h]
	not EAX
	cmpxchg8b [0000h]

	;; We know we're OK, but let's check for Pentium Pro+
	.return2:	
	mov word [ISRRETURNADDR], .return3
	cmovne AX, BX

	;; Now we have to compare the number of 'invalid opcodes'
	;; generated
	.return3:	
	mov AL, byte [INVALIDOPCODE]

	cmp AL, 3
	jae .badCPU

	;; If we fall through, we have an acceptible CPU

	;; We make the distinction between different processors
	;; based on how many invalid opcodes we got
	mov AL, byte [INVALIDOPCODE]

	cmp AL, 2
	jb .checkPentium
	;; It's a 486
	mov dword [CPUTYPE], i486
	jmp .detectMMX
	
	.checkPentium:	
	cmp AL, 1
	jb .pentiumPro
	;; Pentium CPU
	mov dword [CPUTYPE], pentium
	jmp .detectMMX

	.pentiumPro:	
	;; Pentium pro CPU
	mov dword [CPUTYPE], pentiumPro
	
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
	jnz .print

	mov dword [MMXEXT], 1
	
	.print:
	;; Done.  Print information about what we found
	cmp word [PRINTINFO], 1
	jne .unhook6
	call printCpuInfo
	jmp .unhook6
		
	.badCPU:	
	;; Print out the fatal message that we're not running an
	;; adequate processor
	mov DL, ERRORCOLOR	; Use error color
	mov SI, SAD
	call loaderPrint
	mov SI, PROCESSOR
	call loaderPrint

	mov SI, CPU386
	call loaderPrint

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
	call loaderPrintNewline

	;; Register the fatal error
	add byte [FATALERROR], 01h

	;; We're finished

	.unhook6:
	;; Unhook the 'invalid opcode' interrupt
	call int6_restore

	.done:	
	;; Restore regs
	popa
	ret
	

detectMemory:	
	;; Determine the amount of extended memory

	;; Save regs
	pusha
	
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
	cmp word [PRINTINFO], 1
	jne .noPrint
	call printMemoryInfo
	.noPrint:
	
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
	cmp word [PRINTINFO], 1
	jne .done
	
	;; Print message about the disk scan

	mov DL, 02h		; Use green color
	mov SI, HAPPY
	call loaderPrint
	mov SI, FDDCHECK
	call loaderPrint

	mov DL, FOREGROUNDCOLOR	; Switch to foreground color
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
	cmp word [PRINTINFO], 1
	jne .noPrint1
	;; Print messages about the disk scan
	mov DL, 02h		; Use green color
	mov SI, HAPPY
	call loaderPrint
	mov SI, HDDCHECK
	call loaderPrint

	mov EAX, dword [HARDDISKS]
	call loaderPrintNumber
	mov DL, FOREGROUNDCOLOR
	mov SI, DISKCHECK
	call loaderPrint
	call loaderPrintNewline
	.noPrint1:	
	
	;; Attempt to determine information about the drives

	;; Start with drive 0
	mov ECX, 0
	mov EDI, HDD0HEADS

	.driveLoop:
	
	;; If we are booting from this disk, record the boot sector LBA
	mov AX, CX
	add AX, 80h
	cmp AX, word [DRIVENUMBER]
	jne .notBoot		; This is not the boot device
	mov SI, PARTENTRY
	mov EAX, dword [SI + 8]
	mov dword [BOOTSECT], EAX

	.notBoot:
	;; This interrupt call will destroy ES, so save it
	push ECX		; Save this first
	push ES
	
	mov AH, 08h		; Read disk drive parameters
	mov DL, CL
	add DL, 80h
	int 13h

	;; Restore
	pop ES
	
	;; If carry set, the call was unsuccessful (for whatever reason)
	;; and we will move to the next disk
	jnc .okOldCall

	;; Error
	pop ECX
	jmp .nextDisk

	.okOldCall:
	;; Save the numbers of heads, cylinders, and sectors, and the
	;; sector size (usually 512)

	;; heads
	xor EAX, EAX
	mov AL, DH
	inc AX			; Number is 0-based
	mov dword [EDI], EAX
	;; cylinders
	xor EAX, EAX
	mov AL, CL		; Two bits of cylinder number in bits 6&7
	and AL, 11000000b	; Mask it
	shl AX, 2		; Move them to bits 8&9
	mov AL, CH		; Rest of the cylinder bits
	add EAX, 2		; Number is 0-based
	mov dword [EDI + (HDD0CYLS - HDD0HEADS)], EAX
	;; sectors
	xor EAX, EAX
	mov AL, CL		; Bits 0-5
	and AL, 00111111b	; Mask it
	mov dword [EDI + (HDD0SECPERCYL - HDD0HEADS)], EAX
	mov dword [EDI + (HDD0SECSIZE - HDD0HEADS)], 512 ; Assume 512 BPS

	;; Restore ECX
	pop ECX

	.gotInfo:
	;; Calculate the disk size.  EDI contains the pointer...
	call diskSize

	cmp word [PRINTINFO], 1
	jne .noPrint2
	;; Print information about the disk.  EBX contains the pointer...
	mov EBX, EDI
	call printHddInfo
	.noPrint2:	
	
	;; Reset/specify/recalibrate the disk and controller
	push ECX
	mov AX, 0D00h
	mov DL, 80h
	add DL, CL
	int 13h
	pop ECX

	.nextDisk:	
	;; Any more disks to inventory?
	inc ECX
	cmp ECX, dword [HARDDISKS]
	jae .done

	;; Go to the next drive
	add EDI, (HDD1HEADS - HDD0HEADS)
	jmp .driveLoop

	.done:
	popa
	ret


detectSerial:
	;; Detects the serial ports

	pusha
	push GS

	xor EAX, EAX

	push 0040h
	pop GS
	
	mov AX, word [GS:00h]
	mov dword [SERIAL1], EAX
	mov AX, word [GS:02h]
	mov dword [SERIAL2], EAX
	mov AX, word [GS:04h]
	mov dword [SERIAL3], EAX
	mov AX, word [GS:06h]
	mov dword [SERIAL4], EAX

	pop GS
	popa
	ret


diskSize:
	;; This calculates the total number of sectors on the disk.  Takes
	;; a pointer to the disk data in EDI, and puts the number in the
	;; "totalsectors" slot of the data block

	;; Save regs
	pusha
	push EDI

	;; Determine whether we can use an extended BIOS function to give
	;; us the number of sectors

	mov word [HDDINFO], 42h  ; Size of the info buffer we provide
	mov AH, 48h
	mov DL, CL
	add DL, 80h
	mov SI, HDDINFO
	int 13h
	
	;; Function call successful?
	jc .noEBIOS

	pop EDI
	;; Save the number of sectors
	mov EAX, dword [HDDINFO + 10h]
	mov dword [EDI + (HDD0TOTALSECS - HDD0HEADS)], EAX

	;; Recalculate the number of cylinders
	mov EAX, dword [EDI]				; heads
	mul dword [EDI + (HDD0SECPERCYL - HDD0HEADS)]	; sectors per cyl
	mov ECX, EAX					; total secs per cyl 
	xor EDX, EDX
	mov EAX, dword [EDI + (HDD0TOTALSECS - HDD0HEADS)]
	div ECX
	mov dword [EDI + (HDD0CYLS - HDD0HEADS)], EAX	; new cyls value 
	
	jmp .done
		
	.noEBIOS:
	pop EDI
	mov EAX, dword [EDI]				; heads
	mul dword [EDI + (HDD0CYLS - HDD0HEADS)]	; cylinders
	mul dword [EDI + (HDD0SECPERCYL - HDD0HEADS)]	; sectors per cyl
	
	mov dword [EDI + (HDD0TOTALSECS - HDD0HEADS)], EAX

	.done:
	popa
	ret


printCpuInfo:
	;; Takes no parameter, and prints info about the CPU that was
	;; detected

	pusha		

	mov DL, 02h		; Use green color
	mov SI, HAPPY
	call loaderPrint
	mov SI, PROCESSOR
	call loaderPrint

	;; Switch to foreground color
	mov DL, FOREGROUNDCOLOR
	
	;; What type of CPU was it?
	mov EAX, dword [CPUTYPE]
	
	cmp EAX, pentiumPro
	jne .notPPro
	;; Say we found a pentium pro CPU
	mov SI, CPUPPRO
	jmp .printType
	
	.notPPro:
	cmp EAX, pentium
	jne .notPentium
	;; Say we found a Pentium CPU
	mov SI, CPUPENTIUM
	jmp .printType
	
	.notPentium:
	;; Say we found a 486 CPU
	mov SI, CPU486
	
	.printType:
	call loaderPrint

	
	;; If we have a pentium or better, we can find out some more
	;; information using the cpuid instruction
	cmp dword [CPUTYPE], i486
	je .noCPUID
	
	mov EAX, 0
	cpuid
	
	;; Now, EBX:EDX:ECX should contain the vendor "string".  This might
	;; be, for example "AuthenticAMD" or "GenuineIntel"
	mov dword [CPUVEND], EBX
	mov dword [(CPUVEND + 4)], EDX
	mov dword [(CPUVEND + 8)], ECX
	mov byte [(CPUVEND + 12)], 0
	
	;; Print the CPU vendor string
	mov SI, CPUVEND
	mov DL, FOREGROUNDCOLOR
	call loaderPrint
	mov SI, CLOSEBRACKETS
	call loaderPrint
	.noCPUID:

	
	;; Do we have MMX?
	cmp dword [MMXEXT], 1
	jne .noMMX
	mov SI, MMX			;; Say we found MMX
	mov DL, FOREGROUNDCOLOR
	call loaderPrint
	.noMMX:	
	
	call loaderPrintNewline

	popa
	ret

	
printMemoryInfo:
	;;  Takes no parameters and prints out the amount of memory detected

	pusha
	
	mov DL, 02h		; Use green color
	mov SI, HAPPY
	call loaderPrint
	mov SI, MEMDETECT1
	call loaderPrint

	mov EAX, dword [EXTENDEDMEMORY]
	call loaderPrintNumber
	mov SI, KREPORTED
	mov DL, FOREGROUNDCOLOR
	call loaderPrint
	call loaderPrintNewline

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

	mov DL, FOREGROUNDCOLOR
	mov SI, HEADS
	call loaderPrint
				
	mov EAX, dword [(EBX + 8)]
	call loaderPrintNumber

	mov DL, FOREGROUNDCOLOR
	mov SI, TRACKS
	call loaderPrint
				
	mov EAX, dword [(EBX + 12)]
	call loaderPrintNumber

	mov DL, FOREGROUNDCOLOR
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

	mov DL, FOREGROUNDCOLOR
	mov SI, HEADS
	call loaderPrint
				
	mov EAX, dword [(EBX + 4)]
	call loaderPrintNumber

	mov DL, FOREGROUNDCOLOR
	mov SI, CYLS
	call loaderPrint
				
	mov EAX, dword [(EBX + 8)]
	call loaderPrintNumber

	mov DL, FOREGROUNDCOLOR
	mov SI, SECTS
	call loaderPrint

	mov EAX, dword [(EBX + 16)]
	call loaderPrintNumber
	
	mov DL, FOREGROUNDCOLOR
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
	VIDEOMODE	dd 0 	;; Mode
	VIDEOLFB	dd 0 	;; Address
	VIDEOX		dd 0	;; maximum X resolution
	VIDEOY		dd 0	;; maximum Y resolution
	VIDEOBPP	dd 0	;; maximum bits per pixel
	BOOTDEVICE	dd 0	;; BIOS boot device number
	BOOTSECT	dd 0	;; Booted sector
	BOOTDISK        db 0, 0, 0, 0  ;; Boot disk string 
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
	HDD0SECPERCYL	dd 0	;; Sectors per cylinder, disk 0
	HDD0SECSIZE	dd 0	;; Bytes per sector, disk 0
	HDD0TOTALSECS	dd 0	;; Total sectors, disk 0
	;; Disk 1
	HDD1HEADS	dd 0	;; Number of heads, disk 1
	HDD1CYLS	dd 0	;; Number of cylinders, disk 1
	HDD1SECPERCYL	dd 0	;; Sectors per cylinder, disk 1
	HDD1SECSIZE	dd 0	;; Bytes per sector, disk 1
	HDD1TOTALSECS	dd 0	;; Total sectors, disk 1
	;; Disk 2
	HDD2HEADS	dd 0	;; Number of heads, disk 2
	HDD2CYLS	dd 0	;; Number of cylinders, disk 2
	HDD2SECPERCYL	dd 0	;; Sectors per cylinder, disk 2
	HDD2SECSIZE	dd 0	;; Bytes per sector, disk 2
	HDD2TOTALSECS	dd 0	;; Total sectors, disk 2
	;; Disk 3
	HDD3HEADS	dd 0	;; Number of heads, disk 3
	HDD3CYLS	dd 0	;; Number of cylinders, disk 3
	HDD3SECPERCYL	dd 0	;; Sectors per cylinder, disk 3
	HDD3SECSIZE	dd 0	;; Bytes per sector, disk 3
	HDD3TOTALSECS	dd 0	;; Total sectors, disk 3
	;; Info about the serial ports
	SERIAL1		dd 0	;; Port address
	SERIAL2		dd 0	;; Port address
	SERIAL3		dd 0	;; Port address
	SERIAL4		dd 0	;; Port address
	;; Info about mouses
	MOUSEPORT	dd 0	;; Port address
	MOUSETYPE	dd 0	;; ID byte 
	
;; 
;; These are general messages related to hardware detection
;;

EMULATION	db 0
HAPPY		db 01h, ' ', 0
BLANK		db '               ', 10h, ' ', 0
PROCESSOR	db 'Processor    ', 10h, ' ', 0
CPUPPRO		db 'Pentium Pro or better ("', 0
CPUPENTIUM	db 'Pentium ("', 0
CPU486		db 'i486', 0
CPU386		db 'i386 (or lower)', 0
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
NOGRAPHICS	db 'NOGRAPH    ', 0

EMUL_SAVE	times 20 db 0
	
;;
;; These are error messages related to hardware detection
;;

SAD		db 'x ', 0
CPUCHECKBAD	db ': This processor is not adequate to run Visopsys.  Visopsys', 0
CPUCHECKBAD2	db 'requires an i486 or better processor in order to function', 0
CPUCHECKBAD3	db 'properly.  Please see your computer dealer.', 0
