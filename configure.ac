AC_PREREQ([2.68])
AC_INIT([libvmod-gossip], [trunk], [], [vmod-gossip])
AC_COPYRIGHT([Copyright (c) Carlos Abalde <carlos.abalde@gmail.com>])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR(src/vmod_gossip.vcc)
AC_CONFIG_HEADER([config.h])

AC_GNU_SOURCE

AM_INIT_AUTOMAKE([1.12 -Wall -Werror foreign parallel-tests])
AM_SILENT_RULES([yes])
AM_PROG_AR

LT_PREREQ([2.2.6])
LT_INIT([dlopen disable-static])

ax_enable_compile_warnings=error
AX_CODE_COVERAGE
AX_COMPILER_FLAGS_CFLAGS
AX_PTHREAD(,[AC_MSG_ERROR([Could not configure pthreads support])])

LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

AC_ARG_WITH([rst2man],
    [
        AS_HELP_STRING(
            [--with-rst2man=PATH],
            [Location of rst2man (auto)])
    ],
    [RST2MAN="$withval"],
    [AC_CHECK_PROGS(RST2MAN, [rst2man rst2man.py], [])])

# Check for pkg-config
PKG_PROG_PKG_CONFIG

m4_ifndef([VARNISH_PREREQ], AC_MSG_ERROR([Need varnish.m4 -- see README.rst]))

#VARNISH_PREREQ([6.0], [6.1])
VARNISH_VMODS([gossip])

VMOD_TESTS="$(cd $srcdir/src && echo tests/*.vtc)"
AC_SUBST(VMOD_TESTS)

PKG_CHECK_VAR([LIBVARNISHAPI_LIBDIR], [varnishapi], [libdir])
AC_SUBST([VARNISH_LIBRARY_PATH],
	[$LIBVARNISHAPI_LIBDIR:$LIBVARNISHAPI_LIBDIR/varnish])

save_CPPFLAGS=$CPPFLAGS
CPPFLAGS=$VARNISHAPI_CFLAGS
AC_CHECK_HEADERS([cache/cache_varnishd.h])
CPPFLAGS=$save_CPPFLAGS

AC_CONFIG_FILES([
	Makefile
	src/Makefile
])
AC_OUTPUT
