#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2020 J. Andrew McLaughlin
##
##  api2txt.sh
##

# This is run against the C library's kernel API code, as well as the VSH
# library, Visopsys library, and window library, to generate a simple
# formatted TEXT listing of the function declarations with their descriptions

if [ $# -ne 1 ] ; then
	echo "Usage: $0 <filename>"
	exit 1
fi

sed -n -e 's/[ ]*_U_[ ]*//g ; s/[ 	]*_X_[ ]*//p' \
	-e '/[ ]*\/\/[ ]*Desc[ ]*: /a\
\
' -e 's/[ ]*\/\/[ ]*Desc[ ]*: /\n	/p' $1

