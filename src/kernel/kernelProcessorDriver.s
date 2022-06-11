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
;;  kernelProcessorDriver.s
;;

	SEGMENT .text
	BITS 32

	GLOBAL kernelProcessorDriverInitialize
	GLOBAL kernelProcessorDriverReadTimestamp

	%include "kernelAssemblerHeader.h"


kernelProcessorDriverInitialize:
	;; This function will initialize the processor driver
	;; C prototype:	
	;; int kernelProcessorInitialize(void);

	;; Initialize the timestamp counter
	mov dword [TIMESTAMPCOUNTER], 0
	mov dword [TIMESTAMPCOUNTER + 4], 0

	;; Return success
	mov EAX, 0
	ret


kernelProcessorDriverReadTimestamp:
	;; This function will ask the driver to return the value
	;; of its internal timestamp counter.
	;; C prototype:	
	;; unsigned long *kernelProcessorDriverReadTimeStamp(void);

	push EDX

	rdtsc
	mov dword [TIMESTAMPCOUNTER], EAX
	mov dword [TIMESTAMPCOUNTER + 4], EDX

	pop EDX
	mov EAX, TIMESTAMPCOUNTER
	ret
	

	SEGMENT .data
	ALIGN 4
	
TIMESTAMPCOUNTER:	dd 0, 0