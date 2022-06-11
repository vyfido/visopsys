#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
##  kernel-symbols.sh
##

# This thing just dumps the symbol table of the kernel and creates a simple
# list of all global function symbols and their addresses.

TMPFILE=symbols$$.tmp

if [ $# -ne 2 ] ; then
	echo "Usage: $0 <kernel_file> <output_file>"
	exit 1
fi

/usr/bin/objdump --syms $1 | grep 'F .text' > "$TMPFILE"1
/bin/cat "$TMPFILE"1 | /bin/sort > "$TMPFILE"2
/bin/sed -e 's/[	 ][lg][	 ]*F[	 ].text[	 ][^	 ]*[	 ]*/=/g' "$TMPFILE"2 > $2
rm -f "$TMPFILE"1 "$TMPFILE"2
exit 0
