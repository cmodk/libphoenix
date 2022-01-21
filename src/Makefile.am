lib_LTLIBRARIES = libphoenix.la

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}
AM_CFLAGS=${common_CFLAGS}
AM_LDFLAGS=${common_LDFLAGS}

if CLOUDGATE
AM_CFLAGS+=-DCLOUDGATE
endif

nobase_include_HEADERS=phoenix.h debug.h

libphoenix_la_SOURCES=\
		connection.c \
		mqtt.c \
		http.c \
		provisioning.c \
		db.c \
		db_commands.c
libphoenix_la_LDFLAGS=-lsqlite3
