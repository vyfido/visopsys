#!/bin/csh -f
##
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
##  image-floppy.sh
##

# Create a floppy image distribution.  Requires a floppy in the drive.


echo ""
echo "Making Visopsys FLOPPY IMAGE file"
echo ""

# Make sure we know where we are
if !("$PWD" != "utils") then
    echo ""
    echo "Error!  Please run this script from the utils directory"
    echo ""
    exit -1
endif

# Are we doing a release version?  If the argument is "-r" then we use
# the release number in the destination directory name.  Otherwise, we
# assume an interim package and use the date instead
if ("$1" == "-r") then
    echo "(doing RELEASE version)"
    # What is the current release version?
    set RELEASE = `./release.sh`
    # Use it to set the detination directory name
    set DEST = visopsys-$RELEASE
else
    echo "(doing INTERIM version -- use -r flag for RELEASES)"
    # What is the date?
    set DATE = `date +%Y-%m-%d`
    # Use it to set the detination directory name
    set DEST = visopsys-$DATE
endif

set IMAGEFILE=$DEST.img
set ZIPFILE=$DEST-img.zip

# Clear off the floppy, so there's no old data.  This will keep the size
# of the compressed image to a minimum
echo -n "Clearing... "
dd if=/dev/zero of=/dev/fd0 >& /dev/null
echo done

# Install on the floppy
echo -n "Intalling... "
./install.sh /dev/fd0 >& /dev/null
echo done

# Remove any existing files here
rm -f $IMAGEFILE $ZIPFILE

# Copy back from the floppy
echo -n "Copying... "
dd if=/dev/fd0 of=$IMAGEFILE >& /dev/null
echo done

# Zip the image
echo -n "Archiving... "
zip -9 -r $ZIPFILE $IMAGEFILE >& /dev/null
echo done

# Remove the image file
echo -n "Cleaning up... "
rm -f $IMAGEFILE
echo done

echo "File is: $ZIPFILE"
echo ""

exit 0
