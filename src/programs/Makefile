##
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
## 
##  Makefile
##

BUILDDIR=../../build/programs

CC		= gcc
RELEASE		= $(shell ../../utils/release.sh)
STDDEPS		= Makefile*
INCLUDES	= -I../include
CFLAGS		= -nostdlib -nodefaultlibs -nostartfiles -nostdinc -O2 \
		-mcpu=pentium -fno-strength-reduce -fno-strict-aliasing \
		-pipe -Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror \
		-L../../build/system/libraries \
		-Wl,../../build/system/libraries/crt0.o,--warn-common,-X,--oformat,elf32-i386,-Ttext,0x0

OBJS	= ${BUILDDIR}/adduser \
	${BUILDDIR}/cat \
	${BUILDDIR}/cdrom \
	${BUILDDIR}/chkdisk \
	${BUILDDIR}/clock \
	${BUILDDIR}/confedit \
	${BUILDDIR}/console \
	${BUILDDIR}/cp \
	${BUILDDIR}/date \
	${BUILDDIR}/disks \
	${BUILDDIR}/disprops \
	${BUILDDIR}/fdisk \
	${BUILDDIR}/find \
	${BUILDDIR}/format \
	${BUILDDIR}/help \
	${BUILDDIR}/imgboot \
	${BUILDDIR}/install \
	${BUILDDIR}/keymap \
	${BUILDDIR}/kill \
	${BUILDDIR}/loadfont \
	${BUILDDIR}/login \
	${BUILDDIR}/logout \
	${BUILDDIR}/ls \
	${BUILDDIR}/md5 \
	${BUILDDIR}/mem \
	${BUILDDIR}/mkdir \
	${BUILDDIR}/more \
	${BUILDDIR}/mount \
	${BUILDDIR}/move \
	${BUILDDIR}/mv \
	${BUILDDIR}/passwd \
	${BUILDDIR}/progman \
	${BUILDDIR}/ps \
	${BUILDDIR}/reboot \
	${BUILDDIR}/renice \
	${BUILDDIR}/rm \
	${BUILDDIR}/rmdir \
	${BUILDDIR}/screenshot \
	${BUILDDIR}/shutdown \
	${BUILDDIR}/sync \
	${BUILDDIR}/touch \
	${BUILDDIR}/umount \
	${BUILDDIR}/uname \
	${BUILDDIR}/uptime \
	${BUILDDIR}/users \
	${BUILDDIR}/view \
	${BUILDDIR}/vsh \
	${BUILDDIR}/wallpaper \
	${BUILDDIR}/window

all: target-dirs ${OBJS}
	#make -C mines

target-dirs:
	mkdir -p ${BUILDDIR}

${BUILDDIR}/adduser: adduser.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} adduser.c -lvsh -lc -o $@

${BUILDDIR}/cat: cat.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} cat.c -lvsh -lc -o $@

${BUILDDIR}/cdrom: cdrom.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} cdrom.c -lc -o $@

${BUILDDIR}/chkdisk: chkdisk.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} chkdisk.c -lc -o $@

${BUILDDIR}/clock: clock.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} clock.c -lc -o $@

${BUILDDIR}/confedit: confedit.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} confedit.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/console: console.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} console.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/cp: cp.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} cp.c -lvsh -lc -o $@

${BUILDDIR}/date: date.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} date.c -lc -o $@

${BUILDDIR}/disks: disks.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} disks.c -lc -o $@

${BUILDDIR}/disprops: disprops.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} disprops.c -lwindow -lc -o $@

${BUILDDIR}/fdisk: fdisk.c fdisk.h ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} fdisk.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/find: find.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} find.c -lvsh -lc -o $@

${BUILDDIR}/format: format.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} format.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/help: help.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} help.c -lc -o $@

${BUILDDIR}/imgboot: imgboot.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} imgboot.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/install: install.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} install.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/keymap: keymap.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} keymap.c -lwindow -lc -o $@

${BUILDDIR}/kill: kill.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} kill.c -lc -o $@

${BUILDDIR}/loadfont: loadfont.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} loadfont.c -lvsh -lc -o $@

${BUILDDIR}/login: login.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} -DRELEASE=\"${RELEASE}\" login.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/logout: logout.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} logout.c -lc -o $@

${BUILDDIR}/ls: ls.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} ls.c -lvsh -lc -o $@

${BUILDDIR}/md5: md5.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} md5.c -lc -o $@

${BUILDDIR}/mem: mem.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} mem.c -lc -o $@

${BUILDDIR}/mines: mines.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} mines.c -lc -o $@

${BUILDDIR}/mkdir: mkdir.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} mkdir.c -lvsh -lc -o $@

${BUILDDIR}/more: more.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} more.c -lvsh -lc -o $@

${BUILDDIR}/mount: mount.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} mount.c -lvsh -lc -o $@

${BUILDDIR}/move: move.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} move.c -lvsh -lc -o $@

${BUILDDIR}/mv: mv.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} mv.c -lvsh -lc -o $@

${BUILDDIR}/passwd: passwd.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} passwd.c -lvsh -lc -o $@

${BUILDDIR}/progman: progman.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} progman.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/ps: ps.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} ps.c -lc -o $@

${BUILDDIR}/reboot: reboot.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} reboot.c -lc -o $@

${BUILDDIR}/renice: renice.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} renice.c -lc -o $@

${BUILDDIR}/rm: rm.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} rm.c -lvsh -lc -o $@

${BUILDDIR}/rmdir: rmdir.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} rmdir.c -lvsh -lc -o $@

${BUILDDIR}/screenshot: screenshot.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} screenshot.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/shutdown: shutdown.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} shutdown.c -lwindow -lc -o $@

${BUILDDIR}/sync: sync.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} sync.c -lc -o $@

${BUILDDIR}/touch: touch.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} touch.c -lvsh -lc -o $@

${BUILDDIR}/umount: umount.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} umount.c -lvsh -lc -o $@

${BUILDDIR}/uname: uname.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} uname.c -lc -o $@

${BUILDDIR}/uptime: uptime.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} uptime.c -lc -o $@

${BUILDDIR}/users: users.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} users.c -lwindow -lc -o $@

${BUILDDIR}/view: view.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} view.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/vsh: vsh.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} -DRELEASE=\"${RELEASE}\" vsh.c -lvsh -lc -o $@

${BUILDDIR}/wallpaper: wallpaper.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} wallpaper.c -lwindow -lvsh -lc -o $@

${BUILDDIR}/window: window.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} window.c -lwindow -lvsh -lc -o $@

# Miscellany

clean:
	rm -f *~ *.o core
	rm -f ${OBJS}
	make -C mines clean
