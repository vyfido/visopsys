#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2020 J. Andrew McLaughlin
##
##  change-year.sh
##

find . -type f -exec sed -i -e 's/Copyright (C) 1998-2020/Copyright (C) 1998-2020/g' {} \;

