#!/bin/sh
##
##  Visopsys
##  Copyright (C) 1998-2006 J. Andrew McLaughlin
## 
##  arch.sh
##

# Echoes the minimal and optimal architecture and CPU requirements:

# Required to run
MARCH=pentium

# Optimized for
MCPU=pentium

echo "-march=$MARCH -mcpu=$MCPU"
exit 0
