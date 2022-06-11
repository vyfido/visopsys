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
;;  writeboot.s
;;

	SEGMENT .text
	BITS 16
	ORG 0100h

Main:
	;; This little program will write 512-bytes of the contents of a file
	;; to the first sector of the first floppy disk on a DOS system. 
	;; It treats the DOS politely in that wherever possible it does not 
	;; directly call BIOS functions to get the job done -- it uses
	;; DOS calls.

	;; Save regs
	pusha

	;; Make sure DS = CS
	push CS
	pop DS
	
	;; Print the message
	mov DX, TITLE
	mov AH, 9
	int 21h
	
	;; We need to get our argument from DOS.  DOS conveniently puts
	;; it in the PSP (Program Segment Prefix).  We are expecting the
	;; following argument:  Boot sector filename.  Once we have this, we
	;; can proceed to open the file for reading.  We will find our
	;; "command tail" (as it's called) at offset 81h.  Copy up to 127
	;; bytes into our FILENAME space (only until we reach the end of
	;; the string).

	;; Make sure DS = CS
	push CS
	pop DS
	
	mov SI, 81h		; In PSP
	mov CX, 127		; Up to 127 bytes of command tail
	
	mov DI, FILENAME	; Our filename buffer
	
	.getNameLoop:
	mov AL, byte [DS:SI]	
	;; Make sure it isn't CR, which denotes the end of the command tail
	cmp AL, 0Dh
	je .doneNameLoop
	;; Is it a [space] character?
	cmp AL, ' '
	jne .notSpace2
	cmp CX, 127
	;; It's a space, but not a leading one, so we're done
	jne .doneNameLoop
	;; It's a leading space -- skip it
	inc SI
	loop .getNameLoop
	.notSpace2:	
	;; Put it in the destination
	mov byte [DS:DI], AL
	;; Increment
	inc SI
	inc DI
	loop .getNameLoop

	.doneNameLoop:
	;; Make sure there is a NULL after the filename, followed
	;; by a '$'
	mov byte [DS:DI], 0
	mov byte [DS:(DI + 1)], '$'
	
	;; Get our destination drive number
	inc SI
	mov AL, byte [DS:SI]
	;; We will restrict it to a number between 0-9
	cmp AL, '0'
	jb .diskNumberError
	cmp AL, '9'
	ja .diskNumberError
	;; Turn it into a number instead of a char
	sub AL, '0'
	mov byte [DISKNUMBER], AL
			
	;; Print the name of the file we're going to use
	mov DX, NAMEIS
	mov AH, 9
	int 21h
	mov DX, FILENAME
	mov AH, 9
	int 21h
	mov DX, NEWLINE
	mov AH, 9
	int 21h
	
	;; Can we open it for reading?  We will try an open mode that 
	;; allows others read-only access to the file
	mov AH, 3Dh
	mov AL, 00100000b
	mov DX, FILENAME
	int 21h

	;; Were we successful?
	jnc .fileOpened			
	
	;; We could not open the input file
	mov DX, NOOPEN
	mov AH, 9
	int 21h
	jmp .badexit

	.fileOpened:
	;; We found the input file.  Save the file handle from AX
	mov word [DS:FILEHANDLE], AX
	
	;; Ok, read one sector of the file into the buffer we have reserved
	mov AH, 3Fh
	mov BX, word [DS:FILEHANDLE]
	mov CX, 512
	mov DX, BUFFER
	int 21h

	;; Were we successful?
	jnc .fileRead

	;; We couldn't read the file
	mov DX, NOREAD
	mov AH, 9
	int 21h
	jmp .badexit

	.fileRead:
	;; We read (up to) 512 bytes.  If it's less, the buffer was
	;; previously filled with NULL anyway.  Now, we want to do an
	;; "absolute" 1-sector write to head 0, sector 1, track 0 of 
	;; disk 0.  We use a BIOS function for this.  We attempt this up
	;; to five times.

	push word 5

	.tryLoop:
	mov AH, 3
	mov AL, 1		; 1 sector
	mov CX, 0001h		; at track 0, sector 1
	mov DH, 00h		; at head 0
	mov DL, [DISKNUMBER]	; disk
	push CS
	pop ES
	mov BX, BUFFER
	int 13h

	;; Were we successful?
	jnc .success

	;; Do we have any retries left
	pop AX
	dec AX			; Decrement it
	cmp AX, 0
	je .fail

	;; Put it back and try again
	push AX
	jmp .tryLoop

	.diskNumberError:
	mov DX, DNUMERR
	mov AH, 9
	int 21h
	jmp .badexit
		
	.fail:	
	;; We couldn't write the boot sector
	mov DX, NOWRITE
	mov AH, 9
	int 21h
	jmp .badexit
			
	.success:
	;; Pop the counter off the stack
	pop AX
	
	;; Print a happy message
	mov DX, DONE
	mov AH, 9
	int 21h
	
	;; Restore regs
	popa
	
	;; Call the routine to exit responsibly to DOS
	mov AX, 4C00h
	int 21h

	;; Just in case
	ret

	.badexit:
	;; Restore regs
	popa
	
	;; Call the routine to exit responsibly to DOS
	mov AX, 4C01h
	int 21h

	;; Just in case
	ret
	


	;; Data
	
TITLE	db 0Dh, 0Ah, 'WRITEBOOT - Visopsys boot sector utility for DOS' 
	db 0Dh, 0Ah, 'Copyright (C) 1998-2001 Andrew McLaughlin'
	db 0Dh, 0Ah, '$'
NAMEIS	db 'Writing boot sector file: $'
NOOPEN	db 0Dh, 0Ah, 'Could not open the input file for reading'
	db 0Dh, 0Ah, '  Tip: use only DOS 8.3 (short) filenames'
	db 0Dh, 0Ah, '$'
NOREAD	db 0Dh, 0Ah, 'Could not read the input file!', 0Dh, 0Ah, '$'
NOWRITE	db 0Dh, 0Ah, 'Could not write first sector of disk A:'
	db 0Dh, 0Ah, '$'
DNUMERR db 'The specified disk number is invalid', 0Dh, 0Ah, '$'
DONE	db 'done.', 0Dh, 0Ah, '$' 
NEWLINE	db 0Dh, 0Ah, '$'
DISKNUMBER	db 0
;; A place for the file handle
FILEHANDLE	dw 0
;; Use this to save the name of the bootsector file
FILENAME	times 128	db 0
;; Use this to save the boot sector data
BUFFER		times 512	db 0