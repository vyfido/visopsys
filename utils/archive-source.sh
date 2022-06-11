#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2013 J. Andrew McLaughlin
## 
##  archive-source.sh
##

# This just does all of the things necessary to prepare an archive (zipfile)
# of the Visopsys sources and utilities.

ZIPLOG=./zip.log

echo ""
echo "Making Visopsys SOURCE archive"
echo ""

# Are we doing a release version?  If the argument is "-r" then we use
# the release number in the destination directory name.  Otherwise, we
# assume an interim package and use the date instead
if [ "$1" = "-r" ] ; then
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

DESTDIR=visopsys-"$RELEASE"-src

# Make a copy of the Visopsys directory.  We will not fiddle with the current
# working area
rm -Rf "$DESTDIR" /tmp/"$DESTDIR"
mkdir -p /tmp/"$DESTDIR"
(cd ..; tar cf - *) | (cd /tmp/"$DESTDIR"; tar xf - )
mv /tmp/"$DESTDIR" ./

# Make sure it's clean
echo -n "Making clean... "
make -C "$DESTDIR" clean > /dev/null 2>&1
# Remove all the things we don't want to distribute
# CVS droppings
find "$DESTDIR" -name CVS -exec rm -R {} \; > /dev/null 2>&1
# Other stuff
rm -f "$DESTDIR"/*.patch
rm -Rf "$DESTDIR"/work
rm -f "$DESTDIR"/src/HARDWARE.txt
rm -f "$DESTDIR"/src/ISSUES.txt
rm -Rf "$DESTDIR"/patches
# Stuff from the 'plus' distribution
rm -f "$DESTDIR"/src/kernel/kernelFilesystemFatPlus.c
rm -f "$DESTDIR"/src/programs/sysdiag.c
rm -f "$DESTDIR"/src/programs/sysdiag.pot
rm -Rf "$DESTDIR"/src/lib/liblic/
rm -f "$DESTDIR"/src/include/sys/keygen.h
rm -Rf "$DESTDIR"/ports/ntfsprogs/de
rm -Rf "$DESTDIR"/src/lib/libwindow/de
rm -Rf "$DESTDIR"/src/kernel/de
rm -Rf "$DESTDIR"/src/programs/fdisk/de
rm -Rf "$DESTDIR"/src/programs/de
echo Done

echo -n "Archiving... "
echo "Visopsys $RELEASE Source Release" > /tmp/comment
echo "Copyright (C) 1998-2013 J. Andrew McLaughlin" >> /tmp/comment
rm -f "$DESTDIR".zip
zip -9 -z -r "$DESTDIR".zip "$DESTDIR" < /tmp/comment > $ZIPLOG 2>&1
if [ $? -ne 0 ] ; then
	echo ""
	echo -n "Not able to create zip file "$DESTDIR".zip.  "
	echo "See $ZIPLOG.  Terminating."
	echo ""
	exit 1
fi
rm -f /tmp/comment $ZIPLOG
echo Done

echo -n "Cleaning up... "
# Remove the working directory
rm -Rf "$DESTDIR"
echo Done

echo "File is: $DESTDIR.zip"
echo ""

exit 0
