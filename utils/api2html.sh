#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2007 J. Andrew McLaughlin
## 
##  api2html.sh
##

# This is used against the kernel API header file, as well as the VSH
# and window library header files, to generate a simple formatted HTML
# listing of the function declarations with their descriptions

if [ $# != 1 ] ; then
	echo "Usage: $0 <filename>"
	exit 1
fi

sed -n	-e  's/static inline //' -e 's/[ 	]*_X_ /<p><font face="Courier New">/p ; /<font/a</font></p>' -e 's/[ ]*\/\/[ ]*Desc[ ]*: /<blockquote>\n  <p>/p ; /<block/a</p></blockquote>' $1 
