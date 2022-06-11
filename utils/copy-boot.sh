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
##  copy-boot.sh
##

# Writes the boot sector from the first argument (device?) to the second
# without discarding the FAT filesystem parameters from the second boot
# sector (i.e. leaves the 12th through 38th bytes intact

echo -n "Installing boot sector...  "

set INPUT=$1
set OUTPUT=$2

rm -f bootsect.inp bootsect.out bootsect.tmp

dd count=1 if=$INPUT of=bootsect.inp >& /dev/null
dd count=1 if=$OUTPUT of=bootsect.out >& /dev/null

head --bytes 3 bootsect.inp > bootsect.tmp
tail --bytes 509 bootsect.out >> bootsect.tmp
head --bytes 62 bootsect.tmp > bootsect.out

tail --bytes 450 bootsect.inp >> bootsect.out
cat bootsect.out > $OUTPUT

rm -f bootsect.inp bootsect.out bootsect.tmp

echo Done

