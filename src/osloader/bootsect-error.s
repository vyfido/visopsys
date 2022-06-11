;;
;;  Visopsys
;;  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
;;  bootsect-error.s
;;

;; This code is a common error reporting for bootsector code.  It's just
;; meant to be %included, not compiled separately.

	
IOError:
	;; If we got a fatal IO error or something, we just have to print
	;; an error and try to let the BIOS select another device to boot.
	;; This isn't very helpful, but unfortunately this piece of code
	;; is too small to do very much else.

	mov SI, IOERR
	call print

	;; Wait for a key press
	mov AX, 0000h
	int 16h

	;; Continue to the next bootable media
	int 18h

	;; Stop, just in case
	.fatalErrorLoop:
	jmp .fatalErrorLoop
