#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  image-floppy.sh
##

# Installs the Visopsys system into a zipped floppy image file

ARCH=x86
BUILDDIR=../build
BLANKFLOPPY=./blankfloppy.gz
INSTSCRIPT=./install.sh
BOOTSECTOR=visopsys.bss
MOUNTDIR=./tmp_mnt
SYSLINUXCFG=./syslinux.cfg
SYSLINUXLOG=./syslinux.log
ZIPLOG=./zip.log

echo ""
echo "Making Visopsys FLOPPY IMAGE file"

while [ -n "$1" ] ; do
	# Are we doing a release version?  If the argument is "-r" then we use the
	# release number in the destination directory name.  Otherwise, we assume
	# an interim package and use the date instead.
	if [ "$1" = "-r" ] ; then
		# What is the current release version?
		RELEASE=`./release.sh`
		echo " - doing RELEASE version $RELEASE"

	elif [ "$1" = "-isoboot" ] ; then
		# Only doing an ISO boot floppy (just the kernel and OS loader)
		ISOBOOT=-isoboot
		echo " - doing ISO boot image"

	elif [ "$1" = "-syslinux" ] ; then
		# Make it a syslinux image
		SYSLINUX=-syslinux
		echo " - doing SYSLINUX boot image"

	# Was an architecture specified?
	elif [ -d "$BUILDDIR/$1" ] ; then
		ARCH="$1"
		echo " - using architecture $ARCH"

	else
		echo ""
		echo "Unexpected argument $1.  Terminating"
		echo ""
		exit 1
	fi

	shift
done

# Check for things we need to be in the current directory
for FILE in $BLANKFLOPPY $INSTSCRIPT ; do
	if [ ! -f $FILE ] ; then
		echo ""
		echo "Required file $FILE not found.  Terminating"
		echo ""
		exit 1
	fi
done

if [ -z "$RELEASE" ] ; then
	# What is the date?
	RELEASE=`date +%Y-%m-%d`
	echo " - doing INTERIM version $RELEASE (use -r flag for RELEASES)"
fi

NAME="visopsys-$RELEASE-$ARCH"
IMAGEFILE="$NAME""$ISOBOOT"-floppy.img
ZIPFILE="$NAME""$ISOBOOT"-floppy-img.zip
rm -f $IMAGEFILE
cp $BLANKFLOPPY "$IMAGEFILE".gz
gunzip "$IMAGEFILE".gz

# Run the installation script
if [ -n "$ISOBOOT" ] ; then
	$INSTSCRIPT $ISOBOOT $ARCH $IMAGEFILE
else
	$INSTSCRIPT -basic $ARCH $IMAGEFILE
fi
if [ $? -ne 0 ] ; then
	echo ""
	echo "Install failure.  Terminating"
	echo ""
	exit 1
fi

if [ -n "$SYSLINUX" ] ; then
	dd if=$IMAGEFILE bs=512 count=1 of=./$BOOTSECTOR > /dev/null 2>&1
	syslinux $IMAGEFILE > $SYSLINUXLOG 2>&1
	if [ $? -ne 0 ] ; then
		echo ""
		echo -n "Not able to make syslinux image.  See $SYSLINUXLOG."
		echo "Terminating"
		echo ""
		exit 1
	fi
	MOUNTDIR=tmp_mnt
	mkdir -p $MOUNTDIR
	mount -o loop -t msdos $IMAGEFILE $MOUNTDIR
	echo "default Visopsys" > $SYSLINUXCFG
	echo "label Visopsys" >> $SYSLINUXCFG
	echo "	kernel $BOOTSECTOR" >> $SYSLINUXCFG
	cp ./$BOOTSECTOR $SYSLINUXCFG $MOUNTDIR
	umount $MOUNTDIR
	rm -f ./$BOOTSECTOR $SYSLINUXCFG $SYSLINUXLOG
	if [ -d $MOUNTDIR ] ; then
		rmdir $MOUNTDIR
	fi
fi

echo -n "Archiving... "
echo "Visopsys $RELEASE Image Release" > /tmp/comment
echo "Copyright (C) 1998-2021 J. Andrew McLaughlin" >> /tmp/comment
rm -f $ZIPFILE
zip -9 -z -r $ZIPFILE $IMAGEFILE < /tmp/comment > $ZIPLOG 2>&1
if [ $? -ne 0 ] ; then
	echo ""
	echo -n "Not able to create zip file $ZIPFILE.  "
	echo "See $ZIPLOG.  Terminating."
	echo ""
	exit 1
fi
rm -f /tmp/comment $IMAGEFILE $ZIPLOG
echo Done

echo ""
echo "File is: $ZIPFILE"
echo ""

exit 0

