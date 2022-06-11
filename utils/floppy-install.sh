#!/bin/csh -f

##
##  Visopsys
##  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
##  floppy-install.sh
##

#  Writes the boot sector, osloader and kernel files onto a floppy
#  in $floppydev.  Note that the floppy device must have permissions which
#  allow writes by the invoking user. 


set PLATFORM = `./platform.sh`
set floppydev = /dev/fd0
set bootsector = ../src/osloader/${PLATFORM}/bootsect.f12
set osloader = ../src/osloader/${PLATFORM}/vloader
set kernel = ../src/kernel/${PLATFORM}/visopsys
set progdir = ../src/programs/${PLATFORM}
set input_image

set mkdosfs = mkdosfs
set awk = awk
set grep = grep
set mount = mount
set umount = umount


echo ""
echo "FLOPPY-INSTALL - Visopsys floppy disk installation script for UNIX"
echo "Copyright (C) 1998-2001 Andrew McLaughlin"
echo " -= NOTE: suitable for use only with Visopsys SOURCE distribution =-"
echo ""
echo "Making Visopsys boot diskette in $floppydev"

#  Figure out which "fstab" file is used on this system.  On most systems
#  it is /etc/fstab.  We'll set it to that and then override it if
#  necessary
set FSTAB = /etc/fstab
if ("$PLATFORM" == "sparc-solaris") then
    set FSTAB = /etc/vfstab
else if ("$PLATFORM" =~ "hp-hpux") then
    set FSTAB = /etc/checklist
endif

echo Reading floppy device info from $FSTAB


#  Check for a -i argument which can be used to specify an
#  input disk image file

if ("$1" == "-i") then
    # discard the -i
    shift
    if ("$1" != "") then
	set input_image = "$1"
	#  Make sure it really exists
	if !(-e "$input_image") then
	    echo ""
	    echo Input disk image file $input_image could not be found
	    echo Terminating.
	    echo ""
	    exit 1
	endif
	echo -- using input image $input_image --
	# Discard the argument
	shift
    else
	echo ""
	echo "Usage: $0 [-i <input disk image file>]"
	echo "          [-o <output disk image file>]"
	echo Terminating.
	echo ""
	exit 2
    endif
endif

#  Check for a -o argument which can be used to specify an
#  output disk image file

if ("$1" == "-o") then
    #  Make sure the input image has been specified also
    if ($input_image == "") then
	echo ""
	echo "When specifying output image file, you must also specify the"
	echo "input image file (cannot do a regular format of an image file)"
	echo Terminating.
	echo ""
	exit 3
    endif
    # discard the -o
    shift
    if ("$1" != "") then
	#  We override the floppy device file variable with the desired
	# output image file name
	set floppydev = "$1"
	echo -- using output image file $floppydev --
	# Discard the argument
	shift
    else
	echo ""
	echo "Usage: $0 [-i <input disk image file>]"
	echo "          [-o <output disk image file>]"
	echo Terminating.
	echo ""
	exit 4
    endif
else
    #  We're not using an output image file.  We're actually writing to
    #  the floppy device file.  Make sure we have write permission to
    #  do so
    if !(-w $floppydev) then
	echo ""
	echo "You do not have permission to write directly to the floppy"
	echo "disk device file ($floppydev) on this system"
	echo Terminating.
	echo ""
	exit 5
    endif
endif


#  Make sure that the files we need to transfer have been built
foreach targetfile ($bootsector $osloader $kernel)
if !(-e $targetfile) then
	echo ""
	echo "$targetfile is missing.  Have you done a make yet?"
	echo Terminating.
	echo ""
	exit 6
    endif
end


#  Check for some commands this file needs.  Check in /bin and /sbin
#  first.  If it's not there, search the PATH

foreach COMMAND ($mkdosfs $awk $grep $mount $umount)

    #  Check in /bin and /sbin
    if (-e /bin/$COMMAND) then
	set $COMMAND = /bin/$COMMAND
    else if (-e /sbin/$COMMAND) then
	set $COMMAND = /sbin/$COMMAND
    else if (-e `which $COMMAND`) then
	set $COMMAND = `which $COMMAND`
    else
	echo ""
	echo $COMMAND command is missing.  It is needed to use $0
	echo Make sure it is installed, and is in your '$PATH'
	echo Terminating.
	echo ""
	exit 7
    endif
end


#  Try to figure out where the floppy disk gets mounted by reading the
#  fstab file.  If this is not working on your system, you should 
#  override this manually (or fix the following command)
set FLOPPYDIR = (`cat $FSTAB | $grep $floppydev | $awk '{print $2}'`)


if ("$input_image" == "") then
    #  Format the floppy using the format command.  
    #  Stop if the command fails.
    if !( { $mkdosfs $floppydev } ) then
	echo ""
	echo "Not able to format disk in $floppydev.  Terminating."
	echo Terminating.
	echo ""
	exit 8
    endif
else
    #  Format the floppy by writing the fresh disk image to the
    #  device.  Stop if the command fails.
    if !( { (echo -n "Dumping disk image... "; \
	    cat $input_image > $floppydev) } ) then
	echo ""
	echo "Not able to format disk in $floppydev.  Terminating."
	echo Terminating.
	echo ""
	exit 9
    endif
    echo Done
endif


#  Mount the floppy
if !( { $mount $floppydev } ) then
    echo ""
    echo "Not able (or not permitted) to mount disk in $floppydev."
    echo "Terminating."
    echo ""
    exit 10
endif


#  Install the boot sector
echo -n "Installing boot sector...  "
cat $bootsector > $floppydev
echo Done


#  Copy the os loader
echo -n "Copying os loader...  "
cp $osloader $FLOPPYDIR/
sync
echo Done


#  Copy the kernel.  We don't need to error-check this is the previous
#  write was successful.
echo -n "Copying kernel...  "
cp $kernel $FLOPPYDIR/
sync
echo Done


#  Copy the programs
echo -n "Copying miscellany...  "
mkdir $FLOPPYDIR/programs
find $progdir -name \*.o -exec rm {} \;
cp $progdir/* $FLOPPYDIR/programs/

#  Copy the Visopsys logo picture file
#cp ../docs/working/pics/visopsys.bmp $FLOPPYDIR/
# No splash screen
touch $FLOPPYDIR/nosplash
sync
echo Done

#  Unmount the floppy disk
$umount $FLOPPYDIR
