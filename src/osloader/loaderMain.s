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
;;  loaderMain.s
;;

	EXTERN loaderSetTextDisplay
	EXTERN loaderCalcVolInfo
	EXTERN loaderFindFile
	EXTERN loaderDetectHardware
	EXTERN loaderLoadKernel
	EXTERN loaderSetGraphicDisplay
	EXTERN loaderPrint
	EXTERN loaderPrintNewline
	EXTERN loaderPrintNumber
	EXTERN PARTENTRY
	EXTERN HARDWAREINFO
	EXTERN KERNELGMODE
	EXTERN HDDINFO

	GLOBAL loaderMain
	GLOBAL loaderMemCopy
	GLOBAL loaderMemSet
	GLOBAL KERNELSIZE
	GLOBAL BYTESPERSECT
	GLOBAL ROOTDIRENTS
	GLOBAL FSTYPE
	GLOBAL RESSECS
	GLOBAL FATSECS
	GLOBAL SECPERTRACK
	GLOBAL HEADS
	GLOBAL SECPERCLUST
	GLOBAL ROOTDIRCLUST
	GLOBAL DRIVENUMBER
	GLOBAL FATALERROR
	GLOBAL PRINTINFO

	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"

	
loaderMain:	
	;; This is the main OS loader driver code.  It calls a number
	;; of other routines for the puposes of detecting hardware,
	;; loading the kernel, etc.  After everything else is done, it
	;; switches the processor to protected mode and starts the kernel.

	cli
	
	;; Make sure all of the data segment registers point to the same
	;; segment as the code segment
	mov EAX, (LDRCODESEGMENTLOCATION / 16)
	mov DS, EAX
	mov ES, EAX
	mov FS, EAX
	mov GS, EAX

	;; Now ensure the stack segment and stack pointer are set to 
	;; something more appropriate for the loader
	mov EAX, (LDRSTCKSEGMENTLOCATION / 16)
	mov SS, EAX
	mov SP, LDRSTCKBASE

	sti

	;; The boot sector is loaded at location 7C00h and starts with
	;; some info about the filesystem.  Grab the info we need and store
	;; it in some more convenient locations
	
	push FS
	xor AX, AX
	mov FS, AX
	
	mov AL, byte [FS:7C0Dh]
	mov word [SECPERCLUST], AX
	mov AL, byte [FS:7C10h]
	mov word [FATS], AX
	mov AL, byte [FS:7C24h]
	mov word [DRIVENUMBER], AX
	mov AX, word [FS:7C0Bh]
	mov word [BYTESPERSECT], AX
	mov AX, word [FS:7C0Eh]
	mov word [RESSECS], AX
	mov AX, word [FS:7C11h]
	mov word [ROOTDIRENTS], AX
	mov AX, word [FS:7C16h]
	mov word [FATSECS], AX

	;; Determine the type of FAT filesystem just based on the FSType
	;; field.  Not reliable, but it's what we do anyway.
	mov EAX, dword [FS:7C37h]
	cmp EAX, 0x32315441		; ('AT12')
	jne .checkFat16
	mov word [FSTYPE], FS_FAT12
	jmp .doneFS
	.checkFat16:
	cmp EAX, 0x36315441		; ('AT16')
	jne .checkFat32
	mov word [FSTYPE], FS_FAT16
	jmp .doneFS
	.checkFat32:
	mov EAX, dword [FS:7C53h]
	cmp EAX, 0x32335441		; ('AT32')
	jne .unknown
	mov word [FSTYPE], FS_FAT32
	;; With FAT32, some of the values are in different places
	mov EAX, dword [FS:7C24h]
	mov dword [FATSECS], EAX
	mov EAX, dword [FS:7C2Ch]
	mov dword [ROOTDIRCLUST], EAX
	mov AL, byte [FS:7C40h]
	mov word [DRIVENUMBER], AX
	jmp .doneFS
	.unknown:
	mov word [FSTYPE], FS_UNKNOWN
	
	.doneFS:
	pop FS

	;; If we are not booting from a floppy, then the boot sector code
	;; should have put a pointer to the MBR record for this partition
	;; in SI.  Copy the entry.
	cmp word [DRIVENUMBER], 80h
	jb .notHDD
	push DS
	push 0
	pop DS
	mov CX, 16
	mov DI, PARTENTRY
	rep movsb
	pop DS
	.notHDD:
	
	;; Initialize the 'fatal error' flag
	mov byte [FATALERROR], 00h

	;; Set the text display to a good mode, clearing the screen
	push word 0
	call loaderSetTextDisplay
	add SP, 2
	
	;; Print the 'Visopsys loader' messages
	mov SI, LOADMSG1
	mov DL, FOREGROUNDCOLOR
	call loaderPrint
	call loaderPrintNewline
	mov SI, LOADMSG2
	call loaderPrint
	call loaderPrintNewline
	call loaderPrintNewline

	;; Gather information about the boot device
	call bootDevice

	;; Calculate values that will help us deal with the filesystem
	;; volume correctly
	call loaderCalcVolInfo
	
	;; Before we print any other info, determine whether the user wants
	;; to see any hardware info messages.  If the BOOTINFO file exists,
	;; then we print the messages
        push word BOOTINFO
        call loaderFindFile
        add SP, 2
	mov word [PRINTINFO], AX

	;; Print out the boot device information
	cmp word [PRINTINFO], 1
	jne .noPrint1
	call printBootDevice
	.noPrint1:	

	;; Set up the GDT (Global Descriptor Table) for "big real mode"
	;; and protected mode
	call gdtSetup

	;; Enable the A20 address line so that we will have access to the
	;; entire extended memory space
	call enableA20

	;; Call the routine to do the hardware detection
	call loaderDetectHardware

	;; Do a fatal error check before loading
	call fatalErrorCheck

	call loaderPrintNewline
	mov SI, LOADING
	mov DL, FOREGROUNDCOLOR
	call loaderPrint
	call loaderPrintNewline

	;; Load the kernel
	call loaderLoadKernel

	;; Make sure the kernel load was successful
	cmp AX, 0
	jge .okLoad
	
	add byte [FATALERROR], 1
	
	.okLoad:
	;; Check for fatal errors before attempting to start the kernel
	call fatalErrorCheck

	;; Did we find a good graphics mode?
	cmp word [KERNELGMODE], 0
	je .noGraphics

	;; Get the graphics mode for the kernel and switch to it
	push word [KERNELGMODE]
	call loaderSetGraphicDisplay
	add SP, 2

	.noGraphics:
	;; Disable the cursor
	mov CX, 2000h
	mov AH, 01h
	int 10h
	
	;; Disable interrupts
	cli

	;; Here's the big moment.  Switch permanently to protected mode.
	mov EAX, CR0
	or AL, 01h
	mov CR0, EAX

	BITS 32

	;; Set EIP to protected mode values by spoofing a far jump
	db 0EAh
	dw .returnLabel, LDRCODESELECTOR
	.returnLabel:	
	
	;; Now enable very basic paging
	call pagingSetup
	
	;; Make the data and stack segment registers contain correct
	;; values for the kernel in protected mode

	;; First the data registers (all point to the whole memory as data)
	mov EAX, PRIV_DATASELECTOR
	mov DS, EAX
	mov ES, EAX
	mov FS, EAX
	mov GS, EAX

	;; Now the stack registers
	mov EAX, PRIV_STCKSELECTOR
	mov SS, AX
	mov EAX, KERNELVIRTUALADDRESS
	add EAX, dword [(LDRCODESEGMENTLOCATION + KERNELSIZE)]
	add EAX, (KERNELSTACKSIZE - 4)
	mov ESP, EAX
	
	;; Pass the kernel arguments.  
	
	;; First the hardware structure
	push dword (LDRCODESEGMENTLOCATION + HARDWAREINFO)

	;; Next the amount of used kernel memory.  We need to add the
	;; size of the stack we allocated to the kernel image size
	mov EAX, dword [(LDRCODESEGMENTLOCATION + KERNELSIZE)]
	add EAX, KERNELSTACKSIZE
	push EAX

	;; Waste some space on the stack that would normally be used to
	;; store the return address in a call.  This will ensure that the
	;; kernel's arguments are located where gcc thinks they should be
	;; for a "normal" function call.  We will push a NULL return address
	push dword 0
	
	;; (The kernel's initial state will be with interrupts disabled.
	;; It will have to do the appropriate setup before reenabling
	;; them.)

	;; Start the kernel.  Set the CS and IP in the process.  Simulate
	;; a "call" stack-wise so that the kernel can get its parameters.
	jmp PRIV_CODESELECTOR:KERNELVIRTUALADDRESS

	BITS 16

	;;--------------------------------------------------------------

	
fatalErrorCheck:
	xor AX, AX
	mov AL, byte [FATALERROR]
	cmp AX, 0000h

	jne .errors
	ret

	.errors:
	call loaderPrintNewline

	;; Print the fact that fatal errors were detected,
	;; and stop
	xor EAX, EAX
	mov AL, byte [FATALERROR]
	call loaderPrintNumber

	mov SI, FATALERROR1
	mov DL, FOREGROUNDCOLOR

	call loaderPrint
	call loaderPrintNewline

	mov SI, FATALERROR2
	mov DL, FOREGROUNDCOLOR

	call loaderPrint
	call loaderPrintNewline

	mov SI, FATALERROR3
	mov DL, FOREGROUNDCOLOR
	call loaderPrint
	call loaderPrintNewline
	
	;; Print the message indicating system halt/reboot
	mov SI, PRESSREBOOT
	mov DL, FOREGROUNDCOLOR
	call loaderPrint

	mov AX, 0000h
	int 16h

	mov SI, REBOOTING
	mov DL, FOREGROUNDCOLOR
	call loaderPrint

	;; Write the reset command to the keyboard controller
	mov AL, 0FEh
	out 64h, AL
	jecxz $+2
	jecxz $+2

	;; Done.  The computer is now rebooting.

	;; Just in case.  Should never get here.
	hlt


bootDevice:
	;; This function will gather some needed information about the
	;; boot device (and print messages about what it finds)

	pusha

	;; Gather some more disk information directly from the
	;; BIOS using interrupt 13, subfunction 8

	;; This interrupt call will destroy ES, so save it
	push ES

	;; Guards againt BIOS bugs, apparently
	push word 0
	pop ES
	xor DI, DI
	
	mov AX, 0800h
	xor BX, BX
	xor CX, CX
	mov DX, word [DRIVENUMBER]
	int 13h

	;; Restore ES
	pop ES

	jnc .gotDiskInfo

	;; Ooops, the BIOS isn't helping us...
	;; Print out the fatal error message

	;; Change to the error color
	mov DL, ERRORCOLOR
	
	mov SI, SAD
	call loaderPrint
	mov SI, BOOTDEV
	call loaderPrint
	mov SI, BIOSERR
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, BIOSERR2
	call loaderPrint
	call loaderPrintNewline

	;; We can't continue with any disk-related stuff, so we can't load
	;; the kernel.
	add  byte [FATALERROR], 1
	jmp .done

	.gotDiskInfo:

	;; Heads
	xor EAX, EAX
	mov AL, DH
	inc AX			; Number is 0-based
	mov dword [HEADS], EAX

	;; cylinders
	xor EAX, EAX
	mov AL, CL		; Two bits of cylinder number in bits 6&7
	and AL, 11000000b	; Mask it
	shl AX, 2		; Move them to bits 8&9
	mov AL, CH		; Rest of the cylinder bits
	inc AX			; Number is 0-based
	mov dword [CYLINDERS], EAX
	
	;; sectors
	xor EAX, EAX
	mov AL, CL		; Bits 0-5
	and AL, 00111111b	; Mask it
	mov dword [SECPERTRACK], EAX
	
	;; Determine whether we can use an extended BIOS function to give
	;; us the number of sectors

	mov word [HDDINFO], 42h  ; Size of the info buffer we provide
	mov AX, 4800h
	mov DX, word [DRIVENUMBER]
	mov SI, HDDINFO
	int 13h
	
	;; Function call successful?
	jc .done

	;; Save the number of sectors
	mov EAX, dword [HDDINFO + 10h]
	mov dword [TOTALSECS], EAX

	;; Recalculate the number of cylinders
	mov EAX, dword [HEADS]		; heads
	mul dword [SECPERTRACK]		; sectors per cyl
	mov ECX, EAX			; total secs per cyl 
	xor EDX, EDX
	mov EAX, dword [TOTALSECS]
	div ECX
	mov dword [CYLINDERS], EAX	; new cyls value

	.done:
	popa
	ret


printBootDevice:
	;; Prints info about the boot device

	pusha
	
	mov DL, 02h		; Use green color
	mov SI, HAPPY
	call loaderPrint
	mov SI, BOOTDEV
	call loaderPrint

	mov DL, FOREGROUNDCOLOR	; Switch to foreground color
	mov SI, DEVDISK		
	call loaderPrint
	xor EAX, EAX
	mov AX, word [DRIVENUMBER]
	cmp AX, 80h
	jb .noSub
	sub AX, 80h
	.noSub:
	call loaderPrintNumber
	mov SI, DEVDISK2
	call loaderPrint
	
	mov EAX, dword [HEADS]
	call loaderPrintNumber
	mov SI, DEVHEADS
	call loaderPrint
	
	mov EAX, dword [CYLINDERS]
	call loaderPrintNumber
	mov SI, DEVCYLS
	call loaderPrint
	
	mov EAX, dword [SECPERTRACK]
	call loaderPrintNumber
	mov SI, DEVSECTS
	call loaderPrint

	;; Print the Filesystem type
	mov AX, word [FSTYPE]
	cmp AX, FS_FAT12
	je .fat12
	cmp AX, FS_FAT16
	je .fat16
	cmp AX, FS_FAT32
	je .fat32

	;; Fall through for UNKNOWN
	mov SI, UNKNOWNFS
	mov DL, FOREGROUNDCOLOR
	call loaderPrint
	call loaderPrintNewline
	jmp .done
	
	.fat12:
	;; Print FAT12
	mov SI, FAT12MES
	mov DL, FOREGROUNDCOLOR
	call loaderPrint
	call loaderPrintNewline
	jmp .done

	.fat16:
	;; Print FAT16
	mov SI, FAT16MES
	mov DL, FOREGROUNDCOLOR
	call loaderPrint
	call loaderPrintNewline
	jmp .done

	.fat32:
	;; Print FAT32
	mov SI, FAT32MES
	mov DL, FOREGROUNDCOLOR
	call loaderPrint
	call loaderPrintNewline

	.done:	
	popa
	ret
		

enableA20:
	;; This routine will enable the A20 address line in the
	;; keyboard controller.  Takes no arguments.

	pusha
	
	;; Make sure interrupts are disabled
	cli

	;; Keep a counter so that we can make multiple attempts to turn
	;; on A20 if necessary
	mov CX, 5

	.startAttempt:		
	;; Wait for the controller to be ready for a command
	.commandWait1:
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc .commandWait1

	;; Tell the controller we want to read the current status.
	;; Send the command D0h: read output port.
	mov AL, 0D0h
	out 64h, AL

	;; Delay
	jcxz $+2
	jcxz $+2

	;; Wait for the controller to be ready with a byte of data
	.dataWait1:
	xor AX, AX
	in AL, 64h
	bt AX, 0
	jnc .dataWait1

	;; Read the current port status from port 60h
	xor AX, AX
	in AL, 60h

	;; Check to see whether A20 is already enabled.  Seems to be true on
	;; a number of machines.
	bt AX, 1
	jc near .done
	
	;; Save the current value of EAX
	push AX
		
	;; Wait for the controller to be ready for a command
	.commandWait2:
	in AL, 64h
	bt AX, 1
	jc .commandWait2

	;; Tell the controller we want to write the status byte again
	mov AL, 0D1h
	out 64h, AL	

	;; Delay
	jcxz $+2
	jcxz $+2

	;; Wait for the controller to be ready for the data
	.commandWait3:
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc .commandWait3

	;; Write the new value to port 60h.  Remember we saved the old
	;; value on the stack
	pop AX
	;; Turn on the A20 enable bit
	or AL, 00000010b
	out 60h, AL

	;; Delay
	jcxz $+2
	jcxz $+2

	;; Finally, we will attempt to read back the A20 status
	;; to ensure it was enabled.

	;; Wait for the controller to be ready for a command
	.commandWait4:
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc .commandWait4

	;; Send the command D0h: read output port.
	mov AL, 0D0h
	out 64h, AL	

	;; Wait for the controller to be ready with a byte of data
	.dataWait2:
	xor AX, AX
	in AL, 64h
	bt AX, 0
	jnc .dataWait2

	;; Read the current port status from port 60h
	xor AX, AX
	in AL, 60h

	;; Is A20 enabled?
	bt AX, 1
		
	;; Check the result.  If carry is on, A20 is on.
	jc near .done

	;; Should we retry the operation?  If the counter value in ECX
	;; has not reached zero, we will retry
	dec CX
	cmp CX, 0
	jne near .startAttempt

	;; Well, our initial attempt to set A20 has failed.  Now we will
	;; try a backup method (which is supposedly not supported on many
	;; chipsets).
		
	;; Keep a counter so that we can make multiple attempts to turn
	;; on A20 if necessary
	mov CX, 5

	.startAttempt2:
	;; Wait for the keyboard to be ready for another command
	.commandWait6:
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc .commandWait6

	;; Tell the controller we want to turn on A20
	mov AL, 0DFh
	out 64h, AL

	;; Delay
	jcxz $+2
	jcxz $+2

	;; Again, we will attempt to read back the A20 status
	;; to ensure it was enabled.

	;; Wait for the controller to be ready for a command
	.commandWait7:
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc .commandWait7

	;; Send the command D0h: read output port.
	mov AL, 0D0h
	out 64h, AL	

	;; Wait for the controller to be ready with a byte of data
	.dataWait3:
	xor AX, AX
	in AL, 64h
	bt AX, 0
	jnc .dataWait3

	;; Read the current port status from port 60h
	xor AX, AX
	in AL, 60h

	;; Is A20 enabled?
	bt AX, 1
		
	;; Check the result.  If carry is on, A20 is on, but we should warn
	;; that we had to use this alternate method
	jc .warn

	;; Should we retry the operation?  If the counter value in ECX
	;; has not reached zero, we will retry
	dec CX
	cmp CX, 0
	jne near .startAttempt2
	
	;; OK, we weren't able to set the A20 address line, so we'll
	;; not be able to access much memory.  We can give a fairly
	;; helpful error message, however, because in my experience,
	;; this tends to happen when laptops have external keyboards
	;; attached

	;; Print an error message, make a fatal error, and finish
	
	;; Switch to the error color
	mov DL, ERRORCOLOR
	
	mov SI, SAD
	call loaderPrint
	mov SI, A20
	call loaderPrint
	mov SI, A20BAD1
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, A20BAD2
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, A20BAD3
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, A20BAD4
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, A20BAD5
	call loaderPrint
	call loaderPrintNewline

	add byte [FATALERROR], 1
	jmp .done

	.warn:
	;; Here we print a warning about the fact that we had to use the
	;; alternate enabling method
	mov DL, 02h		; Use green color
	mov SI, HAPPY
	call loaderPrint
	mov SI, A20
	call loaderPrint
	mov DL, FOREGROUNDCOLOR	; Switch to foreground color
	mov SI, A20WARN
	call loaderPrint
	call loaderPrintNewline
			
	.done:
	sti
	popa
	ret
	
	
gdtSetup:
	;; This function installs a temporary GDT (Global Descriptor
	;; Table) that is used in protected mode (and big real mode)
	;; until the kernel replaces it later with a permanent version

	pusha

	;; Make the correct address values appear in the GDT selectors

	;; The PRIV_CODESELECTOR, PRIV_DATASELECTOR and PRIV_STCKSELECTORs are 
	;; already correct.

	;; The loader's code segment descriptor
	mov EAX, LDRCODESEGMENTLOCATION
	mov word [(ldrcode_desc + 2)], AX
	shr EAX, 16
	mov byte [(ldrcode_desc + 4)], AL
	mov byte [(ldrcode_desc + 7)], AH

	;; Load the address of the GDT into the GDTR
	;; First move the data to the memory location
	mov AX, CS
	and EAX, 0000FFFFh
	shl EAX, 4      	;; Make it a 32-bit address
	add EAX, dummy_desc
	mov dword [(GDTSTART + 2)], EAX

	lgdt [GDTSTART]

	popa
	ret


loaderMemCopy:
	;; Tries to use real mode interrupt 15h:87h to move data in extended
	;; memory.  If that doesn't work it tries a 'big real mode' method.
	;; Proto:
	;;   void loaderMemCopy(dword *src, dword *dest, dword size);
	
	pusha

	;; Save the stack pointer
	mov BP, SP

	push DS
	push ES

;;	mov EAX, CS
;;	mov DS, EAX
;;	mov ES, EAX
;;	
;;	;; Set up the temporary GDT
;;	
;;	;; Source descriptor
;;	mov EBX, dword [SS:(BP + 18)]		; Source address
;;	mov word [TMPGDT.srclow], BX		; Source address low word
;;	shr EBX, 16
;;	mov byte [TMPGDT.srcmid], BL		; Source address 3rd byte
;;	mov byte [TMPGDT.srchi], BH		; Source address 4th byte
;;
;;	;; Destination descriptor
;;	mov EBX, dword [SS:(BP + 22)]		; Dest address
;;	mov word [TMPGDT.destlow], BX		; Dest address low word
;;	shr EBX, 16
;;	mov byte [TMPGDT.destmid], BL		; Dest address 3rd byte
;;	mov byte [TMPGDT.desthi], BH		; Dest address 4th byte
;;
;;	mov AX, 8700h
;;	mov ECX, dword [SS:(BP + 26)]	; Size in bytes
;;	shr ECX, 1			; Size is in words
;;	mov SI, TMPGDT
;;	int 15h
;;	
;;	jc .fail
;;	cmp AH, 0
;;	jnz .fail
;;	
;;	pop ES
;;	pop DS
;;	jmp .out
;;	

	.fail:
	;; That method didn't work.  Do it manually.

	mov ESI, dword [SS:(BP + 18)]	; Source address
	mov EDI, dword [SS:(BP + 22)]	; Dest address
	mov ECX, dword [SS:(BP + 26)]	; Size in bytes
	;; shr ECX, 2			; Divide by 4
	
	;; Disable interrupts
	cli

	;; Switch to protected mode temporarily
	mov EAX, CR0
	or AL, 01h
	mov CR0, EAX

	BITS 32
	
	;; Load DS and ES with the global data segment selector
	mov EAX, dword PRIV_DATASELECTOR
	mov DS, EAX
	mov ES, EAX
	
	;; Return to real mode
	mov EAX, CR0
	and AL, 0FEh
	mov CR0, EAX

	BITS 16
	
	cld				; Clear direction flag
	a32 rep movsb			; Do the copy
	
	pop ES
	pop DS
	
	;; Reenable interrupts
	sti

	.out:
	popa
	ret
	

loaderMemSet:
	;; Uses a 'big real mode' method for initializing a memory region.
	;; Proto:
	;;   void loaderMemSet(byte value, dword *dest, dword size);
	
	pusha

	;; Save the stack pointer
	mov BP, SP

	push DS
	push ES

	;; Disable interrupts
	cli

	;; Switch to protected mode temporarily
	mov EAX, CR0
	or AL, 01h
	mov CR0, EAX

	BITS 32
	
	;; Load DS and ES with the global data segment selector
	mov EAX, dword PRIV_DATASELECTOR
	mov DS, EAX
	mov ES, EAX
	
	;; Return to real mode
	mov EAX, CR0
	and AL, 0FEh
	mov CR0, EAX

	BITS 16

	mov EAX, dword [SS:(BP + 18)]	; Value
	mov EDI, dword [SS:(BP + 20)]	; Dest address
	mov ECX, dword [SS:(BP + 24)]	; Size in bytes
	
	cld				; Clear direction flag
	a32 rep stosb			; Do the copy
	
	pop ES
	pop DS
	
	;; Reenable interrupts
	sti

	.out:
	popa
	ret
	

pagingSetup:
	;; This will setup a simple paging environment for the kernel and
	;; enable it.  This involves making a master page directory plus
	;; one page table, and enabling paging.  The kernel can make its own
	;; tables at startup, so this only needs to be temporary.  This 
	;; function takes no arguments and returns nothing.  Called only
	;; after protected mode has been entered.
	
	BITS 32
	
	pusha

	;; Interrupts should already be disabled

	;; Make sure ES has the selector that points to the whole memory
	;; space
	mov EAX, PRIV_DATASELECTOR
	mov ES, AX
		
	;; Create a page table to identity-map the first 4 megabytes of
	;; the system's memory.  This is so that the loader can operate
	;; normally after paging has been enabled.  This is 1024 entries, 
	;; each one representing 4Kb of real memory.  We will start the table
	;; at the address (LDRPAGINGDATA + 1000h)
	
	mov EBX, 0		; Location we're mapping
	mov ECX, 1024		; 1024 entries
	mov EDI, (LDRPAGINGDATA + 1000h)
	
	.entryLoop1:
	;; Make one page table entry.
	mov EAX, EBX
	and AX, 0F000h		; Clear bits 0-11, just in case
	;; Set the entry's page present bit, the writable bit, and the
	;; write-through bit.
	or AL, 00001011b
	stosd			; Store the page table entry at ES:EDI
	add EBX, 1000h		; Move to next 4Kb
	loop .entryLoop1

	;; Create a page table to represent the virtual address space that
	;; the kernel's code will occupy.  Start this table at address
	;; (PAGINGDATA + 2000h)

	mov EBX, KERNELCODEDATALOCATION		; Location we're mapping
	mov ECX, 1024				; 1024 entries
	mov EDI, (LDRPAGINGDATA + 2000h)	; location in LDRPAGINGDATA
	
	.entryLoop2:
	;; Make one page table entry.
	mov EAX, EBX
	and AX, 0F000h		; Clear bits 0-11, just in case
	;; Set the entry's page present bit, the writable bit, and the
	;; write-through bit.
	or AL, 00001011b
	stosd			; Store the page table entry at ES:EDI
	add EBX, 1000h		; Move to next 4Kb
	loop .entryLoop2

	;; We will create a master page directory with two entries
	;; representing the page tables we created.  The master page
	;; directory will start at PAGINGDATA

	;; Make sure there are NULL entries throughout the table
	;; to start
	xor EAX, EAX
	mov ECX, 1024
	mov EDI, LDRPAGINGDATA
	rep stosd

	;; The first entry we need to create in this table represents
	;; the first page table we made, which identity-maps the first
	;; 4 megs of address space.  This will be the first entry in our
	;; new table.
	;; The address of the first table
	mov EAX, (LDRPAGINGDATA + 1000h)
	and AX, 0F000h			; Clear bits 0-11, just in case
	;; Set the entry's page present bit, the writable bit, and the
	;; write-through bit.
	or AL, 00001011b
	;; Put it in the first entry
	mov EDI, LDRPAGINGDATA
	stosd
	
	;; We make the second entry based on the virtual address of the
	;; kernel.
	;; The address of the second table
	mov EAX, (LDRPAGINGDATA + 2000h)
	and AX, 0F000h			; Clear bits 0-11, just in case
	;; Set the entry's page present bit, the writable bit, and the
	;; write-through bit.
	or AL, 00001011b
	mov EDI, KERNELVIRTUALADDRESS	; Kernel's virtual address
	;; We shift right by 22, to make it a multiple of 4 megs, but then
	;; we shift it right again by 2, since the offsets of entries in the
	;; table are multiples of 4 bytes
	shr EDI, 20
	;; Add the offset of the table
	add EDI, LDRPAGINGDATA
	stosd
	
	;; Move the base address of the master page directory into CR3
	xor EAX, EAX		; CR3 supposed to be zeroed
	or AL, 00001000b	; Set the page write-through bit
	;; The address of the directory
	mov EBX, LDRPAGINGDATA
	and EBX, 0FFFFF800h	; Clear bits 0-10, just in case
	or EAX, EBX		; Combine them into the new CR3
	mov CR3, EAX

	;; Make sure caching is not globally disabled
	mov EAX, CR0
	and EAX, 9FFFFFFFh	; Clear CD (30) and NW (29)
	mov CR0, EAX
	
	;; Clear out the page cache before we turn on paging, since if
	;; we don't, rebooting from Windows or other OSes can cause us to
	;; crash
	wbinvd
	invd
		
	;; Here we go.  Turn on paging in the processor.
	or EAX, 80000000h
	mov CR0, EAX

	;; Supposed to do a near jump after all that
	jmp near .pagingOn
	nop

	.pagingOn:
	;; Enable 'global' pages for processors that support it
	mov EAX, CR4
	or EAX, 00000080h
	mov CR4, EAX
		
	.done:	
	;; Done
	popa
	ret

	BITS 16
	

;;
;; The data segment
;;

	SEGMENT .data
	ALIGN 4
	
;;
;; Things generated internally
;;
	
KERNELSIZE	dd 0
;; This records the number of fatal errors recorded
PRINTINFO	dw 0	;; Show hardware information messages?
FATALERROR	db 0 	;; Fatal error encountered?

	
;;
;; Info about our boot device and filesystem.
;;
	ALIGN 4

BYTESPERSECT	dw 0
ROOTDIRENTS	dw 0
ROOTDIRCLUST	dd 0
FSTYPE		dw 0
RESSECS		dw 0
FATSECS		dd 0
TOTALSECS	dd 0
CYLINDERS	dd 0
HEADS		dd 0
SECPERTRACK	dd 0
SECPERCLUST	dw 0
FATS		dw 0
DRIVENUMBER	dw 0
	

;;
;; Tables, desciptors, etc., used for protected mode
;;
	ALIGN 4

GDTSTART	dw GDTLENGTH
		dd 0			;; modified in the code
dummy_desc	dd 0 			;; The empty first descriptor
		dd 0
allcode_desc	dw 0FFFFh
		dw 0			
		db 0			
		db PRIV_CODEINFO1
		db PRIV_CODEINFO2
		db 0			
alldata_desc	dw 0FFFFh
		dw 0			
		db 0			
		db PRIV_DATAINFO1
		db PRIV_DATAINFO2
		db 0			
allstck_desc	dw 0FFFFh
		dw 0			
		db 0			
		db PRIV_STCKINFO1
		db PRIV_STCKINFO2
		db 0			
ldrcode_desc	dw LDRCODESEGMENTSIZE
		dw 0			;; modified in the code
		db 0			;; modified in the code
		db LDRCODEINFO1
		db LDRCODEINFO2
		db 0			;; modified in the code
GDTLENGTH	equ $-dummy_desc

	ALIGN 4

;;TMPGDT		times 8 db 0	; empty (used by BIOS)
;;			times 8 db 0	; empty (used by BIOS)
;;			dw 0FFFFh	; source segment length in bytes
;;	TMPGDT.srclow	dw 0		; low word of linear source address
;;	TMPGDT.srcmid	db 0		; middle byte of linear source address
;;			db 93h		; source segment access rights
;;			db 0		; source extended access rights
;;	
;;	TMPGDT.srchi	db 0		; high byte of source address
;;			dw 0FFFFh	; dest segment length in bytes
;;	TMPGDT.destlow	dw 0		; low word of linear dest address
;;	TMPGDT.destmid	db 0		; middle byte of linear dest address
;;			db 93h		; dest segment access rights
;;			db 0		; dest extended access rights
;;	TMPGDT.desthi	db 0		; high byte of dest address
;;			times 8 db 0	; empty (used by BIOS)
;;			times 8 db 0	; empty (used by BIOS)
	
;;
;; The good/informational messages
;;

HAPPY		db 01h, ' ', 0
BLANK		db '               ', 10h, ' ', 0
LOADMSG1	db 'Visopsys OS Loader v0.64' , 0
LOADMSG2	db 'Copyright (C) 1998-2006 J. Andrew McLaughlin', 0
BOOTDEV		db 'Boot device  ', 10h, ' ', 0
DEVDISK		db 'Disk ', 0
DEVDISK2	db ', ', 0
DEVHEADS	db ' heads, ', 0
DEVCYLS		db ' cyls, ', 0
DEVSECTS	db ' sects, type: ', 0
FAT12MES	db 'FAT12', 0
FAT16MES	db 'FAT16', 0
FAT32MES	db 'FAT32', 0
UNKNOWNFS	db 'UNKNOWN', 0
A20		db 'Gate A20     ', 10h, ' ', 0
A20WARN		db 'Enabled using alternate method.', 0
LOADING		db 'Loading Visopsys', 0
PRESSREBOOT	db 'Press any key to reboot.', 0
REBOOTING	db '  ...Rebooting', 0
BOOTINFO	db 'BOOTINFO   ', 0

;;
;; The error messages
;;

SAD		db 'x ', 0
BIOSERR		db 'The computer', 27h, 's BIOS was unable to provide information about', 0
BIOSERR2	db 'the boot device.  Please check the BIOS for errors.', 0
A20BAD1		db 'Could not enable the A20 address line, which would cause', 0
A20BAD2		db 'serious memory problems for the kernel.  As strange as it may', 0
A20BAD3		db 'sound, this is generally associated with keyboard errors. ', 0
A20BAD4		db 'If you are using a laptop computer with an external keyboard,', 0
A20BAD5		db 'please consider removing it before retrying.', 0
FATALERROR1	db ' unrecoverable error(s) were recorded, and the boot process cannot continue.', 0
FATALERROR2	db 'Any applicable error information is noted above.  Please attempt to rectify', 0
FATALERROR3	db 'these problems before retrying.', 0
