#!/bin/sh
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
##  image-cd.sh
##

echo ""
echo "Making Visopsys CD-ROM IMAGE file"
echo ""

# Are we doing a release version?  If the argument is "-r" then we use
# the release number in the destination directory name.  Otherwise, we
# assume an interim package and use the date instead
if [ "$1" == "-r" ] ; then
	echo "(doing RELEASE version)"
	echo ""
	# What is the current release version?
	RELEASE=`./release.sh`
	RELFLAG=-r
else
	echo "(doing INTERIM version -- use -r flag for RELEASES)"
	echo ""
	# What is the date?
	RELEASE=`date +%Y-%m-%d`
	RELFLAG=
fi

NAME=visopsys-$RELEASE
FLOPPYIMAGE=floppy.img #$NAME.img
SOURCEDIR=$NAME-src
ISOIMAGE=visopsys.iso #$NAME.iso
ZIPFILE=$NAME-iso.zip
TMPDIR=/tmp/iso$$.tmp

echo -n "Making floppy image... "
./image-floppy.sh $RELFLAG >& /dev/null
echo Done

# Get the basic files from the floppy image
echo -n "Copying base files... "
mount $FLOPPYIMAGE
rm -Rf $TMPDIR
mkdir $TMPDIR
pushd /mnt/floppy >& /dev/null
tar cf - * | (cd $TMPDIR; tar xf - )
popd >& /dev/null
umount /mnt/floppy
cp $FLOPPYIMAGE $TMPDIR
echo Done

echo -n "Making source archive... "
./archive-source.sh $RELFLAG >& /dev/null
echo Done

# Copy sources
echo -n "Copying source files... "
unzip $SOURCEDIR.zip >& /dev/null
mkdir -p $TMPDIR/source
mv $SOURCEDIR $TMPDIR/source/
echo Done

rm -f $ISOIMAGE
mkisofs -U -D -floppy-boot -b $FLOPPYIMAGE -c boot.catalog -hide $FLOPPYIMAGE -hide boot.catalog -V "Visopsys $RELEASE" -iso-level 3 -L -o $ISOIMAGE $TMPDIR

echo "Visopsys $RELEASE CD-ROM Release" > /tmp/comment
echo "Copyright (C) 1998-2004 J. Andrew McLaughlin" >> /tmp/comment
rm -f $ZIPFILE
zip -9 -z -r $ZIPFILE $ISOIMAGE < /tmp/comment >& /dev/null

rm -f /tmp/comment
#rm -f $FLOPPYIMAGE $ISOIMAGE
rm -Rf $TMPDIR

echo ""
echo "File is: $ZIPFILE"
echo ""

exit 0
