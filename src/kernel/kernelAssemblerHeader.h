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
;;  kernelAssemblerHeader.h
;;
	
;; Constants

%define SCREENSTART 0x000B8000

%define VIDEOPAGE	 0
%define ROWS		 50
%define COLUMNS		 80
%define FOREGROUNDCOLOR  0x07
%define BACKGROUNDCOLOR  0x01
%define ERRORCOLOR       0x06

;; Selectors in the GDT
%define PRIV_CODESELECTOR 0x0008
%define PRIV_DATASELECTOR 0x0010
%define PRIV_STCKSELECTOR 0x0018
%define LDRCODESELECTOR   0x0020

;; Routines needed by the asm modules
EXTERN kernelConsoleLogin
EXTERN kernelDmaFunctionsEnableChannel
EXTERN kernelDmaFunctionsCloseChannel
EXTERN kernelDmaFunctionsSetupChannel
EXTERN kernelDmaFunctionsReadData
EXTERN kernelDmaFunctionsWriteData
EXTERN kernelKeyboardReadData
EXTERN kernelMouseReadData
EXTERN kernelMultitaskerDumpProcessList
EXTERN kernelMultitaskerYield
EXTERN kernelResourceManagerLock
EXTERN kernelResourceManagerUnlock
EXTERN kernelShutdown
EXTERN kernelSysTimerTick
EXTERN kernelSysTimerRead
