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
AC_INIT(eegpanel,[lib_current.lib_age.lib_revision],nicolas.bourdaud@epfl.ch)
AC_CONFIG_SRCDIR([src/eegpanel.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config/config.h])
AM_INIT_AUTOMAKE([-Wall])
AC_REQUIRE_AUX_FILE([convert-image.sh])
AC_REQUIRE_AUX_FILE([file-to-string.sh])

# Initialize Automake
AM_INIT_AUTOMAKE([subdir-objects no-dist-gzip dist-bzip2 color-tests])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CANONICAL_HOST
AC_CANONICAL_BUILD

# Checks for programs.
AC_PROG_CC_C99
LT_INIT([win32-dll disable-static])
AC_SUBST(LIBTOOL_DEPS)

AM_PATH_GTK_2_0(2.12.0,,AC_MSG_ERROR(GTK+ libraries 2.12 or more recent should be installed),[gthread])

AC_SEARCH_LIBS([rtf_create_filter], [rtfilter], [], AC_MSG_ERROR([The rtfilter library must be installed.]))

AC_SUBST([CURRENT],[lib_current])
AC_SUBST([REVISION],[lib_revision])
AC_SUBST([AGE],[lib_age])

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])
AC_OUTPUT

