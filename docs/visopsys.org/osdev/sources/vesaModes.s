;;
;; vesaModes.s (adapted from Visopsys OS-loader)
;;
;; Copyright (c) 2000, J. Andrew McLaughlin
;; You're free to use this code in any manner you like, as long as this
;; notice is included (and you give credit where it is due), and as long
;; as you understand and accept that it comes with NO WARRANTY OF ANY KIND.
;; Contact me at <andy@visopsys.org> about any bugs or problems.
;;

	;; Change this to zero if you don't require Linear Framebuffer
	;; abilities in your graphics modes.  This could be turned into
	;; a function parameter of the routine below, if desired.
	%define REQUIRELFB 1

	
	BITS 16
	
	
findGraphicMode:
	;; The VESA 2.0 specification states that they will no longer
	;; create standard video mode numbers, and video card vendors are
	;; no longer required to support the old numbers.  This routine
	;; will dynamically find a supported mode number from the VIDEO BIOS,
	;; according to the desired resolutions, etc.
	
	;; The function takes parameters that describe the desired graphic
	;; mode, (X resolution, Y resolution, and Bits Per Pixel) and returns
	; the VESA mode number in EAX, if found.  Returns 0 on error.
	
	;; The C prototype for this function would look like the following:
	;; int findGraphicMode(short int x_res, short int y_res, 
	;;                     short int bpp);
	;; (Push bpp, then y_res, then x_res onto the 16-bit stack before
	;; calling.  Caller pops them off again after the call.)

	;; Save space on the stack for the mode number we're returning
	sub SP, 2

	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; By default, return the value 0 (failure)
	mov word [SS:(BP + 16)], 0

	;; Get the VESA info block.  Save ES, since this call could
	;; destroy it
        push ES
        mov DI, VESAINFO
        mov AX, 4F00h
        int 10h
        ;; Restore ES
        pop ES

        cmp AX, 004Fh
	;; Fail
        jne .done
	
	;; We need to get a far pointer to a list of graphics mode numbers
	;; from the VESA info block we just retrieved.  Get the offset now,
	;; and the segment inside the loop.
	mov SI, [VESAINFO + 0Eh]

	;; Do a loop through the supported modes
	.modeLoop:
	
	;; Save ES
	push ES

	;; Now get the segment of the far pointer, as mentioned above
	mov AX, [VESAINFO + 10h]
	mov ES, AX

	;; ES:SI is now a pointer to the next supported mode.  The list
	;; terminates with the value FFFFh

	;; Get the first/next mode number
	mov DX, word [ES:SI]

	;; Restore ES
	pop ES

	;; Is it the end of the mode number list?
	cmp DX, 0FFFFh
	je near .done

	;; Increment the pointer for the next loop
	add SI, 2

	;; We have a mode number.  Now we need to do a VBE call to
	;; determine whether this mode number suits our needs.
	;; This call will put a bunch of info in the buffer pointed to
	;; by ES:DI

	mov CX, DX
	mov AX, 4F01h
	mov DI, MODEINFO
	int 10h

	;; Make sure the function call is supported
	cmp AL, 4Fh
	;; Fail
	jne near .done
	
	;; Is the mode supported by this call? (sometimes, they're not)
	cmp AH, 00h
	jne .modeLoop

	;; We need to look for a few features to determine whether this
	;; is the mode we want.  First, it needs to be supported, and it
	;; needs to be a graphics mode.  Next, it needs to match the
	;; requested attributes of resolution and BPP

	;; Get the first word of the buffer
	mov AX, word [MODEINFO]
	
	;; Is the mode supported?
	bt AX, 0
	jnc .modeLoop

	;; Is this mode a graphics mode?
	bt AX, 4
	jnc .modeLoop

	%if REQUIRELFB
	;; Does this mode support a linear frame buffer?
	bt AX, 7
	jnc .modeLoop
	%endif

	;; Does the horizontal resolution of this mode match the requested
	;; number?
	mov AX, word [MODEINFO + 12h]
	cmp AX, word [SS:(BP + 20)]
	jne near .modeLoop

	;; Does the vertical resolution of this mode match the requested
	;; number?
	mov AX, word [MODEINFO + 14h]
	cmp AX, word [SS:(BP + 22)]
	jne near .modeLoop

	;; Do the Bits Per Pixel of this mode match the requested number?
	xor AX, AX
	mov AL, byte [MODEINFO + 19h]
	cmp AX, word [SS:(BP + 24)]
	jne near .modeLoop

	;; If we fall through to here, this is the mode we want.
	mov word [SS:(BP + 16)], DX
	
	.done:
	popa
	;; Return the mode number
	xor EAX, EAX
	pop AX
	ret


;;
;; The data segment
;;

	SEGMENT .data
	ALIGN 4

VESAINFO  	db 'VBE2'		;; Space for info ret by vid BIOS
		times 508  db 0
MODEINFO	times 256 db 0
