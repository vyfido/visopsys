#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2011 J. Andrew McLaughlin
## 
##  wget-site.sh
##

# Retrieves the relevant parts of the visopsys.org website that we include
# in the 'docs' directory.

wget --recursive --page-requisites --html-extension --convert-links --restrict-file-names=windows --reject zip --domains visopsys.org visopsys.org

rm -Rf visopsys.org/forums

exit 0
