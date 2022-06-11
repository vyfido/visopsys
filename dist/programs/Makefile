##
##  Visopsys
##  Copyright (C) 1998-2006 J. Andrew McLaughlin
## 
##  Makefile
##

BUILDDIR=../../build/programs

all:
	mkdir -p ${BUILDDIR}
	make -C helpfiles
	make -C mines.dir
	make -C snake.dir

clean:
	rm -f *~ core
	make -C helpfiles clean
	make -C mines.dir clean
	make -C snake.dir clean
