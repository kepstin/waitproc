AC_PREREQ([2.65])
AC_INIT([waitproc], [1.0], [calvin.walton@gmail.com])
AC_CONFIG_SRCDIR([waitproc.c])

AM_INIT_AUTOMAKE([1.11 foreign])

AC_CANONICAL_HOST
AS_CASE([$build_os],
        [linux*], [:],
	[AC_MSG_ERROR([This application is not portable to non-Linux])])

AC_PROG_CC

AC_CHECK_LIB([popt],[poptGetContext])
AS_IF([test x$ac_cv_lib_popt_poptGetContext = xno],
      [AC_MSG_ERROR([The popt library could not be located])])

AC_CHECK_HEADER([popt.h])
AS_IF([test x$ac_cv_header_popt_h = xno],
      [AC_MSG_ERROR([The popt headers could not be located])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
