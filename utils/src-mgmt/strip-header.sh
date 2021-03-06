#!/bin/csh -f
##
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
##  strip-header.sh
##

#  This is designed for removing standard headers from source code.  It is 
#  NOTHING FANCY, I guarantee, but it will make the process a little bit
#  easier.


# We expect two sets of parameters.  One is the number of lines to remove
# from the destination files.  The rest of the parameters are the names
# of the files to remove the header from.  Like so:
#    strip-header.sh <lines-to-remove> <file> [file2 file3 ... fileN]

if ("$1" == "" || "$1" == "-h" || "$1" == "-help") then
    goto Usage
endif

# First we get the number of lines to remove from each destination file

set NUMBERLINES = "$1"

if ("$NUMBERLINES" == "0") then
    echo "Cannot interpret number of lines to remove: $NUMBERLINES"
    goto Usage
    exit 1
endif

# Get the name of the target file

set TARGET = "$2"

# Make sure the file exists
if !(-e "$TARGET") then
    echo "File $TARGET does not exist!"
    exit 1
endif

set BACKUPDIR = ./strip-header-backup

# Remove any previous temp-file
rm -f ./strip-header-tmpfile.tmp

# Make a new backup directory
mkdir -p $BACKUPDIR

# Copy it to a safe location for backup
cp $TARGET $BACKUPDIR/

# Copy it to the temp file
cp $TARGET ./strip-header-tmpfile.tmp

# Add one to the number of lines to remove
@ NUMBERLINES++

# Replace the file with the tail-ified version
tail +$NUMBERLINES ./strip-header-tmpfile.tmp > $TARGET

# Remove the temp-file
rm -f ./strip-header-tmpfile.tmp

echo "changed $TARGET"
echo "Backup is in $BACKUPDIR"

exit 0

Usage:
    echo ""
    echo "Usage:"
    echo "       $0 <lines-to-remove> <file>"
    echo ""
    exit 0


