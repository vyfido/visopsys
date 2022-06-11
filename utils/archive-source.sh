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
##  archive-source.sh
##

# This just does all of the things necessary to prepare an archive (zipfile)
# of the visopsys sources and utilities.


echo ""
echo "Making Visopsys SOURCE archive"
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
    set DESTDIR = visopsys-$RELEASE-src
else
    echo "(doing INTERIM version -- use -r flag for RELEASES)"
    # What is the date?
    set DATE = `date +%Y-%m-%d`
    # Use it to set the detination directory name
    set DESTDIR = visopsys-$DATE-src
endif

# Make a copy of the visopsys directory.  We will not fiddle with the current
# working area
rm -Rf $DESTDIR
cp -R .. /tmp/$DESTDIR
mv /tmp/$DESTDIR ./

echo -n "Making clean... "

# Make sure it's clean
make -C $DESTDIR clean >& /dev/null

# Remove all the things we don't want to distribute
# CVS droppings
find $DESTDIR -name CVS -exec rm -R {} \; >& /dev/null
# Other stuff
rm -Rf $DESTDIR/work
rm -f $DESTDIR/src/ISSUES.txt

echo done
echo -n "Archiving... "

# Remove any existing archive files here
rm -f $DESTDIR.zip

# Zip the working directory
zip -9 -r $DESTDIR.zip $DESTDIR >& /dev/null

echo done
echo -n "Cleaning up... "

# Remove the working directory
rm -Rf $DESTDIR

echo done

echo "File is: $DESTDIR.zip"
echo ""

exit 0
