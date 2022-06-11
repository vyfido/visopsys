##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

ROOT = ../..
BUILDDIR = build
OUTPUTDIR = ${ROOT}/${BUILDDIR}/programs
INCDIR = ../include
LIBDIR = ${ROOT}/${BUILDDIR}/system/libraries
UTILSDIR = ${ROOT}/utils

include ${ROOT}/Makefile.include

STDDEPS = ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h \
	${INCDIR}/sys/*.h ${INCDIR}/c++/* ${LIBDIR}/crt0.o
INCLUDE = -nostdinc -I${INCDIR} -I${INCDIR}/c++
CFLAGS = ${OPT} ${ARCHFLAGS} ${CCODEGEN} ${CWARN} ${INCLUDE} ${DEBUG}
C++FLAGS = ${OPT} ${ARCHFLAGS} ${C++CODEGEN} ${C++WARN} ${INCLUDE} ${DEBUG}
LFLAGS = -L${LIBDIR} -nostdlib \
	-Wl,${LIBDIR}/crt0.o,--warn-common,-X,--oformat,elf32-i386
RELEASE = $(shell ${UTILSDIR}/release.sh)

ifdef DEBUG
	override DEBUG = -DDEBUG
else
	STRIP = strip -s ${COBJS} ${C++OBJS}
endif

CNAMES = \
	adduser \
	archman \
	bootmenu \
	cal \
	calc \
	cat \
	cdrom \
	chkdisk \
	clock \
	cmdwin \
	computer \
	confedit \
	console \
	copy-boot \
	copy-mbr \
	cp \
	date \
	defrag \
	deluser \
	deskwin \
	disks \
	disprops \
	domainname \
	edit \
	file \
	filebrowse \
	filesys \
	find \
	fontutil \
	format \
	help \
	hexdump \
	host \
	hostname \
	iconwin \
	ifconfig \
	imgboot \
	imgedit \
	install \
	keyboard \
	keymap \
	kill \
	login \
	logout \
	ls \
	lsdev \
	md5 \
	mem \
	mines \
	mkdir \
	more \
	mount \
	mv \
	netsniff \
	netstat \
	nm \
	passwd \
	ping \
	progman \
	ps \
	ramdisk \
	reboot \
	renice \
	rm \
	rmdir \
	screenshot \
	sha1pass \
	sha1sum \
	sha256pass \
	sha256sum \
	shutdown \
	snake \
	software \
	store \
	sync \
	sysdiag \
	tar \
	telnet \
	test \
	touch \
	umount \
	uname \
	unzip \
	uptime \
	users \
	view \
	vsh \
	vspinst \
	vspmake \
	wallpaper \
	wget \
	who \
	zip

COBJS = $(addprefix ${OUTPUTDIR}/, ${CNAMES})
C++OBJS = $(addprefix ${OUTPUTDIR}/, ${C++NAMES})

# Targets

all: target-dirs strip
	${MAKE} -C fdisk ARCH=${ARCH} BUILDDIR=${BUILDDIR}
	${MAKE} -C de BUILDDIR=${BUILDDIR}
	${MAKE} -C es BUILDDIR=${BUILDDIR}
	${MAKE} -C ru BUILDDIR=${BUILDDIR}
	${MAKE} -C tr BUILDDIR=${BUILDDIR}

target-dirs:
	mkdir -p ${OUTPUTDIR}

${OUTPUTDIR}/%: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lwindow -lvis -lintl -lvsh -lc -lgcc -o $@

${OUTPUTDIR}/%: %.cpp ${STDDEPS}
	${CC++} ${C++FLAGS} ${LFLAGS} $< -lstdc++ -lc -lgcc -o $@

strip: ${COBJS} ${C++OBJS}
	${STRIP}

clean:
	${MAKE} -C fdisk BUILDDIR=${BUILDDIR} clean
	${MAKE} -C de BUILDDIR=${BUILDDIR} clean
	${MAKE} -C es BUILDDIR=${BUILDDIR} clean
	${MAKE} -C ru BUILDDIR=${BUILDDIR} clean
	${MAKE} -C tr BUILDDIR=${BUILDDIR} clean
	rm -Rf *~ *.o core ${COBJS} ${C++OBJS}

# Object files with explicit rules

${OUTPUTDIR}/archman: archman.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lcompress -lwindow -lvis -lintl -lc -lgcc -o $@

${OUTPUTDIR}/copy-boot: copy-boot.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lintl -lc -lgcc -o $@

${OUTPUTDIR}/format: format.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lwindow -lvis -lintl -ldl -lvsh -lc -o $@

${OUTPUTDIR}/software: software.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -linstall -lcompress -lwindow -lvis -lintl -lvsh -lc -lgcc -o $@

${OUTPUTDIR}/store: store.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} -D_VERSION_=\"${RELEASE}\" $< -linstall -lcompress -lpthread -lintl -lc -o $@

${OUTPUTDIR}/tar: tar.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lcompress -lintl -lvsh -lc -lgcc -o $@

${OUTPUTDIR}/telnet: telnet.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -ltelnet -lwindow -lvis -lintl -lc -lgcc -o $@

${OUTPUTDIR}/test: test.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lwindow -lvis -lintl -ldl -lc -lgcc -o $@

${OUTPUTDIR}/unzip: unzip.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lcompress -lintl -lvsh -lc -lgcc -o $@

${OUTPUTDIR}/view: view.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lhtml -lxml -lwindow -lvis -lintl -lc -lgcc -o $@

${OUTPUTDIR}/vspinst: vspinst.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -linstall -lcompress -lintl -lc -lgcc -o $@

${OUTPUTDIR}/vspmake: vspmake.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -linstall -lcompress -lintl -lc -lgcc -o $@

${OUTPUTDIR}/wget: wget.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lhttp -lvis -lintl -lvsh -lc -o $@

${OUTPUTDIR}/zip: zip.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lcompress -lintl -lvsh -lc -lgcc -o $@

