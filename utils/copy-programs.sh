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
##  copy-programs.sh
##

#  Writes the programs onto a floppy in $floppydev.  Note that the floppy 
#  device must have permissions which allow writes by the invoking user. 


set PLATFORM = `./platform.sh`
set floppydev = /dev/fd0
set programs = ../src/programs/${PLATFORM}

echo ""
echo "Copying Visopsys programs to $floppydev"

#  Figure out which "fstab" file is used on this system.  On most systems
#  it is /etc/fstab.  We'll set it to that and then override it if
#  necessary
set FSTAB = /etc/fstab
if ("$PLATFORM" == "sparc-solaris") then
    set FSTAB = /etc/vfstab
else if ("$PLATFORM" =~ "hp-hpux") then
    set FSTAB = /etc/checklist
endif

#  Make sure that the programs have been built
if !(-e $programs) then
	echo ""
	echo "$programs is missing.  Have you done a make yet?"
	echo Terminating.
	echo ""
	exit 6
endif

#  Try to figure out where the floppy disk gets mounted by reading the
#  fstab file.  If this is not working on your system, you should 
#  override this manually (or fix the following command)
set FLOPPYDIR = (`/bin/cat $FSTAB | /bin/grep $floppydev | /bin/awk '{print $2}'`)

#  Mount the floppy
if !( { /bin/mount $floppydev } ) then
    echo ""
    echo "Not able (or not permitted) to mount disk in $floppydev."
    echo "Terminating."
    echo ""
    exit 10
endif

#  Copy the programs.  We don't need to error-check that the previous
#  write was successful.
echo -n "Copying programs...  "
mkdir -p $programs/tmp
cp $programs/* $programs/tmp/ >& /dev/null
rm -f $programs/tmp/*.o >& /dev/null
mv $programs/tmp/* $FLOPPYDIR/programs/
rmdir $programs/tmp

#  Unmount the floppy disk
/bin/umount $FLOPPYDIR
echo Done
