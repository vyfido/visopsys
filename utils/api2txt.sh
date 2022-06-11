#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2006 J. Andrew McLaughlin
## 
##  api2txt.sh
##

# This is used against the kernel API header file, as well as the VSH
# and window library header files, to generate a simple formatted TEXT
# listing of the function declarations with their descriptions

if [ $# != 1 ] ; then
	echo "Usage: $0 <filename>"
	exit 1
fi

sed -n -e 's/static inline /\n\n/' -e 's/[        ]*_X_ //p' -e 's/[ ]*\/\/[ ]*Desc[ ]*: /\n	/p' $1

