#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
## 
##  helpfiles.sh
##

CODEDIR=../src/programs
TEXTDIR=../dist/programs/helpfiles
TMPFILE=/tmp/help.tmp

# This is used against the .c files in the $CODEDIR directory in order
# to harvest the help text into .txt files in the $TEXTDIR directory

# Get all the names of the files
CODEFILES=`(cd $CODEDIR ; find -name '*.c')`
echo $CODEFILES
CODEFILES=`echo $CODEFILES | sed -e 's/\.\///g' -e 's/\.c//g'`

for FILE in $CODEFILES; do
    # Get the help text from the file
    sed -n '/<help>/,/<\/help>/p' $CODEDIR/$FILE.c | sed -e '/<help>/d' | sed -e '/<\/help>/d' > $TMPFILE

    if [ -s $TMPFILE ] ; then
	mv $TMPFILE $TEXTDIR/$FILE.txt
    fi
done

# Notify about any files that don't have help text
for FILE in $CODEFILES; do
    if [ ! -f $TEXTDIR/$FILE.txt ] ; then
	echo - $FILE does not have a help file
    fi
done

rm -f $TMPFILE
