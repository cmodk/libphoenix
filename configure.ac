AC_INIT([phoenix], [2019.3.6], [casper.mogensen@gmail.com])
AM_INIT_AUTOMAKE
LT_INIT
AC_PROG_CC
AC_PROG_RANLIB

AC_ARG_ENABLE([cloudgate],
[  --enable-cloudgate    Build for cloudgate],
[case "${enableval}" in
  yes) cloudgate=true ;;
  no)  cloudgate=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-cloudgate]) ;;
esac],[cloudgate=false])
AM_CONDITIONAL([CLOUDGATE], [test x$cloudgate = xtrue])

# Checks for library functions.
common_CFLAGS="\
	-I../src \
	-Wall -g -Werror=implicit-function-declaration -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast \
        -Wl,--start-group"
AC_SUBST([common_CFLAGS])

common_LDFLAGS="-lm -lcurl -lz -ljson-c -lssl -lcrypto -lpthread" 
AC_SUBST([common_LDFLAGS])



AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])
AC_OUTPUT

