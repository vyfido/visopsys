#!/bin/sh

# Writes the boot sector from the first argument (device?) to the second
# without discarding the FAT filesystem parameters from the second boot
# sector (i.e. leaves the 12th through 38th bytes intact

echo -n "Copying boot sector...  "

INPUT=$1
OUTPUT=$2

rm -f bootsect.inp bootsect.out bootsect.tmp

dd count=1 if=$INPUT of=bootsect.inp >& /dev/null
dd count=1 if=$OUTPUT of=bootsect.out >& /dev/null

head --bytes 3 bootsect.inp > bootsect.tmp
tail --bytes 509 bootsect.out >> bootsect.tmp
head --bytes 62 bootsect.tmp > bootsect.out

tail --bytes 450 bootsect.inp >> bootsect.out

if [ -b "$OUTPUT" ] ; then
    dd if=bootsect.out of=$OUTPUT count=1 >& /dev/null
else
    # If it's not a block device, assume it's an image file
    dd skip=1 if=$OUTPUT of=bootsect.tmp >& /dev/null
    cat bootsect.out > $OUTPUT
    cat bootsect.tmp >> $OUTPUT
fi

rm -f bootsect.inp bootsect.out bootsect.tmp

echo Done

