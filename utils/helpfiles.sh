#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2006 J. Andrew McLaughlin
## 
##  helpfiles.sh
##

# This is used against the program files listed in the install files
# in order to harvest the help text into .txt files in the $TEXTDIR directory

SRCDIR=../src
DISTDIR=../dist
TEXTDIR=$DISTDIR/programs/helpfiles
TMPFILE=/tmp/help.tmp
INST_FILES="$DISTDIR/system/install-files.basic $DISTDIR/system/install-files.full"

# Make sure each program listed in the install files has a help file
for INSTFILE in $INST_FILES ; do
	for FILE in /programs/help `cat $INSTFILE | grep ^/programs` ; do
		if [ -f $SRCDIR$FILE.c ] ; then

			NAME=`echo $FILE | sed -e 's/\/programs\///'`

			# Get the help text from the file
			sed -n '/<help>/,/<\/help>/p' $SRCDIR$FILE.c | sed -e '/<help>/d' | sed -e '/<\/help>/d' > $TMPFILE

			if [ -s $TMPFILE ] ; then
				mv $TMPFILE $TEXTDIR/$NAME.txt
			fi

			rm -f $TMPFILE

			# Make sure it has a help file
			if [ ! -f $TEXTDIR/$NAME.txt ] ; then
				echo - $FILE does not have a help file

			# Make sure it has an entry in the main help file
			elif [ "`grep \"^$NAME \" $TEXTDIR/help.txt`" == "" ]
			then
				echo - $FILE does not have a help summary entry
			fi
		fi
	done
done
