##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

ROOT = ../../..
BUILDDIR = build
OUTPUTDIR = ${ROOT}/${BUILDDIR}/system/libraries
INCDIR = ../../include

include ${ROOT}/Makefile.include

ifdef DEBUG
	override DEBUG = -DDEBUG
else
	STRIP = strip -s ${SHAREDLIB}
endif

STDDEPS = ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h ${INCDIR}/sys/*.h
INCLUDE = -nostdinc -I${INCDIR}
CFLAGS = ${OPT} ${ARCHFLAGS} ${CCODEGEN} ${CWARN} ${INCLUDE} ${DEBUG}

CDEFNAMES = \
	_dbl2str \
	_digits \
	_fdesc \
	_flt2str \
	_fmtinpt \
	_kernapi \
	_ldigits \
	_lnum2str \
	_num2str \
	_str2num \
	_xpndfmt

CTYPENAMES = \
	isalnum \
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

DIRENTNAMES = \
	closedir \
	opendir \
	readdir \
	readdir_r \
	rewinddir

FCNTLNAMES = open

LIBGENNAMES = \
	basename \
	dirname

LOCALENAMES = setlocale

MATHNAMES = \
	ceil \
	cos \
	cosf \
	fabs \
	fabsf \
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

NETNAMES = \
	accept \
	bind \
	connect \
	freeaddrinfo \
	gai_strerror \
	getaddrinfo \
	getprotobyname \
	getprotobynumber \
	getservbyname \
	getservbyport \
	inet_ntop \
	inet_pton \
	listen \
	poll \
	recv \
	send \
	shutdown \
	socket

SIGNALNAMES = signal

STATNAMES = \
	mkdir \
	stat

STDIONAMES = \
	fclose \
	fdopen \
	feof \
	fflush \
	fgetpos \
	fgets \
	fopen \
	fprintf \
	fread \
	fscanf \
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
	sprintf \
	sscanf \
	strtol \
	strtoll \
	strtoul \
	strtoull \
	tmpfile \
	vfprintf \
	vfscanf \
	vprintf \
	vscanf \
	vsnprintf \
	vsprintf \
	vsscanf

STDLIBNAMES = \
	abort \
	abs \
	calloc \
	exit \
	getenv \
	labs \
	malloc \
	mblen \
	mbstowcs \
	mbtowc \
	mkdtemp \
	mkstemp \
	rand \
	realloc \
	realpath \
	setenv \
	srand \
	system \
	wcstombs \
	wctomb

STRINGNAMES = \
	bcopy \
	bzero \
	ffs \
	fls \
	mbslen \
	memcmp \
	memcpy \
	memmove \
	memset \
	strcasecmp \
	strcasestr \
	strcat \
	strchr \
	strcmp \
	strcpy \
	strdup \
	strerror \
	strlen \
	strncasecmp \
	strncat \
	strncmp \
	strncpy \
	strnlen \
	strrchr \
	strspn \
	strstr \
	strtok \
	strtok_r

TIMENAMES = \
	asctime \
	clock \
	ctime \
	difftime \
	gmtime \
	mktime \
	time

UNISTDNAMES = \
	chdir \
	close \
	ftruncate \
	getcwd \
	getopt \
	lseek \
	read \
	rmdir \
	sleep \
	swab \
	truncate \
	unlink \
	write

WCHARNAMES = \
	getwchar \
	putwchar \
	wcscmp \
	wcscpy \
	wcslen

MISCNAMES = \
	_data \
	readline \
	sched_yield \
	uname

ALLNAMES = ${CDEFNAMES} ${CTYPENAMES} ${DIRENTNAMES} ${FCNTLNAMES} \
	${LIBGENNAMES} ${LOCALENAMES} ${MATHNAMES} ${NETNAMES} ${SIGNALNAMES} \
	${STATNAMES} ${STDIONAMES} ${STDLIBNAMES} ${STRINGNAMES} ${TIMENAMES} \
	${UNISTDNAMES} ${WCHARNAMES} ${MISCNAMES}

OBJDIR = obj
PICOBJDIR = picobj
OBJS = $(addprefix ${OBJDIR}/, $(addsuffix .o, ${ALLNAMES}))
PICOBJS = $(addprefix ${PICOBJDIR}/, $(addsuffix .o, ${ALLNAMES}))
CRT0 = ${OUTPUTDIR}/crt0.o
LIBRARY = ${OUTPUTDIR}/libc.a
SONAME = libc.so
SHAREDLIB = ${OUTPUTDIR}/${SONAME}

# Targets

all: target-dirs ${CRT0} ${LIBRARY} ${SHAREDLIB} strip

target-dirs:
	mkdir -p ${OBJDIR} ${PICOBJDIR} ${OUTPUTDIR}

${CRT0}: crt0.c ${STDDEPS}
	${CC} ${CFLAGS} -c $< -o $@

${LIBRARY}: ${OBJS}
	${AR} -rs $@ ${OBJS}

${SHAREDLIB}: ${PICOBJS}
	${CC} ${ARCHFLAGS} -nostdlib -Wl,-shared -Wl,--soname=${SONAME} \
		-Wl,-z,norelro ${PICOBJS} -static -lgcc -o $@

strip: ${SHAREDLIB}
	${STRIP}

${OBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} -c $< -o $@

${PICOBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} -fpic -c $< -o $@

clean:
	rm -Rf *~ *.o core ${OBJDIR} ${PICOBJDIR} ${CRT0} ${LIBRARY} ${SHAREDLIB}

