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
;;  loaderVideo.s
;;

	EXTERN loaderPrint
	EXTERN loaderPrintNumber
	EXTERN loaderPrintNewline
	EXTERN PRINTINFO
		
	GLOBAL loaderSetTextDisplay
	GLOBAL loaderDetectVideo
	GLOBAL loaderSetGraphicDisplay
	GLOBAL KERNELGMODE
	GLOBAL CURRENTGMODE
		
	SEGMENT .text
	BITS 16
	ALIGN 4

	%include "loader.h"

	
loaderSetTextDisplay:
	
	;; This function will set the text display mode to a known state
	;; for the initial loading messages, etc.

	pusha

	;; Save the stack pointer
	mov BP, SP

	;; Set the active display page to VIDEOPAGE
	mov AH, 05h
	mov AL, VIDEOPAGE
	int 10h

	;; Set the overscan color.  That's the color that forms the
	;; default backdrop, and sometimes appears as a border around
	;; the printable screen
	mov AX, 0B00h
	mov BH, 0
	mov BL, BACKGROUNDCOLOR
	int 10h

	;; We will try to change text modes to a more attractive 80x50
	;; mode.  This takes a couple of steps
	
	;; Change the number of scan lines in the text display
	mov AX, 1202h		; 400 scan lines
	mov BX, 0030h		; Change scan lines command
	int 10h

	;; Set the text video mode to make the change take effect
	mov AX, 0003h
	;; Should we clear the screen?
	mov BX, word [SS:(BP + 18)]
	shl BX, 7
	or AX, BX
	int 10h

	;; The following call messes with ES, so save it
	push ES
	
	;; Change the VGA font to match a 80x50 configuration
	mov AX, 1112h		; 8x8 character set
	mov BL, 0
	int 10h

	;; Restore ES
	pop ES

        ;; Should we blank the screen?
	cmp word [SS:(BP + 18)], 0
	jne .done	
        mov AX, 0700h
        mov BH, BACKGROUNDCOLOR
        and BH, 00000111b
        shl BH, 4
        or BH, FOREGROUNDCOLOR
        mov CX, 0000h
        mov DH, ROWS
        mov DL, COLUMNS
        int 10h

	.done:
	;; We are now in text mode
	mov word [CURRENTGMODE], 0
	
	popa
	ret


loaderDetectVideo:
	;; Check for the mandatory SVGA video card

	;; Save regs
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; We are passed a pointer to the place we should store video
	;; data (the loader's hardware structure).  Save it.
	mov AX, word [SS:(BP + 18)]
	mov word [VIDEODATA_P], AX
	
	;; Save ES, since this call could destroy it
	push ES
	
	mov DI, VESAINFO
	mov AX, 4F00h
	int 10h

	;; Restore ES
	pop ES
	
	;; Bochs hack
	;; jmp .okSVGA
	
	cmp AX, 004Fh
	jne .noSVGA

	;; If the signature 'VESA' appears at the beginning of the info
	;; block, we are OK
	mov EAX, dword [VESAINFO]
	cmp EAX, 41534556h
	je .okSVGA

	.noSVGA:	
	;; There is no SVGA video detected
	mov DL, ERRORCOLOR	; Use the error color
	mov SI, SAD
	call loaderPrint
	mov SI, SVGA
	call loaderPrint
	mov SI, NOSVGA
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, TEXTMODE
	call loaderPrint
	call loaderPrintNewline

	jmp .done

	
	.okSVGA:

	;; Figure out which VESA version this card supports.  Since we use
	;; the Linear Frame Buffer approach, we will require version 2.0
	;; or greater
	mov AX, word [VESAINFO + 04h]
	cmp AX, 0200h
	jae .okVersion
	
	;; This video card is too old to support VESA version 2.0.  We won't
	;; be able to use the Linear Frame Buffer
	mov DL, ERRORCOLOR	; Use the error color
	mov SI, SAD
	call loaderPrint
	mov SI, SVGA
	call loaderPrint
	mov SI, SVGAVER
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, TEXTMODE
	call loaderPrint
	call loaderPrintNewline

	jmp .done

	
	.okVersion:

	cmp word [PRINTINFO], 1
	jne .noPrint1
	;; Indicate SVGA detected
	mov DL, 02h		; Use green color
	mov SI, HAPPY
	call loaderPrint
	mov SI, SVGA
	call loaderPrint

	mov DL, FOREGROUNDCOLOR	; Switch to foreground color
	mov SI, VESA20
	call loaderPrint
	.noPrint1:	
		
	;; Get the amount of video memory
	xor EAX, EAX
	mov AX, word [(VESAINFO + 12h)]
	shl EAX, 6	;; (multiply by 64 to get 1K blocks)
	mov SI, word [VIDEODATA_P]
	mov dword [SI], EAX

	;; Write out how much video memory detected.  The amount is in EAX.
	cmp word [PRINTINFO], 1
	jne .noPrint2
	call loaderPrintNumber
	mov SI, VIDEORAM
	call loaderPrint
	call loaderPrintNewline
	.noPrint2:	
	
	;; Make note that we can use SVGA.
	mov byte [SVGAAVAIL], 1

	
	;; Check whether linear framebuffer is available in any mode
	call checkLinearFramebuffer
	cmp AX, 1
	je .okFramebuffer

	mov DL, ERRORCOLOR	; Use the error color
	mov SI, SAD
	call loaderPrint
	mov SI, SVGA
	call loaderPrint
	mov SI, NOFRAMEBUFF
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, TEXTMODE
	call loaderPrint
	call loaderPrintNewline

	jmp .done
	
	
	.okFramebuffer:	
	
	;; Select a video mode to use for the kernel that this hardware
	;; can display.
	call selectGraphicMode
	
	;; Save the eventual LFB pointer as well
	push word [KERNELGMODE]
	call loaderGetLinearFramebuffer
	add SP, 2
	mov SI, word [VIDEODATA_P]
	mov dword [SI + 8], EAX
	
	.done:
	;; Restore regs
	popa
	
	ret


loaderSetGraphicDisplay:
	
	;; This routine switches the video adapter into the requested 
	;; graphics mode.

	pusha

	;; Save the stack pointer
	mov BP, SP

	;; Make sure SVGA is available
	cmp byte [SVGAAVAIL], 1
	jne near .done
		
	;; Get the requested graphic mode from the stack and save it
	mov AX, word [SS:(BP + 18)]
	mov word [CURRENTGMODE], AX

	;; Get the information about this graphic mode
	mov AX, 4F01h
	mov CX, word [CURRENTGMODE]
	mov DI, MODEINFO
	int 10h

	cmp AX, 004Fh			; Returns this value if successful
	jne near .done

	;; Here's where we actually change to the selected graphics mode
	mov AX, 4F02h			; SVGA function 4F, subfunction 2
	mov BX, word [CURRENTGMODE]	; Mode number
	or BX, 4000h			; Clear, use linear frame buffer
	int 10h

	cmp AX, 004Fh			; Returns this value if successful
	je .switchok

	;; Couldn't switch to graphics mode.  Print an error message.
	mov DL, ERRORCOLOR	; Switch to the error color
	mov SI, SAD
	call loaderPrint
	mov SI, VIDMODE
	call loaderPrint
	mov SI, NOSWITCH1
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, NOSWITCH2
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, TEXTMODE
	call loaderPrint
	call loaderPrintNewline

	mov word [CURRENTGMODE], 0
	jmp .done
	
	.switchok:	
	;; Scan line lengths should be the same (in pixels) as the 
	;; X resolution.  Careful; kills AX, BX, CX, DX
	mov AX, 4F06h
	xor BX, BX
	mov CX, word [MODEINFO + 12h]
	int 10h

	;; Make sure the display starts at the same place as the
	;; frame buffer (line 0, pixel 0).  Careful; kills AX, BX, CX, DX
	mov AX, 4F07h
	xor BX, BX
	mov CX, 0
	mov DX, 0
	int 10h

	.done:
	popa
	ret


loaderFindGraphicMode:
	;; This function takes parameters that describe the desired graphic
	;; mode, and returns the VBE graphic mode number, if found.
	;; The C prototype for the parameters would look like this:
	;; int loaderFindGraphicMode(int xres, int yres, int bpp);
	;; (X resolution, Y resolution, Bits per pixel)
	;; On success it returns the mode number of the applicable graphics
	;; mode.  Otherwise, it returns 0

	;; Save space on the stack for the mode number we're returning
	sub SP, 2
	
	pusha

	;; Save the stack pointer
	mov BP, SP

	;; By default, return the value 0
	mov word [SS:(BP + 16)], 0
	
	;; We need to get a pointer to a list of graphics mode numbers
	;; from the VBE.  We can gather this from the VESA info block
	;; retrieved earlier by the video hardware detection routines.
	;; Get the offset now, and the segment inside the loop.
	mov SI, [VESAINFO + 0Eh]

	;; Do a loop through the supported modes
	.modeLoop:
	
	;; Save ES
	push ES

	;; Now get the segment of the pointer, as mentioned above
	mov AX, [VESAINFO + 10h]
	mov ES, AX

	;; ES:SI is now a pointer to a word list of supported modes, 
	;; terminated with the value FFFFh

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
	jne near .done
	;; Is the mode supported by this call?
	cmp AH, 00h
	jne .modeLoop

	;; We need to look for a few features to determine whether this
	;; is the mode we want.  First, it needs to be supported, and it
	;; needs to be a graphics mode.  Next, it needs to match the
	;; requested attributes of resolution and bpp

	;; Get the first word of the buffer
	mov AX, word [MODEINFO]
	
	;; Is the mode supported?
	bt AX, 0
	jnc .modeLoop

	;; Is this mode a graphics mode?
	bt AX, 4
	jnc .modeLoop

	;; Does this mode support a linear frame buffer?
	bt AX, 7
	jnc .modeLoop

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
	pop AX
	ret


checkLinearFramebuffer:
	;; Returns 1 if any linear framebuffer modes are supported.  0
	;; otherwise

        ;; Save space on the stack our return code
        sub SP, 2

	pusha

	;; Save the stack pointer
	mov BP, SP

	;; By default, return the value 0
	mov word [SS:(BP + 16)], 0
	
	;; We need to get a pointer to a list of graphics mode numbers
	;; from the VBE.  We can gather this from the VESA info block
	;; retrieved earlier by the video hardware detection routines.
	;; Get the offset now, and the segment inside the loop.
	mov SI, [VESAINFO + 0Eh]

	;; Do a loop through the supported modes
	.modeLoop:
	
	;; Save ES
	push ES

	;; Now get the segment of the pointer, as mentioned above
	mov AX, [VESAINFO + 10h]
	mov ES, AX

	;; ES:SI is now a pointer to a word list of supported modes, 
	;; terminated with the value FFFFh

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
	jne near .done
	;; Is the mode supported by this call?
	cmp AH, 00h
	jne .modeLoop

	;; We need to look for a few features to determine whether this
	;; is the mode we want.  First, it needs to be supported, and it
	;; needs to be a graphics mode.  Next, it needs to match the
	;; requested attributes of resolution and bpp

	;; Get the first word of the buffer
	mov AX, word [MODEINFO]
	
	;; Is the mode supported?
	bt AX, 0
	jnc .modeLoop

	;; Is this mode a graphics mode?
	bt AX, 4
	jnc .modeLoop

	;; Does this mode support a linear frame buffer?
	bt AX, 7
	jnc .modeLoop

	;; Framebuffer is supported
	mov word [SS:(BP + 16)], 1	

	;; push SI
        ;; mov DL, 02h             ; Use green color
	;; mov SI, HAPPY
	;; call loaderPrint
	;; xor EAX, EAX
        ;; mov AX, word [MODEINFO + 12h]
	;; call loaderPrintNumber
        ;; mov SI, HAPPY
        ;; call loaderPrint
	;; xor EAX, EAX
        ;; mov AX, word [MODEINFO + 14h]
	;; call loaderPrintNumber
        ;; mov SI, HAPPY
        ;; call loaderPrint
	;; xor EAX, EAX 
        ;; mov AL, byte [MODEINFO + 19h]
        ;; call loaderPrintNumber
        ;; call loaderPrintNewline
	;; pop SI
	;; jmp .modeLoop

	.done:
	popa
	pop AX
	ret
	

loaderGetLinearFramebuffer:

	;; This will return the address of the Linear Frame Buffer for
	;; the requested video mode

	pusha

	;; Save stack pointer
	mov BP, SP
	
	;; Make sure SVGA is available
	cmp byte [SVGAAVAIL], 1
	jne near .error

	mov AX, 4F01h
	mov ECX, dword [SS:(BP + 18)]
	mov DI, MODEINFO
	int 10h

	;; Make sure the function call was successful
	cmp AX, 004Fh
	jne .error

	popa
	mov EAX, dword [MODEINFO + 28h]
	ret

	.error:
	popa
	mov EAX, 0
	ret
		
	
selectGraphicMode:
	
	;; This function will select a graphics mode for the kernel based
	;; on which ones are available with this video card.  It has built-in
	;; preferences, so that it attempts to use them in a particular
	;; order.

	pusha

	;; We must decide which graphics mode we will use based
	;; on modes supported by the adapter

	mov SI, VIDEOMODES

	.modeLoop:
	cmp word [SI], 0
	je .fail		; No more modes to try
	push word [SI + 4]
	push word [SI + 2]
	push word [SI]
	call loaderFindGraphicMode

	;; Supported?
	cmp AX, 0
	jne near .success

	;; Remove those unsuccessful values from the stack
	add SP, 6

	;; Move to the next mode
	add SI, 6
	
	jmp .modeLoop

	.fail:
	;; If we fall through to here, none of our supported modes are
	;; available.  Make an error message.
	mov DL, ERRORCOLOR	; Switch to the error color
	mov SI, SAD
	call loaderPrint
	mov SI, VIDMODE
	call loaderPrint
	mov SI, NOMODE
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, TEXTMODE
	call loaderPrint
	call loaderPrintNewline
	mov SI, BLANK
	call loaderPrint
	mov SI, DUBIOUS
	call loaderPrint
	call loaderPrintNewline

	popa
	mov AX, 0
	ret
	
	.success:
	;; The appropriate mode number should be in AX.  Save it so it
	;; can be called later.
	mov word [KERNELGMODE], AX

	;; The successful attributes should still be on the stack.  Save
	;; them in the loader's hardware structure
	
	mov SI, word [VIDEODATA_P]
	xor EAX, EAX

	;; The graphic mode
	mov AX, word [KERNELGMODE]
	mov dword [SI + 4], EAX

	;; The horizontal resolution
	pop AX
	mov dword [SI + 12], EAX

	;; The vertical resolution
	pop AX
	mov dword [SI + 16], EAX

	;; The bits per pixel
	pop AX
	mov dword [SI + 20], EAX

	cmp word [PRINTINFO], 1
	jne .noPrint
	;; Say we found a mode. 
	mov DL, 02h		; Use green color
	mov SI, BLANK
	call loaderPrint
	mov DL, FOREGROUNDCOLOR	; Switch to foreground color
	mov SI, MODESTATS1
	call loaderPrint
	mov SI, word [VIDEODATA_P]
	mov EAX, dword [SI + 12]
	call loaderPrintNumber
	mov SI, MODESTATS2
	call loaderPrint
	mov SI, word [VIDEODATA_P]
	mov EAX, dword [SI + 16]
	call loaderPrintNumber
	mov SI, SPACE
	call loaderPrint
	mov SI, word [VIDEODATA_P]
	mov EAX, dword [SI + 20]
	call loaderPrintNumber
	mov SI, MODESTATS3
	call loaderPrint
	call loaderPrintNewline
	.noPrint:	
	
	popa
	mov AX, 1
	ret


;;
;; The data segment
;;

	SEGMENT .data
	ALIGN 4

CURRENTGMODE	dw 0
KERNELGMODE	dw 0
VIDEODATA_P	dw 0
VESAINFO  	db 'VBE2'		;; Space for info ret by vid BIOS
		times 508  db 0
MODEINFO	times 256 db 0
SVGAAVAIL	db 0

	
;;
;; Video modes, in preference order
;; 

VIDEOMODES:
	dw 1400, 1050, 32	; 1400 x 1050 x 32 bpp
;;	dw 1400, 1050, 24	; 1400 x 1050 x 24 bpp
	dw 1280, 1024, 32	; 1280 x 1024 x 32 bpp
;;	dw 1280, 1024, 24	; 1280 x 1024 x 24 bpp
;;	dw 1152, 864, 32	; 1152 x 864 x 32 bpp
;;	dw 1152, 864, 24	; 1152 x 864 x 24 bpp
	dw 1024, 768, 32	; 1024 x 768 x 32 bpp
	dw 1024, 768, 24	; 1024 x 768 x 24 bpp
	dw 1024, 768, 16	; 1024 x 768 x 16 bpp
	dw 1024, 768, 15	; 1024 x 768 x 15 bpp
	dw 800, 600, 32		; 800 x 600 x 32 bpp
	dw 800, 600, 24		; 800 x 600 x 24 bpp
	dw 800, 600, 16		; 800 x 600 x 16 bpp
	dw 800, 600, 15		; 800 x 600 x 15 bpp
	dw 640, 480, 32		; 640 x 480 x 32 bpp
	dw 640, 480, 24		; 640 x 480 x 24 bpp
	dw 640, 480, 16		; 640 x 480 x 16 bpp
	dw 640, 480, 15		; 640 x 480 x 15 bpp
	dw 0
		
	
;;
;; The good/informational messages
;;

HAPPY		db 01h, ' ', 0
BLANK		db '               ', 10h, ' ', 0
SVGA		db 'SVGA video   ', 10h, ' ', 0
VESA20		db 'VESA 2.0 or greater, ', 0
VIDEORAM	db 'K video RAM', 0
VIDMODE		db 'Video mode   ', 10h, ' ', 0
MODESTATS1      db 'Selected mode: ', 0
MODESTATS2	db 'x', 0
MODESTATS3	db ' bits/pixel', 0
SPACE		db ' ',0
	

;;
;; The error messages
;;

SAD		db 'x ', 0
NOSVGA		db 'SVGA video not detected.', 0
SVGAVER		db 'VESA version 2.0 or greater not available.', 0
NOFRAMEBUFF	db 'Linear framebuffer video not available.', 0
NOMODE		db 'Could not find a supported graphics mode to use.', 0
NOSWITCH1	db 'Could not switch to graphics mode.  You may wish to', 0
NOSWITCH2	db 'investigate possible problems with your video card.', 0
TEXTMODE	db 'This session will be restricted to text mode operation.', 0
DUBIOUS		db 'Note: The VESA compatibility of your video card is dubious.', 0
