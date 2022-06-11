## 
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
##  
##  Makefile
##

BUILDDIR=../../../build/system/libraries

CC		= gcc
AR		= ar
CPU		= $(shell ../../../utils/cpu.sh)
STDDEPS		= Makefile ../shared/*
CODEGEN		= -O2 ${CPU} -fno-strength-reduce -fno-strict-aliasing
WARN		= -Wall -Wmissing-prototypes -Wstrict-prototypes \
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

LOCALENAMES	= localeconv \
		setlocale

MATHNAMES	= acos \
		asin \
		atan \
		atan2 \
		ceil \
		cos \
		cosh \
		exp \
		fabs \
		floor \
		fmod \
		frexp \
		ldexp \
		log \
		log10 \
		modf \
		pow \
		sin \
		sinh \
		sqrt \
		tan \
		tanh

MISCNAMES	= readline

SIGNALNAMES	= raise \
		signal

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
		atexit \
		atof \
		atoi \
		atol \
		bsearch \
		calloc \
		div \
		exit \
		free \
		getenv \
		itoa \
		itox \
		labs \
		ldiv \
		malloc \
		rand \
		realloc \
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
		ctime \
		difftime \
		gmtime \
		localtime \
		mktime \
		time

UNISTDNAMES	= close \
		getopt \
		lseek \
		read \
		stat \
		write

GLOBALNAMES  	= _globdata

ALLNAMES = ${CTYPENAMES} ${FCNTLNAMES} ${LOCALENAMES} ${MATHNAMES} ${MISCNAMES} ${STDIONAMES} ${STDLIBNAMES} ${STRINGNAMES} ${TIMENAMES} ${UNISTDNAMES} ${GLOBALNAMES}
OBJS = $(addprefix obj/, $(addsuffix .o, ${ALLNAMES}))
CRT0 = ${BUILDDIR}/crt0.o
LIBRARY = ${BUILDDIR}/libc.a

# Targets

all: target-dirs ${LIBRARY}

target-dirs:
	mkdir -p obj
	mkdir -p ${BUILDDIR}

${LIBRARY}: ${CRT0} ${OBJS} ${STARTOBJS}
	${AR} -rs $@ ${OBJS} ${STARTOBJS}

${CRT0}: crt0.c ${STDDEPS}
	${CC} ${OPTS} -c $< -o $@

obj/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -Rf *~ *.o core obj ${BUILDDIR}/crt0.o ${LIBRARY}
