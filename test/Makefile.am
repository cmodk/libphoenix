ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}
AM_CFLAGS=${common_CFLAGS} -static
AM_LDFLAGS=${common_LDFLAGS} -static


bin_PROGRAMS=reference_device test_database generate_key test_certificate
reference_device_SOURCES=\
  reference_device.c 
reference_device_LDADD=../src/.libs/libphoenix.a -lmosquitto -lssl -lcrypto -lsqlite3 -lcurl -lm
test_database_SOURCES=\
		      test_database.c
test_database_LDADD=../src/.libs/libphoenix.a -lsqlite3 -lcurl -lm -lmosquitto

generate_key_SOURCES=\
		     generate_key.c
generate_key_LDADD=../src/.libs/libphoenix.a -lssl -lcrypto -lsqlite3 -lmosquitto -lcurl -lz -lm

test_certificate_LDADD=../src/.libs/libphoenix.a -lssl -lcrypto -lsqlite3 -lmosquitto -lcurl -lz -lm



