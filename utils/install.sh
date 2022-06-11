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
##  install.sh
##

# Installs the Visopsys system on the requested device.  Suitable for use
# only with Visopsys SOURCE distribution.  Note that the device must have
# permissions which allow direct writes by the invoking user. 

set OSLOADERDIR = ../src/osloader/build
set BOOTSECTOR = $OSLOADERDIR/bootsect.fat12
set OSLOADER = $OSLOADERDIR/vloader
set KERNELDIR = ../src/kernel/build
set KERNEL = $KERNELDIR/visopsys
set PROGDIR = ../src/programs/build
set FSTAB = /etc/fstab


echo ""
echo "INSTALL.SH - Visopsys installation script for UNIX"
echo "Copyright (C) 1998-2003 Andrew McLaughlin"
echo " -= NOTE: Suitable for use only with Visopsys SOURCE distribution =-"
echo ""

set DEVICE = $1
if ("$DEVICE" == "") then
    echo "No installation device specified."
    echo "Usage: $0 <installation device>"
    echo Terminating.
    echo ""
    exit 1
else if !(-e $DEVICE) then
    echo "Installation device does not exist."
    echo "Usage: $0 <installation device>"
    echo Terminating.
    echo ""
    exit 1
endif
echo "Installing Visopsys on $DEVICE"

echo Reading device info from $FSTAB

# Make sure we have write permission on the device
if !(-w $DEVICE) then
    echo ""
    echo "You do not have permission to write directly to the disk device"
    echo "file ($DEVICE) on this system.  (try 'chmod go+rw $DEVICE')."
    echo Terminating.
    echo ""
    exit 1
endif

# Make sure that the files we need to transfer have been built
foreach targetfile ($BOOTSECTOR $OSLOADER $KERNEL)
if !(-e $targetfile) then
	echo ""
	echo "$targetfile is missing.  Have you done a make yet?"
	echo Terminating.
	echo ""
	exit 1
    endif
end

# Try to figure out where the disk gets mounted by reading the fstab
# file.  If this is not working on your system, you should override
# this manually (or fix the following command)
set MOUNTDIR = (`cat $FSTAB | grep $DEVICE | awk '{print $2}'`)

if ( `grep $DEVICE $FSTAB | grep loop` == "") then
	# Format the disk.  Stop if the command fails.
	if !( { /sbin/mkdosfs -F12 "$DEVICE" } ) then
    		echo ""
    		echo Not able to format disk "$DEVICE".  Terminating.
    		echo ""
    		exit 1
	endif
endif

# Install the boot sector
./copy-boot.sh $BOOTSECTOR $DEVICE

# Mount the filesystem
if !( { mount $DEVICE } ) then
    echo ""
    echo "Not able (or not permitted) to mount disk $DEVICE.  Terminating."
    echo ""
    exit 1
endif

# Copy the os loader
echo -n "Copying os loader...  "
cp $OSLOADER $MOUNTDIR/
mkdir -p $MOUNTDIR/system/boot
cp $OSLOADERDIR/bootsect.* $MOUNTDIR/system/boot/
# Uncomment to have the os loader print out hardware/boot information
# touch $MOUNTDIR/bootinfo
# Uncomment to disable graphics mode
# touch $MOUNTDIR/nograph
sync
echo Done

# Copy the kernel.
echo -n "Copying kernel...  "
cp $KERNEL $MOUNTDIR/
sync
echo Done

# Copy the programs and other things
echo -n "Copying miscellany...  "
mkdir -p $MOUNTDIR/programs
cp $PROGDIR/* $MOUNTDIR/programs/
mkdir -p $MOUNTDIR/system
cp $KERNELDIR/kernelSymbols.txt $MOUNTDIR/system/
cp dist/system/*.bmp $MOUNTDIR/system/
cp dist/system/*.conf $MOUNTDIR/system/
cp dist/system/*.txt $MOUNTDIR/system/
cp ../COPYING.TXT $MOUNTDIR/system/

sync
echo Done

# Unmount the filesystem
umount $MOUNTDIR

exit 0
