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
CODEGEN		= -O2 ${CPU} -fno-strength-reduce -fno-strict-aliasing \
		-ffreestanding
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
		fabs \
		floor \
		fmod \
		modf \
		pow \
		sqrt

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
		sprintf

STDLIBNAMES	= abort \
		abs \
		atoi \
		calloc \
		div \
		exit \
		free \
		itoa \
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
OBJS = $(addprefix ${OBJDIR}/, $(addsuffix .o, ${ALLNAMES}))
CRT0 = ${BUILDDIR}/crt0.o
LIBRARY = ${BUILDDIR}/libc.a

# Targets

all: target-dirs ${CRT0} ${LIBRARY}

target-dirs:
	mkdir -p ${OBJDIR} ${BUILDDIR}

${CRT0}: crt0.c ${STDDEPS}
	${CC} ${OPTS} -c $< -o $@

${LIBRARY}: ${OBJS}
	${AR} -rs $@ ${OBJS}

${OBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -Rf *~ *.o core ${OBJDIR} ${CRT0} ${LIBRARY}
