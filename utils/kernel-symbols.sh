#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2011 J. Andrew McLaughlin
## 
##  kernel-symbols.sh
##

# This thing just dumps the symbol table of the kernel and creates a simple
# list of all global function symbols and their addresses.

TMPFILE=symbols$$.tmp

if [ $# -ne 2 ] ; then
	echo "Usage: $0 <kernel_file> <output_file>"
	exit 1
fi

# Sort can be in different places.  We won't try to guess.
SORT=`which sort`
if [ ! -x "$SORT" ] ; then
	echo "No 'sort' command found in your PATH"
	exit 1
fi

/usr/bin/objdump --syms $1 | grep 'F .text' > "$TMPFILE"1
/bin/cat "$TMPFILE"1 | $SORT > "$TMPFILE"2
/bin/sed -e 's/[	 ][lg][	 ]*F[	 ].text[	 ][^	 ]*[	 ]*/=/g' "$TMPFILE"2 > $2
rm -f "$TMPFILE"1 "$TMPFILE"2
exit 0
