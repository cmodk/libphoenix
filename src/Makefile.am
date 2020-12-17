lib_LTLIBRARIES = libphoenix.la

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}
AM_CFLAGS=${common_CFLAGS}
AM_LDFLAGS=${common_LDFLAGS}

nobase_include_HEADERS=phoenix.h

libphoenix_la_SOURCES=\
		mqtt.c \
		provisioning.c \
		db.c
libphoenix_la_LDFLAGS=-lsqlite3
