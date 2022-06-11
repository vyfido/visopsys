## 
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
##  
##  This library is free software; you can redistribute it and/or modify it
##  under the terms of the GNU Lesser General Public License as published by
##  the Free Software Foundation; either version 2.1 of the License, or (at
##  your option) any later version.
##
##  This library is distributed in the hope that it will be useful, but
##  WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
##  General Public License for more details.
##
##  You should have received a copy of the GNU Lesser General Public License
##  along with this library; if not, write to the Free Software Foundation,
##  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
##
##  Makefile
##

# This stuff is compile-only.  It is not linked except into a single
# cohesive object file.


CC		= gcc
AR		= ar

OPTS		= -nostdlib -nodefaultlibs -nostartfiles -nostdinc -O2 \
		-mcpu=pentium -fno-strength-reduce -pipe -Wall -Werror

INCLUDES	= -I../include
STDDEP		= Makefile shared/*

CTYPEOBJS	= build/isalnum.o \
		build/isalpha.o \
		build/isascii.o \
		build/iscntrl.o \
		build/isdigit.o \
		build/isgraph.o \
		build/islower.o \
		build/isprint.o \
		build/ispunct.o \
		build/isspace.o \
		build/isupper.o \
		build/isxdigit.o \
		build/tolower.o \
		build/toupper.o

LOCALEOBJS	= build/localeconv.o \
		build/setlocale.o

MATHOBJS	= build/acos.o \
		build/asin.o \
		build/atan.o \
		build/atan2.o \
		build/ceil.o \
		build/cos.o \
		build/cosh.o \
		build/exp.o \
		build/fabs.o \
		build/floor.o \
		build/fmod.o \
		build/frexp.o \
		build/ldexp.o \
		build/log.o \
		build/log10.o \
		build/modf.o \
		build/pow.o \
		build/sin.o \
		build/sinh.o \
		build/sqrt.o \
		build/tan.o \
		build/tanh.o

MISCOBJS	= build/readline.o

SIGNALOBJS	= build/raise.o \
		build/signal.o

STDIOOBJS	= build/_fmtinpt.o \
		build/_xpndfmt.o \
		build/fgetpos.o \
		build/fseek.o \
		build/fsetpos.o \
		build/ftell.o \
		build/getc.o \
		build/getchar.o \
		build/gets.o \
		build/perror.o \
		build/printf.o \
		build/putc.o \
		build/putchar.o \
		build/puts.o \
		build/remove.o \
		build/rename.o \
		build/rewind.o \
		build/scanf.o \
		build/sprintf.o

STDLIBOBJS	= build/abort.o \
		build/abs.o \
		build/atexit.o \
		build/atof.o \
		build/atoi.o \
		build/atol.o \
		build/bsearch.o \
		build/calloc.o \
		build/div.o \
		build/exit.o \
		build/free.o \
		build/getenv.o \
		build/itoa.o \
		build/itox.o \
		build/labs.o \
		build/ldiv.o \
		build/malloc.o \
		build/rand.o \
		build/realloc.o \
		build/srand.o \
		build/system.o \
		build/utoa.o \
		build/xtoi.o

STRINGOBJS	= build/memcmp.o \
		build/memcpy.o \
		build/memmove.o \
		build/strcasecmp.o \
		build/strcat.o \
		build/strcmp.o \
		build/strcpy.o \
		build/strlen.o \
		build/strncat.o \
		build/strncmp.o \
		build/strncpy.o \
		build/strspn.o \
		build/strstr.o

TIMEOBJS	= build/asctime.o \
		build/clock.o \
		build/ctime.o \
		build/difftime.o \
		build/gmtime.o \
		build/localtime.o \
		build/mktime.o \
		build/time.o

STARTOBJS  	= build/_globdata.o \
		build/crt0.o

# Visopsys libraries
VISOBJS		= build/libvsh.o \
		build/libwindow.o


all: target-dir libc libvsh libwindow

libc: ${CTYPEOBJS} ${LOCALEOBJS} ${MATHOBJS} ${MISCOBJS} ${SIGNALOBJS} ${STDIOOBJS} ${STRINGOBJS} ${STDLIBOBJS} ${TIMEOBJS} ${STARTOBJS} ${STDDEP}
	${AR} -rs build/libc.a  ${CTYPEOBJS} ${LOCALEOBJS} ${MATHOBJS} ${SIGNALOBJS} ${STDIOOBJS} ${STRINGOBJS} ${STDLIBOBJS} ${TIMEOBJS} ${STARTOBJS}

libvsh: build/libvsh.o ${STDDEP}
	${AR} -rs build/libvsh.a build/libvsh.o

libwindow: build/libwindow.o ${STDDEP}
	${AR} -rs build/libwindow.a build/libwindow.o

target-dir:
	#
	#  Making libraries
	#
	mkdir -p build


# The standard C library files


# Ctype.h

build/isalnum.o: isalnum.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isalnum.c -o $@

build/isalpha.o: isalpha.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isalpha.c -o $@

build/isascii.o: isascii.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isascii.c -o $@

build/iscntrl.o: iscntrl.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c iscntrl.c -o $@

build/isdigit.o: isdigit.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isdigit.c -o $@

build/isgraph.o: isgraph.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isgraph.c -o $@

build/islower.o: islower.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c islower.c -o $@

build/isprint.o: isprint.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isprint.c -o $@

build/ispunct.o: ispunct.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ispunct.c -o $@

build/isspace.o: isspace.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isspace.c -o $@

build/isupper.o: isupper.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isupper.c -o $@

build/isxdigit.o: isxdigit.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isxdigit.c -o $@

build/tolower.o: tolower.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c tolower.c -o $@

build/toupper.o: toupper.c ../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c toupper.c -o $@


# Locale.h

build/localeconv.o: localeconv.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c localeconv.c -o $@

build/setlocale.o: setlocale.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c setlocale.c -o $@


# Math.h

build/acos.o: acos.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c acos.c -o $@

build/asin.o: asin.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c asin.c -o $@

build/atan.o: atan.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atan.c -o $@

build/atan2.o: atan2.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atan2.c -o $@

build/ceil.o: ceil.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ceil.c -o $@

build/cos.o: cos.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c cos.c -o $@

build/cosh.o: cosh.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c cosh.c -o $@

build/exp.o: exp.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c exp.c -o $@

build/fabs.o: fabs.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fabs.c -o $@

build/floor.o: floor.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c floor.c -o $@

build/fmod.o: fmod.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fmod.c -o $@

build/frexp.o: frexp.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c frexp.c -o $@

build/ldexp.o: ldexp.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ldexp.c -o $@

build/log.o: log.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c log.c -o $@

build/log10.o: log10.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c log10.c -o $@

build/modf.o: modf.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c modf.c -o $@

build/pow.o: pow.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c pow.c -o $@

build/sin.o: sin.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sin.c -o $@

build/sinh.o: sinh.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sinh.c -o $@

build/sqrt.o: sqrt.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sqrt.c -o $@

build/tan.o: tan.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c tan.c -o $@

build/tanh.o: tanh.c ../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c tanh.c -o $@


# Misc

build/readline.o: readline.c ../include/readline.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c readline.c -o $@


# Signal.h

build/raise.o: raise.c ../include/signal.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c raise.c -o $@

build/signal.o: signal.c ../include/signal.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c signal.c -o $@


# Stdio.h

build/_fmtinpt.o : _fmtinpt.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c _fmtinpt.c -o $@

build/_xpndfmt.o : _xpndfmt.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c _xpndfmt.c -o $@

build/fgetpos.o: fgetpos.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fgetpos.c -o $@

build/fseek.o: fseek.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fseek.c -o $@

build/fsetpos.o: fsetpos.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fsetpos.c -o $@

build/ftell.o: ftell.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ftell.c -o $@

build/getc.o: getc.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c getc.c -o $@

build/getchar.o: getchar.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c getchar.c -o $@

build/gets.o: gets.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c gets.c -o $@

build/perror.o: perror.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c perror.c -o $@

build/printf.o: printf.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c printf.c -o $@

build/putc.o: putc.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c putc.c -o $@

build/putchar.o: putchar.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c putchar.c -o $@

build/puts.o: puts.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c puts.c -o $@

build/remove.o: remove.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c remove.c -o $@

build/rename.o: rename.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c rename.c -o $@

build/rewind.o: rewind.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c rewind.c -o $@

build/scanf.o: scanf.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c scanf.c -o $@

build/sprintf.o: sprintf.c ../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sprintf.c -o $@


# Stdlib.h

build/abort.o: abort.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c abort.c -o $@

build/abs.o: abs.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c abs.c -o $@

build/atexit.o: atexit.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atexit.c -o $@

build/atof.o: atof.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atof.c -o $@

build/atoi.o: atoi.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atoi.c -o $@

build/atol.o: atol.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atol.c -o $@

build/bsearch.o: bsearch.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c bsearch.c -o $@

build/calloc.o: calloc.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c calloc.c -o $@

build/div.o: div.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c div.c -o $@

build/exit.o: exit.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c exit.c -o $@

build/free.o: free.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c free.c -o $@

build/getenv.o: getenv.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c getenv.c -o $@

build/itoa.o: itoa.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c itoa.c -o $@

build/itox.o: itox.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c itox.c -o $@

build/labs.o: labs.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c labs.c -o $@

build/ldiv.o: ldiv.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ldiv.c -o $@

build/malloc.o: malloc.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c malloc.c -o $@

build/rand.o: rand.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c rand.c -o $@

build/realloc.o: realloc.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c realloc.c -o $@

build/srand.o: srand.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c srand.c -o $@

build/system.o: system.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c system.c -o $@

build/utoa.o: utoa.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c utoa.c -o $@

build/xtoi.o: xtoi.c ../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c xtoi.c -o $@


# String.h

build/memcmp.o: memcmp.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c memcmp.c -o $@

build/memcpy.o: memcpy.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c memcpy.c -o $@

build/memmove.o: memmove.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c memmove.c -o $@

build/strcasecmp.o: strcasecmp.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcasecmp.c -o $@

build/strcat.o: strcat.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcat.c -o $@

build/strcmp.o: strcmp.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcmp.c -o $@

build/strcpy.o: strcpy.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcpy.c -o $@

build/strlen.o: strlen.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strlen.c -o $@

build/strncat.o: strncat.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strncat.c -o $@

build/strncmp.o: strncmp.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strncmp.c -o $@

build/strncpy.o: strncpy.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strncpy.c -o $@

build/strspn.o: strspn.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strspn.c -o $@

build/strstr.o: strstr.c ../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strstr.c -o $@


# Time.h

build/asctime.o: asctime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c asctime.c -o $@

build/clock.o: clock.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c clock.c -o $@

build/ctime.o: ctime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ctime.c -o $@

build/difftime.o: difftime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c difftime.c -o $@

build/gmtime.o: gmtime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c gmtime.c -o $@

build/localtime.o: localtime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c localtime.c -o $@

build/mktime.o: mktime.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c mktime.c -o $@

build/time.o: time.c ../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c time.c -o $@


# Startfiles

build/_globdata.o: _globdata.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c _globdata.c -o $@

build/crt0.o: crt0.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c crt0.c -o $@


# The Visopsys library files

build/libvsh.o: libvsh.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c libvsh.c -o $@

build/libwindow.o: libwindow.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c libwindow.c -o $@


# Miscellany

clean:
	rm -Rf *~ *.o core libc.a build
