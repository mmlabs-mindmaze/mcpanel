# - If the library source code has changed at all since the last update,
# then increment revision.
# - If any interfaces have been added, removed, or changed since the last
# update, increment current, and set revision to 0.
# - If any interfaces have been added since the last public release, then
# increment age.
# - If any interfaces have been removed since the last public release, then
# set age to 0.

m4_define([lib_current],0)
m4_define([lib_revision],0)
m4_define([lib_age],0)


# Setup Automake
AC_INIT(mcpanel,[lib_current.lib_revision],nicolas.bourdaud@gmail.com)
AC_CONFIG_SRCDIR([src/mcpanel.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_LIBOBJ_DIR([lib])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config/config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([subdir-objects no-dist-gzip dist-bzip2 color-tests])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CANONICAL_HOST
AC_CANONICAL_BUILD

# Checks for programs.
AC_PROG_CC_C99
LT_INIT([win32-dll disable-static])
AC_SUBST(LIBTOOL_DEPS)

gl_LD_OUTPUT_DEF
AC_DEF_API_EXPORT_ATTRS

PKG_CHECK_MODULES_EXT(GTK2, gtk+-2.0)
PKG_CHECK_MODULES_EXT(GTHREAD2, gthread-2.0)

AC_SEARCH_LIBS([rtf_create_filter], [rtfilter], [], AC_MSG_ERROR([The rtfilter library must be installed.]))
AC_SEARCH_LIBS([cos], [m])

AC_ARG_ENABLE([debug],
	AC_HELP_STRING([--enable-debug@<:@=ARG@:>@],[Enable debugging @<:@DEFAULT=no@:>@]),
	[case "${enableval}" in
		yes) debug=true ;;
		no) debug=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
	esac], [debug=false])

if test "x$debug" = "xfalse"; then
	AC_DEFINE([G_DISABLE_CAST_CHECKS], [], [Define to disable the glib checks])
fi

# Replacement checks
AC_SEARCH_LIBS([clock_gettime], [rt posix4])
AC_SEARCH_LIBS([clock_nanosleep], [rt posix4])
AC_CHECK_TYPES([struct timespec, clockid_t])
AC_CHECK_DECLS([clock_gettime, clock_nanosleep],,,[#include <time.h>])
AC_CHECK_FUNCS([nanosleep gettimeofday ftime _ftime])
AC_CHECK_FUNC_FNARG([GetSystemTimeAsFileTime], [0], [#include <windows.h>])
AC_REPLACE_FUNCS([clock_gettime clock_nanosleep])

AC_SUBST([CURRENT],[lib_current])
AC_SUBST([REVISION],[lib_revision])
AC_SUBST([AGE],[lib_age])

AC_CONFIG_FILES([Makefile lib/Makefile src/Makefile test/Makefile])
AC_OUTPUT

