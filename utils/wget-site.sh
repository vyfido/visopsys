#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2017 J. Andrew McLaughlin
##
##  wget-site.sh
##

# Retrieves the relevant parts of the visopsys.org website that we include
# in the 'docs' directory.
SITE=visopsys.org

wget --recursive --level=99 --page-requisites --convert-links --restrict-file-names=windows -X /forums --reject zip --domains $SITE $SITE

rm -Rf $SITE/feed $SITE/comments
rm -f $SITE/index.html\@*

exit 0
