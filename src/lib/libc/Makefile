## 
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
##  
##  Makefile
##

BUILDDIR=../../../build/system/libraries

CC		= gcc
AR		= ar

OPTS		= -nostdlib -nodefaultlibs -nostartfiles -nostdinc -O2 \
		-mcpu=pentium -fno-strength-reduce -fno-strict-aliasing \
		-pipe -Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror

INCLUDES	= -I../../include
STDDEP		= Makefile ../shared/*

CTYPEOBJS	= obj/isalnum.o \
		obj/isalpha.o \
		obj/isascii.o \
		obj/iscntrl.o \
		obj/isdigit.o \
		obj/isgraph.o \
		obj/islower.o \
		obj/isprint.o \
		obj/ispunct.o \
		obj/isspace.o \
		obj/isupper.o \
		obj/isxdigit.o \
		obj/tolower.o \
		obj/toupper.o

LOCALEOBJS	= obj/localeconv.o \
		obj/setlocale.o

MATHOBJS	= obj/acos.o \
		obj/asin.o \
		obj/atan.o \
		obj/atan2.o \
		obj/ceil.o \
		obj/cos.o \
		obj/cosh.o \
		obj/exp.o \
		obj/fabs.o \
		obj/floor.o \
		obj/fmod.o \
		obj/frexp.o \
		obj/ldexp.o \
		obj/log.o \
		obj/log10.o \
		obj/modf.o \
		obj/pow.o \
		obj/sin.o \
		obj/sinh.o \
		obj/sqrt.o \
		obj/tan.o \
		obj/tanh.o

MISCOBJS	= obj/readline.o \
		obj/getopt.o

SIGNALOBJS	= obj/raise.o \
		obj/signal.o

STDIOOBJS	= obj/_fmtinpt.o \
		obj/_xpndfmt.o \
		obj/fgetpos.o \
		obj/fseek.o \
		obj/fsetpos.o \
		obj/ftell.o \
		obj/getc.o \
		obj/getchar.o \
		obj/gets.o \
		obj/perror.o \
		obj/printf.o \
		obj/putc.o \
		obj/putchar.o \
		obj/puts.o \
		obj/remove.o \
		obj/rename.o \
		obj/rewind.o \
		obj/scanf.o \
		obj/sprintf.o

STDLIBOBJS	= obj/abort.o \
		obj/abs.o \
		obj/atexit.o \
		obj/atof.o \
		obj/atoi.o \
		obj/atol.o \
		obj/bsearch.o \
		obj/calloc.o \
		obj/div.o \
		obj/exit.o \
		obj/free.o \
		obj/getenv.o \
		obj/itoa.o \
		obj/itox.o \
		obj/labs.o \
		obj/ldiv.o \
		obj/malloc.o \
		obj/rand.o \
		obj/realloc.o \
		obj/srand.o \
		obj/system.o \
		obj/utoa.o \
		obj/xtoi.o

STRINGOBJS	= obj/bcopy.o \
		obj/bzero.o \
		obj/memcmp.o \
		obj/memcpy.o \
		obj/memmove.o \
		obj/memset.o \
		obj/strcasecmp.o \
		obj/strcat.o \
		obj/strchr.o \
		obj/strcmp.o \
		obj/strcpy.o \
		obj/strlen.o \
		obj/strncasecmp.o \
		obj/strncat.o \
		obj/strncmp.o \
		obj/strncpy.o \
		obj/strrchr.o \
		obj/strspn.o \
		obj/strstr.o

TIMEOBJS	= obj/asctime.o \
		obj/clock.o \
		obj/ctime.o \
		obj/difftime.o \
		obj/gmtime.o \
		obj/localtime.o \
		obj/mktime.o \
		obj/time.o

STARTOBJS  	= obj/_globdata.o \
		${BUILDDIR}/crt0.o

all: target-dirs ${CTYPEOBJS} ${LOCALEOBJS} ${MATHOBJS} ${MISCOBJS} ${SIGNALOBJS} ${STDIOOBJS} ${STRINGOBJS} ${STDLIBOBJS} ${TIMEOBJS} ${STARTOBJS} ${STDDEP}
	${AR} -rs ${BUILDDIR}/libc.a  ${CTYPEOBJS} ${LOCALEOBJS} ${MATHOBJS} ${MISCOBJS} ${SIGNALOBJS} ${STDIOOBJS} ${STRINGOBJS} ${STDLIBOBJS} ${TIMEOBJS} ${STARTOBJS}

target-dirs:
	mkdir -p obj
	mkdir -p ${BUILDDIR}


# The standard C library files


# Ctype.h

obj/isalnum.o: isalnum.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isalnum.c -o $@

obj/isalpha.o: isalpha.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isalpha.c -o $@

obj/isascii.o: isascii.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isascii.c -o $@

obj/iscntrl.o: iscntrl.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c iscntrl.c -o $@

obj/isdigit.o: isdigit.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isdigit.c -o $@

obj/isgraph.o: isgraph.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isgraph.c -o $@

obj/islower.o: islower.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c islower.c -o $@

obj/isprint.o: isprint.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isprint.c -o $@

obj/ispunct.o: ispunct.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ispunct.c -o $@

obj/isspace.o: isspace.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isspace.c -o $@

obj/isupper.o: isupper.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isupper.c -o $@

obj/isxdigit.o: isxdigit.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c isxdigit.c -o $@

obj/tolower.o: tolower.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c tolower.c -o $@

obj/toupper.o: toupper.c ../../include/ctype.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c toupper.c -o $@


# Locale.h

obj/localeconv.o: localeconv.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c localeconv.c -o $@

obj/setlocale.o: setlocale.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c setlocale.c -o $@


# Math.h

obj/acos.o: acos.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c acos.c -o $@

obj/asin.o: asin.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c asin.c -o $@

obj/atan.o: atan.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atan.c -o $@

obj/atan2.o: atan2.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atan2.c -o $@

obj/ceil.o: ceil.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ceil.c -o $@

obj/cos.o: cos.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c cos.c -o $@

obj/cosh.o: cosh.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c cosh.c -o $@

obj/exp.o: exp.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c exp.c -o $@

obj/fabs.o: fabs.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fabs.c -o $@

obj/floor.o: floor.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c floor.c -o $@

obj/fmod.o: fmod.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fmod.c -o $@

obj/frexp.o: frexp.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c frexp.c -o $@

obj/ldexp.o: ldexp.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ldexp.c -o $@

obj/log.o: log.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c log.c -o $@

obj/log10.o: log10.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c log10.c -o $@

obj/modf.o: modf.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c modf.c -o $@

obj/pow.o: pow.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c pow.c -o $@

obj/sin.o: sin.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sin.c -o $@

obj/sinh.o: sinh.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sinh.c -o $@

obj/sqrt.o: sqrt.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sqrt.c -o $@

obj/tan.o: tan.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c tan.c -o $@

obj/tanh.o: tanh.c ../../include/math.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c tanh.c -o $@


# Misc

obj/readline.o: readline.c ../../include/readline.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c readline.c -o $@

obj/getopt.o: getopt.c ../../include/unistd.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c getopt.c -o $@


# Signal.h

obj/raise.o: raise.c ../../include/signal.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c raise.c -o $@

obj/signal.o: signal.c ../../include/signal.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c signal.c -o $@


# Stdio.h

obj/_fmtinpt.o : _fmtinpt.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c _fmtinpt.c -o $@

obj/_xpndfmt.o : _xpndfmt.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c _xpndfmt.c -o $@

obj/fgetpos.o: fgetpos.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fgetpos.c -o $@

obj/fseek.o: fseek.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fseek.c -o $@

obj/fsetpos.o: fsetpos.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c fsetpos.c -o $@

obj/ftell.o: ftell.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ftell.c -o $@

obj/getc.o: getc.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c getc.c -o $@

obj/getchar.o: getchar.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c getchar.c -o $@

obj/gets.o: gets.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c gets.c -o $@

obj/perror.o: perror.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c perror.c -o $@

obj/printf.o: printf.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c printf.c -o $@

obj/putc.o: putc.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c putc.c -o $@

obj/putchar.o: putchar.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c putchar.c -o $@

obj/puts.o: puts.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c puts.c -o $@

obj/remove.o: remove.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c remove.c -o $@

obj/rename.o: rename.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c rename.c -o $@

obj/rewind.o: rewind.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c rewind.c -o $@

obj/scanf.o: scanf.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c scanf.c -o $@

obj/sprintf.o: sprintf.c ../../include/stdio.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c sprintf.c -o $@


# Stdlib.h

obj/abort.o: abort.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c abort.c -o $@

obj/abs.o: abs.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c abs.c -o $@

obj/atexit.o: atexit.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atexit.c -o $@

obj/atof.o: atof.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atof.c -o $@

obj/atoi.o: atoi.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atoi.c -o $@

obj/atol.o: atol.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c atol.c -o $@

obj/bsearch.o: bsearch.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c bsearch.c -o $@

obj/calloc.o: calloc.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c calloc.c -o $@

obj/div.o: div.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c div.c -o $@

obj/exit.o: exit.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c exit.c -o $@

obj/free.o: free.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c free.c -o $@

obj/getenv.o: getenv.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c getenv.c -o $@

obj/itoa.o: itoa.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c itoa.c -o $@

obj/itox.o: itox.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c itox.c -o $@

obj/labs.o: labs.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c labs.c -o $@

obj/ldiv.o: ldiv.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ldiv.c -o $@

obj/malloc.o: malloc.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c malloc.c -o $@

obj/rand.o: rand.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c rand.c -o $@

obj/realloc.o: realloc.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c realloc.c -o $@

obj/srand.o: srand.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c srand.c -o $@

obj/system.o: system.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c system.c -o $@

obj/utoa.o: utoa.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c utoa.c -o $@

obj/xtoi.o: xtoi.c ../../include/stdlib.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c xtoi.c -o $@


# String.h

obj/bcopy.o: bcopy.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c bcopy.c -o $@

obj/bzero.o: bzero.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c bzero.c -o $@

obj/memcmp.o: memcmp.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c memcmp.c -o $@

obj/memcpy.o: memcpy.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c memcpy.c -o $@

obj/memmove.o: memmove.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c memmove.c -o $@

obj/memset.o: memset.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c memset.c -o $@

obj/strcasecmp.o: strcasecmp.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcasecmp.c -o $@

obj/strcat.o: strcat.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcat.c -o $@

obj/strchr.o: strchr.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strchr.c -o $@

obj/strcmp.o: strcmp.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcmp.c -o $@

obj/strcpy.o: strcpy.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strcpy.c -o $@

obj/strlen.o: strlen.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strlen.c -o $@

obj/strncasecmp.o: strncasecmp.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strncasecmp.c -o $@

obj/strncat.o: strncat.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strncat.c -o $@

obj/strncmp.o: strncmp.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strncmp.c -o $@

obj/strncpy.o: strncpy.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strncpy.c -o $@

obj/strrchr.o: strrchr.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strrchr.c -o $@

obj/strspn.o: strspn.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strspn.c -o $@

obj/strstr.o: strstr.c ../../include/string.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c strstr.c -o $@


# Time.h

obj/asctime.o: asctime.c ../../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c asctime.c -o $@

obj/clock.o: clock.c ../../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c clock.c -o $@

obj/ctime.o: ctime.c ../../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c ctime.c -o $@

obj/difftime.o: difftime.c ../../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c difftime.c -o $@

obj/gmtime.o: gmtime.c ../../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c gmtime.c -o $@

obj/localtime.o: localtime.c ../../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c localtime.c -o $@

obj/mktime.o: mktime.c ../../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c mktime.c -o $@

obj/time.o: time.c ../../include/time.h ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c time.c -o $@


# Startfiles

obj/_globdata.o: _globdata.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c _globdata.c -o $@

${BUILDDIR}/crt0.o: crt0.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c crt0.c -o $@


# Miscellany

clean:
	rm -Rf *~ *.o core obj ${BUILDDIR}/crt0.o ${BUILDDIR}/libc.a
