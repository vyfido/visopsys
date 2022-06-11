#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2006 J. Andrew McLaughlin
## 
##  image-floppy.sh
##

# Installs the Visopsys system into a zipped floppy image file

echo ""
echo "Making Visopsys FLOPPY IMAGE file"

while [ "$1" != "" ] ; do

    # Are we doing a release version?  If the argument is "-r" then we use
    # the release number in the destination directory name.  Otherwise, we
    # assume an interim package and use the date instead
    if [ "$1" == "-r" ] ; then
	# What is the current release version?
	RELEASE=`./release.sh`
	echo " - doing RELEASE version $RELEASE"
    fi

    if [ "$1" == "-isoboot" ] ; then
	# Only doing an ISO boot floppy (just the kernel and OS loader)
	ISOBOOT=-isoboot
	echo " - doing ISO boot image"
    fi

    shift
done

if [ "$RELEASE" == "" ] ; then
    # What is the date?
    RELEASE=`date +%Y-%m-%d`
    echo " - doing INTERIM version $RELEASE (use -r flag for RELEASES)"
fi

NAME=visopsys-"$RELEASE"
IMAGEFILE="$NAME""$ISOBOOT".img
ZIPFILE="$NAME""$ISOBOOT"-img.zip
rm -f $IMAGEFILE
cp blankfloppy.gz "$IMAGEFILE".gz
gunzip "$IMAGEFILE".gz

if [ "$ISOBOOT" != "" ] ; then
    ./install.sh -isoboot $IMAGEFILE
else
    ./install.sh -basic $IMAGEFILE
fi

if [ $? -ne 0 ] ; then
    exit $?
fi

echo "Visopsys $RELEASE Image Release" > /tmp/comment
echo "Copyright (C) 1998-2006 J. Andrew McLaughlin" >> /tmp/comment
rm -f $ZIPFILE
zip -9 -z -r $ZIPFILE $IMAGEFILE < /tmp/comment >& /dev/null

rm -f /tmp/comment $IMAGEFILE

echo ""
echo "File is: $ZIPFILE"
echo ""

exit 0
