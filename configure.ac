AC_INIT(eegpanel,0.2)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
#AC_CONFIG_LIBOBJ_DIR([lib])
AC_CONFIG_HEADERS([config/config.h])
AM_INIT_AUTOMAKE([-Wall])
AC_REQUIRE_AUX_FILE([convert-image.sh])
AC_REQUIRE_AUX_FILE([file-to-string.sh])

AC_PROG_CC_C99
AM_PROG_CC_C_O
AM_PATH_GTK_2_0(2.12.0,,AC_MSG_ERROR(GTK+ libraries 2.12 or more recent should be installed),[gthread])

#AC_DISABLE_SHARED
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

AC_HEADER_STDC
AC_HEADER_TIME
AC_C_CONST
AC_C_VOLATILE

#AC_SEARCH_LIBS(OpenDriverAsync, [act2api], [], AC_MSG_ERROR([The library act2api must be installed]))
#AC_SEARCH_LIBS([pthread_create], [pthread posix4])
#AC_SEARCH_LIBS(clock_gettime, [rt posix4])

#AC_CHECK_DECLS([clock_gettime, nanosleep], [], [], [[#include <time.h>]])
#AC_CHECK_TYPES([[struct timespec], [clockid_t]])
#AC_REPLACE_FUNCS([clock_gettime nanosleep])
#AC_CHECK_FUNCS([gettimeofday ftime _ftime select Sleep])


AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])
AC_OUTPUT

