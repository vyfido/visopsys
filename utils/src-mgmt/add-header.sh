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
##  add-header.sh
##

#  This is designed for adding standard headers to source code.  It is 
#  NOTHING FANCY, I guarantee, but it will make the process a little bit
#  easier.


# We expect three parameters.  The first is the number of lines of header
# to add to the destination file.  The next is the name of a text file 
# containing the new header text.  The last is the name of the file to add
# the header to.  Like so:
#    add-header.sh <number of lines> <header-text-file> <file>

if ("$1" == "" || "$1" == "-h" || "$1" == "-help") then
    goto Usage
    exit 0
endif

# First we get the number of lines to add

set NUMBERLINES = "$1"

# Next we get the name of the file to use as the new header

set HEADER = "$2"

if !(-e "$HEADER") then
    echo "Header file $HEADER cannot be found!"
    goto Usage
    exit 1
endif

# Get the name of the target file

set TARGET = "$3"

# Make sure the file exists
if !(-e "$TARGET") then
    echo "File $TARGET does not exist!"
    exit 1
endif

set BACKUPDIR = ./add-header-backup

# Remove any previous temp-file
rm -f ./add-header-tmpfile.tmp

# Make a new backup directory if necessary
mkdir -p $BACKUPDIR

# Copy it to a safe location for backup
cp $TARGET $BACKUPDIR/

# Copy it to the temp file
cp $TARGET ./add-header-tmpfile.tmp

# Replace the file with the new standard header
head -$NUMBERLINES $HEADER > $TARGET

# Attach the file to the end
cat ./add-header-tmpfile.tmp >> $TARGET

# Remove the temp-file
rm -f ./add-header-tmpfile.tmp

echo "changed $TARGET"
echo "Backup is in $BACKUPDIR"

exit 0

Usage:
    echo ""
    echo "Usage:"
    echo "       $0 <lines to add> <header-text-file> <file>"
    echo ""
    exit 0


