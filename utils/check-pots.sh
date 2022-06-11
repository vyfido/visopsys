#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2020 J. Andrew McLaughlin
##
##  check-pots.sh
##

PROGDIR=../src/programs

for FILE in ${PROGDIR}/*.c ; do
	BASE=$(basename -s .c $FILE)
	if [ ! -e "${PROGDIR}/${BASE}.pot" ] ; then
		echo "$BASE.c does not have a .pot"
	fi
done

