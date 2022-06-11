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
##  Makefile
##

BUILDDIR=../../../build/system

all:
	mkdir -p ${BUILDDIR}
	cp *.bmp install-files.* kernel.conf password.blank windowmanager.conf ${BUILDDIR}
	mkdir -p ${BUILDDIR}/icons 
	cp icons/*.bmp ${BUILDDIR}/icons
	mkdir -p ${BUILDDIR}/fonts
	cp fonts/*.bmp ${BUILDDIR}/fonts

clean:
	rm -f *~ core
	rm -Rf ${BUILDDIR}/icons ${BUILDDIR}/fonts ${BUILDDIR}/*.bmp ${BUILDDIR}/install-files.* ${BUILDDIR}/kernel.conf ${BUILDDIR}/password.blank ${BUILDDIR}/windowmanager.conf
