#!/bin/csh -f

##
##  Visopsys
##  Copyright (C) 1998-2003 J. Andrew McLaughlin
## 
##  This program is free software; you can redistribute it and/or modify it
##  under the terms of the GNU General Public License as published by the Free
##  Software Foundation; either version 2 of the License, or (at your option)
##  any later version.
## 
##  This program is distributed in the hope that it will be useful, but
##  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
##  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
##  for more details.
##  
##  You should have received a copy of the GNU General Public License along
##  with this program; if not, write to the Free Software Foundation, Inc.,
##  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
##
##  platform.sh
##

# This thing just analyzes the output from uname and spits out
# a platform name.


set OSNAME = `uname -s`
set OSVER = `uname -r`

if ($OSNAME == "SunOS") then
    if ($OSVER =~ 4.*) then
	echo "sparc-sunos"
	exit 0
    else if ($OSVER =~ 5.*) then
	echo "sparc-solaris"
	exit 0
    else 
	echo "unknown-sun"
	exit -1
    endif

else if ($OSNAME =~ IRIX*) then
    if ($OSVER =~ 5.*) then
	echo "sgi-irix"
	exit 0
    else if ($OSVER =~ 6.*) then
	echo "sgi-irix6"
	exit 0
    else 
	echo "unknown-sgi"
	exit -1
    endif

else if ($OSNAME =~ HP-UX*) then
    if ($OSVER =~ *.09.*) then
	echo "hp-hpux"
	exit 0
    else if ($OSVER =~ *.10.*) then
	echo "hp-hpux10"
	exit 0
    else if ($OSVER =~ *.11.*) then
	echo "hp-hpux11"
        exit 0
    else 
	echo "unknown-hp"
	exit -1
    endif

else if ($OSNAME =~ OSF1*) then
    if ($OSVER =~ V3*) then
	echo "alpha-digitalunix"
	exit 0
    else if ($OSVER =~ V4*) then
	echo "alpha-digitalunix"
	exit 0
    else
	echo "unknown-alpha"
	exit -1
    endif

else if ($OSNAME =~ Linux*) then
    echo "x86-linux"
    exit 0

else if ($OSNAME =~ AIX*) then
    set OSVER = `uname -v`
    if ($OSVER == "3") then
	echo "rs6000-aix"
	exit 0
    else if ($OSVER == "4") then
	echo "rs6000-aix4"
	exit 0
    else 
	echo "unknown-rs6000"
	exit -1
    endif

else
    echo "unknown"
    exit -1
