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
##  zip-instfiles.sh
##


echo ""
echo "Making Visopsys ZIP archive"
echo ""

set DEST_DIR = tmp_zip_dir
rm -Rf $DEST_DIR
mkdir -p $DEST_DIR

# Make sure we know where we are
if !(-e ../../visopsys) then
    echo ""
    echo "Error!  Please run this script from the utils directory"
    echo ""
    exit 1
endif

# What platform are we on?
set PLATFORM = `./platform.sh`

# Make a directory to hold the new distribution.  Remove any existing one
# first

# Directories in the source tree
set OSLOADERDIR = ../src/osloader/$PLATFORM
set KERNELDIR   = ../src/kernel/$PLATFORM
set DOCSDIR     = ../docs
set PROGSDIR    = ../src/programs/$PLATFORM
set SOURCEDIR   = ../src
set LIBSDIR     = ../src/lib/$PLATFORM

# The Visopsys files
set BOOTSECT    = bootsect.f12
set LOADER      = vloader
set KERNEL      = visopsys

cp $OSLOADERDIR/$LOADER $KERNELDIR/$KERNEL $DEST_DIR/

# Copy everything from the programs directory
mkdir -p $DEST_DIR/programs
cp -R $PROGSDIR/* $DEST_DIR/programs/

# The system directory
mkdir -p $DEST_DIR/system
cp dist/system/*.bmp $DEST_DIR/system
mv $DEST_DIR/system/backgrnd1.bmp $DEST_DIR/system/backgrnd.bmp
cp dist/system/*.conf* $DEST_DIR/system/
cp dist/system/*.txt $DEST_DIR/system/
mkdir -p $DEST_DIR/system/headers/sys
cp $SOURCEDIR/include/*.h $DEST_DIR/system/headers/
cp $SOURCEDIR/include/sys/*.h $DEST_DIR/system/headers/sys/
find $DEST_DIR/system/headers -name $PLATFORM -exec rm -Rf {} \;
find $DEST_DIR/system/headers -name CVS -exec rm -Rf {} \;
mkdir -p $DEST_DIR/system/libraries
cp -R $LIBSDIR/*.a $DEST_DIR/system/libraries/

# Remove extra stuff
find $DEST_DIR -name CVS -exec rm -Rf {} \;

# Make sure that the loader and kernel are the first things in the archive
(cd $DEST_DIR; zip ../visopsys.zip $LOADER $KERNEL)
# Finish the archive
(cd $DEST_DIR; zip -r ../visopsys.zip * -x $LOADER $KERNEL)

# Remove the destination directory we created
rm -Rf $DEST_DIR

# Done
exit 0
