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
;;  loaderSplash.s
;;

	GLOBAL loaderDisplaySplash
	GLOBAL loaderRemoveSplash

	EXTERN loaderLoadFile
	EXTERN loaderSetTextDisplay
	EXTERN loaderSaveTextDisplay
	EXTERN loaderRestoreTextDisplay
	EXTERN loaderFindGraphicMode
	EXTERN loaderSetGraphicDisplay
	EXTERN loaderGetScanlineLength
	EXTERN loaderGetLinearFramebuffer
	EXTERN loaderPrint
	EXTERN loaderPrintNumber
	EXTERN loaderPrintNewline
	EXTERN SVGAAVAIL
	EXTERN CURRENTGMODE

	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"
	%include "../kernel/kernelAssemblerHeader.h"


loaderDisplaySplash:
	;; While loading, if possible, display a splash screen to the
	;; user.  If any part of this fails, don't make an error message.
	;; The splash screen is only eye candy; it is inconsequential.

	pusha

	;; Make sure the splash screen isn't on already
	cmp byte [SPLASHON], 0
	jne near .done
		
	;; Do we have SVGA and an appropriate graphics mode?

	;; Do we have SVGA ability?
	cmp byte [SVGAAVAIL], 0
	je near .done

	;; Ask for a mode with 800x600x8bpp
	push word 8
	push word 600
	push word 800
	call loaderFindGraphicMode
	add SP, 6

	;; Did we find an appropriate mode?
	cmp AX, 0
	je .done
	
	;; Save the mode number in BX
	mov BX, AX

	;; Load the image file into memory.  We will temporarily use the 
	;; space reserved for the kernel to load the image file before we
	;; transfer it to the screen buffer.
	push dword KERNELCODEDATALOCATION
	push word IMAGENAME
	call loaderLoadFile
	add SP, 6

	;; Make sure the load was successful
	cmp AX, 0
	jl .done

	;; We have the capabilities we need.  Save the contents of the
	;; current text display
	call loaderSaveTextDisplay

	;; Set the display using the mode number saved in BX
	push BX
	call loaderSetGraphicDisplay
	add SP, 2

	;; Display the bitmap.  Expand it into the screen buffer
	push dword KERNELCODEDATALOCATION
	call expandBitmap
	add SP, 4

	;; Was it successful?
	cmp AX, 0
	jge .success

	;; Oops.  No image.  Cancel, cancel.  Go back to the text display
	push word 1		; Don't clear the display
	call loaderSetTextDisplay
	add SP, 2
	call loaderRestoreTextDisplay
	;; Make note of the fact that the splash screen is off
	mov byte [SPLASHON], 0
	jmp .done	
	
	.success:
	;; Make note of the fact that the splash screen is on
	mov byte [SPLASHON], 1

	.done:
	popa
	ret


loaderRemoveSplash:
	;; This function will remove the splash screen, if it is being
	;; displayed

	pusha

	;; Switch back to text mode, if the splash screen was displayed
	;; successfully
	
	cmp byte [SPLASHON], 1
	jne .done

	;; Go back to the text display
	push word 1		; Don't clear the display
	call loaderSetTextDisplay
	add SP, 2
	
	call loaderRestoreTextDisplay

	;; Make note of the fact that the splash screen is off
	mov byte [SPLASHON], 0
	
	.done:
	popa
	ret


expandBitmap:
	;; This function will expand an 8-bit (256-colour) uncompressed
	;; bitmap from memory into the linear framebuffer.  It expects the
	;; image to be preloaded at the linear memory location passed as a
	;; parameter.

	;; Save a word of stack space for our return value
	sub SP, 2
	
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; Now get from the stack our parameter which tells us where
	;; the compressed image data can be found.
	mov EAX, dword [SS:(BP + 20)]
	mov dword [IMAGEDATA], EAX

	;; IMAGEDATA now contains a linear pointer to the start of the
	;; bitmap image's header in memory.

	;; In order to generically access this bitmap data, we need to make
	;; a segment register that we can use to access all of linear
	;; memory (also the linear framebuffer.  We will briefly switch to
	;; protected mode in order to make the segment register suit our
	;; purposes this way.
	
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
	
	;; Ok -- now GS is our global segment register.  We need to gather
	;; some information from the image header before we can begin to
	;; process and copy the image data.

	mov ESI, dword [IMAGEDATA]

	;; Make sure it's really a bitmap image.  It should have the bytes
	;; "BM" at the beginning
	mov AH, byte 'M'
	mov AL, byte 'B'
	cmp word [GS:ESI], AX
	je .yesBitmap

	;; This is not a bitmap image
	mov word [SS:(BP + 16)], -1
	jmp .done
	
	.yesBitmap:
	;; Now we make sure this is an 8-bit bitmap.
	cmp word [GS:(ESI + 1Ch)], 8
	je .yes8Bit
	
	;; This is not an 8-bit bitmap
	mov word [SS:(BP + 16)], -2
	jmp .done
	
	.yes8Bit:
	;; Now we make sure that this is an uncompressed bitmap
	cmp dword [GS:(ESI + 1Eh)], 0
	je .notCompressed

	;; Oops, this bitmap has been compressed
	mov word [SS:(BP + 16)], -3
	jmp .done
	
	.notCompressed:
	;; That's all the checking we need to do.  Get the address of the
	;; Linear Frame Buffer
	push word [CURRENTGMODE]
	call loaderGetLinearFramebuffer
	add ESP, 2
	
	cmp EAX, 0
	jne .okFramebuffer

	;; Oops, we couldn't get the framebuffer address
	mov word [SS:(BP + 16)], -4
	jmp .done

	.okFramebuffer:
	;; Move the framebuffer address into LFB
	mov dword [LFB], EAX

	;; Get the width and height values of this bitmap (in pixels).
	mov EAX, dword [GS:(ESI + 12h)]
	mov dword [IMAGEWIDTH], EAX
	mov EAX, dword [GS:(ESI + 16h)]
	mov dword [IMAGEHEIGHT], EAX

	;; We need to set the SVGA palette based on the palette described
	;; in the bitmap header.  Conveniently, the data structures in
	;; the bitmap header match the required format for the VBE call.
	;; Inconveniently, the VBE call requires a real-mode pointer to
	;; the palette data, which is difficult to produce when the image
	;; data might be at an address > 1Mb in memory.  We will temporarily
	;; copy the data to our loader's data buffer.

	push ES
	mov AX, (LDRDATABUFFER / 16)
	mov ES, AX
	
	;; Copy palette data.  Take no more than 6 significant bits of
	;; primary colour (assume DAC is set for 6 bits/primary colour)
	mov EBX, ESI
	add EBX, 36h
	xor DI, DI
	mov ECX, dword [GS:(ESI + 2Eh)]	; Number of colours
	shl ECX, 2			; Multiply by 4
	.paletteLoop:
	mov AL, byte [GS:EBX]
	shr AL, 2
	mov byte [ES:DI], AL
	inc EBX
	inc DI
	loop .paletteLoop
	
	;; Now set the palette
	mov AX, 4F09h
	xor BX, BX			; Set palette data
	mov ECX, dword [GS:(ESI + 2Eh)]	; Number of colours
	xor DX, DX			; Start at register 0
	xor DI, DI			; Start at offset 0
	int 10h

	pop ES

	;; That's all the info we need from the header.  Now make ESI
	;; point to the location where the bitmap data starts.
	add ESI, dword [GS:(ESI + 0Ah)]

	;; Bitmaps are upside down.  The first row of pixels we encounter
	;; is actually the last row in the image.  Thus, our starting
	;; offset for EDI will be:
	;; LFB + ((height + ((600 - height) / 2)) * 800) + ((800 - width) / 2)

	mov EDI, dword [LFB]
	
	mov EAX, 600
	sub EAX, dword [IMAGEHEIGHT]
	shr EAX, 1
	add EAX, dword [IMAGEHEIGHT]
	mov EBX, 800
	mul EBX
	add EDI, EAX
		
	mov EAX, 800
	sub EAX, dword [IMAGEWIDTH]
	shr EAX, 1
	add EDI, EAX

	;; Now ESI and EDI are set.  We need to do a single-nested loop
	;; to draw each image, from the bottom up.

	mov EAX, dword [IMAGEHEIGHT]
	push word AX

	.lineLoop:
	mov ECX, dword [IMAGEWIDTH]

	.pixelLoop:
	mov AL, byte [GS:ESI]
	mov byte [GS:EDI], AL
	inc ESI
	inc EDI
	loop .pixelLoop

	;; We've done one line.  Now we reposition EDI so that it will
	;; point to the start of the previous line
	sub EDI, dword [IMAGEWIDTH]
	sub EDI, 800

	pop AX
	dec AX
	cmp AX, 0
	je .doneImage
	push AX
	jmp .lineLoop
	
	.doneImage:
	
	;; Return success
	mov word [SS:(BP + 16)], 0
	
	.done:
	;; Restore GS
	pop GS
	;; Restore regs
	popa
	;; Pop our return code
	xor EAX, EAX
	pop AX
	ret
	

;;
;; The data segment
;;

	SEGMENT .data
	ALIGN 4
	
IMAGEDATA	dd 0
LFB		dd 0
PALETTE		dd 0
IMAGEWIDTH	dd 0
IMAGEHEIGHT	dd 0
SPLASHON	db 0

	
;;
;; The good/informational messages
;;

IMAGENAME	db 'VISOPSYSBMP', 0