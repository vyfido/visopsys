#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
## 
##  image-cd.sh
##

# Installs the Visopsys system into a zipped CD-ROM ISO image file

echo ""
echo "Making Visopsys CD-ROM IMAGE file"
echo ""

# Are we doing a release version?  If the argument is "-r" then we use
# the release number in the destination directory name.  Otherwise, we
# assume an interim package and use the date instead
if [ "$1" == "-r" ] ; then
    # What is the current release version?
    RELEASE=`./release.sh`
    echo "(doing RELEASE version $RELEASE)"
    echo ""
    RELFLAG=-r
else
    # What is the date?
    RELEASE=`date +%Y-%m-%d`
    echo "(doing INTERIM version $RELEASE -- use -r flag for RELEASES)"
    echo ""
	RELFLAG=
fi

BUILDDIR=../build
NAME=visopsys-$RELEASE
FLOPPYZIP=$NAME-img.zip
FLOPPYIMAGE=$NAME.img
SOURCEDIR=$NAME-src
ISOIMAGE=$NAME.iso
ZIPFILE=$NAME-iso.zip

TMPDIR=/tmp/iso$$.tmp
rm -Rf $TMPDIR
mkdir -p $TMPDIR

echo -n "Making/copying floppy image... "
./image-floppy.sh $RELFLAG >& /dev/null
if [ $? != 0 ] ; then
    exit $?
fi
unzip $FLOPPYZIP >& /dev/null
mv $FLOPPYIMAGE $TMPDIR
echo Done

# Copy all of the files from the build directory
echo -n "Copying build files... "
(cd $BUILDDIR ; tar cf - * ) | (cd $TMPDIR; tar xf - )
echo Done

echo -n "Archiving/copying source files... "
./archive-source.sh $RELFLAG >& /dev/null
if [ $? != 0 ] ; then
    exit $?
fi
unzip $SOURCEDIR.zip >& /dev/null
mkdir -p $TMPDIR/source
mv $SOURCEDIR $TMPDIR/source/
mv $TMPDIR/source/$SOURCEDIR/docs $TMPDIR/
echo Done

rm -f $ISOIMAGE
mkisofs -U -D -floppy-boot -b $FLOPPYIMAGE -c boot.catalog -hide $FLOPPYIMAGE -hide boot.catalog -V "Visopsys $RELEASE" -iso-level 3 -L -o $ISOIMAGE $TMPDIR >& /dev/null
if [ $? != 0 ] ; then
    exit $?
fi

echo "Visopsys $RELEASE CD-ROM Release" > /tmp/comment
echo "Copyright (C) 1998-2004 J. Andrew McLaughlin" >> /tmp/comment
rm -f $ZIPFILE
zip -9 -z -r $ZIPFILE $ISOIMAGE < /tmp/comment >& /dev/null

rm -f /tmp/comment
rm -f $ISOIMAGE
rm -Rf $TMPDIR

echo ""
echo "File is: $ZIPFILE"
echo ""

exit 0
