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
;;  kernelIdeDriver.s
;;

;; Error codes
%define ERR_ADDRESSMARK    1
%define ERR_TRACK0         2
%define ERR_INVALIDCOMMAND 3
%define ERR_MEDIAREQ       4
%define ERR_SECTNOTFOUND   5
%define ERR_MEDIACHANGED   6
%define ERR_BADDATA        7
%define ERR_BADSECTOR      8
%define ERR_UNKNOWN        9
%define ERR_TIMEOUT        10

	
	SEGMENT .text
	BITS 32

	GLOBAL kernelIdeDriverInitialize
	GLOBAL kernelIdeDriverReset
	GLOBAL kernelIdeDriverRecalibrate
	GLOBAL kernelIdeDriverReadSectors
	GLOBAL kernelIdeDriverWriteSectors
	GLOBAL kernelIdeDriverLastErrorCode
	GLOBAL kernelIdeDriverLastErrorMessage
	GLOBAL kernelIdeDriverReceiveInterrupt
	
	%include "kernelAssemblerHeader.h"


selectDrive:	
	;; Takes the disk to select as its only parameter.  Returns 0
	;; on success, negative otherwise

	pusha
	
	;; Save the stack pointer
	mov EBP, ESP

	;; Get the disk number
	mov EBX, dword [SS:(EBP + 36)]

	;; Make sure it's legal (7 or fewer, since this driver only
	;; supports 8).
	cmp EBX, 7
	ja near .error

	;; Save the number of the selected drive
	mov byte [SELECTEDDISK], BL

	;; We need to gather all of the correct port numbers
	;; based on the relevant controller number.  A more clever
	;; way to do this would be to have a simple pointer that
	;; points to one of four controller port lists.  Then we could
	;; just move the pointer to select a series of port numbers for a 
	;; disk.  However, for the moment I want to keep things simple, 
	;; so thatthere is a well-defined "name" for each data variable 
	;; (port type).  This will be easier to debug for the moment.
	
	xor EAX, EAX

	;; Multiply EBX by 2, since we are using it as an offset into
	;; lists of word values
	shl EBX, 1
		
	;; Get the data port for the controller
	mov AX, word [EBX + _DATA]
	;; Move it to the "current" spot
	mov word [DATA], AX
	
	;; Get the error port for the controller
	mov AX, word [EBX + _ERROR]
	;; Move it to the "current" spot
	mov word [ERROR], AX

	;; Get the sector count port for the controller
	mov AX, word [EBX + _SECTORCOUNT]
	;; Move it to the "current" spot
	mov word [SECTORCOUNT], AX

	;; Get the sector number port for the controller
	mov AX, word [EBX + _SECTORNUMBER]
	;; Move it to the "current" spot
	mov word [SECTORNUMBER], AX

	;; Get the low cylinder number port for the controller
	mov AX, word [EBX + _CYLINDERLOW]
	;; Move it to the "current" spot
	mov word [CYLINDERLOW], AX

	;; Get the high cylinder port for the controller
	mov AX, word [EBX + _CYLINDERHIGH]
	;; Move it to the "current" spot
	mov word [CYLINDERHIGH], AX

	;; Get the drive & head port for the controller
	mov AX, word [EBX + _DRIVEHEAD]
	;; Move it to the "current" spot
	mov word [DRIVEHEAD], AX

	;; Get the command/status port for the controller
	mov AX, word [EBX + _COMSTAT]
	;; Move it to the "current" spot
	mov word [COMSTAT], AX

	;; Set the drive select bit in the drive/head register.  This
	;; will help to introduce some delay between disk selection
	;; and any actual commands.
	mov AL, byte [SELECTEDDISK]
	and AL, 00000001b	; Disk number is LSBit.
	shl AL, 4		; Move disk number to bit 4
	or AL, 10100000b	; NO LBA

	mov DX, word [DRIVEHEAD]
	out DX, AL
		
	;; Return success
	popa
	mov EAX, 0
	ret

	;; Failed
	.error:
	popa
	mov EAX, -1
	ret

		
waitOperationComplete:
	;; This routine reads the "interrupt received" byte, waiting for
	;; the last command to complete.  Every time the command has not
	;; completed, the driver returns the remainder of the process'
	;; timeslice to the multitasker.  When the interrupt byte becomes 1, 
	;; it resets the byte and returns.  If the wait times out, the 
	;; function returns -1.  Otherwise, it returns 0.
	
	pusha

	;; Save the current time so we don't wait too long
	call kernelSysTimerRead
	add EAX, 20
	push dword EAX

	.loopStart:
	
	call kernelSysTimerRead
	cmp EAX, dword [SS:ESP]
	jae .timeOut

	;; Yield the rest of this timeslice if we are in multitasking mode
	call kernelMultitaskerYield
	
	cmp byte [INTERRUPTRECEIVED], 1
	jne .loopStart

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


waitControllerReady:
	;; Returns when the disk controller is ready for a command.
	;; The routine returns 0 when the controller is ready, unless
	;; it times out -- in which case it returns -1

	pusha

	;; Save the current time so we don't wait too long
	call kernelSysTimerRead
	add EAX, 20
	push dword EAX

	;; Get the status register number for the controller of the 
	;; selected disk in DX.
	mov DX, word [COMSTAT]	
		
	.loopStart:
	push EDX		; This gets clobbered
	call kernelSysTimerRead
	pop EDX			; Restore EDX
	cmp EAX, dword [SS:ESP]	; Timeout?
	jae .timeOut

	;; Get the current status from the port
	xor EAX, EAX
	in AL, DX

	;; We are interested in bit 7.
	bt AX, 7

	;; It should be clear
	jc .loopStart

	;; Return success
	.done:
	add ESP, 4
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0
	popa
	mov EAX, 0
	ret

	.timeOut:
	add ESP, 4
	mov dword [LASTERRORMESSAGE], ETO	; Time out error message
	mov byte [LASTERRORCODE], ERR_TIMEOUT
	popa
	mov EAX, -1
	ret


waitDiskReady:
	;; Returns when the disk is ready for a command.
	;; The routine returns 0 when the disk is ready, unless
	;; it times out -- in which case it returns -1

	pusha

	;; Save the current time so we don't wait too long
	call kernelSysTimerRead
	add EAX, 20
	push dword EAX

	;; Get the status register number for the controller of the 
	;; selected disk in DX.
	mov DX, word [COMSTAT]	
		
	.loopStart:
	push EDX		; This gets clobbered
	call kernelSysTimerRead
	pop EDX			; Restore EDX
	cmp EAX, dword [SS:ESP]	; Timeout?
	jae .timeOut

	;; Get the current status from the port
	xor AX, AX
	in AL, DX

	;; First, we are interested in bit 6.
	bt AX, 6

	;; It should be set
	jnc .loopStart

	;; Now check bit 4 (seek completed)
	bt AX, 4

	;; It should be set
	jnc .loopStart

	;; Return success
	.done:
	add ESP, 4
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0
	popa
	mov EAX, 0
	ret

	.timeOut:
	add ESP, 4
	mov dword [LASTERRORMESSAGE], ETO	; Time out error message
	mov byte [LASTERRORCODE], ERR_TIMEOUT
	popa
	mov EAX, -1
	ret


CHSSetup:
	;; This routine is strictly internal, and is used to set up the
	;; disk controller registers with head, cylinder, sector and
	;; sector-count values (prior to a read, write, seek, etc.)
	;; The prototype for the function looks like this:
	;; void CHSSetup(int head, int cylinder,
	;;		int startSector);
	;; It doesn't return anything.
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Set the drive and head.  The drive number for the particular
	;; controller will be the least-significant bit of the selected
	;; disk number
	mov AL, byte [SELECTEDDISK]
	and AL, 00000001b	; Disk number is LSBit.
	shl AL, 4		; Move disk number to bit 4
	mov EBX, dword [SS:(EBP + 36)]  ; Get the head value from the stack
	and BL, 00001111b	; Head value is 4 LSBits
	or AL, BL		; Put head in AL
	or AL, 10100000b	; NO LBA

	;; Get the drive/head register number of the selected disk in DX
	mov DX, word [DRIVEHEAD]

	;; Send the drive/head value
	out DX, AL

	;; Get the Cylinder number in EAX
	mov EAX, dword [SS:(EBP + 40)]		; cylinder number 
	
	;; Get the cylinder low register number of the selected disk in DX
	mov DX, word [CYLINDERLOW]

	;; Send the low cylinder byte
	out DX, AL
			
	;; Get the cylinder high register number of the selected disk in DX
	mov DX, word [CYLINDERHIGH]

	;; Send the high cylinder byte
	shr AX, 8
	out DX, AL
	
	;; Get the sector number register of the selected disk in DX
	mov DX, word [SECTORNUMBER]		; starting sector

	;; Get the starting sector number in EAX
	mov EAX, dword [SS:(EBP + 44)]
	
	;; Send the starting sector byte
	out DX, AL

	;; Send a value of FFh (no precompensation) to the error/precomp
	;; register
	mov DX, word [ERROR]
	mov AL, 0FFh
	out DX, AL
	
	popa
	ret
	

LBASetup:
	;; This routine is strictly internal, and is used to set up the
	;; disk controller registers with an LBA disk address in the
	;; disk/head, cylinder low, cylinder high, and start sector
	;; registers.  The prototype for the function looks like this:
	;; void CHSSetup(unsigned LBAAddress);
	;; It doesn't return anything.
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	mov ECX, dword [SS:(EBP + 36)]		; LBA address
	
	;; Set the drive and head register.  The drive number for the 
	;; particular controller will be the least-significant bit of the 
	;; selected disk number.
	mov AL, byte [SELECTEDDISK]
	and AL, 00000001b	; Disk number is LSBit.
	shl AL, 4		; Move disk number to bit 4
	mov EBX, ECX		; Get LBA bits 24-27 in ECX
	shr EBX, 24
	and BL, 0Fh		; LBA data is 4 LSBits
	or AL, BL		; Put LBA data in AL
	or AL, 11100000b	; LBA is active

	;; Get the drive/head register number of the selected disk in DX
	mov DX, word [DRIVEHEAD]

	;; Send the drive/head value
	out DX, AL
	
	;; Get the cylinder low register number of the selected disk in DX
	mov DX, word [CYLINDERLOW]

	;; Set the cylinder low byte with bits 8-15 of the LBA address
	mov EAX, ECX
	shr EAX, 8
	out DX, AL
	
	;; Get the cylinder high register number of the selected disk in DX
	mov DX, word [CYLINDERHIGH]

	;; Set the cylinder low byte with bits 16-23 of the LBA address
	mov EAX, ECX
	shr EAX, 16
	out DX, AL
	
	;; Get the start sector register of the selected disk in DX
	mov DX, word [SECTORNUMBER]

	;; Set the cylinder low byte with bits 0-7 of the LBA address
	mov EAX, ECX
	out DX, AL

	;; Send a value of FFh (no precompensation) to the error/precomp
	;; register
	mov DX, word [ERROR]
	mov AL, 0FFh
	out DX, AL
	
	popa
	ret
	

errorStatus:
	;; This routine will check the error status on the disk controller
	;; of the selected disk.  It evaluates the returned byte and matches 
	;; conditions to error codes and error messages

	pusha

	;; Read the error register
	mov DX, word [ERROR]
	xor AX, AX
	in AL, DX

	;; AL contains the error byte
	bt AX, 0
	jnc .E2
	mov dword [LASTERRORMESSAGE], E1
	mov byte [LASTERRORCODE], ERR_ADDRESSMARK
	jmp .done
	
	.E2:
	;; AL contains the error byte
	bt AX, 1
	jnc .E3
	mov dword [LASTERRORMESSAGE], E2
	mov byte [LASTERRORCODE], ERR_TRACK0
	jmp .done
	
	.E3:
	;; AL contains the error byte
	bt AX, 2
	jnc .E4
	mov dword [LASTERRORMESSAGE], E3
	mov byte [LASTERRORCODE], ERR_INVALIDCOMMAND
	jmp .done
	
	.E4:
	;; AL contains the error byte
	bt AX, 3
	jnc .E5
	mov dword [LASTERRORMESSAGE], E4
	mov byte [LASTERRORCODE], ERR_MEDIAREQ
	jmp .done
	
	.E5:
	;; AL contains the error byte
	bt AX, 4
	jnc .E6
	mov dword [LASTERRORMESSAGE], E5
	mov byte [LASTERRORCODE], ERR_SECTNOTFOUND
	jmp .done
	
	.E6:
	;; AL contains the error byte
	bt AX, 5
	jnc .E7
	mov dword [LASTERRORMESSAGE], E6
	mov byte [LASTERRORCODE], ERR_MEDIACHANGED
	jmp .done
	
	.E7:
	;; AL contains the error byte
	bt AX, 6
	jnc .E8
	mov dword [LASTERRORMESSAGE], E7
	mov byte [LASTERRORCODE], ERR_BADDATA
	jmp .done
	
	.E8:
	;; AL contains the error byte
	bt AX, 7
	jnc .EUN
	mov dword [LASTERRORMESSAGE], E8
	mov byte [LASTERRORCODE], ERR_BADSECTOR
	jmp .done
	
	.EUN:
	;; Hmm.  We don't know what caused this error
	mov dword [LASTERRORMESSAGE], EUN
	mov byte [LASTERRORCODE], ERR_UNKNOWN

	.done:
	popa
	ret
	

;; 
;; Below here, the functions are exported for external use
;; 

	
kernelIdeDriverInitialize:
	;; This initializes the driver and returns success.
	;; int kernelIdeDriverInitialize(void);

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

	
kernelIdeDriverReset:
	;; Does a software reset of the requested disk controller.
	;; int kernelIdeDriverReset(int diskNumber);

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
		
	;; Get the adapter status register number of the selected disk
	;; in DX
	mov DX, word [COMSTAT]	

	;; We need to set bit 2 for at least 4.8 microseconds.
	;; We will set the bit and then we will tell the multitasker
	;; to make us "wait" for at least one timer tick
	in AL, DX
	bts AX, 2
	out DX, AL
	
	;; Delay
	mov ECX, 100
	.delay:
	dec ECX
	jecxz .delay

	;; Clear bit 2 again
	in AL, DX
	btr AX, 2
	out DX, AL
	
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4

	;; Return success
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0
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
	

kernelIdeDriverRecalibrate:
	;; Recalibrates the requested drive, causing it to seek to track 0
	;; int kernelIdeDriverRecalibrate(int diskNumber);
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Wait for a lock on the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerLock
	add ESP, 4

	cmp EAX, 0
	jne near .error
		
	;; Get the requested disk number
	mov EAX, dword [SS:(EBP + 36)]

	;; Select the disk
	push dword EAX
	call selectDrive
	add ESP, 4
		
	;; Wait for the controller to be ready
	call waitControllerReady

	;; Make sure we didn't time out
	cmp EAX, -1
	je near .timeOut

	;; Call the routine that will send disk/head information
	;; to the controller registers prior to the recalibration.
	
	push dword 0		; Sector value:	 don't care
	push dword 0		; Cylinder value: 0 by definition
	push dword 0		; Head:	Head zero is O.K.
	call CHSSetup
	add ESP, 12
	
	;; Wait for the selected disk to be ready
	call waitDiskReady
	
	;; Get the command register number of the appropriate controller 
	;; in DX
	mov DX, word [COMSTAT]	

	;; Clear the "interrupt received" byte
	mov byte [INTERRUPTRECEIVED], 0
	
	;; Send the recalibrate command
	mov AL, 10h
	out DX, AL

	;; Wait for the recalibration to complete
	call waitOperationComplete
		
	;; Make sure we didn't time out
	cmp EAX, -1
	je .timeOut

	;; Check for disk controller errors.  Test the error bit in the 
	;; status register.
	mov DX, word [COMSTAT]
	in AL, DX
	bt AX, 0
	jc .error

	;; The current cylinder will now be 0
	mov word [CURRENTCYLINDER], 0

	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4

	;; Return success
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0
	popa
	mov EAX, 0
	ret

	.error:
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4

	;; An error occurred during the command.  Check for disk 
	;; controller errors
	call errorStatus
	popa
	mov EAX, -1
	ret

	.timeOut:
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4

	mov dword [LASTERRORMESSAGE], ETO	; Time out error message
	mov byte [LASTERRORCODE], ERR_TIMEOUT
	popa
	mov EAX, -2
	ret
	

kernelIdeDriverReadSectors:
	;; This routine reads sectors from the disk.  It will use either
	;; the CHS method or the LBA method, depending on the parameters
	;; specified.  The prototype looks like this:
	;; int kernelIdeDriverReadSectors(int diskNum, int head,
	;;	 int cylinder, int startSector, unsigned LBA,
	;; 	 int sectorCount, void *buffer);
	;; If the startSector value is 0 (which is invalid on a real disk)
	;; the routine will assume we are using LBA, and use that parameter
	;; instead of the CHS method.  Returns 0 on success, negative
	;; otherwise.
	
	pusha

	;; Save the current stack pointer
	mov EBP, ESP

	;; Wait for a lock on the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerLock
	add ESP, 4

	cmp EAX, 0
	jne near .error
		
	;; Get the requested disk number
	mov EAX, dword [SS:(EBP + 36)]

	;; Select the disk
	push dword EAX
	call selectDrive
	add ESP, 4
		
	;; Wait for the controller to be ready
	call waitControllerReady

	;; Make sure we didn't time out
	cmp EAX, -1
	je near .timeOut

	;; We have to figure out whether we're using LBA or not.  The
	;; method we have defined is to check whether the startSector
	;; parameter is zero.  If it is, we're using LBA
	cmp dword [SS:(EBP + 48)], 0
	je .LBA
	
	;; We're not using LBA, so we have to pay attention to the
	;; head, cylinder, and startSector parameters when we fill
	;; the command registers

	push dword [SS:(EBP + 48)]	; Starting sector
	push dword [SS:(EBP + 44)]	; Cylinder
	push dword [SS:(EBP + 40)]	; Head
	call CHSSetup
	add ESP, 12
	
	;; We're finished with the CHS-specific setup.  Now we can issue
	;; the command to the disk controller
	jmp .goCommand
	
	.LBA:
	;; We will be using LBA for this operation.  We will assume that
	;; the LBA address is in the appropriate spot.  Get it off the
	;; stack and break it up into the various ports where it should
	;; be deposited.

	push dword [SS:(EBP + 52)]	; LBA Address
	call LBASetup
	add ESP, 4
	
	;; We're done with the LBA-specific set-up.  Now we can issue
	;; the command to the disk controller
	
	.goCommand:
	;; This is where we send the actual command to the disk
	;; controller.  We still have to get the number of sectors
	;; to read, since this is common to both the LBA and CHS
	;; methods.
	
	;; Get the sector count register of the selected disk in DX
	mov DX, word [SECTORCOUNT]		; Number to transfer

	;; Get the sector count number in EAX
	mov EAX, dword [SS:(EBP + 56)]

	;; Check this sector number.  If it's 256, we need to change it
	;; to zero.  If it's more than 256, it's an error.
	cmp EAX, 256
	jb .countOK
	ja near .error

	;; Make it zero, which means 256 to the disk controller
	xor EAX, EAX
	
	.countOK:	
	;; Send the sector count byte
	out DX, AL

	;; Wait for the selected disk to be ready
	call waitDiskReady
	
	;; Get the command register number of the appropriate controller 
	;; in DX
	mov DX, word [COMSTAT]	

	;; Clear the "interrupt received" byte
	mov byte [INTERRUPTRECEIVED], 0
	
	;; Send the "read multiple sectors with retries" command
	mov AL, 20h
	out DX, AL
		
	;; Set the number of bytes read value to zero
	mov dword [BYTESXFERRED], 0

	.dataLoop:
	;; We must now read the data from the disk controller, and
	;; put it in the target location.  We need to do a loop, reading
	;; the data one dword at a time after we ensure that no error
	;; occurred

	;; Wait for the controller to have data ready for us
	call waitOperationComplete
		
	;; Make sure we didn't time out
	cmp EAX, -1
	je near .timeOut

	;; Check for disk controller errors.  Test the error bit in the 
	;; status register.
	mov DX, word [COMSTAT]
	in AL, DX
	bt AX, 0
	jc .error

	;; Ok, the data should be waiting for us at the data port.  We
	;; need to do a little loop to read a sector's worth of data
	;; from there
	mov DX, word [DATA]		; Controller's data port
	mov EDI, dword [SS:(EBP + 60)]	; buffer location
	add EDI, dword [BYTESXFERRED]	; add bytes read so far
	mov ECX, (512 / 2)		; Sector size, 2 bytes per rep
	cld			; Clear the direction flag
	pushfd
	cli			; Disable interrupts while transferring data
	
	rep insw	; Moves ECX words from port specified in DX to 
			; location at [ES:EDI] and increments EDI by
			; the appropriate number of bytes

	;; Restore interrupts
	popfd

	;; Keep track of what we've read
	add dword [BYTESXFERRED], 512

	;; Are we still awaiting more data from the controller that hasn't
	;; arrived yet?  Shift right by 9 to get sectors read (divide by 512)
	mov EAX, dword [BYTESXFERRED]
	shr EAX, 9
	cmp EAX, dword [SS:(EBP + 56)]
	jb .dataLoop

	;; We are finished.  The data should be transferred.
		
	;; The current cylinder will now be the one requested for
	;; the read operation
	mov EAX, dword [SS:(EBP + 44)]
	mov word [CURRENTCYLINDER], AX

	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	;; Return success
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0
	popa
	mov EAX, 0
	ret

	.error:
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	;; An error occurred during the command.  Call the error routine
	;; to determine the error information
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0
	call errorStatus
	popa
	mov EAX, -1
	ret

	.timeOut:
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	mov dword [LASTERRORMESSAGE], ETO	; Time out error message
	mov byte [LASTERRORCODE], ERR_TIMEOUT
	popa
	mov EAX, -2
	ret
	
	
kernelIdeDriverWriteSectors:
	;; This routine writes sectors to the disk.  It will use either
	;; the CHS method or the LBA method, depending on the parameters
	;; specified.  The prototype looks like this:
	;; int kernelIdeDriverWriteSectors(int diskNum, int head,
	;;	 int cylinder, int startSector, unsigned LBA,
	;; 	 int sectorCount, void *buffer);
	;; If the startSector value is 0 (which is invalid on a real disk)
	;; the routine will assume we are using LBA, and use that parameter
	;; instead of the CHS method.  Returns 0 on success, negative
	;; otherwise.

	pusha

	;; Save the current stack pointer
	mov EBP, ESP

	;; Wait for a lock on the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerLock
	add ESP, 4

	cmp EAX, 0
	jne near .error
		
	;; Get the requested disk number
	mov EAX, dword [SS:(EBP + 36)]

	;; Select the disk
	push dword EAX
	call selectDrive
	add ESP, 4
		
	;; Wait for the controller to be ready
	call waitControllerReady

	;; Make sure we didn't time out
	cmp EAX, -1
	je near .timeOut

	;; We have to figure out whether we're using LBA or not.  The
	;; method we have defined is to check whether the startSector
	;; parameter is zero.  If it is, we're using LBA
	cmp dword [SS:(EBP + 48)], 0
	je .LBA
	
	;; We're not using LBA, so we have to pay attention to the
	;; head, cylinder, and startSector parameters when we fill
	;; the command registers

	push dword [SS:(EBP + 48)]	; Starting sector
	push dword [SS:(EBP + 44)]	; Cylinder
	push dword [SS:(EBP + 40)]	; Head
	call CHSSetup
	add ESP, 12
	
	;; We're finished with the CHS-specific setup.  Now we can issue
	;; the command to the disk controller
	jmp .goCommand
	
	.LBA:
	;; We will be using LBA for this operation.  We will assume that
	;; the LBA address is in the appropriate spot.  Get it off the
	;; stack and break it up into the various ports where it should
	;; be deposited.

	push dword [SS:(EBP + 52)]	; LBA Address
	call LBASetup
	add ESP, 4
	
	;; We're done with the LBA-specific set-up.  Now we can issue
	;; the command to the disk controller
	
	.goCommand:
	;; This is where we send the actual command to the disk
	;; controller.  We still have to get the number of sectors
	;; to read, since this is common to both the LBA and CHS
	;; methods.
	
	;; Get the sector count register of the selected disk in DX
	mov DX, word [SECTORCOUNT]		; Number to transfer

	;; Get the sector count number in EAX
	mov EAX, dword [SS:(EBP + 56)]

	;; Check this sector number.  If it's 256, we need to change it
	;; to zero.  If it's more than 256, it's an error.
	cmp EAX, 256
	jb .countOK
	ja near .error

	;; Make it zero, which means 256 to the disk controller
	xor EAX, EAX
	
	.countOK:	
	;; Send the sector count byte
	out DX, AL

	;; Wait for the selected disk to be ready
	call waitDiskReady
	
	;; Get the command register number of the appropriate controller 
	;; in DX
	mov DX, word [COMSTAT]	

	;; Clear the "interrupt received" byte
	mov byte [INTERRUPTRECEIVED], 0
	
	;; Send the "read multiple sectors with retries" command
	mov AL, 30h
	out DX, AL
		
	;; Set the number of bytes read value to zero
	mov dword [BYTESXFERRED], 0

	.dataLoop:
	;; We must now read the data from the disk controller, and
	;; put it in the target location.  We need to do a loop, reading
	;; the data one word at a time after we ensure that no error
	;; occurred

	;; Ok, the data should be waiting for us at the data port.  We
	;; need to do a little loop to read a sector's worth of data
	;; from there
	mov ESI, dword [SS:(EBP + 60)]	; buffer location
	add ESI, dword [BYTESXFERRED]	; add bytes read so far
	mov ECX, (512 / 2)		; Sector size, 2 bytes per rep
	
	pushfd
	cli			; Disable interrupts while transferring data
	.sectorLoop:
	mov DX, word [COMSTAT]	
	in AL, DX
	bt AX, 3
	jnc .sectorLoop
	mov DX, word [DATA]		; Controller's data port
	mov AX, word [ES:ESI]
	out DX, AX
	add ESI, 2
	loop .sectorLoop	
	
	;; Restore interrupts
	popfd

	;; Wait for the controller to have data ready for us
	call waitOperationComplete
		
	;; Make sure we didn't time out
	cmp EAX, -1
	je near .timeOut

	;; Check for disk controller errors.  Test the error bit in the 
	;; status register.
	mov DX, word [COMSTAT]
	in AL, DX
	bt AX, 0
	jc .error

	;; Keep track of what we've read
	add dword [BYTESXFERRED], 512

	;; Are we still awaiting more data from the controller that hasn't
	;; arrived yet?  Shift right by 9 to get sectors read (divide by 512)
	mov EAX, dword [BYTESXFERRED]
	shr EAX, 9
	cmp EAX, dword [SS:(EBP + 56)]
	jb .dataLoop

	;; We are finished.  The data should be transferred.
		
	;; The current cylinder will now be the one requested for
	;; the read operation
	mov EAX, dword [SS:(EBP + 44)]
	mov word [CURRENTCYLINDER], AX

	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	;; Return success
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0
	popa
	mov EAX, 0
	ret

	.error:
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	;; An error occurred during the command.  Call the error routine
	;; to determine the error information
	mov dword [LASTERRORMESSAGE], E0
	mov byte [LASTERRORCODE], 0
	call errorStatus
	popa
	mov EAX, -1
	ret

	.timeOut:
	;; Unlock the controller
	push dword CONTROLLER_LOCK
	call kernelResourceManagerUnlock
	add ESP, 4
	
	mov dword [LASTERRORMESSAGE], ETO	; Time out error message
	mov byte [LASTERRORCODE], ERR_TIMEOUT
	popa
	mov EAX, -2
	ret
	
	
kernelIdeDriverLastErrorCode:
	;; This routine can be called to get the numeric code
	;; associated with the last error.  It takes no parameters, 
	;; and returns the integer code

	xor EAX, EAX
	mov AL, byte [LASTERRORCODE]
	ret


kernelIdeDriverLastErrorMessage:
	;; This routine can be called to get the message associated
	;; with the last error.  It takes no parameters, and returns
	;; a pointer to the appropriate error message

	mov EAX, dword [LASTERRORMESSAGE]
	ret
	

kernelIdeDriverReceiveInterrupt:
	;; This routine will be called whenever the disk drive issues
	;; its service interrupt.  It will simply change a data value
	;; to indicate that one has been received, and acknowldege the
	;; interrupt to the PIC.  It's up to other routines to do something 
	;; useful with the information.

	;; Tell the other routines that the operation is finished
	mov byte [INTERRUPTRECEIVED], 1

	ret


	SEGMENT .data
	ALIGN 4
	
	;; Error stuff
LASTERRORMESSAGE	dd 0, 0, 0, 0
LASTERRORCODE		db 0, 0, 0, 0

	;; Port numbers for the currently selected disk.
DATA		dw 0
ERROR		dw 0
SECTORCOUNT	dw 0
SECTORNUMBER	dw 0
CYLINDERLOW	dw 0
CYLINDERHIGH	dw 0
DRIVEHEAD	dw 0
COMSTAT		dw 0
	
	;; Port numbers for the 8 possible disks
_DATA		dw 01F0h, 01F0h, 0170h, 0170h, 00F0h, 00F0h, 0070h, 0070h
_ERROR		dw 01F1h, 01F1h, 0171h, 0171h, 00F1h, 00F1h, 0071h, 0071h
_SECTORCOUNT	dw 01F2h, 01F2h, 0172h, 0172h, 00F2h, 00F2h, 0072h, 0072h
_SECTORNUMBER	dw 01F3h, 01F3h, 0173h, 0173h, 00F3h, 00F3h, 0073h, 0073h
_CYLINDERLOW	dw 01F4h, 01F4h, 0174h, 0174h, 00F4h, 00F4h, 0074h, 0074h
_CYLINDERHIGH	dw 01F5h, 01F5h, 0175h, 0175h, 00F5h, 00F5h, 0075h, 0075h
_DRIVEHEAD	dw 01F6h, 01F6h, 0176h, 0176h, 00F6h, 00F6h, 0076h, 0076h
_COMSTAT	dw 01F7h, 01F7h, 0177h, 0177h, 00F7h, 00F7h, 0077h, 0077h

	ALIGN 4
	
	;; Data from the drive controller and data about the 
	;; disk drive environment
CONTROLLER_LOCK		dd 0
BYTESXFERRED		dd 0
CURRENTCYLINDER		dw 0
SELECTEDDISK		db 0
INTERRUPTRECEIVED	db 0
	
	;; Error messages
E0	db 'No previous error message', 0
E1	db 'Address mark not found', 0
E2	db 'Track 0 not found', 0
E3	db 'Command aborted - invalid command', 0
E4	db 'Media change requested', 0
E5	db 'ID or target sector not found', 0
E6	db 'Media changed', 0
E7	db 'Uncorrectable data error', 0
E8	db 'Bad sector detected', 0
EUN	db 'Unknown error', 0
ETO	db 'Command timed out', 0
