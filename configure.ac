AC_INIT([goldiservices],[1.0])
AM_INIT_AUTOMAKE
AC_ARG_ENABLE([cu],
[  --enable-cu],
[case "${enableval}" in
  yes) cu=true ;;
  no)  cu=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-cu]) ;;
esac],[cu=false])
AM_CONDITIONAL([CONTROLUNIT], [test x$cu = xtrue])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_PROG_RANLIB
AC_SUBST([GSTREAMER_CFLAGS])
AC_SUBST([GSTREAMER_LIBS])
PKG_CHECK_MODULES([GSTREAMER],[gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0])
AC_PROG_CC
AC_OUTPUT
