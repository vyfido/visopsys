#!/bin/sh

find . -type f -exec sed -i -e 's/Copyright (C) 1998-2004/Copyright (C) 1998-2004/g' {} \;
