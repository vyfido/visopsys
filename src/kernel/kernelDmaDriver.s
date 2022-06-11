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
;;  kernelDmaDriver.s
;;
	
%define STATREG 	(0 * 2)
%define COMMREG		(1 * 2)
%define REQREG		(2 * 2)
%define MASKREG		(3 * 2)
%define MODEREG		(4 * 2)
%define CLEARREG	(5 * 2)
%define TEMPREG		(6 * 2)
%define DISREG		(7 * 2)
%define CLMASKREG	(8 * 2)
%define WRMASKREG	(9 * 2)
			
	SEGMENT .text
	BITS 32

	GLOBAL kernelDmaDriverInitialize
	GLOBAL kernelDmaDriverSetupChannel
	GLOBAL kernelDmaDriverSetMode
	GLOBAL kernelDmaDriverEnableChannel
	GLOBAL kernelDmaDriverCloseChannel

	%include "kernelAssemblerHeader.h"


	;; These driver routines should always return an int as a
	;; status indicator.

	
kernelDmaDriverEnableController:
	;; Enables the selected DMA controller.  Disabling is recommended
	;; before setting other registers.  The stack parameter looks 
	;; like this:
	;; 0 - controller number (1 or 2)

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	cmp dword [SS:(EBP + 36)], 1	; Controller number
	je .controllerOne
	cmp dword [SS:(EBP + 36)], 2	; Controller number
	je .controllerTwo

	jmp .errorDone		; Ooops.  Error, wrong number

	.controllerOne:
	;; Controller 1.  Get the port number
	mov DX, word [DMA1PORTS + COMMREG]
	jmp .selectedController
	
	.controllerTwo:
	;; Controller 2.  Get the port number
	mov DX, word [DMA2PORTS + COMMREG]

	.selectedController:
	;; Clear interrupts while setting DMA controller registers
	pushfd
	cli
	
	xor AL, AL		; Bit 2 is cleared
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Reenable interrupts
	popfd

	.done:
	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:	
	popa
	mov EAX, -1		; Return failure
	ret
	

kernelDmaDriverDisableController:
	;; Disables the selected DMA controller, which is recommended
	;; before setting other registers.  The stack parameter looks 
	;; like this:
	;; 0 - controller number (1 or 2)

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	cmp dword [SS:(EBP + 36)], 1	; Controller number
	je .controllerOne
	cmp dword [SS:(EBP + 36)], 2	; Controller number
	je .controllerTwo
	
	jmp .errorDone		; Error, wrong number

	.controllerOne:
	;; Controller 1.  Get the port number
	mov DX, word [DMA1PORTS + COMMREG]
	jmp .selectedController
	
	.controllerTwo:
	;; Controller 2.  Get the port number
	mov DX, word [DMA2PORTS + COMMREG]

	.selectedController:
	;; Clear interrupts while setting DMA controller registers
	pushfd
	cli
	
	mov AL, 04h		; Bit 2 is set
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Reenable interrupts
	popfd

	.done:
	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:	
	popa
	mov EAX, -1		; Return failure
	ret
	

kernelDmaDriverInitialize:
	;; This routine will initialize the DMA controllers and this
	;; driver

	mov EAX, 0		; Return success
	ret


kernelDmaDriverResetByteFlipFlop:
	;; Resets the byte flip-flop of the selected DMA controller, 
	;; which is required before doing word-writes on byte-ports.  
	;; The stack parameter looks like this:
	;; 0 - controller number (1 or 2)
	;; ASSUMES THAT THE APPROPRIATE CONTROLLER HAS BEEN DISABLED
	;; IN ORDER TO SET THE APPROPRIATE REGISTER.

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	cmp dword [SS:(EBP + 36)], 1	; Controller number
	je .controllerOne
	cmp dword [SS:(EBP + 36)], 2	; Controller number
	je .controllerTwo
	
	jmp .errorDone		; Error, wrong number

	.controllerOne:
	;; Controller 1.  Get the port number
	mov DX, word [DMA1PORTS + CLEARREG]
	jmp .selectedController
	
	.controllerTwo:
	;; Controller 2.  Get the port number
	mov DX, word [DMA2PORTS + CLEARREG]

	.selectedController:
	;; Clear interrupts while setting DMA controller registers
	pushfd
	cli
	
	out DX, AL		; AL's value is unimportant
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Reenable interrupts
	popfd

	.done:
	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:	
	popa
	mov EAX, -1		; Return failure
	ret
	

kernelDmaDriverSetupChannel:	
	;; This routine prepares the registers of the specified DMA
	;; channel for a data transfer.   This routine calls a series 
	;; of other routines that set individual registers.  The parameters 
	;; look like this on the stack:
	;; 0 - Channel to set up
	;; 1 - Base and current address
	;; 2 - Base and current count
	;; 3 - Page register

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Determine the port addresses to use based on the channel number
	mov EAX, dword [SS:(EBP + 36)]		; Channel number

	cmp EAX, 00000000h
	je .channel0
	cmp EAX, 00000001h
	je .channel1
	cmp EAX, 00000002h
	je .channel2
	cmp EAX, 00000003h
	je .channel3
	cmp EAX, 00000004h
	je .channel4
	cmp EAX, 00000005h
	je .channel5
	cmp EAX, 00000006h
	je .channel6
	cmp EAX, 00000007h
	je .channel7

	;; Oops.  This isn't a valid channel number.
	jmp .errorDone

	.channel0:
	mov ESI, CHANNEL0PORTS
	jmp .gotPortsAddress
	
	.channel1:	
	mov ESI, CHANNEL1PORTS
	jmp .gotPortsAddress
	
	.channel2:	
	mov ESI, CHANNEL2PORTS
	jmp .gotPortsAddress
	
	.channel3:	
	mov ESI, CHANNEL3PORTS
	jmp .gotPortsAddress
	
	.channel4:	
	mov ESI, CHANNEL4PORTS
	jmp .gotPortsAddress
	
	.channel5:	
	mov ESI, CHANNEL5PORTS
	jmp .gotPortsAddress
	
	.channel6:	
	mov ESI, CHANNEL6PORTS
	jmp .gotPortsAddress
	
	.channel7:
	mov ESI, CHANNEL7PORTS
	;; Fall through to 'gotPortsAddress'

	.gotPortsAddress:	
	;; Clear out some registers
	xor EBX, EBX
	xor ECX, ECX
	xor EDX, EDX
	
	mov BX, word [ESI]
	mov CX, word [ESI + 2]
	mov DX, word [ESI + 4]

	
	;; Disable the appropriate controller while setting register values
	cmp dword [SS:(EBP + 36)], 4		; Channel number
	jae .disableControllerTwo
	push dword 1
	jmp .goDisableController
	.disableControllerTwo:
	push dword 2
	.goDisableController:
	call kernelDmaDriverDisableController
	add ESP, 4

	;; Error-check the result from the previous call
	cmp EAX, 0
	je .disableOK
	jmp .errorDone		; Error recorded

	.disableOK:
	;; We should have the port number for the Base and current 
	;; address, the Base and current count, and the Page register
	;; in EBX, ECX, and EDX, respectively.  We will pass them to the 
	;; routines that deal with those specific DMA registers.

	;; Reset the byte flip-flop before the following action, as it
	;; requires two consecutive port writes.
	cmp dword [SS:(EBP + 36)], 4		; Channel number
	jae .rbffControllerTwo
	push dword 1
	jmp .goRbffController
	.rbffControllerTwo:
	push dword 2
	.goRbffController:
	call kernelDmaDriverResetByteFlipFlop
	add ESP, 4
	
	;; Error-check the result from the previous call
	cmp EAX, 0
	je .rbffOK
	jmp .errorDone		; Error recorded

	.rbffOK:
	;; Set the base and current address register
	push dword [SS:(EBP + 40)]		; Address
	push dword EBX
	call kernelDmaDriverSetBaseAndCurrentAddress
	add ESP, 8

	;; This function always returns success

	;; Set the base and current count register, but subtract 1 first
	sub dword [SS:(EBP + 44)], 1		; Count
	push dword [SS:(EBP + 44)]		; Count
	push dword ECX
	call kernelDmaDriverSetBaseAndCurrentCount
	add ESP, 8

	;; This function always returns success

	;; Set the page register
	push dword [SS:(EBP + 48)]		; Page
	push dword EDX
	call kernelDmaDriverSetPageRegister
	add ESP, 8

	;; This function always returns success

	;; Re-enable the appropriate controller
	cmp dword [SS:(EBP + 36)], 4		; Channel number
	jae .controllerTwo2
	push dword 1
	jmp .enableController
	.controllerTwo2:
	push dword 2
	.enableController:
	call kernelDmaDriverEnableController
	add ESP, 4
	
	;; Error-check the result from the previous call
	cmp EAX, 0
	je .done
	jmp .errorDone		; Error recorded

	
	.done:
	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:	
	popa
	mov EAX, -1		; Return failure
	ret


kernelDmaDriverSetBaseAndCurrentAddress:
	;; Sets the base/current address registers of the selected
	;; channel.  The stack parameters look like this:
	;; 0 - Port number
	;; 1 - Address to use 
	;; ASSUMES THAT THE APPROPRIATE CONTROLLER HAS BEEN DISABLED
	;; AND THAT THE CONTROLLER'S BYTE FLIP-FLOP HAS BEEN RESET
	;; IN ORDER TO SET THE APPROPRIATE REGISTER.

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Get the parameters ready
	mov EAX, [SS:(EBP + 40)]		; Address
	mov EDX, [SS:(EBP + 36)]		; Port number
			
	;; Clear interrupts while setting DMA controller registers
	pushfd
	cli
	
	;; Set the controller register
	;; Send the low byte
	out DX, AL
	jecxz $+2		; A slight delay might be necessary
	jecxz $+2		; before the second consecutive access
	;; Send the high byte
	shr EAX, 8
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Reenable interrupts
	popfd

	popa
	mov EAX, 0		; Return success
	ret
	

kernelDmaDriverSetBaseAndCurrentCount:
	;; Sets the base/current count registers of the selected
	;; channel.  The stack parameters look like this:
	;; 0 - Port number
	;; 1 - Count to use 
	;; ASSUMES THAT THE APPROPRIATE CONTROLLER HAS BEEN DISABLED
	;; AND THAT THE CONTROLLER'S BYTE FLIP-FLOP HAS BEEN RESET
	;; IN ORDER TO SET THE APPROPRIATE REGISTER.

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Get the parameters ready
	mov EAX, [SS:(EBP + 40)]		; Count
	mov EDX, [SS:(EBP + 36)]		; Port number
			
	;; Clear interrupts while setting DMA controller registers
	pushfd
	cli
	
	;; Set the controller register
	;; Send the low byte
	out DX, AL
	jecxz $+2		; A slight delay might be necessary
	jecxz $+2		; before the second consecutive access
	;; Send the high byte
	shr EAX, 8
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Reenable interrupts
	popfd

	popa
	mov EAX, 0		; Return success
	ret
	

kernelDmaDriverSetPageRegister:
	;; Sets the "page" register of the selected
	;; channel.  The stack parameters look like this:
	;; 0 - Port number
	;; 1 - Page register value to use 
	;; ASSUMES THAT THE APPROPRIATE CONTROLLER HAS BEEN DISABLED

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Get the parameters ready
	mov EAX, [SS:(EBP + 40)]		; Page
	mov EDX, [SS:(EBP + 36)]		; Port number
			
	;; Clear interrupts while setting DMA controller registers
	pushfd
	cli
	
	;; Set the controller register
	;; This is only a single-byte register.  We only send the low byte
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Reenable interrupts
	popfd

	popa
	mov EAX, 0		; Return success
	ret
	

kernelDmaDriverEnableChannel:
	;; This routine enables the selected DMA channel by clearing the
	;; appropriate mask bit.  The stack parameters look like this:
	;; 0 - Channel number

	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Figure out which controller to use depending on the channel
	;; number.
	cmp dword [SS:(EBP + 36)], 4	; Channel number
	jb .useFirstController
	cmp dword [SS:(EBP + 36)], 8	; Channel number
	jb .useSecondController

	jmp .errorDone		; Ooops.  Invalid channel number

	.useFirstController:
	;; Use the first controller
	mov DX, word [DMA1PORTS + MASKREG]

	;; Push the argument to disable the controller while setting registers
	push dword 1

	jmp .goSelectedController
	
	.useSecondController:
	;; Use the second controller
	mov DX, word [DMA2PORTS + MASKREG]

	;; Push the argument to disable the controller while setting registers
	push dword 2

	.goSelectedController:
	;; Disable the controller.  The port argument should be on the stack
	call kernelDmaDriverDisableController
	add ESP, 4
	
	;; Error-check the result from the previous call
	cmp EAX, 0
	je .controllerDisabledOK
	jmp .errorDone

	.controllerDisabledOK:	
	;; Get the channel number into AL
	mov EAX, dword [SS:(EBP + 36)]		; Channel number
	
	;; Mask out all but the bottom two bits.  This technique will
	;; work for both controllers, which expect a value between 0
	;; and 3.  Think about it, bit-wise.  It works for channels
	;; 4 through 7 as well as it does for 0 through 3.
	and AL, 00000011b

	;; The mask bit will already be cleared, so we're ready
	;; to go.

	;; Clear interrupts while setting DMA controller registers
	pushfd
	cli
	
	;; AL has the command.  DX has the port.  Do it.
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Reenable interrupts
	popfd

	;; Re-enable the appropriate controller
	cmp dword [SS:(EBP + 36)], 4		; Channel number
	jae .enableControllerTwo
	push dword 1
	jmp .goEnableController
	.enableControllerTwo:
	push dword 2
	.goEnableController:
	call kernelDmaDriverEnableController
	add ESP, 4

	;; Error-check the result from the previous call
	cmp EAX, 0
	jne .errorDone	
		
	.done:	
	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:	
	popa
	mov EAX, -1		; Return failure
	ret

	
kernelDmaDriverCloseChannel:
	;; This routine disables the selected DMA channel by setting the
	;; appropriate mask bit.  The stack parameters look like this:
	;; 0 - Channel number
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Figure out which controller to use depending on the channel
	;; number.
	cmp dword [SS:(EBP + 36)], 4	; Channel number
	jb .useFirstController
	cmp dword [SS:(EBP + 36)], 8	; Channel number
	jb .useSecondController

	jmp .errorDone		; Ooops.  Invalid channel number

	.useFirstController:
	;; Use the first controller
	mov DX, word [DMA1PORTS + MASKREG]
	
	;; Push the argument to disable the controller while setting registers
	push dword 1
	jmp .goSelectedController
	
	.useSecondController:
	;; Use the second controller
	mov DX, word [DMA2PORTS + MASKREG]

	;; Push the argument to disable the controller while setting registers
	push dword 2

	.goSelectedController:
	;; Disable the controller.  The port argument should be on the stack
	call kernelDmaDriverDisableController
	add ESP, 4
	
	;; Error-check the result from the previous call
	cmp EAX, 0
	jne .errorDone	
		
	;; Get the channel number into AL
	mov EAX, dword [SS:(EBP + 36)]		; Channel number

	;; Mask out all but the bottom two bits.  This technique will
	;; work for both controllers, which expect a value between 0
	;; and 3.  Think about it, bit-wise.  It works for channels
	;; 4 through 7 as well as it does for 0 through 3.
	and AL, 00000011b

	;; Set the mask bit (disable the channel)
	or AL, 00000100b

	;; Clear interrupts while setting DMA controller registers
	pushfd
	cli
	
	;; AL has the command.  DX has the port.  Do it.
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2

	;; Reenable interrupts
	popfd

	;; Re-enable the appropriate controller
	cmp dword [SS:(EBP + 36)], 4		; Channel number
	jae .enableControllerTwo
	push dword 1
	jmp .goEnableController
	.enableControllerTwo:
	push dword 2
	.goEnableController:
	call kernelDmaDriverEnableController
	add ESP, 4

	;; Error-check the result from the previous call
	cmp EAX, 0
	jne .errorDone	
		
	.done:
	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:	
	popa
	mov EAX, -1		; Return failure
	ret

	
kernelDmaDriverSetMode:	
	;; This routine sets the transfer mode on the specified channel.
	;; The stack parameters look like this:
	;; 0 - Channel number
	;; 1 - Mode
	
	pusha

	;; Save the stack pointer
	mov EBP, ESP

	;; Figure out which port to use based on the channel number
	cmp dword [SS:(EBP + 36)], 4	; Channel number
	jb .useFirstController
	cmp dword [SS:(EBP + 36)], 8	; Channel number
	jb .useSecondController

	jmp .errorDone		; Ooops.  Invalid channel number

	.useFirstController:
	;; Use the first controller
	mov DX, word [DMA1PORTS + MODEREG]
	
	;; Push the argument to disable the controller while setting registers
	push dword 1
	jmp .goSelectedController
	
	.useSecondController:
	;; Use the second controller
	mov DX, word [DMA2PORTS + MODEREG]

	;; Push the argument to disable the controller while setting registers
	push dword 2

	.goSelectedController:
	;; Disable the controller.  The parameter should be on the stack
	call kernelDmaDriverDisableController
	add ESP, 4
	
	;; Error-check the result from the previous call
	cmp EAX, 0
	jne .errorDone

	;; Get the channel number from the parameter on the stack
	mov EAX, dword [SS:(EBP + 36)]		; Channel number
	;; Get the mode number
	mov EBX, dword [SS:(EBP + 40)]		; Mode
	;; "or" the channel with the mode
	or AL, BL

	;; Clear interrupts while setting DMA controller registers
	pushfd
	cli

	;; Now we can set the mode.  DX has the port number and
	;; AL has the mode/channel
	
	out DX, AL
	;; Delay
	jecxz $+2
	jecxz $+2
		
	;; Reenable interrupts
	popfd

	;; Re-enable the appropriate controller
	cmp dword [SS:(EBP + 36)], 4
	jae .enableControllerTwo
	push dword 1
	jmp .goEnableController
	.enableControllerTwo:
	push dword 2
	.goEnableController:
	call kernelDmaDriverEnableController
	add ESP, 4
	
	;; Error-check the result from the previous call
	cmp EAX, 0
	jne .errorDone	

	.done:
	popa
	mov EAX, 0		; Return success
	ret

	.errorDone:	
	popa
	mov EAX, -1		; Return failure
	ret

	
	SEGMENT .data
	ALIGN 4
	
;; These contain the global port addresses specific to each DMA controller.

DMA1PORTS:
		dw	08h	;; 0. Status register
		dw	08h	;; 1. Command register
		dw	09h	;; 2. Request register
		dw	0Ah	;; 3. Mask register
		dw	0Bh	;; 4. Mode register
		dw	0Ch	;; 5. Clear byte
		dw	0Dh	;; 6. Temporary register
		dw	0Dh	;; 7. Master disable
		dw	0Eh	;; 8. Clear mask register
		dw	0Fh	;; 9. Write all mask bits
	
DMA2PORTS:
		dw	0D0h	;; 0. Status register
		dw	0D0h	;; 1. Command register
		dw	0D2h	;; 2. Request register
		dw	0D4h	;; 3. Mask register
		dw	0D6h	;; 4. Mode register
		dw	0D8h	;; 5. Clear byte
		dw	0DAh	;; 6. Temporary register
		dw	0DAh	;; 7. Master disable
		dw	0DCh	;; 8. Clear mask register
		dw	0DEh	;; 9. Write all mask bits


;; These contain the port addresses specific to each channel.

CHANNEL0PORTS:
		dw	00h	;; 0. Base and current address
		dw	01h	;; 1. Base and current count
		dw	87h	;; 2. Page register

CHANNEL1PORTS:
		dw	02h	;; 0. Base and current address
		dw	03h	;; 1. Base and current count
		dw	83h	;; 2. Page register
	
CHANNEL2PORTS:
		dw	04h	;; 0. Base and current address
		dw	05h	;; 1. Base and current count
		dw	81h	;; 2. Page register
	
CHANNEL3PORTS:
		dw	06h	;; 0. Base and current address
		dw	07h	;; 1. Base and current count
		dw	82h	;; 2. Page register

CHANNEL4PORTS:
		dw	0C0h	;; 0. Base and current address
		dw	0C2h	;; 1. Base and current count
		dw	08Fh	;; 2. Page register
	
CHANNEL5PORTS:
		dw	0C4h	;; 0. Base and current address
		dw	0C6h	;; 1. Base and current count
		dw	08Bh	;; 2. Page register
	
CHANNEL6PORTS:
		dw	0C8h	;; 0. Base and current address
		dw	0CAh	;; 1. Base and current count
		dw	089h	;; 2. Page register
	
CHANNEL7PORTS:
		dw	0CCh	;; 0. Base and current address
		dw	0CEh	;; 1. Base and current count
		dw	08Ah	;; 2. Page register
