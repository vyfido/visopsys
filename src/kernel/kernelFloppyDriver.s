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
;;  kernelFloppyDriver.s
;;

	SEGMENT .text
	BITS 32

	GLOBAL kernelFloppyDriverInitialize
	GLOBAL kernelFloppyDriverDescribe
	GLOBAL kernelFloppyDriverReset
	GLOBAL kernelFloppyDriverMotorOn
	GLOBAL kernelFloppyDriverMotorOff
	GLOBAL kernelFloppyDriverRecalibrate
	GLOBAL kernelFloppyDriverDiskChanged
	GLOBAL kernelFloppyDriverReadSectors
	GLOBAL kernelFloppyDriverWriteSectors
	GLOBAL kernelFloppyDriverLastErrorCode
	GLOBAL kernelFloppyDriverLastErrorMessage
	GLOBAL kernelFloppyDriverReceiveInterrupt

	%include "kernelAssemblerHeader.h"


specify:
	;; Sends some essential timing information to the floppy drive
	;; controller about the selected disk.
	
	pusha

	;; Construct the data rate byte
	xor EAX, EAX
	mov EDI, DATA_RATE
	mov AL, byte [SELECTEDFLOPPY]
	add EDI, EAX
	mov AL, byte [EDI]
	mov DX, 03F7h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2
	
	xor EBX, EBX
	xor ECX, ECX

	;; BL -> Command byte
	;; CL -> Step rate/head unload time
	;; CH -> Head load time

	;; Construct the command byte
	mov BL, 03h		; Specify command

	;; Construct the step rate/head unload byte
	xor EAX, EAX
	mov EDI, STEP_RATE
	mov AL, byte [SELECTEDFLOPPY]
	add EDI, EAX
	mov AL, byte [EDI]
	shl AL, 4
	mov CL, AL

	xor EAX, EAX
	mov EDI, HEAD_UNLOAD
	mov AL, byte [SELECTEDFLOPPY]
	add EDI, EAX
	mov AL, byte [EDI]
	and AL, 00001111b
	or CL, AL

	;; Construct the head load time byte
	xor EAX, EAX
	mov EDI, HEAD_LOAD
	mov AL, byte [SELECTEDFLOPPY]
	add EDI, EAX
	mov AL, byte [EDI]
	shl AL, 1
	mov CH, AL

	;; Make sure that DMA mode is enabled
	and CH, 11111110b
	
	
	;; Submit the command to the controller

	call waitCommandWrite
		
	mov DX, 03F5h
	mov AL, BL
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	call waitCommandWrite
	
	mov DX, 03F5h
	mov AL, CL
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2
	
	call waitCommandWrite
	
	mov DX, 03F5h
	mov AL, CH
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; There is no status information or interrupt after this command
	
	popa
	mov EAX, 0		; Return success
	ret
	

selectDrive:	
	;; Takes the floppy number to select as its only parameter:
	;; 0 - Disk number

	pusha
	
	;; Save the stack pointer
	mov EBP, ESP

	mov EAX, dword [SS:(EBP + 36)]
	and EAX, 00000003h
	
	;; Save the number of the selected drive
	mov byte [SELECTEDFLOPPY], AL

	;; Select the drive on the controller
	mov DX, 03F2h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Make sure the DMA/Interrupt and reset-off bits are set
	or AL, 00001100b
	
	;; Clear out the selection bits
	and AL, 11111100b
	
	;; Set the selection bits
	or AL, byte [SELECTEDFLOPPY]
	
	;; Issue the command
	mov DX, 03F2h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	popa
	ret


waitCommandWrite:
	;; Returns when the floppy controller is ready for a new command
	;; (or part thereof) in port 03F5h

	pusha

	;; Save the current time so we don't wait too long
	call kernelSysTimerRead
	add EAX, 20
	push dword EAX

	.loopStart:
	
	call kernelSysTimerRead
	cmp EAX, dword [SS:ESP]
	jae .breakLoop
		
	;; Get the drive transfer mode from the port
	mov DX, 03F4h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Check whether access is permitted
	shr AL, 6

	cmp AL, 2
	jne .loopStart
	
	.breakLoop:	
	add ESP, 4
	
	popa
	ret

	
waitStatusRead:
	;; Returns when the controller is ready for a read of port 03F5h

	pusha

	;; Save the current time so we don't wait too long
	call kernelSysTimerRead
	add EAX, 20
	push dword EAX

	.loopStart:

	call kernelSysTimerRead
	cmp EAX, dword [SS:ESP]
	jae .breakLoop
		
	;; Get the drive transfer mode from the port
	mov DX, 03F4h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Check whether access is permitted
	shr AL, 6

	cmp AL, 3
	jne .loopStart

	.breakLoop:	
	add ESP, 4
	
	popa
	ret

	
waitOperationComplete:
	;; This routine just loops, reading the "interrupt received"
	;; byte.  When the byte becomes 1, it resets the byte and returns.
	;; If the wait times out, the function returns -1.  Otherwise, it
	;; returns 0.
	
	pusha

	;; Save the current time so we don't wait too long
	call kernelSysTimerRead
	add EAX, 40
	push dword EAX

	.loopStart:
	call kernelSysTimerRead
	cmp EAX, dword [SS:ESP]
	jae .timeOut

	cmp byte [INTERRUPTRECEIVED], 1
	je .done

	;; Yield the rest of this timeslice if we are in multitasking mode
	call kernelMultitaskerYield
	jmp .loopStart
	
	.done:
	add ESP, 4
	mov byte [INTERRUPTRECEIVED], 0
	popa
	mov EAX, 0
	ret

	.timeOut:
	add ESP, 4
	mov byte [INTERRUPTRECEIVED], 0
	popa
	mov EAX, -1
	ret


seek:
	;; This takes head, and track numbers as parameters.  Seeks the
	;; selected floppy to the designated location after turning 
	;; on the motor.  The parameters look like this on the stack:
	;; 0 - Head
	;; 1 - Track/cylinder
	
	pusha

	;; Save the current stack pointer
	mov EBP, ESP

	;; Tell the interrupt-received routine to issue the
	;; "sense interrupt status" command after the operation
	mov byte [READSTATUSONINTERRUPT], 1
	
	xor EBX, EBX
	xor ECX, ECX

	;; BL -> Command byte
	;; CL -> Drive/head select byte
	;; CH -> Cylinder number byte

	;; Construct the command byte
	mov BL, 0Fh		; Seek command

	;; Construct the drive/head select byte
	;; Format [00000 (head 1 bit)(drive 2 bits)]
	mov EAX, dword [SS:(EBP + 36)]; Head
	and AL, 00000001b
	shl AL, 2
	mov CL, AL
	or CL, byte [SELECTEDFLOPPY]
	
	;; Construct the cylinder number byte
	mov EAX, dword [SS:(EBP + 40)]; Track/cylinder
	mov CH, AL
	
	;; Submit the command to the controller

	call waitCommandWrite
		
	mov DX, 03F5h
	mov AL, BL
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	call waitCommandWrite
	
	mov DX, 03F5h
	mov AL, CL
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2
	
	call waitCommandWrite
	
	mov DX, 03F5h
	mov AL, CH
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	call waitOperationComplete
	
	cmp EAX, 0
	jne .errorDone		; The operation timed out

	;; Check error conditions in the status byte
	mov AL, byte [STATUSREGISTER0]
	and AL, 11111000b
	cmp AL, 00100000b
	jne .errorDone

	;; Make sure that we are now at the correct cylinder
	xor EAX, EAX
	mov AL, byte [CURRENTCYLINDER]
	cmp EAX, dword [SS:(EBP + 40)]; Track/cylinder
	jne .errorDone
	
	;; If we fall through to here, the operation was a success.

	.done:
	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:		
	popa
	mov EAX, -1		; Return failure
	ret

	
evaluateError:
	;; This is an internal-only routine that takes no parameters
	;; and returns no value.  It evaluates the returned bytes in
	;; the STATUSREGISTERX bytes and matches conditions to error
	;; codes and error messages

	pusha

	.E1:
	;; Check for abnormal termination of command
	mov AL, byte [STATUSREGISTER0]
	and AL, 11000000b
	cmp AL, 01000000b
	jne .E2
	mov dword [LASTERRORMESSAGE], E1
	mov byte [LASTERRORCODE], 1
	jmp .done
	
	.E2:
	;; Check for invalid command
	mov AL, byte [STATUSREGISTER0]
	and AL, 11000000b
	cmp AL, 10000000b
	jne .E3
	mov dword [LASTERRORMESSAGE], E2
	mov byte [LASTERRORCODE], 2
	jmp .done
	
	.E3:
	;; Check for Equipment check error
	mov AL, byte [STATUSREGISTER0]
	bt AX, 4		; bit 4 (status register 0)
	jnc .E4
	mov dword [LASTERRORMESSAGE], E3
	mov byte [LASTERRORCODE], 3
	jmp .done
	
	.E4:
	;; Check for end-of-track
	mov AL, byte [STATUSREGISTER1]
	bt AX, 7		; bit 7 (status register 1)
	jnc .E5
	mov dword [LASTERRORMESSAGE], E4
	mov byte [LASTERRORCODE], 4
	jmp .done

	;; Bit 6 is unused in status register 1
	
	.E5:
	;; Check for the first kind of data error
	mov AL, byte [STATUSREGISTER1]
	bt AX, 5		; bit 5 (status register 1)
	jnc .E6
	mov dword [LASTERRORMESSAGE], E5
	mov byte [LASTERRORCODE], 5
	jmp .done

	.E6:
	;; Check for overrun/underrun
	mov AL, byte [STATUSREGISTER1]
	bt AX, 4		; bit 4 (status register 1)
	jnc .E7
	mov dword [LASTERRORMESSAGE], E6
	mov byte [LASTERRORCODE], 6
	jmp .done

	;; Bit 3 is unused in status register 1
	
	.E7:
	;; Check for no data error
	mov AL, byte [STATUSREGISTER1]
	bt AX, 2		; bit 2 (status register 1)
	jnc .E8
	mov dword [LASTERRORMESSAGE], E7
	mov byte [LASTERRORCODE], 7
	jmp .done

	.E8:
	;; Check for write protect error
	mov AL, byte [STATUSREGISTER1]
	bt AX, 1		; bit 1 (status register 1)
	jnc .E9
	mov dword [LASTERRORMESSAGE], E8
	mov byte [LASTERRORCODE], 8
	jmp .done

	.E9:
	;; Check for missing address mark
	mov AL, byte [STATUSREGISTER1]
	bt AX, 0		; bit 0 (status register 1)
	jnc .E10
	mov dword [LASTERRORMESSAGE], E9
	mov byte [LASTERRORCODE], 9
	jmp .done

	;; Bit 7 is unused in status register 2
	
	.E10:
	;; Check for control mark error
	mov AL, byte [STATUSREGISTER2]
	bt AX, 6		; bit 6 (status register 2)
	jnc .E11
	mov dword [LASTERRORMESSAGE], E10
	mov byte [LASTERRORCODE], 10
	jmp .done

	.E11:
	;; Check for the second kind of data error
	mov AL, byte [STATUSREGISTER2]
	bt AX, 5		; bit 5 (status register 2)
	jnc .E12
	mov dword [LASTERRORMESSAGE], E11
	mov byte [LASTERRORCODE], 11
	jmp .done

	.E12:
	;; Check for invalid track / wrong cylinder
	mov AL, byte [STATUSREGISTER2]
	bt AX, 4		; bit 4 (status register 2)
	jnc .E13
	mov dword [LASTERRORMESSAGE], E12
	mov byte [LASTERRORCODE], 12
	jmp .done

	;; Bit 3 is unused in status register 2
	;; Bit 2 is unused in status register 2
	
	.E13:
	;; Check for bad track
	mov AL, byte [STATUSREGISTER2]
	bt AX, 1		; bit 1 (status register 2)
	jnc .E14
	mov dword [LASTERRORMESSAGE], E13
	mov byte [LASTERRORCODE], 13
	jmp .done

	.E14:
	;; Check for bad address mark
	mov AL, byte [STATUSREGISTER2]
	bt AX, 0		; bit 0 (status register 2)
	jnc .EUN
	mov dword [LASTERRORMESSAGE], E14
	mov byte [LASTERRORCODE], 14
	jmp .done

	.EUN:
	;; Hmm.  We don't know what caused this error
	mov dword [LASTERRORMESSAGE], EUN
	mov byte [LASTERRORCODE], -1
	jmp .done

	.done:
	popa
	ret
	

;; 
;; Below here, the functions are exported for external use
;; 

	
kernelFloppyDriverInitialize:
	;; This initializes the driver and returns success

	pusha

	;; Clear the "interrupt received" byte
	mov byte [INTERRUPTRECEIVED], 0

	;; Clear the error information:	the error message pointer and
	;; the error code byte
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0

	popa
	mov EAX, 0		; return success
	ret

	
kernelFloppyDriverDescribe:
	;; This function should be called before any attempt is made to
	;; use the disk in question.  This allows us to store some
	;; information about this disk.  The parameters should be placed
	;; on the stack as follows:
	
	;; 0 - Disk number
	;; 1 - Head load timer
	;; 2 - Head unload timer
	;; 3 - Step rate timer
	;; 4 - Data rate
	;; 5 - Sector size code
	;; 6 - Last sector on track
	;; 7 - Gap length between sectors
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Get the disk number in ECX.  This will be used as an offset to
	;; set each disk's information respectively
	mov ECX, dword [SS:(EBP + 36)]

	;; Save the head load timer
	mov EDI, HEAD_LOAD
	add EDI, ECX
	mov EAX, dword [SS:(EBP + 40)]
	mov byte [EDI], AL

	;; Save the head unload timer
	mov EDI, HEAD_UNLOAD
	add EDI, ECX
	mov EAX, dword [SS:(EBP + 44)]
	mov byte [EDI], AL
	
	;; Save the step rate timer
	mov EDI, STEP_RATE
	add EDI, ECX
	mov EAX, dword [SS:(EBP + 48)]
	mov byte [EDI], AL
	
	;; Save the data rate
	mov EDI, DATA_RATE
	add EDI, ECX
	mov EAX, dword [SS:(EBP + 52)]
	mov byte [EDI], AL
	
	;; Save the sector size code
	mov EDI, SECTOR_SIZE
	add EDI, ECX
	add EDI, ECX
	mov EAX, dword [SS:(EBP + 56)]
	mov word [EDI], AX
	
	;; Save the last sector on the track
	mov EDI, LAST_SECTOR
	add EDI, ECX
	mov EAX, dword [SS:(EBP + 60)]
	mov byte [EDI], AL
	
	;; Save the gap length between sectors
	mov EDI, GAP_LENGTH
	add EDI, ECX
	mov EAX, dword [SS:(EBP + 64)]
	mov byte [EDI], AL
	
	;; Select the disk
	push dword [SS:(EBP + 36)]
	call selectDrive
	add ESP, 4

	;; Set the timing information
	call specify
		
	popa
	mov EAX, 0		; Return success
	ret
	

kernelFloppyDriverReset:
	;; Does a software reset of the requested floppy controller.  Always
	;; returns success.  There's only one parameter:
	;; 0 - Disk number

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Wait for a lock on the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerLock
	add ESP, 4

	cmp EAX, 0
	jne .error
		
	;; Get the requested disk number
	mov EAX, dword [SS:(EBP + 36)]

	;; Select the disk
	push dword EAX
	call selectDrive
	add ESP, 4
		
	xor EAX, EAX
	
	;; Read the port's current state
	mov DX, 03F2h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Mask off the 'reset' bit (go to reset mode)
	and AL, 11111011b

	;; Issue the command
	mov DX, 03F2h
	out DX, AL

	;; Delay a bunch
	jecxz $+2
	jecxz $+2
	jecxz $+2
	jecxz $+2
	jecxz $+2
	jecxz $+2

	;; Now mask on the 'reset' bit (exit reset mode)
	or AL, 00000100b
	
	;; Issue the command
	mov DX, 03F2h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, 0		; Return success
	ret

	.error:
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, -1		; Return failure
	ret
	

kernelFloppyDriverMotorOn:
	;; Turns the requested floppy motor on.  There's only one parameter:
	;; 0 - Disk number

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Wait for a lock on the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerLock
	add ESP, 4

	cmp EAX, 0
	jne .error
		
	;; Get the requested disk number
	mov EAX, dword [SS:(EBP + 36)]

	;; Select the disk
	push dword EAX
	call selectDrive
	add ESP, 4
		
	xor EAX, EAX
	xor EBX, EBX
	xor ECX, ECX

	;; Read the port's current state
	mov DX, 03F2h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Move the motor select bit to the correct location [7:4]
	mov CL, byte [SELECTEDFLOPPY]
	mov BL, 00010000b
	rol BL, CL

	;; Test whether the motor is on already
	mov AH, AL
	and AH, BL
	cmp AH, 0
	jne .finish
	
	;; Turn on the 'motor on' bit
	or AL, BL

	;; Issue the command
	mov DX, 03F2h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	.finish:
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, 0		; Return success
	ret

	.error:	
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, -1		; Return failure
	ret
	

kernelFloppyDriverMotorOff:
	;; Turns the requested floppy motor off.  There's only one parameter:
	;; 0 - Disk number

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Wait for a lock on the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerLock
	add ESP, 4

	cmp EAX, 0
	jne .error
		
	;; Get the requested disk number
	mov EAX, dword [SS:(EBP + 36)]

	;; Select the disk
	push dword EAX
	call selectDrive
	add ESP, 4
		
	xor EAX, EAX
	xor EBX, EBX
	xor ECX, ECX

	;; Read the port's current state
	mov DX, 03F2h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Move the motor select bit to the correct location [7:4]
	mov CL, byte [SELECTEDFLOPPY]
	mov BL, 11101111b
	rol BL, CL

	;; Turn off the 'motor on' bit
	and AL, BL

	;; Issue the command
	mov DX, 03F2h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, 0		; Return success
	ret

	.error:	
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, -1		; Return failure
	ret

	
kernelFloppyDriverRecalibrate:
	;; Recalibrates the selected drive, causing it to seek to track 0
	;; There's only one parameter:
	;; 0 - Disk number
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Wait for a lock on the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerLock
	add ESP, 4

	cmp EAX, 0
	jne .errorDone
		
	;; Get the requested disk number
	mov EAX, dword [SS:(EBP + 36)]

	;; Select the disk
	push dword EAX
	call selectDrive
	add ESP, 4
		
	;; Tell the interrupt-received routine to issue the
	;; "sense interrupt status" command after the operation
	mov byte [READSTATUSONINTERRUPT], 1
	
	;; We have to send two byte commands to the controller
	
	call waitCommandWrite
	
	mov DX, 03F5h
	mov AL, 7
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	call waitCommandWrite

	mov DX, 03F5h
	mov AL, byte [SELECTEDFLOPPY]
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Now wait for the operation to end
	call waitOperationComplete

	cmp EAX, 0
	jne .errorDone		; The operation timed out

	;; Check error conditions in the status byte
	mov AL, byte [STATUSREGISTER0]
	and AL, 11111000b
	cmp AL, 00100000b
	jne .errorDone

	;; Make sure that we are now at the correct cylinder
	mov AL, byte [CURRENTCYLINDER]
	cmp AL, 0
	jne .errorDone
	
	;; If we fall through to here, the operation was a success.

	.done:	
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, -1
	ret
	

kernelFloppyDriverDiskChanged:
	;; This routine simply determines whether there is media in
	;; the floppy drive.  It assumes that the desired disk
	;; has already been selected.  It takes no parameters, and
	;; returns 1 if the disk is missing or has been changed, 0 if it
	;; has not been changed, and  negative if it encounters some 
	;; other type of error.  There's only one parameter
	;; 0 - Disk number
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Wait for a lock on the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerLock
	add ESP, 4

	cmp EAX, 0
	jne .yesMedia
		
	;; Get the requested disk number
	mov EAX, dword [SS:(EBP + 36)]

	;; Select the disk
	push dword EAX
	call selectDrive
	add ESP, 4
		
	;; Now simply read port 03F7h.  Bit 7 is the only part that matters.
	mov DX, 03F7h
	in AL, DX
	
	and AL, 10000000b
	cmp AL, 0
	je .yesMedia
	
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, 1		; Return "disk changed"
	ret

	.yesMedia:
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, 0		; Return "yes media"
	ret

	
kernelFloppyDriverReadSectors:
	;; This routine reads sectors from the disk.
	;; It assumes that the DMA access has been previously arranged by
	;; the higher-level disk access routine, and that the desired disk
	;; has already been selected.  The parameters look like this 
	;; on the stack:
	;; 0 - Disk number
	;; 1 - Head
	;; 2 - Track/Cylinder
	;; 3 - Starting sector
	;; 4 - Logical sector (unused by floppy driver)
	;; 5 - Sectors to transfer
	;; 6 - Pointer to data buffer
		
	pusha

	;; Save the current stack pointer
	mov EBP, ESP

	;; Wait for a lock on the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerLock
	add ESP, 4

	cmp EAX, 0
	jne near .errorDone
		
	;; Get the requested disk number
	mov EAX, dword [SS:(EBP + 36)]

	;; Select the disk
	push dword EAX
	call selectDrive
	add ESP, 4
	
	;; We need to do a seek
	push dword [SS:(EBP + 44)]	; Track/cylinder
	push dword [SS:(EBP + 40)]	; Head
	call seek
	add ESP, 8
	
	;; Success?
	cmp EAX, 0
	jl near .errorDone	; Signed comparison
			
	;; Set up the DMA controller for the read
	  
	;; Disable the DMA channel
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsCloseChannel;
	add ESP, 4

	;; Were we successful?
	cmp EAX, 0
	jl near .errorDone	; Signed comparison

	;; How many bytes will we transfer?
	xor EAX, EAX
	xor ECX, ECX
	mov CL, byte [SELECTEDFLOPPY]
	mov EDI, SECTOR_SIZE
	add EDI, ECX
	add EDI, ECX
	mov AX, word [EDI]
	mov ECX, dword [SS:(EBP + 56)]
	mul ECX
	;; Number of bytes is in EAX
	
	;; Setup the DMA access for the number of bytes to transfer
	;; and the location to transfer to/from
	push dword EAX
	push dword [SS:(EBP + 60)]; physical address
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsSetupChannel
	add ESP, 12

	;; Were we successful?
	cmp EAX, 0
	jl near .errorDone	; Signed comparison

	;; Set the DMA channel for writing TO memory
	push dword 0		; Demand mode
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsWriteData
	add ESP, 8
	
	;; Were we successful?
	cmp EAX, 0
	jl near .errorDone	; Signed comparison

	;; Enable the channel
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsEnableChannel
	add ESP, 4

	;; Were we successful?
	cmp EAX, 0
	jl near .errorDone	; Signed comparison


	;; Tell the interrupt-received routine NOT to issue the
	;; "sense interrupt status" command after the operation
	mov byte [READSTATUSONINTERRUPT], 0

	
	;; Command byte
	;; Drive/head select byte
	;; Cylinder number byte
	;; Head number byte
	;; Sector byte
	;; Sector size code
	;; End of track byte
	;; Gap length byte
	;; Custom sector size byte

	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the command byte
	mov AL, 0E6h		; Read normal data command

	;; Send the command byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the drive/head select byte
	;; Format [00000 (head 1 bit)(drive 2 bits)]
	mov EAX, dword [SS:(EBP + 40)]	; Head
	and AL, 00000001b
	shl AL, 2
	or AL, byte [SELECTEDFLOPPY]

	;; Send the drive/head select byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2
	
		
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the cylinder number byte
	mov EAX, dword [SS:(EBP + 44)]	; Track/cylinder

	;; Send the cylinder number byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2
	
	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the head number byte
	mov EAX, dword [SS:(EBP + 40)]; Head

	;; Send the head number byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2
	

	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the sector byte
	mov EAX, dword [SS:(EBP + 48)]	; Sector

	;; Send the sector byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

		
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the sector size code
	xor EAX, EAX
	xor ECX, ECX
	mov CL, byte [SELECTEDFLOPPY]
	mov EDI, SECTOR_SIZE
	add EDI, ECX
	add EDI, ECX
	mov AX, word [EDI]
	shr AX, 8	; This is the byte value the controller expects

	;; Send the sector size code
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the end of track byte
	xor EAX, EAX
	xor ECX, ECX
	mov CL, byte [SELECTEDFLOPPY]
	mov EDI, LAST_SECTOR
	add EDI, ECX
	mov AL, byte [EDI]

	;; Send the end of track byte
	call waitCommandWrite
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the gap length byte
	xor EAX, EAX
	xor ECX, ECX
	mov CL, byte [SELECTEDFLOPPY]
	mov EDI, GAP_LENGTH
	add EDI, ECX
	mov AL, byte [EDI]
	
	;; Send the gap length byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the custom sector size byte
	mov AL, 0FFh		; Always FFh
	
	;; Send the custom sector size byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2


	;; Wait for the floppy to signal that the operation is complete
	call waitOperationComplete

	cmp EAX, 0
	je .commandOK

	;; The command timed out.  Save the error and return error
	mov dword [LASTERRORMESSAGE], ETO
	mov byte [LASTERRORCODE], 15
	jmp .errorDone
	
	.commandOK:
	;; We have to read the seven status bytes from the controller.
	;; Save them in the designated memory locations

	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER0], AL

	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER1], AL
	
	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER2], AL
	
	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER3], AL

	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER4], AL

	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER5], AL

	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER6], AL

	;; Save the current track
	mov AL, byte [STATUSREGISTER3]
	mov byte [CURRENTCYLINDER], AL
	
	;; Now we can examine the status.  If the top two bits
	;; are zero, then the operation completed normally.
	mov AL, byte [STATUSREGISTER0]
	and AL, 11000000b
	cmp AL, 0
	je .done
	
	;; Oops.  If we got here, we have an error.  We have to try
	;; to determine the cause and set the error message.  We'll
	;; call a routine which does all of this for us.
	call evaluateError
	jmp .errorDone
	
	.done:
	;; Close the DMA channel
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsCloseChannel
	add ESP, 4
  
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	;; Make sure there's no error status left over from some other
	;; operation
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0

	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:
	;; Close the DMA channel
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsCloseChannel
	add ESP, 4
  
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, -1		; Return failure
	ret
	
	
kernelFloppyDriverWriteSectors:
	;; This routine writes sectors to the disk.
	;; It assumes that the DMA access has been previously arranged by
	;; the higher-level disk access routine, and that the desired disk
	;; has already been selected.  The parameters look like this 
	;; on the stack:
	;; 0 - Disk number
	;; 1 - Head
	;; 2 - Track/Cylinder
	;; 3 - Starting sector
	;; 4 - Logical sector (unused by floppy driver)
	;; 5 - Sectors to transfer
	;; 6 - Pointer to data buffer
	
	pusha

	;; Save the current stack pointer
	mov EBP, ESP

	;; Wait for a lock on the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerLock
	add ESP, 4

	cmp EAX, 0
	jne near .errorDone
		
	;; Get the requested disk number
	mov EAX, dword [SS:(EBP + 36)]

	;; Select the disk
	push dword EAX
	call selectDrive
	add ESP, 4
	
	;; We need to do a seek
	push dword [SS:(EBP + 44)]	; Track/cylinder
	push dword [SS:(EBP + 40)]	; Head
	call seek
	add ESP, 8
	
	;; Success?
	cmp EAX, 0
	jl near .errorDone	; Signed comparison

	
	;; Set up the DMA controller for the write
	  
	;; Disable the DMA channel
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsCloseChannel;
	add ESP, 4

	;; Were we successful?
	cmp EAX, 0
	jl near .errorDone	; Signed comparison

	;; How many bytes will we transfer?
	xor EAX, EAX
	xor ECX, ECX
	mov CL, byte [SELECTEDFLOPPY]
	mov EDI, SECTOR_SIZE
	add EDI, ECX
	add EDI, ECX
	mov AX, word [EDI]
	mov ECX, dword [SS:(EBP + 56)]
	mul ECX
	;; Number of bytes is in EAX
	
	;; Setup the DMA access for the number of bytes to transfer
	;; and the location to transfer to/from
	push dword EAX
	push dword [SS:(EBP + 60)]; physical address
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsSetupChannel
	add ESP, 12

	;; Were we successful?
	cmp EAX, 0
	jl near .errorDone	; Signed comparison

	;; Set the channel for reading FROM memory
	push dword 0		; Demand mode
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsReadData
	add ESP, 8
	
	;; Were we successful?
	cmp EAX, 0
	jl near .errorDone	; Signed comparison

	;; Enable the channel
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsEnableChannel
	add ESP, 4

	;; Were we successful?
	cmp EAX, 0
	jl near .errorDone	; Signed comparison


	;; Tell the interrupt-received routine NOT to issue the
	;; "sense interrupt status" command after the operation
	mov byte [READSTATUSONINTERRUPT], 0

	
	;; Command byte
	;; Drive/head select byte
	;; Cylinder number byte
	;; Head number byte
	;; Sector byte
	;; Sector size code
	;; End of track byte
	;; Gap length byte
	;; Custom sector size byte

	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the command byte
	mov AL, 0C5h		; Write data command

	;; Send the command byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the drive/head select byte
	;; Format [00000 (head 1 bit)(drive 2 bits)]
	mov EAX, dword [SS:(EBP + 40)]	; Head
	and AL, 00000001b
	shl AL, 2
	or AL, byte [SELECTEDFLOPPY]

	;; Send the drive/head select byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2
	
		
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the cylinder number byte
	mov EAX, dword [SS:(EBP + 44)]	; Track/cylinder

	;; Send the cylinder number byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2
	
	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the head number byte
	mov EAX, dword [SS:(EBP + 40)]; Head

	;; Send the head number byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2
	

	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the sector byte
	mov EAX, dword [SS:(EBP + 48)]	; Sector

	;; Send the sector byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

		
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the sector size code
	xor EAX, EAX
	xor ECX, ECX
	mov CL, byte [SELECTEDFLOPPY]
	mov EDI, SECTOR_SIZE
	add EDI, ECX
	add EDI, ECX
	mov AX, word [EDI]
	shr AX, 8	; This is the byte value the controller expects

	;; Send the sector size code
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the end of track byte
	xor EAX, EAX
	xor ECX, ECX
	mov CL, byte [SELECTEDFLOPPY]
	mov EDI, LAST_SECTOR
	add EDI, ECX
	mov AL, byte [EDI]

	;; Send the end of track byte
	call waitCommandWrite
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the gap length byte
	xor EAX, EAX
	xor ECX, ECX
	mov CL, byte [SELECTEDFLOPPY]
	mov EDI, GAP_LENGTH
	add EDI, ECX
	mov AL, byte [EDI]
	
	;; Send the gap length byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	
	;; Wait for the controller to be ready
	call waitCommandWrite

	;; Construct the custom sector size byte
	mov AL, 0FFh		; Always FFh
	
	;; Send the custom sector size byte
	mov DX, 03F5h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2


	;; Wait for the floppy to signal that the operation is complete
	call waitOperationComplete

	cmp EAX, 0
	je .commandOK

	;; The command timed out.  Save the error and return error
	mov dword [LASTERRORMESSAGE], ETO
	mov byte [LASTERRORCODE], 15
	jmp .errorDone
	
	.commandOK:
	;; We have to read the seven status bytes from the controller.
	;; Save them in the designated memory locations

	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER0], AL

	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER1], AL
	
	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER2], AL
	
	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER3], AL

	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER4], AL

	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER5], AL

	call waitStatusRead
	mov DX, 03F5h
	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER6], AL

	;; Save the current track
	mov AL, byte [STATUSREGISTER3]
	mov byte [CURRENTCYLINDER], AL
	
	;; Now we can examine the status.  If the top two bits
	;; are zero, then the operation completed normally.
	mov AL, byte [STATUSREGISTER0]
	and AL, 11000000b
	cmp AL, 0
	je .done
	
	;; Oops.  If we got here, we have an error.  We have to try
	;; to determine the cause and set the error message.  We'll
	;; call a routine which does all of this for us.
	call evaluateError
	jmp .errorDone
	
	.done:
	;; Close the DMA channel
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsCloseChannel
	add ESP, 4
  
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4

	;; Make sure there's no error status left over from some other
	;; operation
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0

	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:
	;; Close the DMA channel
	push dword 2		; Floppy uses DMA channel 2
	call kernelDmaFunctionsCloseChannel
	add ESP, 4
  
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	popa
	mov EAX, -1		; Return failure
	ret


kernelFloppyDriverLastErrorCode:
	;; This routine can be called to get the numeric code
	;; associated with the last error.  It takes no parameters, 
	;; and returns the integer code

	xor EAX, EAX
	mov AL, byte [LASTERRORCODE]
	ret


kernelFloppyDriverLastErrorMessage:
	;; This routine can be called to get the message associated
	;; with the last error.  It takes no parameters, and returns
	;; a pointer to the appropriate error message

	mov EAX, dword [LASTERRORMESSAGE]
	ret
	
	
kernelFloppyDriverReceiveInterrupt:
	;; This routine will be called whenever the floppy drive issues
	;; its service interrupt.  It will simply change a data value
	;; to indicate that one has been received, and acknowldege the
	;; interrupt to the PIC.  It's up to other routines to do something 
	;; useful with the information.

	pusha

	;; Check whether to do the "sense interrupt status" command
	;; If not, we're finished
	cmp byte [READSTATUSONINTERRUPT], 0
	je .done		; Otherwise, fall through and do
				; the command

	
	;; Tell the diskette drive that the interrupt was serviced.  This
	;; helps the drive stop doing the operation, and returns some
	;; status operation, which we will save
	
	call waitCommandWrite

	mov DX, 03F5h
	mov AL, 08h
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	call waitStatusRead

	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [STATUSREGISTER0], AL 
	
	call waitStatusRead

	in AL, DX
	;; Delay
	jecxz $+2
	jecxz $+2
	mov byte [CURRENTCYLINDER], AL 


	.done:
	;; Tell the other routines that the operation is finished
	mov byte [INTERRUPTRECEIVED], 1

	popa
	ret


	SEGMENT .data
	ALIGN 4
		
CONTROLLER_LOCK		dd 0
NUMBER_FLOPPIES		dd 0
	
	;; Error stuff
LASTERRORMESSAGE	dd 0
LASTERRORCODE		db 0

	;; Data from the drive controller and data about the 
	;; floppy drive environment
STATUSREGISTER0		db 0
STATUSREGISTER1		db 0
STATUSREGISTER2		db 0
STATUSREGISTER3		db 0
STATUSREGISTER4		db 0
STATUSREGISTER5		db 0
STATUSREGISTER6		db 0
SELECTEDFLOPPY		db 0
CURRENTCYLINDER		db 0
INTERRUPTRECEIVED	db 0
READSTATUSONINTERRUPT	db 0

	ALIGN 4

	;; Information about each floppy disk we're controlling
SECTOR_SIZE	dw 0, 0, 0, 0		;; Sector size code
HEAD_LOAD	db 0, 0, 0, 0		;; Head load timer
HEAD_UNLOAD	db 0, 0, 0, 0		;; Head unload timer
STEP_RATE	db 0, 0, 0, 0		;; Step rate timer
DATA_RATE	db 0, 0, 0, 0		;; Data rate
LAST_SECTOR	db 0, 0, 0, 0		;; Last sector on track
GAP_LENGTH	db 0, 0, 0, 0		;; Gap length between sectors

	;; Error messages
E0	db 'No previous error message', 0
E1	db 'Abnormal termination error - command did not complete', 0
E2	db 'Invalid command error', 0
E3	db 'Equipment check error - seek to invalid track', 0
E4	db 'The requested sector is past the end of the track', 0
E5	db 'ID byte or data error - the CRC integrity check failed', 0
E6	db 'DMA transfer overrun or underrun error', 0
E7	db 'No data error - the requested sector was not found', 0
E8	db 'Write protect error', 0
E9	db 'Missing address mark error', 0
E10	db 'Sector control mark error - data was not the expected type', 0
E11	db 'Data error - the CRC integrity check failed', 0
E12	db 'Invalid or unexpected track error', 0
E13	db 'Bad track error', 0
E14	db 'Bad address mark error', 0
EUN	db 'Unknown error', 0
ETO	db 'Command timed out', 0
