AC_INIT([phoenix], [2019.3.6], [casper.mogensen@gmail.com])
AM_INIT_AUTOMAKE
LT_INIT
AC_PROG_CC
AC_PROG_RANLIB

# Checks for library functions.
common_CFLAGS="\
	-I../src \
	-Wall -g -Werror=implicit-function-declaration"
AC_SUBST([common_CFLAGS])

common_LDFLAGS="-lm -lcurl -lz"
AC_SUBST([common_LDFLAGS])



AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])
AC_OUTPUT

