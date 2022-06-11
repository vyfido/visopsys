;;
;;  Visopsys
;;  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
;;  loader.h
;;

;; Constants

;; Memory locations for loading the kernel

%define LDRCODESEGMENTLOCATION 	00001000h
%define LDRCODESEGMENTSIZE	00005000h
%define LDRSTCKSEGMENTLOCATION 	(LDRCODESEGMENTLOCATION + LDRCODESEGMENTSIZE)
%define LDRSTCKSEGMENTSIZE	00001000h   ; Only needs a small stack
%define LDRSTCKBASE             (LDRSTCKSEGMENTSIZE - 2)
%define LDRPAGINGDATA           (LDRSTCKSEGMENTLOCATION + LDRSTCKSEGMENTSIZE)
%define LDRPAGINGDATASIZE       00003000h
%define TMPSCREENBUFFER         (LDRPAGINGDATA + LDRPAGINGDATASIZE)
%define TMPSCREENSIZE           00002000h
%define LDRDATABUFFER           (TMPSCREENBUFFER + TMPSCREENSIZE)
%define DATABUFFERSIZE          (32 * 1024)

%define PAGINGDATA              (LDRCODESEGMENTLOCATION + LDRPAGINGDATA)
%define KERNELVIRTUALADDRESS    0C0000000h   ;; 3 Gb mark
%define KERNELCODEDATALOCATION	00100000h    ;; 1 Mb mark
%define KERNELSTACKSIZE	        00010000h    ;; 64 Kb

;; The length of the progress indicator during kernel load
%define PROGRESSLENGTH 20

;; Some checks, to make sure the data above is correct

%if ((KERNELCODEDATALOCATION % 4096) != 0)
%error "Kernel code must start on 4Kb boundary"
%endif
%if ((KERNELSTACKSIZE % 4096) != 0)
%error "Kernel stack size must be a multiple of 4Kb"
%endif
%if ((PAGINGDATA % 4096) != 0)
%error "Loader paging data must be a multiple of 4Kb"
%endif

;; Segment descriptor information for the temporary GDT

%define PRIV_CODEINFO1	  10011010b
%define PRIV_CODEINFO2	  11001111b
%define PRIV_DATAINFO1	  10010010b
%define PRIV_DATAINFO2	  11001111b
%define PRIV_STCKINFO1	  10010010b
%define PRIV_STCKINFO2	  11001111b

%define LDRCODEINFO1      10011010b
%define LDRCODEINFO2	  01000000b

%define SCREENSTART       0x000B8000

%define VIDEOPAGE	  0
%define ROWS		  50
%define COLUMNS		  80
%define FOREGROUNDCOLOR   7
%define BACKGROUNDCOLOR   1 
%define ERRORCOLOR        6

;; Selectors in the GDT
%define PRIV_CODESELECTOR 0x0008
%define PRIV_DATASELECTOR 0x0010
%define PRIV_STCKSELECTOR 0x0018
%define LDRCODESELECTOR   0x0020

;; Filesystem types
%define FAT12 0
%define FAT16 1
%define FAT32 2

;; CPU types
%define i486       0
%define pentium    1
%define pentiumPro 2
%define pentiumII  3
%define pentiumIII 4

;; Number of elements in our memory map
%define MEMORYMAPSIZE 50

;; Maximum number of graphics modes we check
%define MAXVIDEOMODES 20

;; Our data structures that we pass to the kernel, mostly having to do with
;; hardware
STRUC graphicsInfoBlock
 .videoMemory:    resd 1 ;; Video memory in Kbytes
 .framebuffer:    resd 1 ;; Address of the framebuffer
 .mode:           resd 1 ;; Current video mode
 .xRes:           resd 1 ;; Current X resolution
 .yRes:           resd 1 ;; Current Y resolution
 .bitsPerPixel:   resd 1 ;; Bits per pixel
 .numberModes:    resd 1 ;; Number of graphics modes in the following list
 .supportedModes: resd (MAXVIDEOMODES * 4)

ENDSTRUC

STRUC memoryInfoBlock
 .start resq 1
 .size  resq 1
 .type  resd 1
ENDSTRUC

;; The data structure created by the loader to describe the particulars
;; about a floppy disk drive to the kernel
STRUC fddInfoBlock
 .type    resd 1
 .heads   resd 1
 .tracks  resd 1
 .sectors resd 1
ENDSTRUC

;; The data structure created by the loader to describe the particulars
;; about a hard disk drive to the kernel
STRUC hddInfoBlock
 .heads          resd 1
 .cylinders      resd 1
 .sectors        resd 1
 .bytesPerSector resd 1
 .totalSectors   resd 1
ENDSTRUC

;; The data structure created by the loader to hold info about the serial
;; ports
STRUC serialInfoBlock
 .port1 resd 1
 .port2 resd 1
 .port3 resd 1
 .port4 resd 1
ENDSTRUC

;; The data structure created by the loader to hold info about the mouse
STRUC mouseInfoBlock
 .port   resd 1
 .idByte resd 1
ENDSTRUC
