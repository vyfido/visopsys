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

%define PRIV_CODEINFO1	10011010b
%define PRIV_CODEINFO2	11001111b
%define PRIV_DATAINFO1	10010010b
%define PRIV_DATAINFO2	11001111b
%define PRIV_STCKINFO1	10010010b
%define PRIV_STCKINFO2	11001111b

%define LDRCODEINFO1    10011010b
%define LDRCODEINFO2	01000000b


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
