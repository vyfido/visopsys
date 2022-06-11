#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
## 
##  image-floppy.sh
##

# Installs the Visopsys system into a zipped floppy image file

echo ""
echo "Making Visopsys FLOPPY IMAGE file"
echo ""

# Are we doing a release version?  If the argument is "-r" then we use
# the release number in the destination directory name.  Otherwise, we
# assume an interim package and use the date instead
if [ "$1" == "-r" ] ; then
    # What is the current release version?
    RELEASE=`./release.sh`
    echo "(doing RELEASE version $RELEASE)"
    echo ""
else
    # What is the date?
    RELEASE=`date +%Y-%m-%d`
    echo "(doing INTERIM version $RELEASE -- use -r flag for RELEASES)"
    echo ""
fi

NAME=visopsys-"$RELEASE"
IMAGEFILE="$NAME".img
ZIPFILE="$NAME"-img.zip

rm -f $IMAGEFILE
cp blankfloppy.gz "$IMAGEFILE".gz
gunzip "$IMAGEFILE".gz

./install.sh -basic $IMAGEFILE
if [ $? -ne 0 ] ; then
    exit $?
fi

echo "Visopsys $RELEASE Image Release" > /tmp/comment
echo "Copyright (C) 1998-2004 J. Andrew McLaughlin" >> /tmp/comment
rm -f $ZIPFILE
zip -9 -z -r $ZIPFILE $IMAGEFILE < /tmp/comment >& /dev/null

rm -f /tmp/comment $IMAGEFILE

echo ""
echo "File is: $ZIPFILE"
echo ""

exit 0
