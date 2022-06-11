##
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
## 
##  Makefile
##

BUILDDIR=../../build/programs
LIBDIR=../../build/system/libraries

CC		= gcc
RELEASE		= $(shell ../../utils/release.sh)
CPU		= $(shell ../../utils/cpu.sh)
STDDEPS		= Makefile* ${LIBDIR}/*.a
CODEGEN		= -O2 ${CPU} -fno-strength-reduce -fno-strict-aliasing \
		-ffreestanding
WARN		= -Wall -W -Wshadow -Wmissing-prototypes -Wstrict-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror
INCLUDE         = -nostdinc -I../include
CFLAGS		= -pipe ${CODEGEN} ${WARN} ${INCLUDE}
LFLAGS		= -L${LIBDIR} -nostdlib -nodefaultlibs -nostartfiles -Wl,../../build/system/libraries/crt0.o,--warn-common,-X,--oformat,elf32-i386,-Ttext,0x0
OPTS		= ${CFLAGS} ${LFLAGS}

NAMES	= adduser \
	cat \
	cdrom \
	chkdisk \
	clock \
	confedit \
	console \
	cp \
	date \
	disks \
	disprops \
	fdisk \
	find \
	format \
	help \
	imgboot \
	install \
	keymap \
	kill \
	loadfont \
	login \
	logout \
	ls \
	md5 \
	mem \
	mkdir \
	more \
	mount \
	mv \
	passwd \
	progman \
	ps \
	reboot \
	renice \
	rm \
	rmdir \
	screenshot \
	shutdown \
	sync \
	touch \
	umount \
	uname \
	uptime \
	users \
	view \
	vsh \
	wallpaper \
	window

OBJS = $(addprefix ${BUILDDIR}/, ${NAMES})

# Targets

all: target-dirs ${OBJS}
	strip -s ${OBJS}
	#make -C mines

target-dirs:
	mkdir -p ${BUILDDIR}

${BUILDDIR}/%: %.c ${STDDEPS}
	${CC} ${OPTS} $< -lwindow -lvsh -lc -o $@

clean:
	rm -Rf *~ *.o core ${OBJS}
	make -C mines clean

# Object files with explicit rules


${BUILDDIR}/fdisk: fdisk.c fdisk.h ${STDDEPS}
	${CC} ${OPTS} ${PARTLOGIC} $< -lwindow -lvsh -lc -o $@

${BUILDDIR}/login: login.c ${STDDEPS}
	${CC} ${OPTS} -DRELEASE=\"${RELEASE}\" $< -lwindow -lvsh -lc -o $@
