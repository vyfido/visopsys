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
;;  kernelEntryPoint.s
;;

;; This file contains the assembly code portion of the kernel API's
;; entry point.
	
	
	SEGMENT .text
	BITS 32

	EXTERN kernelApi
	GLOBAL kernelEntryPoint


kernelEntryPoint:
	
	;; This is the initial entry point for the kernel's API.  This
	;;  function will be first the recipient of all calls to the global
	;; call gate.  The C prototype of the function looks like this:
	;; int kernelEntryPoint();  This function will pass a pointer to
	;; the rest of the arguments to a C function that does all the real
	;; work.  When the C call returns, this function does the appropriate
	;; far return.
	

	;; Save one doubleword on the stack for the integer return code
	sub ESP, 4
	
	;; Save regs
	pusha

	;; We get a pointer to the calling function's parameters differently
	;; depending on whether there was a privilege level switch.  Find
	;; out by checking the privilege of the CS register pushed as
	;; part of the return address.
	mov EAX, dword [ESP + 40]
	and AL, 00000011b
	cmp AL, 0
	je .privilegedCaller
	
	;; The caller is unprivileged, so its stack pointer is on our stack
	;; just beyond the return address, at [ESP + 44].
	mov EAX, dword [ESP + 44]
	jmp .call

	.privilegedCaller:
	;; Point to the first parameter, located just beyond the return
	;; address
	mov EAX, ESP
	add EAX, 44

	.call:
	;; Call a C function to to all the work
	push dword EAX		; arg pointer
	call kernelApi
	add ESP, 4
	
	;; Save the return code of the target function in the spot we
	;; reserved on the stack
	mov dword [ESP + 32], EAX

	;; Restore regs
	popa
	
	;; Restore the return code
	pop EAX
	
	;; We need to do a far return to the caller, popping the additional
	;; dword of our single parameter as well.
	retf
