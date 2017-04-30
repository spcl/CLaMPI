AC_INIT([libclampi], [1.0], [salvo.dg@gmail.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AM_PROG_AR

LT_PREREQ([2.2])
LT_INIT([])

CLAMPI_WITH_DMAPP
CLAMPI_WITH_FOMPI
CLAMPI_WITH_LIBLSB

AM_CONDITIONAL([HAVE_LIBLSB], [test x$liblsb_found = xyes])

AC_ARG_ENABLE([adaptive], AS_HELP_STRING([--enable-adaptive], [Enable adaptive scheme]))
AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [Print debug info]))


AS_IF([test "x$enable_adaptive" = "xyes"], [
  AC_DEFINE(CL_ADAPTIVE, 1, enables adaptive)
])

AS_IF([test "x$enable_debug" = "xyes"], [
  AC_DEFINE(CLDEBUG, 1, enables debug)
])


AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT
