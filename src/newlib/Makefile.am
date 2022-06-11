## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

INCLUDES = $(NEWLIB_CFLAGS) $(CROSS_CFLAGS) $(TARGET_CFLAGS)

noinst_LIBRARIES = lib.a

lib_a_SOURCES = \
	close.c execve.c fcntl.c fork.c fstat.c getpid.c gettod.c isatty.c \
	kill.c link.c lseek.c open.c read.c sbrk.c stat.c times.c unlink.c \
	wait.c write.c

include $(srcdir)/../../../Makefile.shared

all: crt0.o

ACLOCAL_AMFLAGS = -I ../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
