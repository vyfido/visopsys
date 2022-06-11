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
##  archive-binary.sh
##

# This just copies all of the various parts of the current Visopsys
# distribution into the proper structure as a directory called
# visopsys-YYYY-MM-DD in the current directory.


echo ""
echo "Making Visopsys BINARY archive"
echo ""

# Make sure we know where we are
if !(-e ../../visopsys) then
    echo ""
    echo "Error!  Please run this script from the utils directory"
    echo ""
    exit 1
endif

# Are we doing a release version?  If the argument is "-r" then we use
# the release number in the destination directory name.  Otherwise, we
# assume an interim package and use the date instead
if ("$1" == "-r") then
    echo "(doing RELEASE version)"
    # What is the current release version?
    set RELEASE = `./release.sh`
    # Use it to set the detination directory name
    set DESTDIR = ./visopsys-$RELEASE-bin
else
    echo "(doing INTERIM version -- use -r flag for RELEASES)"
    # What is the date?
    set DATE = `date +%Y-%m-%d`
    # Use it to set the detination directory name
    set DESTDIR = ./visopsys-$DATE-bin
endif

# What platform are we on?
set PLATFORM = `./platform.sh`

# Zip up the installation files
rm -f visopsys.zip
./zip-instfiles.sh

# Make a directory to hold the new distribution.  Remove any existing one
# first
rm -Rf $DESTDIR
mkdir -p $DESTDIR

# Destination directories
set INSTFILESDIR = $DESTDIR/files
set DOSDIR       = $DESTDIR/dosutil
set UNIXDIR      = $DESTDIR/unixutil
set DOCSDIR      = $DESTDIR/docs

# Copy the files into the directory tree
echo -n "Copying Visopsys files... "

set MAINFILES   = ( dist/README.TXT \
		    dist/install.sh \
		    dist/install.bat \
		    dist/installer/vInstall.jar \
		    dist/installer/vPic.jpg )

set INSTFILES   = ( ../src/osloader/$PLATFORM/bootsect.f12 \
		    visopsys.zip )

set DOSFILES    = ( dist/dos/format.bat \
		    dist/dos/writeboot.bat \
		    dist/dos/writeboot.com )

set UNIXFILES   = ( ./platform.sh )

set DOCSFILES   = ( ../docs/docs/visopsys.org )

# Make sure all of these files are in place
foreach FILE ( $MAINFILES $INSTFILES $DOSFILES $UNIXFILES )
    if !(-e $FILE) then
	echo ""
	echo "Unable to find file $FILE"
	echo "Have you done a make yet?"
	echo ""
	exit 2
    endif
end


mkdir $UNIXDIR

# Copy 
cp $MAINFILES $DESTDIR
mkdir -p $INSTFILESDIR ; cp $INSTFILES $INSTFILESDIR
mkdir -p $DOSDIR       ; cp $DOSFILES $DOSDIR
mkdir -p $UNIXDIR      ; cp $UNIXFILES $UNIXDIR
mkdir -p $DOCSDIR      ; cp -R $DOCSFILES $DOCSDIR

echo done

# Remove all of the CVS droppings from directories we copied
find $DESTDIR -name CVS -exec rm -R {} \; >& /dev/null

# Archive everything
echo -n "Archiving... "

# Archive-name variables
set ZIPNAME = $DESTDIR.zip
set TARNAME = $DESTDIR.tar

rm -f $ZIPNAME $TARNAME.gz
    

# Now we need to zip/tar/gzip everything
zip -r $ZIPNAME $DESTDIR >& /dev/null
# Tar the directory
tar cf $TARNAME $DESTDIR >& /dev/null
# Gzip the tar file
gzip $TARNAME

echo done

# Remove the destination directory we created
echo -n "Cleaning up... "
rm -Rf $DESTDIR visopsys.zip
echo done

echo "Files are:"
echo $ZIPNAME
echo $TARNAME.gz
echo ""

# Done
exit 0
