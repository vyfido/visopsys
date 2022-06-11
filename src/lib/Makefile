##
##  Visopsys
##  Copyright (C) 1998-2001 J. Andrew McLaughlin
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

# This stuff is compile-only.  It is not linked except into a single
# cohesive object file.


CC		= gcc
AS              = nasm
AR		= ar
PLATFORM	= $(shell ../platform.sh)
BUILD		= ${PLATFORM}

OPTS		= -nodefaultlibs -nostartfiles -nostdinc -O2 -m486 -Wall \
		-malign-functions=2 -fno-strength-reduce -malign-loops=2 \
		-malign-jumps=2 -Werror

ASMOPTS         = -f elf -w+orphan-labels
INCLUDES	= -I../include
STDDEP		= Makefile

CTYPEOBJS	= ${BUILD}/isalnum.o \
		${BUILD}/isalpha.o \
		${BUILD}/isascii.o \
		${BUILD}/iscntrl.o \
		${BUILD}/isdigit.o \
		${BUILD}/isgraph.o \
		${BUILD}/islower.o \
		${BUILD}/isprint.o \
		${BUILD}/ispunct.o \
		${BUILD}/isspace.o \
		${BUILD}/isupper.o \
		${BUILD}/isxdigit.o \
		${BUILD}/tolower.o \
		${BUILD}/toupper.o

LOCALEOBJS	= ${BUILD}/localeconv.o \
		${BUILD}/setlocale.o

MATHOBJS	= ${BUILD}/acos.o \
		${BUILD}/asin.o \
		${BUILD}/atan.o \
		${BUILD}/atan2.o \
		${BUILD}/ceil.o \
		${BUILD}/cos.o \
		${BUILD}/cosh.o \
		${BUILD}/exp.o \
		${BUILD}/fabs.o \
		${BUILD}/floor.o \
		${BUILD}/fmod.o \
		${BUILD}/frexp.o \
		${BUILD}/ldexp.o \
		${BUILD}/log.o \
		${BUILD}/log10.o \
		${BUILD}/modf.o \
		${BUILD}/pow.o \
		${BUILD}/sin.o \
		${BUILD}/sinh.o \
		${BUILD}/sqrt.o \
		${BUILD}/tan.o \
		${BUILD}/tanh.o

SIGNALOBJS	= ${BUILD}/raise.o \
		${BUILD}/signal.o

STDIOOBJS	= ${BUILD}/_xpndfmt.o \
		${BUILD}/fgetpos.o \
		${BUILD}/fseek.o \
		${BUILD}/fsetpos.o \
		${BUILD}/ftell.o \
		${BUILD}/getc.o \
		${BUILD}/getchar.o \
		${BUILD}/gets.o \
		${BUILD}/perror.o \
		${BUILD}/printf.o \
		${BUILD}/putc.o \
		${BUILD}/putchar.o \
		${BUILD}/puts.o \
		${BUILD}/remove.o \
		${BUILD}/rename.o \
		${BUILD}/rewind.o \
		${BUILD}/sprintf.o

STDLIBOBJS	= ${BUILD}/abs.o \
		${BUILD}/atoi.o \
		${BUILD}/calloc.o \
		${BUILD}/div.o \
		${BUILD}/exit.o \
		${BUILD}/free.o \
		${BUILD}/itoa.o \
		${BUILD}/labs.o \
		${BUILD}/ldiv.o \
		${BUILD}/malloc.o \
		${BUILD}/rand.o \
		${BUILD}/realloc.o \
		${BUILD}/srand.o \
		${BUILD}/utoa.o

STRINGOBJS	= ${BUILD}/memcpy.o \
		${BUILD}/memmove.o \
		${BUILD}/strcasecmp.o \
		${BUILD}/strcat.o \
		${BUILD}/strcmp.o \
		${BUILD}/strncmp.o \
		${BUILD}/strcpy.o \
		${BUILD}/strlen.o \
		${BUILD}/strncat.o \
		${BUILD}/strncpy.o \
		${BUILD}/strstr.o

TIMEOBJS	= ${BUILD}/asctime.o \
		${BUILD}/clock.o \
		${BUILD}/ctime.o \
		${BUILD}/difftime.o \
		${BUILD}/gmtime.o \
		${BUILD}/localtime.o \
		${BUILD}/mktime.o \
		${BUILD}/time.o

STARTOBJS  	= ${BUILD}/_globdata.o \
		${BUILD}/start.o


all: target-dir libc

libc: ${CTYPEOBJS} ${LOCALEOBJS} ${MATHOBJS} ${SIGNALOBJS} ${STDIOOBJS} ${STRINGOBJS} ${STDLIBOBJS} ${TIMEOBJS} ${STARTOBJS} ${STDDEP}
	${AR} -rs ${PLATFORM}/libc.a  ${CTYPEOBJS} ${LOCALEOBJS} ${MATHOBJS} ${SIGNALOBJS} ${STDIOOBJS} ${STRINGOBJS} ${STDLIBOBJS} ${TIMEOBJS} ${STARTOBJS}

target-dir:
	#
	#  Making libraries on ${PLATFORM}
	#
	mkdir -p ${BUILD}


# The standard C library files


# Ctype.h

${BUILD}/isalnum.o: isalnum.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isalnum.c -o $@

${BUILD}/isalpha.o: isalpha.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isalpha.c -o $@

${BUILD}/isascii.o: isascii.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isascii.c -o $@

${BUILD}/iscntrl.o: iscntrl.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c iscntrl.c -o $@

${BUILD}/isdigit.o: isdigit.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isdigit.c -o $@

${BUILD}/isgraph.o: isgraph.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isgraph.c -o $@

${BUILD}/islower.o: islower.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c islower.c -o $@

${BUILD}/isprint.o: isprint.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isprint.c -o $@

${BUILD}/ispunct.o: ispunct.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ispunct.c -o $@

${BUILD}/isspace.o: isspace.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isspace.c -o $@

${BUILD}/isupper.o: isupper.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isupper.c -o $@

${BUILD}/isxdigit.o: isxdigit.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isxdigit.c -o $@

${BUILD}/tolower.o: tolower.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c tolower.c -o $@

${BUILD}/toupper.o: toupper.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c toupper.c -o $@


# Locale.h

${BUILD}/localeconv.o: localeconv.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c localeconv.c -o $@

${BUILD}/setlocale.o: setlocale.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c setlocale.c -o $@


# Math.h

${BUILD}/acos.o: acos.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c acos.c -o $@

${BUILD}/asin.o: asin.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c asin.c -o $@

${BUILD}/atan.o: atan.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atan.c -o $@

${BUILD}/atan2.o: atan2.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atan2.c -o $@

${BUILD}/ceil.o: ceil.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ceil.c -o $@

${BUILD}/cos.o: cos.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c cos.c -o $@

${BUILD}/cosh.o: cosh.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c cosh.c -o $@

${BUILD}/exp.o: exp.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c exp.c -o $@

${BUILD}/fabs.o: fabs.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fabs.c -o $@

${BUILD}/floor.o: floor.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c floor.c -o $@

${BUILD}/fmod.o: fmod.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fmod.c -o $@

${BUILD}/frexp.o: frexp.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c frexp.c -o $@

${BUILD}/ldexp.o: ldexp.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ldexp.c -o $@

${BUILD}/log.o: log.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c log.c -o $@

${BUILD}/log10.o: log10.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c log10.c -o $@

${BUILD}/modf.o: modf.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c modf.c -o $@

${BUILD}/pow.o: pow.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c pow.c -o $@

${BUILD}/sin.o: sin.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sin.c -o $@

${BUILD}/sinh.o: sinh.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sinh.c -o $@

${BUILD}/sqrt.o: sqrt.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sqrt.c -o $@

${BUILD}/tan.o: tan.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c tan.c -o $@

${BUILD}/tanh.o: tanh.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c tanh.c -o $@


# Signal.h

${BUILD}/raise.o: raise.c ../include/signal.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c raise.c -o $@

${BUILD}/signal.o: signal.c ../include/signal.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c signal.c -o $@


# Stdio.h

${BUILD}/_xpndfmt.o : _xpndfmt.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c _xpndfmt.c -o $@

${BUILD}/fgetpos.o: fgetpos.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fgetpos.c -o $@

${BUILD}/fseek.o: fseek.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fseek.c -o $@

${BUILD}/fsetpos.o: fsetpos.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fsetpos.c -o $@

${BUILD}/ftell.o: ftell.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ftell.c -o $@

${BUILD}/getc.o: getc.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c getc.c -o $@

${BUILD}/getchar.o: getchar.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c getchar.c -o $@

${BUILD}/gets.o: gets.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c gets.c -o $@

${BUILD}/perror.o: perror.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c perror.c -o $@

${BUILD}/printf.o: printf.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c printf.c -o $@

${BUILD}/putc.o: putc.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c putc.c -o $@

${BUILD}/putchar.o: putchar.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c putchar.c -o $@

${BUILD}/puts.o: puts.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c puts.c -o $@

${BUILD}/remove.o: remove.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c remove.c -o $@

${BUILD}/rename.o: rename.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c rename.c -o $@

${BUILD}/rewind.o: rewind.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c rewind.c -o $@

${BUILD}/sprintf.o: sprintf.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sprintf.c -o $@


# Stdlib.h

${BUILD}/abs.o: abs.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c abs.c -o $@

${BUILD}/atoi.o: atoi.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atoi.c -o $@

${BUILD}/calloc.o: calloc.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c calloc.c -o $@

${BUILD}/div.o: div.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c div.c -o $@

${BUILD}/exit.o: exit.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c exit.c -o $@

${BUILD}/free.o: free.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c free.c -o $@

${BUILD}/itoa.o: itoa.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c itoa.c -o $@

${BUILD}/labs.o: labs.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c labs.c -o $@

${BUILD}/ldiv.o: ldiv.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ldiv.c -o $@

${BUILD}/malloc.o: malloc.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c malloc.c -o $@

${BUILD}/rand.o: rand.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c rand.c -o $@

${BUILD}/realloc.o: realloc.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c realloc.c -o $@

${BUILD}/srand.o: srand.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c srand.c -o $@

${BUILD}/utoa.o: utoa.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c utoa.c -o $@


# String.h

${BUILD}/memcpy.o: memcpy.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c memcpy.c -o $@

${BUILD}/memmove.o: memmove.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c memmove.c -o $@

${BUILD}/strcasecmp.o: strcasecmp.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcasecmp.c -o $@

${BUILD}/strcat.o: strcat.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcat.c -o $@

${BUILD}/strcmp.o: strcmp.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcmp.c -o $@

${BUILD}/strncmp.o: strncmp.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strncmp.c -o $@

${BUILD}/strcpy.o: strcpy.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcpy.c -o $@

${BUILD}/strlen.o: strlen.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strlen.c -o $@

${BUILD}/strncat.o: strncat.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strncat.c -o $@

${BUILD}/strncpy.o: strncpy.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strncpy.c -o $@

${BUILD}/strstr.o: strstr.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strstr.c -o $@


# Time.h

${BUILD}/asctime.o: asctime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c asctime.c -o $@

${BUILD}/clock.o: clock.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c clock.c -o $@

${BUILD}/ctime.o: ctime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ctime.c -o $@

${BUILD}/difftime.o: difftime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c difftime.c -o $@

${BUILD}/gmtime.o: gmtime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c gmtime.c -o $@

${BUILD}/localtime.o: localtime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c localtime.c -o $@

${BUILD}/mktime.o: mktime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c mktime.c -o $@

${BUILD}/time.o: time.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c time.c -o $@


# Startfiles

${BUILD}/_globdata.o: _globdata.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c _globdata.c -o $@

${BUILD}/start.o: start.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c start.c -o $@


# Miscellany

clean:
	rm -Rf *~ *.o core libc.a ${PLATFORM}
