@echo off

::
::  Visopsys Java Installer
::  Copyright (C) 2001 J. Andrew McLaughlin
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
::  format.bat
::

::  Thanks to Bob Watson (rhwatson@sympatico.ca) for maintaining the
::  MS-DOS7 Commands page at http://www3.sympatico.ca/rhwatson/dos7/
::  (I couldn't have remembered this crap without it!)

::  (really cool trick from Bob Watson's page -- suppresses annoying
::   questions from the format command)
:: ren /? | format %1 /q /f:1440 /v:"" > nul
format %1 /q /v:"" /autotest
