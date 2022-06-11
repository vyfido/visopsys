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

# This is the top-level Makefile.

all:
	find . -type f -a ! -name \*.sh -exec chmod -x {} \;
	make -C utils -f Makefile all
	make -C src -f Makefile all

clean:
	find . -type f -a ! -name \*.sh -exec chmod -x {} \;
	make -C src -f Makefile clean
	make -C utils -f Makefile clean
	rm -f *~ core
