@echo off

::
::  Visopsys
::  Copyright (C) 1998-2003 J. Andrew McLaughlin
:: 
::  This program is free software; you can redistribute it and/or modify it
::  under the terms of the GNU General Public License as published by the Free
::  Software Foundation; either version 2 of the License, or (at your option)
::  any later version.
:: 
::  This program is distributed in the hope that it will be useful, but
::  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
::  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
::  for more details.
::  
::  You should have received a copy of the GNU General Public License along
::  with this program; if not, write to the Free Software Foundation, Inc.,
::  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
::
::  floppy-install.bat
::

::  This batch file is for creating a Visopsys installation under DOS using
::  a BINARY distribution.

::  Thanks to Bob Watson (rhwatson@sympatico.ca) for maintaining the
::  MS-DOS7 Commands page at http://www3.sympatico.ca/rhwatson/dos7/
::  (I couldn't have remembered this crap without it!)

echo"
echo FLOPPY-INSTALL - Visopsys floppy disk installation script for DOS
echo Copyright (C) 1998-2003 Andrew McLaughlin
echo  -= NOTE: suitable for use only with Visopsys BINARY distribution =-
echo"

echo Formatting...
::  (really cool trick from Bob Watson's page -- suppresses annoying
::   questions from the format command)
ren /? | format a: /q /f:1440 /v:"" > nul

echo Copying os loader...
copy ..\instfile\vloader a:\vloader > nul

echo Copying kernel...
copy ..\instfile\visopsys a:\visopsys > nul

:: Extras

echo Installing boot sector...
writeboot.com ..\instfile\bootsect.f12

echo"
