AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4
AM_CFLAGS = -std=gnu11 -g -O3 -flto -fuse-linker-plugin -Isrc -Isrc/common

EXTRA_DIST = \
README.rst

bin_PROGRAMS = bin/ts_debug bin/resync

RESYNC_SOURCE_FILES = \
src/resync/ts_adaptation_field.c \
src/resync/ts_packet.c \
src/resync/ts_packets.c \
src/resync/main.c

RESYNC_HEADER_FILES = \
src/resync/ts_adaptation_field.h \
src/resync/ts_packet.h \
src/resync/ts_packets.h

TS_DEBUG_SOURCE_FILES = \
src/ts_debug/ts_packet.c \
src/ts_debug/ts_packets.c \
src/ts_debug/ts_psi.c \
src/ts_debug/ts_pes.c \
src/ts_debug/ts_stream.c \
src/ts_debug/ts_streams.c \
src/ts_debug/main.c

TS_DEBUG_HEADER_FILES = \
src/ts_debug/ts_packet.h \
src/ts_debug/ts_packets.h \
src/ts_debug/ts_psi.h \
src/ts_debug/ts_pes.h \
src/ts_debug/ts_stream.h \
src/ts_debug/ts_streams.h


bin_ts_debug_SOURCES = $(TS_DEBUG_SOURCE_FILES) $(TS_DEBUG_HEADER_FILES)
bin_ts_debug_LDADD = -ldynamic

bin_resync_SOURCES = $(RESYNC_SOURCE_FILES) $(RESYNC_HEADER_FILES)
bin_resync_LDADD = -ldynamic

MAINTAINERCLEANFILES = aclocal.m4 config.h.in configure Makefile.in docs/Makefile.in
maintainer-clean-local:; rm -rf autotools m4

CLEANFILES = {.,src}/*.{gcno,gcda,gcov}
