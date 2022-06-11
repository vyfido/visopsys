##
##  Visopsys
##  Copyright (C) 1998-2014 J. Andrew McLaughlin
## 
##  Makefile
##

ROOT		= ../..
UTILSDIR	= ${ROOT}/utils
BUILDDIR	= ${ROOT}/build/programs
INCDIR		= ../include
LIBDIR		= ${ROOT}/build/system/libraries
MSGDIR		= ${ROOT}/build/system/locale

include ${ROOT}/Makefile.include

ifneq (${PLUS},)
	PLUSNAMES	= sysdiag
endif
ifneq (${BUILDLANG},)
	DEFLANG		= -DBUILDLANG=\"${BUILDLANG}\"
	MAKEMSGS	= ${UTILSDIR}/makemsgs.sh ${BUILDLANG} ${MSGDIR}
endif

STDDEPS		= ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h \
		${INCDIR}/sys/*.h ${LIBDIR}/crt0.o
INCLUDE		= -nostdinc -I${INCDIR} -I${INCDIR}/c++
DEBUG		= # -DDEBUG # Uncomment to turn on debugging
CFLAGS		= ${OPT} ${ARCH} ${CCODEGEN} ${CWARN} ${INCLUDE} ${DEBUG} \
		${DEFLANG}
C++FLAGS	= ${OPT} ${ARCH} ${C++CODEGEN} ${C++WARN} ${INCLUDE}
LFLAGS		= -L${LIBDIR} -nostdlib \
		-Wl,${LIBDIR}/crt0.o,--warn-common,-X,--oformat,elf32-i386

ifeq (${DEBUG},)
	STRIP = strip -s ${COBJS} ${C++OBJS}
endif 

CNAMES	= adduser \
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
	hostname \
	iconwin \
	ifconfig \
	imgboot \
	install \
	keymap \
	kill \
	loadfont \
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
	shutdown \
	snake \
	sync \
	telnet \
	test \
	touch \
	umount \
	uname \
	uptime \
	users \
	view \
	vsh \
	wallpaper \
	${PLUSNAMES}

COBJS	= $(addprefix ${BUILDDIR}/, ${CNAMES})
C++OBJS	= $(addprefix ${BUILDDIR}/, ${C++NAMES})

# Targets

all: target-dirs strip messages
	make -C fdisk

target-dirs:
	mkdir -p ${BUILDDIR}

${BUILDDIR}/%: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lwindow -lintl -lvsh -ldl -lc -lgcc -o $@

${BUILDDIR}/%: %.cpp ${STDDEPS}
	${CC++} ${C++FLAGS} -nostartfiles ${LFLAGS} $< -lstdc++ -lc -lgcc -o $@

strip: ${COBJS} ${C++OBJS}
	${STRIP}

messages: ${OBJS}
	${MAKEMSGS}

clean:
	rm -Rf *~ *.o core ${COBJS} ${C++OBJS}
	make -C fdisk clean

# Object files with explicit rules

${BUILDDIR}/copy-boot: copy-boot.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} -DVISOPSYS $< -lc -o $@

${BUILDDIR}/format: format.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lntfs -lwindow -lintl -lvsh -lc -o $@
