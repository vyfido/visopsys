##
##  Visopsys
##  Copyright (C) 1998-2020 J. Andrew McLaughlin
##
##  Makefile
##

# This is the top-level software directory Makefile

all:
	${MAKE} -C helloworld DEBUG=${DEBUG}

debug:
	${MAKE} all DEBUG=1

clean:
	${MAKE} -C helloworld clean
	rm -Rf *~ core

