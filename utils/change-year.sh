#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2015 J. Andrew McLaughlin
##
##  change-year.sh
##

find . -type f -exec sed -i -e 's/Copyright (C) 1998-2014/Copyright (C) 1998-2015/g' {} \;

