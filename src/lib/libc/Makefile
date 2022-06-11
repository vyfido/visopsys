## 
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
##  
##  Makefile
##

BUILDDIR=../../../build/system/libraries

CC		= gcc
AR		= ar
LD		= ld
CPU		= $(shell ../../../utils/cpu.sh)
STDDEPS		= Makefile ../shared/*
CODEGEN		= -O2 ${CPU} -fno-strength-reduce -fno-strict-aliasing
WARN		= -Wall -W -Wshadow -Wmissing-prototypes -Wstrict-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror
INCLUDE         = -nostdinc -I../../include
CFLAGS		= -pipe ${CODEGEN} ${WARN} ${INCLUDE}
LFLAGS		= -nostdlib -nodefaultlibs -nostartfiles
OPTS		= ${CFLAGS} ${LFLAGS}

CTYPENAMES	= isalnum \
		isalpha \
		isascii \
		iscntrl \
		isdigit \
		isgraph \
		islower \
		isprint \
		ispunct \
		isspace \
		isupper \
		isxdigit \
		tolower \
		toupper

FCNTLNAMES	= open

LOCALENAMES	= setlocale

MATHNAMES	= ceil \
		cos \
		cosf \
		fabs \
		floor \
		floorf \
		fmod \
		modf \
		pow \
		sin \
		sinf \
		sqrt \
		tan \
		tanf

MISCNAMES	= readline

SIGNALNAMES	= signal

STDIONAMES	= _fmtinpt \
		_xpndfmt \
		fgetpos \
		fprintf \
		fread \
		fseek \
		fsetpos \
		ftell \
		fwrite \
		getc \
		getchar \
		gets \
		perror \
		printf \
		putc \
		putchar \
		puts \
		remove \
		rename \
		rewind \
		scanf \
		snprintf \
		sprintf

STDLIBNAMES	= abort \
		abs \
		atoi \
		calloc \
		div \
		exit \
		free \
		itoa \
		itob \
		itox \
		labs \
		ldiv \
		malloc \
		rand \
		srand \
		system \
		utoa \
		xtoi

STRINGNAMES	= bcopy \
		bzero \
		memcmp \
		memcpy \
		memmove \
		memset \
		strcasecmp \
		strcat \
		strchr \
		strcmp \
		strcpy \
		strlen \
		strncasecmp \
		strncat \
		strncmp \
		strncpy \
		strrchr \
		strspn \
		strstr

TIMENAMES	= asctime \
		clock \
		difftime \
		time

UNISTDNAMES	= close \
		getopt \
		lseek \
		read \
		stat \
		swab \
		write

ALLNAMES = ${CTYPENAMES} ${FCNTLNAMES} ${LOCALENAMES} ${MATHNAMES} ${MISCNAMES} ${SIGNALNAMES} ${STDIONAMES} ${STDLIBNAMES} ${STRINGNAMES} ${TIMENAMES} ${UNISTDNAMES}
OBJDIR = obj
PICOBJDIR = picobj
OBJS = $(addprefix ${OBJDIR}/, $(addsuffix .o, ${ALLNAMES}))
PICOBJS = $(addprefix ${PICOBJDIR}/, $(addsuffix .o, ${ALLNAMES}))
CRT0 = ${BUILDDIR}/crt0.o
LIBRARY = ${BUILDDIR}/libc.a
SONAME = libc.so
SHAREDLIB = ${BUILDDIR}/${SONAME}

# Targets

all: target-dirs ${CRT0} ${LIBRARY} ${SHAREDLIB}

target-dirs:
	mkdir -p ${OBJDIR} ${PICOBJDIR} ${BUILDDIR}

${CRT0}: crt0.c ${STDDEPS}
	${CC} ${OPTS} -c $< -o $@

${LIBRARY}: ${OBJS}
	${AR} -rs $@ ${OBJS}

${SHAREDLIB}: ${PICOBJS}
	${LD} -shared -soname=${SONAME} ${PICOBJS} -o $@

${OBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} -c $< -o $@

${PICOBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} -fpic -c $< -o $@

clean:
	rm -Rf *~ *.o core ${OBJDIR} ${PICOBJDIR} ${CRT0} ${LIBRARY} ${SHAREDLIB}
