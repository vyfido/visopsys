#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2007 J. Andrew McLaughlin
## 
##  install.sh
##

# Installs the Visopsys system on the requested device.  Suitable for use
# only with Visopsys SOURCE distribution.  Note that the device must have
# permissions which allow direct writes by the invoking user. 

BUILDDIR=../build
BOOTSECTOR="$BUILDDIR"/system/boot/bootsect.fat
OSLOADER=vloader
FSTAB=/etc/fstab
BASICFILES=../dist/system/install-files.basic
FULLFILES=../dist/system/install-files.full
INSTTYPE=full
COPYBOOTLOG=./copy-boot.log
MKDOSFSLOG=./mkdosfs.log

echo ""
echo "Visopsys installation script for UNIX"
echo ""

if [ "$1" == "-basic" ] ; then
	INSTTYPE=basic
	shift
elif [ "$1" == "-isoboot" ] ; then
	INSTTYPE=isoboot
	shift
fi

DEVICE=$1
if [ "$DEVICE" = "" ] ; then
	echo "No installation device specified."
	echo "Usage: $0 <installation device>"
	echo "Terminating."
	echo ""
	exit 1
elif [ ! -e $DEVICE ] ; then
	echo "Installation device does not exist."
	echo "Usage: $0 <installation device>"
	echo "Terminating."
	echo ""
	exit 1
fi
echo "Installing Visopsys on $DEVICE"

# Make sure we have write permission on the device
if [ ! -w "$DEVICE" ] ; then
	echo ""
	echo "You do not have permission to write directly to the disk device"
	echo "file ($DEVICE) on this system.  (try 'chmod go+rw $DEVICE')."
	echo "Terminating."
	echo ""
	exit 1
fi

# Just to a quick check to see whether we think a build has been done
if [ ! -e "$BUILDDIR" ] ; then
	echo ""
	echo "$BUILDDIR is missing.  Have you done a make yet?"
	echo "Terminating."
	echo ""
	exit 1
fi

MNTARGS=
# If it is a regular device, format it
if [ -b "$DEVICE" ] ; then
	echo -n "Formatting...  "
	# Format the disk.  Stop if the command fails.
	/sbin/mkdosfs -v $DEVICE >& $MKDOSFSLOG
	RET=$? 
	if [ $RET -ne 0 ] ; then
		if [ `grep -c "too large" $MKDOSFSLOG` -ne 0 ] ; then
			# Wait, will specifying FAT32 work?
			echo -n "(trying FAT32)...  "
			/sbin/mkdosfs -F32 -v $DEVICE >& $MKDOSFSLOG
			RET=$?
		fi
	fi
	if [ $RET -ne 0 ] ; then
		echo ""
		echo -n "Not able to format disk $DEVICE.  See $MKDOSFSLOG.  "
		echo Terminating.
		echo ""
		exit 1
	else
		echo Done
	fi
else
	MNTARGS="-o loop,sync"
fi

# Check whether we should be using a different boot sector than the default
if [ -f $MKDOSFSLOG ] ; then
	if [ `grep -c "32-bit" $MKDOSFSLOG` -ne 0 ] ; then
		BOOTSECTOR="$BOOTSECTOR"32
	fi
fi
rm -f $MKDOSFSLOG

# Install the boot sector.
echo -n "Copying boot sector...  "
./copy-boot $BOOTSECTOR $DEVICE >& $COPYBOOTLOG
if [ $? -ne 0 ] ; then
	echo ""
	echo -n "Not able to copy boot sector to $DEVICE.  "
	echo "See $COPYBOOTLOG.  Terminating."
	echo ""
	exit 1
fi
echo Done
rm -f $COPYBOOTLOG

# Try to figure out where the disk gets mounted by reading the fstab
# file.  If this is not working on your system, you should override
# this manually (or fix the following command)
MOUNTDIR=`cat $FSTAB | grep $DEVICE | awk '{print $2}'`

if [ "$MOUNTDIR" != "" ] ; then
	mount $DEVICE
else
	MOUNTDIR=tmp_mnt
	mkdir -p $MOUNTDIR
	mount $MNTARGS $DEVICE $MOUNTDIR
fi

# Mount the filesystem
if [ $? -ne 0 ] ; then
	echo ""
	echo "Not able (or not permitted) to mount disk $DEVICE.  Terminating."
	echo ""
	exit 1
fi

# Copy files from the build directory
echo -n "Copying files...  "
if [ "$INSTTYPE" = "isoboot" ] ; then
	for FILE in /vloader /bootinfo /grphmode /visopsys ; do
		if [ -f "$BUILDDIR""$FILE" ] ; then
			cp "$BUILDDIR""$FILE" "$MOUNTDIR""$FILE"
		fi
	done
else
	for FILE in `cat $BASICFILES` ; do
		if [ -d "$BUILDDIR""$FILE" ] ; then
			mkdir -p "$MOUNTDIR""$FILE"
		elif [ -f "$BUILDDIR""$FILE" ] ; then
			cp "$BUILDDIR""$FILE" "$MOUNTDIR""$FILE"
		fi
	done

	if [ "$INSTTYPE" != "basic" ] ; then
		for FILE in `cat $FULLFILES` ; do
			if [ -d "$BUILDDIR""$FILE" ] ; then
				mkdir -p "$MOUNTDIR""$FILE"
			elif [ -f "$BUILDDIR""$FILE" ] ; then
				cp "$BUILDDIR""$FILE" "$MOUNTDIR""$FILE"
			fi
		done
	fi
fi

sync
echo Done

# Uncomment to have the os loader print out hardware/boot information
# touch $MOUNTDIR/bootinfo
# Uncomment to disable graphics mode
# touch $MOUNTDIR/nograph

# Unmount the filesystem
umount $MOUNTDIR

if [ -d tmp_mnt ] ; then
	rmdir tmp_mnt
fi

exit 0
