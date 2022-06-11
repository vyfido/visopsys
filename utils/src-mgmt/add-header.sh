#!/bin/csh -f

##
##  Visopsys
##  Copyright (C) 1998-2001 J. Andrew McLaughlin
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


# We expect two sets of parameters.  One is the name of a text file 
# containing the new header text.  The rest of the parameters are the names
# of the files to add the header to.  Like so:
#    add-header.sh <header-text-file> <file> [file2 file3 ... fileN]

if ("$1" == "" || "$1" == "-h" || "$1" == "-help") then
    goto Usage
endif

# First we get the name of the file to use as the new header

set HEADER = "$1"

if !(-e "$HEADER") then
    echo "Header file $HEADER cannot be found!"
    goto Usage
endif

# Ok, the file exists.  Make sure there's at least one more parameter
# (shift out the ones we're finished with)
shift

if ("$1" == "") then
    echo "No files to attach header to!"
    goto Usage
    exit 1
endif

# We have at least one more argument (a file to add the header to).
# Now we will loop for each of the trailing arguments

set BACKUPDIR = ./add-header-backup

# Remove any previous backup directory and temp-file
rm -f ./add-header-tmpfile.tmp

# Make a new backup directory
mkdir -p $BACKUPDIR

while ("$1" != "")
    
    # Get the next file
    set ARGFILE = $1
    
    # Shift it out of the argument list
    shift

    # Make sure the file exists
    if !(-e "$ARGFILE") then
	echo "File $ARGFILE does not exist!"
	exit 2
    endif

    # Copy it to a safe location for backup
    cp $ARGFILE $BACKUPDIR/

    # Copy it to the temp file
    cp $ARGFILE ./add-header-tmpfile.tmp

    # Replace the file with the new standard header
    cat $HEADER > $ARGFILE

    # Attach the file to the end
    cat ./add-header-tmpfile.tmp >> $ARGFILE

    # Remove the temp-file
    rm -f ./add-header-tmpfile.tmp

    echo "changed $ARGFILE"
end

    echo "Backups are in $BACKUPDIR"

exit 0

Usage:
    echo ""
    echo "Usage:"
    echo "       $0 <header-text-file> <file> [file2 file3 ... fileN]"
    echo ""
    exit 0


