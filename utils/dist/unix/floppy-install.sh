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
##  floppy-install.sh
##

# This is for use by those who can't, for whatever reason, install Visopsys
# using the Java installer provided.  It's not guaranteed to work or
# anything like that; it's just provided in the hope that it will be
# useful

set PLATFORM = `./platform.sh`
set SRC_DIR  = TMP_UNZIP
set TMP_MNT  = TMP_MNT

echo ""
echo "FLOPPY-INSTALL - Visopsys floppy disk installation script for UNIX"
echo "Copyright (C) 1998-2003 Andrew McLaughlin"
echo " ** NOTE: this script should be run as ROOT"
echo " ** NOTE: suitable for use only with Visopsys BINARY distribution"
echo ""

# Get the installation device from the command line
set FLOPPYDEV = $1
if ("$FLOPPYDEV" == "") then
    echo "Usage: $0 [device file]"
    echo "Terminating."
    echo ""
    exit 1
endif

echo "Making Visopsys boot diskette in $FLOPPYDEV"

# We're writing to the floppy device file.  Make sure we have write 
# permission to do so
if !(-w $FLOPPYDEV) then
    echo "You do not have permission to write directly to the floppy"
    echo "disk device file ($FLOPPYDEV) on this system"
    echo "Terminating."
    echo ""
    exit 1
endif

# Figure out the appropriate commands for this platform
if ("$PLATFORM" == "x86-linux") then
    set FORMAT_CMD = "/sbin/mkdosfs $FLOPPYDEV"
    set MOUNT_CMD  = "/bin/mount -t vfat $FLOPPYDEV $TMP_MNT"
    set UMOUNT_CMD = "/bin/umount $TMP_MNT"
else if ("$PLATFORM" == "sparc-solaris") then
    set FORMAT_CMD = "/bin/fdformat -fU -t dos $FLOPPYDEV"
    set MOUNT_CMD  = "/usr/sbin/mount -F pcfs $FLOPPYDEV $TMP_MNT"
    set UMOUNT_CMD = "/usr/sbin/umount $TMP_MNT"
else
    echo "Unknown platform: $PLATFORM."
    echo "Terminating."
    echo ""
    exit 1
endif

# Format the floppy using the appropriate command for this platform
if !( { $FORMAT_CMD } ) then
    echo "Not able to format disk in $FLOPPYDEV."
    echo Terminating.
    echo ""
    exit 1
endif

# Mount the floppya
mkdir -p $TMP_MNT
if !( { $MOUNT_CMD } ) then
    echo "Not able (or not permitted) to mount disk in $FLOPPYDEV."
    echo "Terminating."
    echo ""
    exit 1
endif

# Unzip the source files into a temporary directory
mkdir -p $SRC_DIR
( cd $SRC_DIR; unzip ../../files/visopsys.zip )

# Install the boot sector
echo -n "Installing boot sector...  "
cat ../files/bootsect.f12 > $FLOPPYDEV
echo "Done"

# Copy all the files in $SRC_DIR
echo -n "Copying files...  "
# Make sure the OS loader is first on the disk
mv $SRC_DIR/vloader $TMP_MNT
foreach file ( $SRC_DIR/* )
    mv $file $TMP_MNT
end
sync
echo "Done"

# Unmount the floppy
if !( { $UMOUNT_CMD } ) then
        echo "Warning: Not able to unmount disk in $FLOPPYDEV."
endif

# Clean up
rmdir $TMP_MNT $SRC_DIR

# Done
exit 0

